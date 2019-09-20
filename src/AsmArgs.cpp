#include <string>
#include <cctype>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "../include/AsmTables.h"
#include "../include/AsmArgs.h"
#include "../include/CoreTypes.h"
#include "../include/Utility.h"

namespace CSX64
{
void AssembleArgs::UpdateLinePos()
{
	// update current line pos
	switch (current_seg)
	{
	case AsmSegment::TEXT: line_pos_in_seg = file.Text.size(); break;
	case AsmSegment::RODATA: line_pos_in_seg = file.Rodata.size(); break;
	case AsmSegment::DATA: line_pos_in_seg = file.Data.size(); break;
	case AsmSegment::BSS: line_pos_in_seg = file.BssLen; break;

	default:; // default does nothing - (nothing to update)
	}
}

bool AssembleArgs::TryExtractLineHeader(std::string &rawline)
{
	// (label:) (times/if imm) (op (arg, arg, ...))

	std::size_t pos, end; // position in line parsing

	// -- parse label prefix -- //

	// find the first white space delimited token
	for (pos = 0; pos < rawline.size() && std::isspace((unsigned char)rawline[pos]); ++pos);
	for (end = pos; end < rawline.size() && !std::isspace((unsigned char)rawline[end]); ++end);

	// if we got a label
	if (pos < rawline.size() && rawline[end - 1] == LabelDefChar)
	{
		// set as label def
		label_def = rawline.substr(pos, end - pos - 1);

		// get another white space delimited token
		for (pos = end; pos < rawline.size() && std::isspace((unsigned char)rawline[pos]); ++pos);
		for (end = pos; end < rawline.size() && !std::isspace((unsigned char)rawline[end]); ++end);
	}
	// otherwise there's no label for this line
	else label_def.clear();

	// -- parse times prefix -- //

	std::unique_ptr<Expr> rep_expr;
	std::string err;

	// extract the found token (as upper case)
	std::string tok = ToUpper(rawline.substr(pos, end - pos));

	// decode tok 0 is no TIMES/IF prefix, 1 is TIMES prefix, 2 is IF prefix
	const int rep_code = tok == "TIMES" ? 1 : tok == "IF" ? 2 : 0;

	// if we got a TIMES/IF prefix (nonzero rep code)
	if (rep_code != 0)
	{
		// extract an expression starting at end (the number of times to repeat the instruction) (store aft index into end)
		if (!TryExtractExpr(rawline, end, rawline.size(), rep_expr, end)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": TIMES/IF expected an expression\n-> " + res.ErrorMsg}; return false; }

		// make sure we didn't consume the whole line (that would mean there was a times prefix with nothing to augment)
		if (end == rawline.size()) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Encountered TIMES/IF prefix with no instruction to augment"}; return false; }

		// get the next white space delimited token
		for (pos = end; pos < rawline.size() && std::isspace((unsigned char)rawline[pos]); ++pos);
		for (end = pos; end < rawline.size() && !std::isspace((unsigned char)rawline[end]); ++end);
	}
	// otherwise there's no times prefix for this line
	else times = 1;
		
	// -- process label -- //

	// if we defined a label for this line, insert it now
	if (!label_def.empty())
	{
		// if it's not a local, mark as last non-local label
		if (label_def[0] != '.') last_nonlocal_label = label_def;

		// mutate and test result for legality
		if (!MutateName(label_def)) return false;
		if (!IsValidName(label_def, err)) { res = { AssembleError::InvalidLabel, "line " + tostr(line) + ": " + err }; return false; }

		// it can't be a reserved symbol
		if (IsReservedSymbol(label_def)) { res = { AssembleError::InvalidLabel, "line " + tostr(line) + ": Symbol name is reserved: " + label_def }; return false; }

		// ensure we don't define an external
		if (Contains(file.ExternalSymbols, label_def)) { res = { AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define external symbol internally: " + label_def }; return false; }

		// if this line isn't an EQU directive, inject a label (EQU will handle the insertion otherwise - prevents erroneous ptrdiff simplifications if first defined as a label)
		if (ToUpper(rawline.substr(pos, end - pos)) != "EQU")
		{
			// ensure we don't redefine a symbol. not outside this if because we definitely insert a symbol here, but EQU might not (e.g. if TIMES = 0) - so let EQU decide how to handle that.
			if (ContainsKey(file.Symbols, label_def)) { res = { AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Symbol was already defined: " + label_def }; return false; }

			// addresses must be in a valid segment
			if (current_seg == AsmSegment::INVALID) { res = { AssembleError::FormatError, "line " + tostr(line) + ": Attempt to address outside of a segment" }; return false; }

			Expr temp;
			temp.OP = Expr::OPs::Add;
			temp.Left = Expr::NewToken(SegOffsets.at(current_seg));
			temp.Right = Expr::NewInt(line_pos_in_seg);
			file.Symbols.emplace(label_def, std::move(temp));
		}
	}

	// -- process times -- //

	// if there was a times directive for this line
	if (rep_expr)
	{
		// make sure the repeat count is an instant imm (critical expression)
		u64 val;
		bool floating;
		if (!rep_expr->Evaluate(file.Symbols, val, floating, err)) { res = { AssembleError::UsageError, "line " + tostr(line) + ": TIMES prefix requires a critical expression\n-> " + err }; return false; }

		// if using a TIMES prefix and the imm is floating, that's an error
		if (rep_code == 1 && floating) { res = { AssembleError::UsageError, "line " + tostr(line) + ": TIMES prefix requires an integral expression" }; return false; }

		// use the evaluated expression as the times count (account for TIMES vs IF prefix logic)
		times = rep_code == 2 ? !IsZero(val, floating) ? 1 : 0 : (i64)val;
	}

	// -- finalize -- //

	// chop off everything we consumed (pos currently points to the start of op)
	rawline = rawline.substr(pos);

	return true;
}

bool AssembleArgs::TryFindMatchingParen(const std::string &str, const std::size_t str_begin, const std::size_t str_end, std::size_t &match)
{
	// assert usage conditions
	if (str_begin >= str_end) throw std::invalid_argument("TryFindMatchingParen(): string region cannot be empty");
	if (str[str_begin] != '(') throw std::invalid_argument("TryFindMatchingParen(): str_begin does not point to an open paren");

	std::size_t pos = str_begin + 1; // skip the open paren since we already know about it
	std::size_t quote = -1; // initially not in a quote
	std::size_t depth = 1; // initially 1 because we're skipping the open paren

	// wind through all the characters in the string
	for (; pos < str_end; ++pos)
	{
		if (quote == (std::size_t)-1)
		{
			if (str[pos] == '(') ++depth;
			else if (str[pos] == ')')
			{
				// if we hit depth zero, this is the matching paren we were looking for
				if (--depth == 0) { match = pos; return true; }
			}
			else if (str[pos] == '"' || str[pos] == '\'' || str[pos] == '`') quote = pos;
		}
		else if (str[pos] == str[quote]) quote = -1;
	}

	// if we get here we failed to find a matching closing paren
	res = { AssembleError::FormatError, "Failed to find a matching closing paren" };
	return false;
}

bool AssembleArgs::TrySplitOnCommas(std::vector<std::string> &vec, const std::string &str, const std::size_t str_begin, const std::size_t str_end)
{
	std::size_t pos, end = str_begin - 1; // -1 cancels out inital +1 inside loop for comma skip
	std::size_t depth = 0; // parenthetical depth
	std::size_t quote;

	// parse the rest of the line as comma-separated tokens
	while (true)
	{
		// skip leading white space (+1 skips the preceding comma)
		for (pos = end + 1; pos < str_end && std::isspace((unsigned char)str[pos]); ++pos);
		// when pos reaches end of token, we're done parsing
		if (pos >= str_end) break;

		// find the next terminator (comma-separated)
		for (end = pos, quote = -1; end < str_end; ++end)
		{
			if (quote == (std::size_t)-1)
			{
				if (str[end] == '(') ++depth;
				else if (str[end] == ')')
				{
					// hitting negative depth is totally not allowed ever
					if (depth-- == 0) { res = {AssembleError::FormatError, "Unmatched parens in argument"}; return false; }
				}
				else if (str[end] == '"' || str[end] == '\'' || str[end] == '`') quote = end;
				else if (depth == 0 && str[end] == ',') break; // comma at depth 0 marks end of token
			}
			else if (str[end] == str[quote]) quote = -1;
		}
		// make sure we closed any quotations
		if (quote != (std::size_t)-1) { res = { AssembleError::FormatError, std::string() + "Unmatched quotation encountered in argument list" }; return false; }
		// make sure we closed any parens
		if (depth != 0) { res = { AssembleError::FormatError, "Unmatched parens in argument" }; return false; }

		// get the arg (remove leading/trailing white space - some logic requires them not be there e.g. address parser)
		std::string arg = Trim(str.substr(pos, end - pos));
		// make sure arg isn't empty
		if (arg.empty()) { res = { AssembleError::FormatError, "Empty argument encountered" }; return false; }
		// add this token
		vec.emplace_back(std::move(arg));
	}

	// successfully parsed line
	return true;
}

bool AssembleArgs::SplitLine(const std::string &rawline)
{
	// (op (arg, arg, ...))

	std::size_t pos = 0, end; // position in line parsing
	
	// discard args from last line
	args.clear();

	// -- parse op -- //

	// get the first white space delimited token
	for (; pos < rawline.size() && std::isspace((unsigned char)rawline[pos]); ++pos);
	for (end = pos; end < rawline.size() && !std::isspace((unsigned char)rawline[end]); ++end);

	// if we got something, record as op, otherwise is empty string
	if (pos < rawline.size()) op = rawline.substr(pos, end - pos);
	else op.clear();

	// -- parse args -- //

	// split the rest of the line as comma separated values and store in args
	if (!TrySplitOnCommas(args, rawline, end, rawline.size())) { res = {AssembleError::FormatError, "line " + tostr(line) + ": " + res.ErrorMsg}; return false; }

	return true;
}

bool AssembleArgs::IsValidName(const std::string &token, std::string &err)
{
	// can't be empty string
	if (token.empty()) { err = "Symbol name was empty string"; return false; }

	// first char is underscore or letter
	if (token[0] != '_' && !std::isalpha(token[0])) { err = "Symbol contained an illegal character"; return false; }
	// all other chars may additionally be numbers or periods
	for (int i = 1; i < (int)token.size(); ++i)
		if (token[i] != '_' && token[i] != '.' && !std::isalnum(token[i])) { err = "Symbol contained an illegal character"; return false; }

	return true;
}
bool AssembleArgs::MutateName(std::string &label)
{
	// if defining a local label
	if (label[0] == '.')
	{
		std::string sub = label.substr(1); // local symbol name
		std::string err;

		// local name can't be empty
		if (!IsValidName(sub, err)) { res = AssembleResult{AssembleError::FormatError, "line " + tostr(line) + ": " + err}; return false; }
		// can't make a local symbol before any non-local ones exist
		if (last_nonlocal_label.empty()) { res = AssembleResult{AssembleError::InvalidLabel, "line " + tostr(line) + ": Local symbol encountered before any non-local declarations"}; return false; }

		// mutate the label
		label = last_nonlocal_label + label;
	}

	return true;
}

bool AssembleArgs::TryReserve(u64 size)
{
	// reserve only works in the bss segment
	if (current_seg != AsmSegment::BSS) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Cannot reserve in this segment"}; return false; }

	// reserve the space
	file.BssLen += size;
	return true;
}
bool AssembleArgs::TryAppendVal(u64 size, u64 val)
{
	switch (current_seg)
	{
		// text and ro/data segments are writable
	case AsmSegment::TEXT: Append(file.Text, size, val); return true;
	case AsmSegment::RODATA: Append(file.Rodata, size, val); return true;
	case AsmSegment::DATA: Append(file.Data, size, val); return true;

		// others are not
	default: res = {AssembleError::FormatError, "line " + tostr(line) + ": Cannot write in this segment"}; return false;
	}
}
bool AssembleArgs::TryAppendByte(u8 val)
{
	switch (current_seg)
	{
		// text and ro/data segments are writable
	case AsmSegment::TEXT: file.Text.push_back(val); return true;
	case AsmSegment::RODATA: file.Rodata.push_back(val); return true;
	case AsmSegment::DATA: file.Data.push_back(val); return true;

		// others are not
	default: res = {AssembleError::FormatError, "line " + tostr(line) + ": Cannot write in this segment"}; return false;
	}
}

u8 AssembleArgs::GetCompactImmPrefix(u64 val)
{
	if ((val & (u64)0xffffffffffffff00) == 0) return (u8)0x00;
	if ((~val & (u64)0xffffffffffffff00) == 0) return (u8)0x10;
	
	if ((val & (u64)0xffffffffffff0000) == 0) return (u8)0x01;
	if ((~val & (u64)0xffffffffffff0000) == 0) return (u8)0x11;

	if ((val & (u64)0xffffffff00000000) == 0) return (u8)0x02;
	if ((~val & (u64)0xffffffff00000000) == 0) return (u8)0x12;

	return (u8)0x03;
}

bool AssembleArgs::TryAppendCompactImm(Expr &&expr, u64 sizecode, bool strict)
{
	u64 val;
	bool floating;
	std::string err;

	// if we can evaluate it immediately
	if (expr.Evaluate(file.Symbols, val, floating, err))
	{
		// get the prefix byte - taking into account strict flag and potentially-default sizecode
		u8 prefix = strict ? (u8)(sizecode) : GetCompactImmPrefix(val);

		// write the prefix byte and its appropriately-sized compact imm
		return TryAppendByte(prefix) && TryAppendVal(Size(prefix & 3), val);
	}
	// otherwise it'll need to be a dynamic hole
	else
	{
		
	}

	throw std::runtime_error("append compact imm not currently implemented");
}

bool AssembleArgs::TryAppendExpr(u64 size, Expr &&expr, std::vector<HoleData> &holes, std::vector<u8> &segment)
{
	std::string err; // evaluation error parsing location

	// create the hole data
	HoleData data;
	data.Address = segment.size();
	data.Size = (decltype(data.Size))size;
	data.Line = line;
	data.expr = std::move(expr);

	// write a dummy (all 1's for easy manual identification)
	Append(segment, size, 0xffffffffffffffff);

	// try to patch it
	switch (TryPatchHole(segment, file.Symbols, data, err))
	{
	case PatchError::None: break;
	case PatchError::Unevaluated: holes.emplace_back(std::move(data)); break;
	case PatchError::Error: res = {AssembleError::ArgError, std::move(err)}; return false;

	default: throw std::runtime_error("Unknown patch error encountered");
	}

	return true;
}
bool AssembleArgs::TryAppendExpr(u64 size, Expr &&expr)
{
	switch (current_seg)
	{
	case AsmSegment::TEXT: return TryAppendExpr(size, std::move(expr), file.TextHoles, file.Text);
	case AsmSegment::RODATA: return TryAppendExpr(size, std::move(expr), file.RodataHoles, file.Rodata);
	case AsmSegment::DATA: return TryAppendExpr(size, std::move(expr), file.DataHoles, file.Data);

	default: res = {AssembleError::FormatError, "line " + tostr(line) + ": Cannot write in this segment"}; return false;
	}
}
bool AssembleArgs::TryAppendAddress(u64 a, u64 b, Expr &&hole)
{
	if (!TryAppendByte((u8)a)) return false;
	if (a & 3) { if (!TryAppendByte((u8)b)) return false; }
	if (a & 0x80) { if (!TryAppendExpr(Size((a >> 2) & 3), std::move(hole))) return false; }

	return true;
}

bool AssembleArgs::TryAlign(u64 size)
{
	// it's really important that size is a power of 2, so do a (hopefully redundant) check
	if (!IsPowerOf2(size)) throw std::invalid_argument("alignment size must be a power of 2");

	switch (current_seg)
	{
	case AsmSegment::TEXT:
		Align(file.Text, size);
		file.TextAlign = std::max(file.TextAlign, (decltype(file.TextAlign))size);
		return true;
	case AsmSegment::RODATA:
		Align(file.Rodata, size);
		file.RodataAlign = std::max(file.RodataAlign, (decltype(file.RodataAlign))size);
		return true;
	case AsmSegment::DATA:
		Align(file.Data, size);
		file.DataAlign = std::max(file.DataAlign, (decltype(file.DataAlign))size);
		return true;
	case AsmSegment::BSS:
		file.BssLen = Align(file.BssLen, size);
		file.BSSAlign = std::max(file.BSSAlign, (decltype(file.BSSAlign))size);
		return true;

	default: res = AssembleResult{AssembleError::FormatError, "line " + tostr(line) + ": Cannot align this segment"}; return false;
	}
}
bool AssembleArgs::TryPad(u64 size)
{
	switch (current_seg)
	{
	case AsmSegment::TEXT: Pad(file.Text, size); return true;
	case AsmSegment::RODATA: Pad(file.Rodata, size); return true;
	case AsmSegment::DATA: Pad(file.Data, size); return true;
	case AsmSegment::BSS: file.BssLen += size; return true;

	default: res = AssembleResult{AssembleError::FormatError, "line " + tostr(line) + ": Cannot pad this segment"}; return false;
	}
}

bool AssembleArgs::TryExtractExpr(const std::string &str, const std::size_t str_begin, const std::size_t str_end, std::unique_ptr<Expr> &expr, std::size_t &aft)
{
	std::size_t pos, end; // position in str

	std::unique_ptr<Expr>  term_root; // holds the root of the current term tree
	Expr                  *term_leaf; // holds the leaf of the current term tree (a valid Expr node to be filled out)

	std::vector<Expr*> stack; // the stack used to manage operator precedence rules
	                          // top of stack shall be refered to as current

	Expr::OPs bin_op;     // extracted binary op (initialized so compiler doesn't complain)
	int       bin_op_len; // length of operator found (in characters)

	int unpaired_conditionals = 0; // number of unpaired conditional ops - i.e. number of ?: operators with the 3rd component

	std::string err; // error location for hole evaluation

	// ----------------------------------------------------------------------------------

	expr = nullptr; // null expr so we know it's currently empty

	stack.push_back(nullptr); // stack will always have a null at its base (simplifies code slightly)

	// skip white space
	for (pos = str_begin; pos < str_end && std::isspace(str[pos]); ++pos);
	// if we're past the end, str was empty
	if (pos >= str_end) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Empty expression encountered"}; return false; }

	while (true)
	{
		// -- read (unary op...)[operand](binary op) -- //

		// create a new term tree and mark its root/leaf
		term_root = std::make_unique<Expr>();
		term_leaf = term_root.get();

		// consume unary ops (allows white space)
		for (; pos < str_end; ++pos)
		{
			switch (str[pos])
			{
			case '+': break; // unary plus does nothing - don't even store it
			case '-': term_leaf->OP = Expr::OPs::Neg;    term_leaf = (term_leaf->Left = std::make_unique<Expr>()).get(); break;
			case '~': term_leaf->OP = Expr::OPs::BitNot; term_leaf = (term_leaf->Left = std::make_unique<Expr>()).get(); break;
			case '!': term_leaf->OP = Expr::OPs::LogNot; term_leaf = (term_leaf->Left = std::make_unique<Expr>()).get(); break;

			default:
				if (std::isspace((unsigned char)str[pos])) break; // allow white space
				goto unary_op_loop_aft; // unrecognized non-white is start of operand
			}
		}
		unary_op_loop_aft:
		// if we're past the end, there were unary ops with no operand
		if (pos >= str_end) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Unary ops encountered without an operand"}; return false; }

		int depth = 0;  // parens depth - initially 0
		int quote = -1; // index of current quote char - initially not in one

		bool numeric = std::isdigit((unsigned char)str[pos]); // flag if this is a numeric literal

		// move end to next logical separator (white space, open paren, or binary op - only at depth 0)
		for (end = pos; end < str_end; ++end)
		{
			// if we're not in a quote
			if (quote < 0)
			{
				// account for important characters
				if (str[end] == '(' && (depth != 0 || end == pos)) ++depth;
				else if (str[end] == ')')
				{
					if (depth > 0) --depth; // if depth > 0 drop down a level
					else break; // otherwise this marks the end of the expression
				}
				else if (numeric && (str[end] == 'e' || str[end] == 'E') && end + 1 < str_end && (str[end + 1] == '+' || str[end + 1] == '-')) ++end; // make sure an exponent sign won't be parsed as binary + or - by skipping it
				else if (str[end] == '"' || str[end] == '\'' || str[end] == '`') quote = (int)end; // quotes mark start of a string
				else if (depth == 0 && (std::isspace((unsigned char)str[end]) || str[end] == '(' || TryGetOp(str, end, bin_op, bin_op_len))) break; // break on white space, open paren, or binary op - only at depth 0
			}
			// otherwise we're in a quote
			else
			{
				// if we have a matching quote, end quote mode
				if (str[end] == str[quote]) quote = -1;
			}
		}
		// if depth isn't back to 0, there was a parens mismatch
		if (depth != 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Mismatched parenthesis in expression"}; return false; }
		// if quote isn't back to -1, there was a quote mismatch
		if (quote >= 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Mismatched quotation in expression"}; return false; }
		// if pos == end we'll have an empty token (e.g. expression was just a binary op)
		if (pos == end) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Empty token encountered in expression"}; return false; }

		// -- convert to expression tree -- //

		// if it's a sub-expression
		if (str[pos] == '(')
		{
			// parse the inside into the term leaf
			std::size_t sub_aft;
			std::unique_ptr<Expr> sub_expr;
			if (!TryExtractExpr(str, pos + 1, end - 1, sub_expr, sub_aft)) return false;
			*term_leaf = std::move(*sub_expr);

			// if the sub expression didn't capture the entire parenthetical region then the interior was not a (single) expression
			if (sub_aft != end - 1) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Interior of parenthesis is not an expression"}; return false; }
		}
		// if it's a user-level assembler token
		else if (str[pos] == '$')
		{
			// get the value to insert (uppercase because user-level assembler tokens are case insensitive)
			std::string val = ToUpper(str.substr(pos, end - pos));

			// if it's the current line macro
			if (val == CurrentLineMacro)
			{
				// must be in a segment
				if (current_seg == AsmSegment::INVALID) { res = { AssembleError::FormatError, "line " + tostr(line) + ": Attempt to take an address outside of a segment" }; return false; }

				term_leaf->OP = Expr::OPs::Add;
				term_leaf->Left = Expr::NewToken(SegOffsets.at(current_seg));
				term_leaf->Right = Expr::NewInt(line_pos_in_seg);
			}
			// if it's the start of segment macro
			else if (val == StartOfSegMacro)
			{
				// must be in a segment
				if (current_seg == AsmSegment::INVALID) { res = { AssembleError::FormatError, "line " + tostr(line) + ": Attempt to take an address outside of a segment" }; return false; }

				term_leaf->Token(SegOrigins.at(current_seg));
			}
			// if it's the TIMES iter id macro
			else if (val == TimesIterIdMacro)
			{
				term_leaf->IntResult(times_i);
			}
			// if it's the string/binary literal macro
			else if (val == StringLiteralMacro || val == BinaryLiteralMacro)
			{
				// get the index of the first paren
				for (; end < str_end && str[end] != '('; ++end);
				if (end >= str_end) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Expected a call expression after function-like operator" }; return false; }

				// find the matching (closing) paren and store to end
				std::size_t _start = end;
				if (!TryFindMatchingParen(str, end, str_end, end)) { res = { AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse binary literal expression\n-> " + res.ErrorMsg }; return false; }
				++end; // bump up end to be 1 past the closing paren (for logic outside of the loop)

				// parse the parenthesis interior into an array of comma-separated arguments
				std::vector<std::string> bin_args;
				if (!TrySplitOnCommas(bin_args, str, _start + 1, end - 1)) { res = { AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse binary literal expression\n-> " + res.ErrorMsg }; return false; }

				// construct the binary literal
				std::vector<u8> literal_value;
				for (const std::string &arg : bin_args)
				{
					Expr _expr;
					u64 _sizecode;
					bool _explicit_size, _strict;

					// if it's a string
					if (std::string _str; TryExtractStringChars(arg, _str, err))
					{
						// dump into the literal (one byte each)
						literal_value.reserve(literal_value.size() + _str.size());
						for (char _ch : _str) literal_value.push_back((u8)_ch);
					}
					// if it's a value (imm)
					else if (TryParseImm(arg, _expr, _sizecode, _explicit_size, _strict))
					{
						// we only allow bytes here, so disallow any kind of explicit size control flags
						if (_explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
						if (_strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

						// additionally, expressions used in binary/string literals are critical expressions
						u64 _val;
						bool _floating;
						if (!_expr.Evaluate(file.Symbols, _val, _floating, err)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expressions to binary literals are critical expressions\n-> " + err}; return false; }

						// bounds check it since it's a compile time constant
						if (_val > 0xff) { res = {AssembleError::UsageError, "line " + tostr(line) + ": (byte) expression to binary literal exceeded 255"}; return false; }

						literal_value.push_back((u8)_val);
					}
					// otherwise we failed to parse it
					else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse operand as a string or imm: " + arg + "\n-> " + res.ErrorMsg}; return false; }
				}

				// we handled both string/binary cases here - if it was actually a string literal, append a terminator
				if (val == StringLiteralMacro) literal_value.push_back(0);

				// binary literal is not allowed to be empty
				if (literal_value.empty()) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Attempt to provision empty binary literal"}; return false; }

				// add resulting (non-empty) binary literal to the binary literals collection
				std::size_t literal_index = file.Literals.add(std::move(literal_value));

				// bind a reference to the binary literal in the expression node
				term_leaf->Token(BinaryLiteralSymbolPrefix + tohex(literal_index));
			}
			// if it's a function-like operator
			else if (TryGetValue(FunctionOperator_to_OP, val, term_leaf->OP))
			{
				// get the index of the first paren
				for (; end < str_end && str[end] != '('; ++end);
				if (end >= str_end) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Expected a call expression after function-like operator" }; return false; }

				// extract the call expression (starting just past first paren (end)) - store it into leaf's left
				if (!TryExtractExpr(str, end + 1, str_end, term_leaf->Left, end)) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Failed to parse call expression\n-> " + res.ErrorMsg }; return false; }
				// make sure it ended in a closing paren - also increment end by 1 to pass it for the rest of the parsing logic
				if (end >= str_end || str[end] != ')') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Unterminated call expression"}; return false; }
				++end;
			}
			// otherwise unrecognized
			else { res = {AssembleError::UnknownOp, "line " + tostr(line) + ": Unrecognized special token: " + val}; return false; }
		}
		// otherwise it's a value
		else
		{
			// get the value to insert
			std::string val = str.substr(pos, end - pos);

			// mutate it
			if (!MutateName(val)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse expression\n-> " + res.ErrorMsg}; return false; }

			// create the hole for it
			term_leaf->Token(val);

			// it either needs to be evaluatable or a valid label name
			if (!term_leaf->Evaluatable(file.Symbols) && !IsValidName(val, err)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to resolve token as a valid imm or symbol name: " + val + "\n-> " + err}; return false; }
		}

		// -- append subtree to main tree --

		// if no tree yet, use this one
		if (expr == nullptr) expr = std::move(term_root);
		// otherwise append to current (guaranteed to be defined by second pass)
		else
		{
			// put it in the right (guaranteed by this algorithm to be empty)
			stack.back()->Right = std::move(term_root);
		}

		// -- get binary op -- //

		// we may have stopped token parsing on white space, so wind up to find a binary op
		for (; end < str_end; ++end)
		{
			if (TryGetOp(str, end, bin_op, bin_op_len)) break; // break when we find an op
			// if we hit a non-white character, there are tokens with no binary ops between them - that's the end of the expression
			else if (!std::isspace((unsigned char)str[end])) goto stop_parsing;
		}
		// if we didn't find any binary ops, we're done
		if (end >= str_end) goto stop_parsing;

		// -- process binary op -- //

		// ternary conditional has special rules
		if (bin_op == Expr::OPs::Pair)
		{
			// seek out nearest conditional without a pair
			for (; stack.back() && (stack.back()->OP != Expr::OPs::Condition || stack.back()->Right->OP == Expr::OPs::Pair); stack.pop_back());
			// if we didn't find anywhere to put it, this is an error
			if (stack.back() == nullptr) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expression contained a ternary conditional pair without a corresponding condition"}; return false; }
		}
		// right-to-left operators
		else if (bin_op == Expr::OPs::Condition)
		{
			// wind current up to correct precedence (right-to-left evaluation, so don't skip equal precedence)
			for (; stack.back() && Precedence.at(stack.back()->OP) < Precedence.at(bin_op); stack.pop_back());
		}
		// left-to-right operators
		else
		{
			// wind current up to correct precedence (left-to-right evaluation, so also skip equal precedence)
			for (; stack.back() && Precedence.at(stack.back()->OP) <= Precedence.at(bin_op); stack.pop_back());
		}

		// if we have a valid current
		if (stack.back())
		{
			// splice in the new operator, moving current's right sub-tree to left of new node
			auto _temp = std::make_unique<Expr>();
			_temp->OP = bin_op;
			_temp->Left = std::move(stack.back()->Right);
			stack.back()->Right = std::move(_temp);
			stack.push_back(stack.back()->Right.get());
		}
		// otherwise we'll have to move the root
		else
		{
			// splice in the new operator, moving entire tree to left of new node
			auto _temp = std::make_unique<Expr>();
			_temp->OP = bin_op;
			_temp->Left = std::move(expr);
			expr = std::move(_temp);
			stack.push_back(expr.get());
		}

		// update unpaired conditionals
		if (bin_op == Expr::OPs::Condition) ++unpaired_conditionals;
		else if (bin_op == Expr::OPs::Pair) --unpaired_conditionals;

		// pass last delimiter and skip white space (end + bin_op_len points just after the binary op)
		for (pos = end + bin_op_len; pos < str_end && std::isspace((unsigned char)str[pos]); ++pos);

		// if pos is now out of bounds, there was a binary op with no second operand
		if (pos >= str_end) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Binary op encountered without a second operand"}; return false; }
	}

	stop_parsing:

	// make sure all conditionals were matched
	if (unpaired_conditionals != 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expression contained incomplete ternary conditionals"}; return false; }

	// run ptrdiff logic on result
	expr = Ptrdiff(std::move(expr));

	// update the aft index (most recent end index during parsing)
	aft = end;

	return true;
}
bool AssembleArgs::TryExtractExpr(const std::string &str, std::unique_ptr<Expr> &expr)
{
	// try to extract an expr from 0,len
	std::size_t aft;
	if (!TryExtractExpr(str, 0, str.size(), expr, aft)) return false;

	// ensure that the entire string was consumed
	if (aft != str.length()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse \"" + str + "\" as an expression"}; return false; }

	return true;
}
	
bool AssembleArgs::TryParseImm(const std::string &token, Expr &expr, u64 &sizecode, bool &explicit_size, bool &strict)
{
	// (STRICT) (BYTE/WORD/DWORD/QWORD) expr

	std::size_t pos, end;

	// get the first white space delimited token
	for (pos = 0; pos < token.size() && std::isspace((unsigned char)token[pos]); ++pos);
	for (end = pos; end < token.size() && !std::isspace((unsigned char)token[end]); ++end);

	// if that's a STRICT specifier
	if (ToUpper(token.substr(pos, end - pos)) == "STRICT")
	{
		strict = true;

		// get the next white space delimited token
		for (pos = end; pos < token.size() && std::isspace((unsigned char)token[pos]); ++pos);
		for (end = pos; end < token.size() && !std::isspace((unsigned char)token[end]); ++end);
	}
	// otherwise no STRICT specifier
	else strict = false;

	// extract that token (as upper case)
	std::string ut = ToUpper(token.substr(pos, end - pos));

	// handle explicit size specifiers
	if (ut == "BYTE") { sizecode = 0; explicit_size = true; }
	else if (ut == "WORD") { sizecode = 1; explicit_size = true; }
	else if (ut == "DWORD") { sizecode = 2; explicit_size = true; }
	else if (ut == "QWORD") { sizecode = 3; explicit_size = true; }
	// otherwise no explicit size specifier
	else { sizecode = 3; explicit_size = false; }

	// if there was a size specifier, wind pos up to the next non-white char after the (size) token
	if (explicit_size) for (pos = end; pos < token.size() && std::isspace((unsigned char)token[pos]); ++pos);

	// parse the rest of the string as an expression - rest of string starts at index pos - place aft index into end
	std::unique_ptr<Expr> ptr;
	if (!TryExtractExpr(token, pos, token.size(), ptr, end)) return false;
	// make sure the entire rest of the string was consumed
	if (end != token.size()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse as an expression: " + token.substr(pos)}; return false; }

	// migrate parsed expression from by-pointer to by-value
	expr = std::move(*ptr);

	return true;
}
bool AssembleArgs::TryParseInstantImm(const std::string &token, u64 &val, bool &floating, u64 &sizecode, bool &explicit_size, bool &strict)
{
	std::string err; // error location for evaluation

	Expr hole;
	if (!TryParseImm(token, hole, sizecode, explicit_size, strict)) return false;
	if (!hole.Evaluate(file.Symbols, val, floating, err)) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Failed to evaluate instant imm: " + token + "\n-> " + err}; return false; }

	return true;
}

bool AssembleArgs::TryExtractPtrVal(const Expr *expr, const Expr *&val, const std::string &_base)
{
	// initially null val
	val = nullptr;

	// if this is a leaf
	if (expr->OP == Expr::OPs::None)
	{
		// if there's no token, fail (not a pointer)
		if (expr->Token() == nullptr) return false;

		// if this is the #base offset itself, value is zero (this can happen with the current line macro)
		if (*expr->Token() == _base) return true;

		// otherwise get the symbol
		if (!TryGetValue(file.Symbols, *expr->Token(), expr)) return false;
	}

	// must be of standard label form
	if (expr->OP != Expr::OPs::Add || (expr->Left->Token() && *expr->Left->Token() != _base)) return false;

	// return the value portion
	val = expr->Right.get();
	return true;
}
std::unique_ptr<Expr> AssembleArgs::Ptrdiff(std::unique_ptr<Expr> expr)
{
	// on null, return null as well
	if (expr == nullptr) return nullptr;

	std::vector<Expr> add; // list of added terms
	std::vector<Expr> sub; // list of subtracted terms

	const Expr *a = nullptr, *b = nullptr; // expression pointers for lookup (initialized cause compiler yelled at me)

	// populate lists
	std::move(*expr).PopulateAddSub(add, sub);

	// perform ptrdiff reduction on anything defined by the linker
	for (const std::string &seg_name : PtrdiffIDs)
	{
		// i and j incremented after each pass to ensure same pair isn't matched next time
		for (int i = 0, j = 0; ; ++i, ++j)
		{
			// wind i up to next add label
			for (; i < (int)add.size() && !TryExtractPtrVal(&add[i], a, seg_name); ++i);
			// if this exceeds bounds, break
			if (i >= (int)add.size()) break;

			// wind j up to next sub label
			for (; j < (int)sub.size() && !TryExtractPtrVal(&sub[j], b, seg_name); ++j);
			// if this exceeds bounds, break
			if (j >= (int)sub.size()) break;

			// we got a pair: replace items in add/sub with their pointer values
			// if extract succeeded but returned null, the "missing" value is zero - just remove from the list
			if (a) add[i] = *a; else { std::swap(add[i], add.back()); add.pop_back(); }
			if (b) sub[j] = *b; else { std::swap(sub[j], sub.back()); sub.pop_back(); }
		}
	}

	// for each add item
	for (std::size_t i = 0; i < add.size(); ++i)
	{
		// if it's not a leaf
		if (!add[i].IsLeaf())
		{
			// recurse on children
			Expr temp;
			temp.OP = add[i].OP;
			temp.Left = Ptrdiff(std::move(add[i].Left)); // we can use move on these because we'll be replacing add[i] anyway
			temp.Right = Ptrdiff(std::move(add[i].Right));
			add[i] = std::move(temp);
		}
	}
	// for each sub item
	for (std::size_t i = 0; i < sub.size(); ++i)
	{
		// if it's not a leaf
		if (!sub[i].IsLeaf())
		{
			// recurse on children
			Expr temp;
			temp.OP = sub[i].OP;
			temp.Left = Ptrdiff(std::move(sub[i].Left)); // we can use move on these because we'll be replacing sub[i] anyway
			temp.Right = Ptrdiff(std::move(sub[i].Right));
			sub[i] = std::move(temp);
		}
	}

	// stitch together the new tree
	if (sub.size() == 0) return std::make_unique<Expr>(Expr::ChainAddition(add));
	else if (add.size() == 0)
	{
		std::unique_ptr<Expr> tree = std::make_unique<Expr>();
		tree->OP = Expr::OPs::Neg;
		tree->Left = std::make_unique<Expr>(Expr::ChainAddition(sub));
		return tree;
	}
	else
	{
		std::unique_ptr<Expr> tree = std::make_unique<Expr>();
		tree->OP = Expr::OPs::Sub;
		tree->Left = std::make_unique<Expr>(Expr::ChainAddition(add));
		tree->Right = std::make_unique<Expr>(Expr::ChainAddition(sub));
		return tree;
	}
}

bool AssembleArgs::TryParseInstantPrefixedImm(const std::string &token, const std::string &prefix, u64 &val, bool &floating, u64 &sizecode, bool &explicit_size, bool &strict)
{
	val = sizecode = 0;
	floating = explicit_size = false;

	// must begin with prefix
	if (!StartsWith(token, prefix)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Token did not start with \"" + prefix + "\" prefix: \"" + token + "\""}; return false; }
	// aside from the prefix, must not be empty
	if (token.size() == prefix.size()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Empty token encountered after \"" + prefix + "\" prefix: \"" + token + "\""}; return false; }

	int end; // ending of expression token

	// if this starts parenthetical region
	if (token[prefix.size()] == '(')
	{
		int depth = 1; // depth of 1

		// start searching for ending parens after first parens
		for (end = (int)prefix.size() + 1; end < (int)token.size() && depth > 0; ++end)
		{
			if (token[end] == '(') ++depth;
			else if (token[end] == ')') --depth;
		}

		// make sure we reached zero depth
		if (depth != 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Mismatched parenthesis in prefixed expression \"" + token + "\""}; return false; }
	}
	// otherwise normal symbol
	else
	{
		// take all legal chars
		for (end = (int)prefix.size(); end < (int)token.size() && (std::isalnum(token[end]) || token[end] == '_' || token[end] == '.'); ++end);
	}

	// make sure we consumed the entire string
	if (end != (int)token.size()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Compound expressions used as prefixed expressions must be parenthesized \"" + token}; return false; }

	// prefix index must be instant imm
	if (!TryParseInstantImm(token.substr(prefix.size()), val, floating, sizecode, explicit_size, strict)) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Failed to parse instant prefixed imm \"" + token + "\"\n-> " + res.ErrorMsg}; return false; }

	return true;
}

bool AssembleArgs::TryParseCPURegister(const std::string &token, u64 &reg, u64 &sizecode, bool &high)
{
	// copy data if we can parse it
	const std::tuple<u8, u8, bool> *info;
	if (TryGetValue(CPURegisterInfo, ToUpper(token), info))
	{
		reg = std::get<0>(*info);
		sizecode = std::get<1>(*info);
		high = std::get<2>(*info);
		return true;
	}
	// otherwise it's not a cpu register
	else
	{
		res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse as cpu register: " + token};
		return false;
	}
}
bool AssembleArgs::TryParseFPURegister(const std::string &token, u64 &reg)
{
	const u8 *info;
	if (TryGetValue(FPURegisterInfo, ToUpper(token), info))
	{
		reg = *info;
		return true;
	}
	else
	{
		res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse as fpu register: " + token};
		return false;
	}
}
bool AssembleArgs::TryParseVPURegister(const std::string &token, u64 &reg, u64 &sizecode)
{
	// copy data if we can parse it
	const std::tuple<u8, u8> *info;
	if (TryGetValue(VPURegisterInfo, ToUpper(token), info))
	{
		reg = std::get<0>(*info);
		sizecode = std::get<1>(*info);
		return true;
	}
	// otherwise it's not a vpu register
	else
	{
		res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse as vpu register: " + token};
		return false;
	}
}

bool AssembleArgs::TryGetRegMult(const std::string &label, Expr &hole, u64 &mult_res)
{
	mult_res = 0; // mult starts at zero (for logic below)

	std::vector<Expr*> path;
	std::vector<Expr*> list;

	std::string err; // evaluation error

	// while we can find this symbol
	while (hole.FindPath(label, path, true))
	{
		// move path into list
		while (path.size() > 0)
		{
			list.push_back(path.back());
			path.pop_back();
		}

		// if it doesn't have a mult section
		if (list.size() == 1 || (list.size() > 1 && list[1]->OP != Expr::OPs::Mul))
		{
			// add in a multiplier of 1
			list[0]->OP = Expr::OPs::Mul;
			list[0]->Left = Expr::NewInt(1);
			list[0]->Right = Expr::NewToken(*list[0]->Token());

			// insert new register location as beginning of path
			list.insert(list.begin(), list[0]->Right.get());
		}

		// start 2 above (just above regular mult code)
		for (int i = 2; i < (int)list.size();)
		{
			switch (list[i]->OP)
			{
			case Expr::OPs::Add: case Expr::OPs::Sub: case Expr::OPs::Neg: ++i; break;

			case Expr::OPs::Mul:
			{
				// toward leads to register, mult leads to mult value
				Expr *toward = list[i - 1], *mult = list[i]->Left.get() == list[i - 1] ? list[i]->Right.get() : list[i]->Left.get();

				// if pos is add/sub, we need to distribute
				if (toward->OP == Expr::OPs::Add || toward->OP == Expr::OPs::Sub)
				{
					// swap operators with toward
					list[i]->OP = toward->OP;
					toward->OP = Expr::OPs::Mul;

					// create the distribution node
					std::unique_ptr<Expr> temp = std::make_unique<Expr>();
					temp->OP = Expr::OPs::Mul;
					temp->Left = std::make_unique<Expr>(*mult);

					// compute right and transfer mult to toward
					if (toward->Left.get() == list[i - 2]) { temp->Right = std::move(toward->Right); toward->Right = std::make_unique<Expr>(*mult); }
					else { temp->Right = std::move(toward->Left); toward->Left = std::make_unique<Expr>(*mult); }

					// add it in
					if (list[i]->Left.get() == mult) list[i]->Left = std::move(temp); else list[i]->Right = std::move(temp);
				}
				// if pos is mul, we need to combine with pre-existing mult code
				else if (toward->OP == Expr::OPs::Mul)
				{
					// create the combination node
					std::unique_ptr<Expr> temp = std::make_unique<Expr>();
					temp->OP = Expr::OPs::Mul;
					temp->Left = std::make_unique<Expr>(*mult);
					temp->Right = toward->Left.get() == list[i - 2] ? std::move(toward->Right) : std::move(toward->Left);

					// add it in
					if (list[i]->Left.get() == mult)
					{
						list[i]->Left = std::move(temp); // replace mult with combination
						list[i]->Right = std::make_unique<Expr>(*list[i - 2]); // bump up toward
					}
					else
					{
						list[i]->Right = std::move(temp);
						list[i]->Left = std::make_unique<Expr>(*list[i - 2]);
					}

					// remove the skipped list[i - 1]
					list.erase(list.begin() + (i - 1));
				}
				// if pos is neg, we need to put the negative on the mult
				else if (toward->OP == Expr::OPs::Neg)
				{
					// create the combination node
					std::unique_ptr<Expr> temp = std::make_unique<Expr>();
					temp->OP = Expr::OPs::Neg;
					temp->Left = std::make_unique<Expr>(*mult);

					// add it in
					if (list[i]->Left.get() == mult)
					{
						list[i]->Left = std::move(temp); // replace mult with combination
						list[i]->Right = std::make_unique<Expr>(*list[i - 2]); // bump up toward
					}
					else
					{
						list[i]->Right = std::move(temp);
						list[i]->Left = std::make_unique<Expr>(*list[i - 2]);
					}

					// remove the skipped list[i - 1]
					list.erase(list.begin() + (i - 1));
				}
				// otherwise something horrible happened (this should never happen, but is left in for sanity-checking and future-proofing)
				else throw std::runtime_error("Unknown address simplification step: OP = " + tostr((int)toward->OP));

				--i; // decrement i to follow the multiplication all the way down the rabbit hole
				if (i < 2) i = 2; // but if it gets under the starting point, reset it

				break;
			}

			default: res = {AssembleError::FormatError, "line " + tostr(line) + ": Muliplier for " + label + " could not be automatically simplified"}; return false;
			}
		}

		// -- finally done with all the algebra -- //

		// extract mult code fragment
		u64 val;
		bool floating;
		if (!(list[1]->Left.get() == list[0] ? list[1]->Right : list[1]->Left)->Evaluate(file.Symbols, val, floating, err))
		{
			res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to evaluate register multiplier as an instant imm\n-> " + err}; return false;
		}
		// make sure it's not floating-point
		if (floating) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Register multiplier may not be floating-point"}; return false; }

		// look through from top to bottom
		for (int i = (int)list.size() - 1; i >= 2; --i)
		{
			// if this will negate the register
			if (list[i]->OP == Expr::OPs::Neg || (list[i]->OP == Expr::OPs::Sub && list[i]->Right.get() == list[i - 1]))
			{
				// negate found partial mult
				val = ~val + 1;
			}
		}

		// remove the register section from the expression (replace with integral 0)
		list[1]->IntResult(0);

		mult_res += val; // add extracted mult to total mult
		list.clear(); // clear list for next pass
	}

	// register successfully parsed
	return true;
}
bool AssembleArgs::TryParseAddress(std::string token, u64 &a, u64 &b, Expr &ptr_base, u64 &sizecode, bool &explicit_size)
{
	a = b = 0; ptr_base.Clear();
	sizecode = 0; explicit_size = false;

	// account for exlicit sizecode prefix
	std::string utoken = ToUpper(token);
	if (StartsWithToken(utoken, "BYTE")) { sizecode = 0; explicit_size = true; utoken = TrimStart(utoken.substr(4)); }
	else if (StartsWithToken(utoken, "WORD")) { sizecode = 1; explicit_size = true; utoken = TrimStart(utoken.substr(4)); }
	else if (StartsWithToken(utoken, "DWORD")) { sizecode = 2; explicit_size = true; utoken = TrimStart(utoken.substr(5)); }
	else if (StartsWithToken(utoken, "QWORD")) { sizecode = 3; explicit_size = true; utoken = TrimStart(utoken.substr(5)); }
	else if (StartsWithToken(utoken, "XMMWORD")) { sizecode = 4; explicit_size = true; utoken = TrimStart(utoken.substr(7)); }
	else if (StartsWithToken(utoken, "YMMWORD")) { sizecode = 5; explicit_size = true; utoken = TrimStart(utoken.substr(7)); }
	else if (StartsWithToken(utoken, "ZMMWORD")) { sizecode = 6; explicit_size = true; utoken = TrimStart(utoken.substr(7)); }

	// if there was an explicit size
	if (explicit_size)
	{
		// CSX64 uses the DWORD PTR syntax, so now we need to start with PTR
		if (!StartsWith(utoken, "PTR")) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Explicit memory operand size encountered without the PTR designator"}; return false; }

		// take all of that stuff off of token
		token = TrimStart(token.substr(token.size() - utoken.size() + 3));
	}

	// must be of [*] format
	if (token.size() < 3 || token[0] != '[' || token[token.size() - 1] != ']') { res = {AssembleError::FormatError, "line " + tostr(line) + ": Invalid address format encountered: " + token}; return false; }

	u64 m1 = 0, r1 = 666, r2 = 666, sz; // final register info - 666 denotes no value - m1 must default to 0 - sz defaults to 64-bit in the case that there's only an imm ptr_base
	bool explicit_sz; // denotes that the ptr_base sizecode is explicit
	bool strict; // denotes that the ptr_base is has a STRICT specifier (inhibit compact imm size optimization)

	// extract the address internals
	token = token.substr(1, token.size() - 2);

	// turn into an expression
	if (!TryParseImm(token, ptr_base, sz, explicit_sz, strict)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse address expression\n-> " + res.ErrorMsg}; return false; }

	// look through all the register names
	for (const auto &entry : CPURegisterInfo)
	{
		// extract the register data
		u64 mult;
		if (!TryGetRegMult(entry.first, ptr_base, mult)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to extract register data\n-> " + res.ErrorMsg}; return false; }

		// if the register is present we need to do something with it
		if (mult != 0)
		{
			// if we have an explicit address component size to enforce
			if (explicit_sz)
			{
				// if this conflicts with the current one, it's an error
				if (sz != std::get<1>(entry.second)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Encountered address components of conflicting sizes"}; return false; }
			}
			// otherwise record this as the size to enforce
			else { sz = std::get<1>(entry.second); explicit_sz = true; }
		}

		// if the multiplier is trivial or has a trivial component
		if (mult & 1)
		{
			mult &= ~(u64)1; // remove the trivial component

			// if r2 is empty, put it there
			if (r2 == 666) r2 = std::get<0>(entry.second);
			// then try r1
			else if (r1 == 666) r1 = std::get<0>(entry.second);
			// otherwise we ran out of registers to use
			else { res = {AssembleError::FormatError, "line " + tostr(line) + ": An address expression may use up to 2 registers"}; return false; }
		}

		// if a non-trivial multiplier is present
		if (mult != 0)
		{
			// decode the mult code into m1
			switch (mult)
			{
			// (mult 1 is trivial and thus handled above)
			case 2: m1 = 1; break;
			case 4: m1 = 2; break;
			case 8: m1 = 3; break;

			default: res = {AssembleError::UsageError, "line " + tostr(line) + ": Register multiplier must be 1, 2, 4, or 8. Got " + tostr((i64)mult) + "*" + entry.first}; return false;
			}

			// if r1 is empty, put it there
			if (r1 == 666) r1 = std::get<0>(entry.second);
			// otherwise we don't have anywhere to put it
			else { res = {AssembleError::FormatError, "line " + tostr(line) + ": An address expression may only use one non-trivial multiplier"}; return false; }
		}
	}

	// -- apply final touches -- //

	// if we still don't have an explicit address size code, use 64-bit
	if (!explicit_sz) sz = 3;
	// 8-bit addressing is not allowed
	else if (sz == 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": 8-bit addressing is not allowed"}; return false; }

	// if we can evaluate the hole to zero, there is no hole (null it)
	u64 _temp;
	bool _btemp;
	bool ptr_base_present = true; // marker for if ptr_base is needed
	if (ptr_base.Evaluate(file.Symbols, _temp, _btemp, utoken) && _temp == 0) ptr_base_present = false;

	// [1: imm][1:][2: mult_1][2: size][1: r1][1: r2]   ([4: r1][4: r2])   ([size: imm])

	a = (ptr_base_present ? 0x80 : 0ul) | (m1 << 4) | (sz << 2) | (r1 != 666 ? 2 : 0ul) | (r2 != 666 ? 1 : 0ul);
	b = (r1 != 666 ? r1 << 4 : 0ul) | (r2 != 666 ? r2 : 0ul);

	// address successfully parsed
	return true;
}

bool AssembleArgs::VerifyLegalExpression(Expr &expr)
{
	// if it's a leaf, it must be something that is defined
	if (expr.IsLeaf())
	{
		// if it's already been evaluated or we know about it somehow, we're good
		if (expr.IsEvaluated()) return true;
		const std::string &tok = *expr.Token();
		if (ContainsKey(file.Symbols, tok) || Contains(file.ExternalSymbols, tok)
			|| ContainsValue(SegOffsets, tok) || ContainsValue(SegOrigins, tok)
			|| Contains(VerifyLegalExpressionIgnores, tok)
			|| StartsWith(tok, BinaryLiteralSymbolPrefix))
			return true;
		// otherwise we don't know what it is
		else { res = {AssembleError::UnknownSymbol, "Unknown symbol: " + tok}; return false; }
	}
	// otherwise children must be legal
	else return VerifyLegalExpression(*expr.Left) && (expr.Right == nullptr || VerifyLegalExpression(*expr.Right));
}
bool AssembleArgs::VerifyIntegrity()
{
	// make sure all global symbols were actually defined prior to link-time
	for (const std::string &global : file.GlobalSymbols)
		if (!ContainsKey(file.Symbols, global)) { res = {AssembleError::UnknownSymbol, "Global symbol was never defined: " + global}; return false; }

	// make sure all symbol expressions were valid
	for (auto &entry : file.Symbols) if (!VerifyLegalExpression(entry.second)) return false;

	// make sure all hole expressions were valid
	for (HoleData &hole : file.TextHoles) if (!VerifyLegalExpression(hole.expr)) return false;
	for (HoleData &hole : file.RodataHoles) if (!VerifyLegalExpression(hole.expr)) return false;
	for (HoleData &hole : file.DataHoles) if (!VerifyLegalExpression(hole.expr)) return false;

	// the hood is good
	return true;
}

bool AssembleArgs::IsReservedSymbol(std::string symbol)
{
	// make the symbol uppercase (all reserved symbols are case insensitive)
	symbol = ToUpper(std::move(symbol));

	// check against register dictionaries
	return ContainsKey(CPURegisterInfo, symbol) || ContainsKey(FPURegisterInfo, symbol)
		|| ContainsKey(VPURegisterInfo, symbol) || Contains(AdditionalReservedSymbols, symbol);
}
	
bool AssembleArgs::TryProcessAlignXX(u64 size)
{
	if (args.size() != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected no operands"}; return false; }

	return TryAlign(size);
}
bool AssembleArgs::TryProcessAlign()
{
	if (args.size() != 1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// parse the alignment value (critical expression)
	u64 val, sizecode;
	bool floating, explicit_size, strict;
	if (!TryParseInstantImm(args[0], val, floating, sizecode, explicit_size, strict)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Alignment value must be instant\n-> " + res.ErrorMsg}; return false; }
	
	// it doesn't really make sense to give size information to an alignment value
	if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
	if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }
	
	// alignment must be an integer power of 2
	if (floating) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Alignment value cannot be floating-point"}; return false; }
	if (val == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Attempt to align to a multiple of zero"}; return false; }
	if (!IsPowerOf2(val)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Alignment value must be a power of 2. Got " + tostr(val)}; return false; }

	return TryAlign(val);
}

bool AssembleArgs::TryProcessGlobal()
{
	if (args.size() == 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected at least one symbol to export"}; return false; }

	std::string err;
	for (const std::string &symbol : args)
	{
		// special error message for using global on local labels
		if (symbol[0] == '.') { res = {AssembleError::ArgError, "line " + tostr(line) + ": Cannot export local symbols without their full declaration"}; return false; }
		// test name for legality
		if (!IsValidName(symbol, err)) { res = {AssembleError::InvalidLabel, "line " + tostr(line) + ": " + err}; return false; }

		// don't add to global list twice
		if (Contains(file.GlobalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Attempt to export \"" + symbol + "\" multiple times"}; return false; }
		// ensure we don't global an external
		if (Contains(file.ExternalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define external \"" + symbol + "\" as global"}; return false; }

		// add it to the globals list
		file.GlobalSymbols.emplace(symbol);
	}

	return true;
}
bool AssembleArgs::TryProcessExtern()
{
	if (args.size() == 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected at least one symbol to import"}; return false; }

	std::string err;
	for (const std::string &symbol : args)
	{
		// special error message for using extern on local labels
		if (symbol[0] == '.') { res = {AssembleError::ArgError, "line " + tostr(line) + ": Cannot import local symbols"}; return false; }
		// test name for legality
		if (!IsValidName(symbol, err)) { res = {AssembleError::InvalidLabel, "line " + tostr(line) + ": " + err}; return false; }

		// ensure we don't extern a symbol that already exists
		if (ContainsKey(file.Symbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define symbol \"" + symbol + "\" (defined internally) as external"}; return false; }

		// don't add to external list twice
		if (Contains(file.ExternalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Attempt to import \"" + symbol + "\" multiple times"}; return false; }
		// ensure we don't extern a global
		if (Contains(file.GlobalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define global \"" + symbol + "\" as external"}; return false; }

		// add it to the external list
		file.ExternalSymbols.emplace(symbol);
	}

	return true;
}

bool AssembleArgs::TryProcessDeclare(u64 size)
{
	if (args.size() == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected at least 1 value to write"}; return false; }

	Expr expr;
	std::string chars, err;
	u64 sizecode;
	bool explicit_size, strict;

	// for each argument (not using foreach because order is incredibly important and i'm paranoid)
	for (int i = 0; i < (int)args.size(); ++i)
	{
		// if it's a string
		if (TryExtractStringChars(args[i], chars, err))
		{
			// dump into memory (one byte each)
			for (std::size_t j = 0; j < chars.size(); ++j) if (!TryAppendByte((u8)chars[j])) return false;
			// make sure we write a multiple of size
			if (!TryPad(AlignOffset((u64)chars.size(), size))) return false;
		}
		// if it's a value (imm)
		else if (TryParseImm(args[i], expr, sizecode, explicit_size, strict))
		{
			// can only use standard sizes
			if (size > 8) { res = { AssembleError::FormatError, "line " + tostr(line) + ": Attempt to write a numeric value in an unsuported format" }; return false; }
			// we're given a size we have to use, so don't allow explicit operand sizes as well
			if (explicit_size) { res = { AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed" }; return false; }
			// in this context, we're writing user-readable binary data, so compacting is not allowed, so explicitly using a STRICT specifier should not be allowed either
			if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

			// write the value
			if (!TryAppendExpr(size, std::move(expr))) return false;
		}
		// otherwise we failed to parse it
		else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse operand as a string or imm: " + args[i] + "\n-> " + res.ErrorMsg}; return false; }
	}

	return true;
}
bool AssembleArgs::TryProcessReserve(u64 size)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Reserve expected one arg"}; return false; }

	// parse the number to reserve
	u64 count, sizecode;
	bool floating, explicit_size, strict;
	if (!TryParseInstantImm(args[0], count, floating, sizecode, explicit_size, strict)) return false;
	
	// it doesn't really make sense to give size information to the length of an array
	if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
	if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

	// must be an integer
	if (floating) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Reserve count cannot be floating-point"}; return false; }

	// reserve the space
	if (!TryReserve(count * size)) return false;

	return true;
}

bool AssembleArgs::TryProcessEQU()
{
	if (args.size() != 1) { res = { AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand" }; return false; }

	// make sure we have a label on this line
	if (label_def.empty()) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Expected a label declaration to link to the value" }; return false; }

	// get the expression
	Expr expr;
	u64 sizecode;
	bool explicit_size, strict;
	if (!TryParseImm(args[0], expr, sizecode, explicit_size, strict)) return false;
	if (explicit_size) { res = { AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed" }; return false; }
	if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

	// make sure the symbol isn't already defined (this could be the case for a TIMES prefix on an EQU directive)
	if (ContainsKey(file.Symbols, label_def)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Symbol " + label_def + " was already defined"}; return false; }

	// inject the symbol
	file.Symbols.emplace(label_def, std::move(expr));

	return true;
}

bool AssembleArgs::TryProcessStaticAssert()
{
	if (args.size() != 1 && args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected an expression and an optional assertion message"}; return false; }

	// get the expression
	Expr expr;
	u64 sizecode;
	bool explicit_size, strict;
	if (!TryParseImm(args[0], expr, sizecode, explicit_size, strict)) return false;
	if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
	if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

	// it must be a critical expression - get its value
	u64 val;
	bool floating;
	std::string err;
	if (!expr.Evaluate(file.Symbols, val, floating, err)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a critical expression\n-> " + err}; return false; }

	// get the assertion message
	std::string msg;
	if (args.size() == 2 && !TryExtractStringChars(args[1], msg, err)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second (optional) argument was not a string\n-> " + err}; return false; }

	// if the assertion failed, assembly fails
	if (IsZero(val, floating)) { res = { AssembleError::Assertion, "line " + tostr(line) + ": Assertion failed" + (args.size() == 2 ? " - " : "") + msg }; return false; }

	return true;
}

bool AssembleArgs::TryProcessSegment()
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// get the segment we're going to
	std::string useg = ToUpper(args[0]);
	if (useg == ".TEXT") current_seg = AsmSegment::TEXT;
	else if (useg == ".RODATA") current_seg = AsmSegment::RODATA;
	else if (useg == ".DATA") current_seg = AsmSegment::DATA;
	else if (useg == ".BSS") current_seg = AsmSegment::BSS;
	else { res = {AssembleError::ArgError, "line " + tostr(line) + ": Unknown segment specified"}; return false; }

	// if this segment has already been done, fail
	if ((int)done_segs & (int)current_seg) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Attempt to redeclare segment"}; return false; }
	// add to list of completed segments
	done_segs = (AsmSegment)((int)done_segs | (int)current_seg);

	// we don't want to have cross-segment local symbols
	last_nonlocal_label.clear();

	return true;
}

bool AssembleArgs::TryProcessINCBIN()
{
	if (args.size() < 1 || args.size() > 3) { res = { AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1-3 arguments" }; return false; }

	std::string path; // the specified path to the binary file to inject
	std::string err;

	std::streamsize bin_pos = 0; // starting position in the file to include (in bytes)
	std::streamsize bin_maxlen = std::numeric_limits<std::streamsize>::max(); // max number of bytes to include (starting at bin_pos)

	std::vector<u8> *seg; // the current segment (to which to append binary data)

	// --------------------------------------------------------------------------------------------

	// ensure the first (required) argument is a string
	if (!TryExtractStringChars(args[0], path, err)) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Failed to parse " + args[0] + " as a string\n-> " + err}; return false; }

	// if there was a second argument, parse it as bin_pos
	if (args.size() > 1)
	{
		u64 val, sizecode;
		bool floating, explicit_size, strict;
		if (!TryParseInstantImm(args[1], val, floating, sizecode, explicit_size, strict)) return false;

		// it doesn't really make sense to give size information to a file position
		if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
		if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

		// must be an integer
		if (floating) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Expected an integer expression as second arg"}; return false; }

		bin_pos = (std::streamsize)val;
		if (bin_pos < 0) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Binary file starting position (arg 2) cannot be negative"}; return false; }
	}

	// if there was a third argument, parse it as bin_maxlen
	if (args.size() > 2)
	{
		u64 val, sizecode;
		bool floating, explicit_size, strict;
		if (!TryParseInstantImm(args[1], val, floating, sizecode, explicit_size, strict)) return false;

		// it doesn't really make sense to give size information to the size of an array
		if (explicit_size) { res = { AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed" }; return false; }
		if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

		// must be an integer
		if (floating) { res = { AssembleError::ArgError, "line " + tostr(line) + ": Expected an integer expression as third arg" }; return false; }

		bin_maxlen = (std::streamsize)val;
		if (bin_maxlen < 0) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Binary file max include length (arg 3) cannot be negative"}; return false; }
	}

	// --------------------------------------------------------------------------------------------

	// get the current segment (we append the binary file to the current segment)
	switch (current_seg)
	{
	case AsmSegment::TEXT: seg = &file.Text; break;
	case AsmSegment::RODATA: seg = &file.Rodata; break;
	case AsmSegment::DATA: seg = &file.Data; break;
	case AsmSegment::BSS: res = { AssembleError::UsageError, "line " + tostr(line) + ": Attempt to write initialized data to the BSS segment" }; return false;

	default: res = { AssembleError::UsageError, "line " + tostr(line) + ": Attempt to write outside of a segment" }; return false;
	}

	// open the binary file and get its size
	std::ifstream bin(path, std::ios::binary | std::ios::ate);
	if (!bin) { res = { AssembleError::ArgError, "line " + tostr(line) + ": Failed to open " + path + " for reading" }; return false; }
	const std::streamsize bin_size = bin.tellg();

	// if bin_pos is out of bounds, we include nothing and can stop here (success)
	if (bin_pos >= bin_size) return true;
	// otherwise seek to the starting position
	bin.seekg(bin_pos);

	// compute the include length - taking into account bin_maxlen
	const std::streamsize inc_len = std::min(bin_size - bin_pos, bin_maxlen);
	// if that's zero we include nothing and can stop here (success)
	if (inc_len <= 0) return true;

	// mark current seglen and comute new seglen
	std::size_t old_seglen = seg->size();
	std::size_t new_seglen = old_seglen + (std::size_t)inc_len;
	// make sure new_seglen computation didn't overflow
	if (new_seglen < old_seglen) { res = {AssembleError::Failure, "line " + tostr(line) + ": Including binary file contents exceeded maximum segment length"}; return false; }

	// make room for the entire included portion of the binary file in the current segment
	try { seg->resize(new_seglen); }
	catch (const std::bad_alloc&) { res = { AssembleError::Failure, "line " + tostr(line) + ": Failed to allocate space for binary file"}; return false; }

	// copy the binary into the segment
	if (!bin.read(reinterpret_cast<char*>(seg->data()) + old_seglen, inc_len) || bin.gcount() != inc_len) { res = { AssembleError::Failure, "line " + tostr(line) + ": Failed to read binary file"}; return false; }

	return true;
}

bool AssembleArgs::TryProcessTernaryOp(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask)
{
	if (args.size() != 3) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 3 args"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	u64 dest, a_sizecode, imm_sz;
	bool dest_high, imm_sz_explicit, imm_strict;
	Expr imm;
	if (!TryParseCPURegister(args[0], dest, a_sizecode, dest_high)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register as first operand"}; return false; }
	if (!TryParseImm(args[2], imm, imm_sz, imm_sz_explicit, imm_strict)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected imm as third operand"}; return false; }

	if (imm_sz_explicit && imm_sz != a_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }
	if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

	// reg
	u64 reg, b_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[1], reg, b_sizecode, reg_high))
	{
		if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

		if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul) | 0)) return false;
		if (!TryAppendExpr(Size(a_sizecode), std::move(imm))) return false;
		if (!TryAppendVal(1, (reg_high ? 128 : 0ul) | reg)) return false;
	}
	// mem
	else if (args[1][args[1].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[1], a, b, ptr_base, b_sizecode, explicit_size)) return false;

		if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

		if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul) | 1)) return false;
		if (!TryAppendExpr(Size(a_sizecode), std::move(imm))) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	// imm
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register or memory value as second operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessBinaryOp(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask, int _force_b_imm_sizecode)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg, *
	u64 dest, a_sizecode;
	bool dest_high;
	if (TryParseCPURegister(args[0], dest, a_sizecode, dest_high))
	{
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		// reg, reg
		u64 src, b_sizecode;
		bool src_high;
		if (TryParseCPURegister(args[1], src, b_sizecode, src_high))
		{
			if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

			if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul) | (src_high ? 1 : 0ul))) return false;
			if (!TryAppendVal(1, src)) return false;
		}
		// reg, mem
		else if (args[1][args[1].size() - 1] == ']')
		{
			u64 a, b;
			bool explicit_size;
			Expr ptr_base;
			if (!TryParseAddress(args[1], a, b, ptr_base, b_sizecode, explicit_size)) return false;

			if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

			if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul))) return false;
			if (!TryAppendVal(1, (2 << 4))) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// reg, imm
		else
		{
			Expr imm;
			bool explicit_size, strict;
			if (!TryParseImm(args[1], imm, b_sizecode, explicit_size, strict)) return false;

			// fix up size codes
			if (_force_b_imm_sizecode == -1)
			{
				if (explicit_size) { if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; } }
				else b_sizecode = a_sizecode;
			}
			else b_sizecode = (u64)_force_b_imm_sizecode;

			if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul))) return false;
			if (!TryAppendVal(1, (1 << 4))) return false;
			if (!TryAppendExpr(Size(b_sizecode), std::move(imm))) return false;
		}
	}
	// mem, *
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool a_explicit;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, a_explicit)) return false;

		// mem, reg
		u64 src, b_sizecode;
		bool src_high;
		if (TryParseCPURegister(args[1], src, b_sizecode, src_high))
		{
			if ((Size(b_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

			if (a_explicit && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Argument size missmatch"}; return false; }

			if (!TryAppendVal(1, (b_sizecode << 2) | (src_high ? 1 : 0ul))) return false;
			if (!TryAppendVal(1, (3 << 4) | src)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// mem, mem
		else if (args[1][args[1].size() - 1] == ']') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Only one operand may be a memory value"}; return false; }
		// mem, imm
		else
		{
			Expr imm;
			bool b_explicit, b_strict;
			if (!TryParseImm(args[1], imm, b_sizecode, b_explicit, b_strict)) return false;

			// fix up the size codes
			if (_force_b_imm_sizecode == -1)
			{
				if (a_explicit && b_explicit) { if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; } }
				else if (b_explicit) a_sizecode = b_sizecode;
				else if (a_explicit) b_sizecode = a_sizecode;
				else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }
			}
			else b_sizecode = (u64)_force_b_imm_sizecode;

			if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

			if (!TryAppendVal(1, a_sizecode << 2)) return false;
			if (!TryAppendVal(1, 4 << 4)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
			if (!TryAppendExpr(Size(b_sizecode), std::move(imm))) return false;
		}
	}
	// imm, *
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register or memory value as first operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessUnaryOp(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg
	u64 reg, a_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, reg_high))
	{
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2) | (reg_high ? 2 : 0ul))) return false;
	}
	// mem
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 1)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	// imm
	else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessIMMRM(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask, int default_sizecode)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg
	u64 reg, a_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, reg_high))
	{
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2) | (reg_high ? 1 : 0ul))) return false;
	}
	// mem
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		if (!explicit_size)
		{
			if (default_sizecode == -1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }
			else a_sizecode = (u64)default_sizecode;
		}
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 3)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	// imm
	else
	{
		Expr imm;
		bool explicit_size, strict;
		if (!TryParseImm(args[0], imm, a_sizecode, explicit_size, strict)) return false;

		if (!explicit_size)
		{
			if (default_sizecode == -1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }
			else a_sizecode = (u64)default_sizecode;
		}
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 2)) return false;
		if (!TryAppendExpr(Size(a_sizecode), std::move(imm))) return false;
	}

	return true;
}
bool AssembleArgs::TryProcessRR_RM(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask)
{
	if (args.size() != 3) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 3 operands"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg, *, *
	u64 dest, sizecode;
	bool dest_high;
	if (TryParseCPURegister(args[0], dest, sizecode, dest_high))
	{
		// apply size mask
		if ((Size(sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"}; return false; }

		// reg, reg, *
		u64 src_1, src_1_sizecode;
		bool src_1_high;
		if (TryParseCPURegister(args[1], src_1, src_1_sizecode, src_1_high))
		{
			// reg, reg, reg
			u64 src_2, src_2_sizecode;
			bool src_2_high;
			if (TryParseCPURegister(args[2], src_2, src_2_sizecode, src_2_high))
			{
				if (sizecode != src_1_sizecode || sizecode != src_2_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

				if (!TryAppendVal(1, (dest << 4) | (sizecode << 2) | (dest_high ? 2 : 0ul) | 0)) return false;
				if (!TryAppendVal(1, (src_1_high ? 128 : 0ul) | src_1)) return false;
				if (!TryAppendVal(1, (src_2_high ? 128 : 0ul) | src_2)) return false;
			}
			// reg, reg, mem
			else if (args[2][args[2].size() - 1] == ']')
			{
				u64 a, b;
				Expr ptr_base;
				bool src_2_explicit_size;
				if (!TryParseAddress(args[2], a, b, ptr_base, src_2_sizecode, src_2_explicit_size)) return false;

				if (sizecode != src_1_sizecode || (src_2_explicit_size && sizecode != src_2_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

				if (!TryAppendVal(1, (dest << 4) | (sizecode << 2) | (dest_high ? 2 : 0ul) | 1)) return false;
				if (!TryAppendVal(1, (src_1_high ? 128 : 0ul) | src_1)) return false;
				if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
			}
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Third operand must be a cpu register or memory value"}; return false; }
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand must be a cpu register"}; return false; }
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be a cpu register"}; return false; }

	return true;
}

bool AssembleArgs::TryProcessBinaryOp_NoBMem(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask, int _force_b_imm_sizecode)
{
	// b can't be memory
	if (args.size() > 1 && args[1][args[1].size() - 1] == ']') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand may not be a memory value"}; return false; }

	// otherwise refer to binary formatter
	return TryProcessBinaryOp(opcode, has_ext_op, ext_op, sizemask, _force_b_imm_sizecode);
}
bool AssembleArgs::TryProcessBinaryOp_R_RM(OPCode opcode, bool has_ext_op, u8 ext_op, u64 sizemask, int _force_b_imm_sizecode)
{
	// a must be register
	u64 reg, sz;
	bool high;
	if (args.size() > 0 && !TryParseCPURegister(args[0], reg, sz, high)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be a cpu register"}; return false; }
	// b must be register or memory
	if (args.size() > 1 && !TryParseCPURegister(args[1], reg, sz, high) && args[1][args[1].size() - 1] != ']') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand must be a cpu register or memory value"}; return false; }

	// otherwise refer to binary formatter
	return TryProcessBinaryOp(opcode, has_ext_op, ext_op, sizemask, _force_b_imm_sizecode);
}

bool AssembleArgs::TryProcessNoArgOp(OPCode opcode, bool has_ext_op, u8 ext_op)
{
	if (args.size() != 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	return true;
}
bool AssembleArgs::TryProcessNoArgOp_no_write()
{
	// only check to make sure there were no operands
	if (args.size() != 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"}; return false; }
	return true;
}

bool AssembleArgs::TryProcessXCHG(OPCode opcode)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;

	// reg, *
	u64 reg, a_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, reg_high))
	{
		// reg, reg
		u64 src, b_sizecode;
		bool src_high;
		if (TryParseCPURegister(args[1], src, b_sizecode, src_high))
		{
			if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

			if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2) | (reg_high ? 2 : 0ul) | 0)) return false;
			if (!TryAppendVal(1, (src_high ? 128 : 0ul) | src)) return false;
		}
		// reg, mem
		else if (args[1][args[1].size() - 1] == ']')
		{
			u64 a, b;
			Expr ptr_base;
			bool explicit_size;
			if (!TryParseAddress(args[1], a, b, ptr_base, b_sizecode, explicit_size)) return false;

			if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

			if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2) | (reg_high ? 2 : 0ul) | 1)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// reg, imm
		else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value as second operand"}; return false; }
	}
	// mem, *
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		// mem, reg
		u64 b_sizecode;
		if (TryParseCPURegister(args[1], reg, b_sizecode, reg_high))
		{
			if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"}; return false; }

			if (!TryAppendVal(1, (reg << 4) | (b_sizecode << 2) | (reg_high ? 2 : 0ul) | 1)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// mem, mem
		else if (args[1][args[1].size() - 1] == ']') { res = {AssembleError::FormatError, "line " + tostr(line) + ": Only one operand may be a memory value"}; return false; }
		// mem, imm
		else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value as second operand"}; return false; }
	}
	// imm, *
	else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value as first operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessLEA(OPCode opcode)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	u64 dest, a_sizecode;
	bool dest_high;
	if (!TryParseCPURegister(args[0], dest, a_sizecode, dest_high)) return false;
	if (a_sizecode == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": 8-bit addressing is not supported"}; return false; }

	u64 a, b, b_sizecode;
	Expr ptr_base;
	bool explicit_size;
	if (!TryParseAddress(args[1], a, b, ptr_base, b_sizecode, explicit_size)) return false;

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2))) return false;
	if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;

	return true;
}
bool AssembleArgs::TryProcessPOP(OPCode opcode)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;

	// reg
	u64 reg, a_sizecode;
	bool a_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, a_high))
	{
		if ((Size(a_sizecode) & 14) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

		if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2))) return false;
	}
	// mem
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// make sure operand size is 2, 4, or 8-bytes
		if ((Size(a_sizecode) & 14) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 1)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a cpu register or memory value"}; return false; }

	return true;
}

bool AssembleArgs::__TryProcessShift_mid()
{
	// reg/mem, reg
	u64 src, b_sizecode;
	bool b_high;
	if (TryParseCPURegister(args[1], src, b_sizecode, b_high))
	{
		if (src != 2 || b_sizecode != 0 || b_high) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Shifts using a register as count source must use CL"}; return false; }

		if (!TryAppendByte(0x80)) return false;
	}
	// reg/mem, imm
	else
	{
		Expr imm;
		bool explicit_size, strict;
		if (!TryParseImm(args[1], imm, b_sizecode, explicit_size, strict)) return false;

		// we're forcing this to be encoded in 1 byte, so don't allow explicit size or strict specifiers (for the imm we just parsed)
		if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
		if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

		// mask the shift count to 6 bits (we just need to make sure it can't set the CL flag)
		imm.Left = std::make_unique<Expr>(std::move(imm));
		imm.Right = Expr::NewInt(0x3f);
		imm.OP = Expr::OPs::BitAnd;

		if (!TryAppendExpr(1, std::move(imm))) return false;
	}

	return true;
}
bool AssembleArgs::TryProcessShift(OPCode opcode)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;

	// reg, *
	u64 dest, a_sizecode;
	bool a_high;
	if (TryParseCPURegister(args[0], dest, a_sizecode, a_high))
	{
		if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (a_high ? 2 : 0ul))) return false;
		if (!__TryProcessShift_mid()) return false;
	}
	// mem, *
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// make sure we're using a normal word size
		if (a_sizecode > 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 1)) return false;
		if (!__TryProcessShift_mid()) return false;
		if (!TryAppendAddress(1, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a cpu register or memory value as first operand"}; return false; }

	return true;
}

// helper for MOVxX formatter - additionally, ensures the sizes for dest/src are valid
bool AssembleArgs::__TryProcessMOVxX_settings_byte(bool sign, u64 dest, u64 dest_sizecode, u64 src_sizecode)
{
	// switch through mode (using 4 bits for sizecodes in case either is a nonstandard size e.g. xmmword memory)
	u64 mode;
	switch ((sign ? 0x100 : 0ul) | (dest_sizecode << 4) | src_sizecode)
	{
	case 0x010: mode = 0; break;
	case 0x110: mode = 1; break;
	case 0x020: mode = 2; break;
	case 0x021: mode = 3; break;
	case 0x120: mode = 4; break;
	case 0x121: mode = 5; break;
	case 0x030: mode = 6; break;
	case 0x031: mode = 7; break;
	case 0x130: mode = 8; break;
	case 0x131: mode = 9; break;
	case 0x132: mode = 10; break;

	default: res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size combination is not supported"}; return false;
	}

	return TryAppendByte((u8)((dest << 4) | mode));
}
bool AssembleArgs::TryProcessMOVxX(OPCode opcode, bool sign)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	u64 dest, dest_sizecode;
	bool __dest_high;
	if (!TryParseCPURegister(args[0], dest, dest_sizecode, __dest_high)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be a cpu register"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;

	// reg, reg
	u64 src, src_sizecode;
	bool src_high;
	if (TryParseCPURegister(args[1], src, src_sizecode, src_high))
	{
		// write the settings byte
		if (!__TryProcessMOVxX_settings_byte(sign, dest, dest_sizecode, src_sizecode)) return false;

		// mark source as register
		if (!TryAppendVal(1, (src_high ? 64 : 0ul) | src)) return false;
	}
	// reg, mem
	else if (args[1][args[1].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// write the settings byte
		if (!__TryProcessMOVxX_settings_byte(sign, dest, dest_sizecode, src_sizecode)) return false;

		// mark source as a memory value and append the address
		if (!TryAppendByte(0x80)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a cpu register or memory value as second operand"}; return false; }

	return true;
}

bool AssembleArgs::__TryGetBinaryStringOpSize(u64 &sizecode)
{
	// must have 2 args
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// args must both be memory
	u64 a, b, sz_1, sz_2;
	Expr expr;
	bool expl_1, expl_2;
	if (!TryParseAddress(args[0], a, b, expr, sz_1, expl_1) || !TryParseAddress(args[1], a, b, expr, sz_2, expl_2)) return false;

	// need an explicit size (that is consistent)
	if (expl_1 && expl_2) { if (sz_1 != sz_2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; } sizecode = sz_1; }
	else if (expl_1) sizecode = sz_1;
	else if (expl_2) sizecode = sz_2;
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

	// make sure sizecode is in range
	if (sizecode > 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size is not supported"}; return false; }

	return true;
}

bool AssembleArgs::TryProcessMOVS_string(OPCode opcode, bool rep)
{
	u64 sizecode;
	if (!__TryGetBinaryStringOpSize(sizecode)) return false;
	
	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte((u8)(((rep ? 1 : 0) << 2) | sizecode))) return false;

	return true;
}
bool AssembleArgs::TryProcessCMPS_string(OPCode opcode, bool repe, bool repne)
{
	u64 sizecode;
	if (!__TryGetBinaryStringOpSize(sizecode)) return false;

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte((u8)(((repne ? 4 : repe ? 3 : 2) << 2) | sizecode))) return false;

	return true;
}
bool AssembleArgs::TryProcessLODS_string(OPCode opcode, bool rep)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// takes one cpu register that is a partition of RAX (may not be AH)
	u64 reg, sizecode;
	bool high;
	if (!TryParseCPURegister(args[0], reg, sizecode, high)) return false;
	if (reg != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand must be a partition of RAX"}; return false; }
	if (high) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand may not be AH"}; return false; }

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte((u8)(((rep ? 6 : 5) << 2) | sizecode))) return false;

	return true;
}
bool AssembleArgs::TryProcessSTOS_string(OPCode opcode, bool rep)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// takes one memory operand of a standard size
	u64 a, b, sizecode;
	Expr ptr_base;
	bool explicit_size;
	if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;
	if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }
	if (sizecode > 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size is not supported"}; return false; }

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte((u8)(((rep ? 8 : 7) << 2) | sizecode))) return false;

	return true;
}
bool AssembleArgs::TryProcessSCAS_string(OPCode opcode, bool repe, bool repne)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// takes one memory operand of a standard size
	u64 a, b, sizecode;
	Expr ptr_base;
	bool explicit_size;
	if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;
	if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }
	if (sizecode > 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size is not supported"}; return false; }

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte((u8)(((repne ? 11 : repe ? 10 : 9) << 2) | sizecode))) return false;

	return true;
}

bool AssembleArgs::TryProcessPrefixOp(std::string &actual)
{
	if (args.size() == 0) { res = { AssembleError::ArgCount, "line " + tostr(line) + ": expected an instruction to augment" }; return false; }

	// first arg contains the instrucion to execute - find first white-space delimiter
	std::size_t len;
	for (len = 0; len < args[0].size() && !std::isspace(args[0][len]); ++len);

	// extract that as the "actual" instruction to modify - convert to uppercase
	actual = ToUpper(args[0].substr(0, len));

	// if we got the whole arg, remove first arg entirely
	if (len == args[0].size()) args.erase(args.begin());
	// otherwise, remove what we took and chop off leading white space
	else args[0] = TrimStart(args[0].substr(len));

	return true;
}

bool AssembleArgs::TryProcessREP()
{
	// initialize
	std::string actual;
	if (!TryProcessPrefixOp(actual)) return false;
	
	// route to proper handlers
	if (actual == "RET") return TryProcessNoArgOp(OPCode::RET);

	else if (actual == "MOVS") return TryProcessMOVS_string(OPCode::string, true);
	else if (actual == "MOVSB") return TryProcessNoArgOp(OPCode::string, true, (1 << 2) | 0);
	else if (actual == "MOVSW") return TryProcessNoArgOp(OPCode::string, true, (1 << 2) | 1);
	else if (actual == "MOVSD") return TryProcessNoArgOp(OPCode::string, true, (1 << 2) | 2);
	else if (actual == "MOVSQ") return TryProcessNoArgOp(OPCode::string, true, (1 << 2) | 3);

	else if (actual == "LODS") return TryProcessLODS_string(OPCode::string, true);
	else if (actual == "LODSB") return TryProcessNoArgOp(OPCode::string, true, (6 << 2) | 0);
	else if (actual == "LODSW") return TryProcessNoArgOp(OPCode::string, true, (6 << 2) | 1);
	else if (actual == "LODSD") return TryProcessNoArgOp(OPCode::string, true, (6 << 2) | 2);
	else if (actual == "LODSQ") return TryProcessNoArgOp(OPCode::string, true, (6 << 2) | 3);

	else if (actual == "STOS") return TryProcessSTOS_string(OPCode::string, true);
	else if (actual == "STOSB") return TryProcessNoArgOp(OPCode::string, true, (8 << 2) | 0);
	else if (actual == "STOSW") return TryProcessNoArgOp(OPCode::string, true, (8 << 2) | 1);
	else if (actual == "STOSD") return TryProcessNoArgOp(OPCode::string, true, (8 << 2) | 2);
	else if (actual == "STOSQ") return TryProcessNoArgOp(OPCode::string, true, (8 << 2) | 3);

	// otherwise this is illegal usage of REP
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": REP cannot be used with the specified instruction"}; return false; }
}
bool AssembleArgs::TryProcessREPE()
{
	// initialize
	std::string actual;
	if (!TryProcessPrefixOp(actual)) return false;

	// route to proper handlers
	if (actual == "RET") return TryProcessNoArgOp(OPCode::RET);

	else if (actual == "CMPS") return TryProcessCMPS_string(OPCode::string, true, false);
	else if (actual == "CMPSB") return TryProcessNoArgOp(OPCode::string, true, (3 << 2) | 0);
	else if (actual == "CMPSW") return TryProcessNoArgOp(OPCode::string, true, (3 << 2) | 1);
	else if (actual == "CMPSD") return TryProcessNoArgOp(OPCode::string, true, (3 << 2) | 2);
	else if (actual == "CMPSQ") return TryProcessNoArgOp(OPCode::string, true, (3 << 2) | 3);

	else if (actual == "SCAS") return TryProcessSCAS_string(OPCode::string, true, false);
	else if (actual == "SCASB") return TryProcessNoArgOp(OPCode::string, true, (10 << 2) | 0);
	else if (actual == "SCASW") return TryProcessNoArgOp(OPCode::string, true, (10 << 2) | 1);
	else if (actual == "SCASD") return TryProcessNoArgOp(OPCode::string, true, (10 << 2) | 2);
	else if (actual == "SCASQ") return TryProcessNoArgOp(OPCode::string, true, (10 << 2) | 3);

	// otherwise this is illegal usage of REP
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": REPE cannot be used with the specified instruction"}; return false; }
}
bool AssembleArgs::TryProcessREPNE()
{
	// initialize
	std::string actual;
	if (!TryProcessPrefixOp(actual)) return false;

	// route to proper handlers
	if (actual == "RET") return TryProcessNoArgOp(OPCode::RET);

	else if (actual == "CMPS") return TryProcessCMPS_string(OPCode::string, false, true);
	else if (actual == "CMPSB") return TryProcessNoArgOp(OPCode::string, true, (4 << 2) | 0);
	else if (actual == "CMPSW") return TryProcessNoArgOp(OPCode::string, true, (4 << 2) | 1);
	else if (actual == "CMPSD") return TryProcessNoArgOp(OPCode::string, true, (4 << 2) | 2);
	else if (actual == "CMPSQ") return TryProcessNoArgOp(OPCode::string, true, (4 << 2) | 3);

	else if (actual == "SCAS") return TryProcessSCAS_string(OPCode::string, false, true);
	else if (actual == "SCASB") return TryProcessNoArgOp(OPCode::string, true, (11 << 2) | 0);
	else if (actual == "SCASW") return TryProcessNoArgOp(OPCode::string, true, (11 << 2) | 1);
	else if (actual == "SCASD") return TryProcessNoArgOp(OPCode::string, true, (11 << 2) | 2);
	else if (actual == "SCASQ") return TryProcessNoArgOp(OPCode::string, true, (11 << 2) | 3);

	// otherwise this is illegal usage of REP
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": REPNE cannot be used with the specified instruction"}; return false; }
}

bool AssembleArgs::TryProcessLOCK()
{
	// initialize
	std::string actual;
	if (!TryProcessPrefixOp(actual)) return false;

	// make sure actual is an instruction that can legally be modified by LOCK
	if (!Contains(valid_lock_instructions, actual)) { res = { AssembleError::UsageError, "line " + tostr(line) + ": LOCK cannot modify the specified instruction" }; return false; }

	// get the proper routing handler
	const asm_router *router;
	if (!TryGetValue(asm_routing_table, actual, router)) { res = { AssembleError::UnknownOp, "line " + tostr(line) + ": Unknown augmented instruction" }; return false; }
	
	// perform the assembly action
	return (*router)(*this);
}

bool AssembleArgs::TryProcessBSx(OPCode opcode, bool forward)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write the op code
	if (!TryAppendByte((u8)opcode)) return false;

	// first arg must be reg
	u64 dest, dest_sz;
	bool high;
	if (!TryParseCPURegister(args[0], dest, dest_sz, high)) return false;
	// 8-bit mode is not allowed
	if (dest_sz == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": 8-bit mode is not supported"}; return false; }

	// reg, reg
	u64 src, src_sz;
	if (TryParseCPURegister(args[1], src, src_sz, high))
	{
		// make sure sizes match
		if (dest_sz != src_sz) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

		if (!TryAppendByte((u8)((forward ? 128 : 0) | (dest_sz << 4) | dest))) return false;
		if (!TryAppendByte((u8)src)) return false;
	}
	// reg, mem
	else if (args[1].back() == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool src_explicit;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sz, src_explicit)) return false;

		// make sure sizes match
		if (src_explicit && dest_sz != src_sz) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

		if (!TryAppendByte((u8)((forward ? 128 : 0) | 64 | (dest_sz << 4) | dest))) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand must be a cpu register or memory value"}; return false; }

	return true;
}

bool AssembleArgs::TryProcessFPUBinaryOp(OPCode opcode, bool integral, bool pop)
{
	// write op code
	if (!TryAppendByte((u8)opcode)) return false;

	// make sure the programmer doesn't pull any funny business due to our arg-count-based approach
	if (integral && args.size() != 1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Integral {op} requires 1 arg"}; return false; }
	if (pop && args.size() != 0 && args.size() != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Popping {op} requires 0 or 2 args"}; return false; }

	// handle arg count cases
	if (args.size() == 0)
	{
		// no args is st(1) <- f(st(1), st(0)), pop
		if (!pop) { res = {AssembleError::UsageError, "line " + tostr(line) + ": This form requires operands"}; return false; }

		if (!TryAppendByte(0x12)) return false;
	}
	else if (args.size() == 1)
	{
		u64 a, b, sizecode;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// integral
		if (integral)
		{
			if (sizecode == 1) { if (!TryAppendByte(5)) return false; }
			else if (sizecode == 2) { if (!TryAppendByte(6)) return false; }
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }
		}
		// floatint-point
		else
		{
			if (sizecode == 2) { if (!TryAppendByte(3)) return false; }
			else if (sizecode == 3) { if (!TryAppendByte(4)) return false; }
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }
		}

		// write the address
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else if (args.size() == 2)
	{
		u64 a, b;
		if (!TryParseFPURegister(args[0], a) || !TryParseFPURegister(args[1], b)) return false;

		// if b is st(0) (do this one first since it handles the pop form)
		if (b == 0)
		{
			if (!TryAppendVal(1, (a << 4) | (pop ? 2 : (u64)1))) return false;
		}
		// if a is st(0)
		else if (a == 0)
		{
			if (pop) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected ST(0) as second operand"}; return false; }

			if (!TryAppendVal(1, b << 4)) return false;
		}
		// x87 requires one of them be st(0)
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": One operand must be ST(0)"}; return false; }
	}
	else { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Too many operands"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessFPURegisterOp(OPCode opcode, bool has_ext_op, u8 ext_op)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	u64 reg;
	if (!TryParseFPURegister(args[0], reg)) return false;

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// write the register
	if (!TryAppendVal(1, reg)) return false;

	return true;
}

bool AssembleArgs::TryProcessFSTLD_WORD(OPCode opcode, u8 mode, u64 _sizecode)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// operand has to be mem
	u64 a, b, sizecode;
	Expr ptr_base;
	bool explicit_size;
	if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

	// must be the dictated size
	if (explicit_size && sizecode != _sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size is not supported"}; return false; }

	// write data
	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte(mode)) return false;
	if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;

	return true;
}

bool AssembleArgs::TryProcessFLD(OPCode opcode, bool integral)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// write op code
	if (!TryAppendByte((u8)opcode)) return false;

	// pushing st(i)
	u64 reg;
	if (TryParseFPURegister(args[0], reg))
	{
		if (!TryAppendVal(1, reg << 4)) return false;
	}
	// pushing memory value
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b, sizecode;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// handle integral cases
		if (integral)
		{
			if (sizecode != 1 && sizecode != 2 && sizecode != 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

			if (!TryAppendVal(1, sizecode + 2)) return false;
		}
		// otherwise floating-point
		else
		{
			if (sizecode != 2 && sizecode != 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

			if (!TryAppendVal(1, sizecode - 1)) return false;
		}

		// and write the address
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected an fpu register or a memory value"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessFST(OPCode opcode, bool integral, bool pop, bool trunc)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	// write the op code
	if (!TryAppendByte((u8)opcode)) return false;

	// if it's an fpu register
	u64 reg;
	if (TryParseFPURegister(args[0], reg))
	{
		// can't be an integral op
		if (integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a memory value"}; return false; }

		if (!TryAppendVal(1, (reg << 4) | (pop ? 1 : 0ul))) return false;
	}
	// if it's a memory destination
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b, sizecode;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// if this is integral (i.e. truncation store)
		if (integral)
		{
			if (sizecode == 1) { if (!TryAppendVal(1, pop ? trunc ? 11 : 7ul : 6)) return false; }
			else if (sizecode == 2) { if (!TryAppendVal(1, pop ? trunc ? 12 : 9ul : 8)) return false; }
			else if (sizecode == 3)
			{
				// there isn't a non-popping 64-bit int store
				if (!pop) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

				if (!TryAppendVal(1, trunc ? 13 : 10ul)) return false;
			}
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }
		}
		// otherwise is floating-point
		else
		{
			if (sizecode == 2) { if (!TryAppendVal(1, pop ? 3 : 2ul)) return false; }
			else if (sizecode == 3) { if (!TryAppendVal(1, pop ? 5 : 4ul)) return false; }
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }
		}

		// and write the address
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected an fpu register or memory value"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessFCOM(OPCode opcode, bool integral, bool pop, bool pop2, bool eflags, bool unordered)
{
	// write the op code
	if (!TryAppendByte((u8)opcode)) return false;

	// handle arg count cases
	if (args.size() == 0)
	{
		if (integral) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }
		if (eflags) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

		// no args is same as using st(1) (plus additional case of double pop)
		if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (1 << 4) | (pop2 ? 2 : pop ? 1 : 0ul))) return false;
	}
	else if (args.size() == 1)
	{
		// double pop doesn't accept operands
		if (pop2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"}; return false; }
		if (eflags) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

		// register
		u64 reg;
		if (TryParseFPURegister(args[0], reg))
		{
			// integral forms only store to memory
			if (integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a memory value"}; return false; }

			if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (reg << 4) | (pop ? 1 : 0ul))) return false;
		}
		// memory
		else if (args[0][args[0].size() - 1] == ']')
		{
			u64 a, b, sizecode;
			Expr ptr_base;
			bool explicit_size;
			if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

			if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

			// handle size cases
			if (sizecode == 1)
			{
				// this mode only allows int
				if (!integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

				if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (pop ? 8 : 7ul))) return false;
			}
			else if (sizecode == 2)
			{
				if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (integral ? pop ? 10 : 9ul : pop ? 4 : 3ul))) return false;
			}
			else if (sizecode == 3)
			{
				// this mode only allows fp
				if (integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

				if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (pop ? 6 : 5ul))) return false;
			}
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"}; return false; }

			// and write the address
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected an fpu register or a memory value"}; return false; }
	}
	else if (args.size() == 2)
	{
		if (integral) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"}; return false; }
		if (pop2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"}; return false; }

		u64 reg_a, reg_b;
		if (!TryParseFPURegister(args[0], reg_a) || !TryParseFPURegister(args[1], reg_b)) return false;

		// first arg must be st0
		if (reg_a != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be ST(0)"}; return false; }

		if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (reg_b << 4) | (pop ? 12 : 11))) return false;
	}
	else { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Too many operands"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessFMOVcc(OPCode opcode, u64 condition)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	u64 reg;
	if (!TryParseFPURegister(args[0], reg) || reg != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be ST(0)"}; return false; }
	if (!TryParseFPURegister(args[1], reg)) return false;

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendVal(1, (reg << 4) | condition)) return false;

	return true;
}

bool AssembleArgs::TryExtractVPUMask(std::string &arg, std::unique_ptr<Expr> &mask, bool &zmask)
{
	mask = nullptr; // no mask is denoted by null
	zmask = false; // by default, mask is not a zmask

	// if it ends in z or Z, it's a zmask
	if (arg.back() == 'z' || arg.back() == 'Z')
	{
		// remove the z
		arg.pop_back();
		arg = TrimEnd(std::move(arg));

		// ensure validity - must be preceded by }
		if (arg.size() == 0 || arg.back() != '}') { res = {AssembleError::FormatError, "line " + tostr(line) + ": Zmask declarator encountered without a corresponding mask"}; return false; }

		// mark as being a zmask
		zmask = true;
	}

	// if it ends in }, there's a white mask
	if (arg[arg.size() - 1] == '}')
	{
		// find the opening bracket
		std::size_t pos = arg.find('{');
		if (pos == std::string::npos) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Ill-formed vpu whitemask encountered"}; return false; }
		if (pos == 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Lone vpu whitemask encountered"}; ;  return false; }

		// extract the whitemask internals
		std::string innards = arg.substr(pos + 1, arg.size() - 2 - pos);
		// pop the whitemask off the arg
		arg = TrimEnd(arg.substr(0, pos));

		// parse the mask expression
		u64 sizecode;
		bool explicit_size, strict;
		Expr _mask;
		if (!TryParseImm(innards, _mask, sizecode, explicit_size, strict)) return false;

		// depending on the vpu register size being used, we're locked into a mask size, so don't allow size directives or strict specifiers
		if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"}; return false; }
		if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

		// we now need to return it as a dynamic object - handle with care
		mask = std::make_unique<Expr>(std::move(_mask));
	}

	return true;
}
bool AssembleArgs::VPUMaskPresent(Expr *mask, u64 elem_count)
{
	std::string err;

	// if it's null, it's not present
	if (mask == nullptr) return false;

	// if we can't evaluate it, it's present
	u64 val;
	bool _f;
	if (!mask->Evaluate(file.Symbols, val, _f, err)) return true;

	// otherwise, if the mask value isn't all 1's over the relevant region, it's present
	switch (elem_count)
	{
	case 1: return (val & 1) != 1;
	case 2: return (val & 3) != 3;
	case 4: return (val & 0xf) != 0xf;
	case 8: return (u8)val != 0xff;
	case 16: return (u16)val != 0xffff;
	case 32: return (u32)val != 0xffffffff;
	case 64: return (u64)val != 0xffffffffffffffff;

	default: throw std::invalid_argument("elem_count was invalid. got: " + tostr(elem_count));
	}
}

bool AssembleArgs::TryProcessVPUMove(OPCode opcode, u64 elem_sizecode, bool maskable, bool aligned, bool scalar)
{
	if (args.size() != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write the op code
	if (!TryAppendByte((u8)opcode)) return false;

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;
	// if it had an explicit mask and we were told not to allow that, it's an error
	if (mask != nullptr && !maskable) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Instruction does not support masking"}; return false; }

	// vreg, *
	u64 dest, dest_sizecode;
	if (TryParseVPURegister(args[0], dest, dest_sizecode))
	{
		u64 elem_count = scalar ? 1 : Size(dest_sizecode) >> elem_sizecode;
		bool mask_present = VPUMaskPresent(mask.get(), elem_count);

		// if we're in vector mode and the mask is not present, we can kick it up to 64-bit mode (for performance)
		if (!scalar && !mask_present) elem_sizecode = 3;

		// vreg, vreg
		u64 src, src_sizecode;
		if (TryParseVPURegister(args[1], src, src_sizecode))
		{
			if (dest_sizecode != src_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

			if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
			if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 0)) return false;
			if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
			if (!TryAppendVal(1, src)) return false;
		}
		// vreg, mem
		else if (args[1][args[1].size() - 1] == ']')
		{
			u64 a, b;
			Expr ptr_base;
			bool src_explicit;
			if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

			if (src_explicit && src_sizecode != (scalar ? elem_sizecode : dest_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

			if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
			if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
			if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// vreg, imm
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register or memory value as second operand"}; return false; }
	}
	// mem, *
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool dest_explicit;
		if (!TryParseAddress(args[0], a, b, ptr_base, dest_sizecode, dest_explicit)) return false;

		// mem, vreg
		u64 src, src_sizecode;
		if (TryParseVPURegister(args[1], src, src_sizecode))
		{
			if (dest_explicit && dest_sizecode != (scalar ? elem_sizecode : src_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

			u64 elem_count = scalar ? 1 : Size(src_sizecode) >> elem_sizecode;
			bool mask_present = VPUMaskPresent(mask.get(), elem_count);

			// if we're in vector mode and the mask is not present, we can kick it up to 64-bit mode (for performance)
			if (!scalar && !mask_present) elem_sizecode = 3;

			if (!TryAppendVal(1, (src << 3) | (aligned ? 4 : 0ul) | (src_sizecode - 4))) return false;
			if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 2)) return false;
			if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// mem, mem/imm
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register as second operand"}; return false; }
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register or a memory value as first operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUBinary(OPCode opcode, u64 elem_sizecode, bool maskable, bool aligned, bool scalar, bool has_ext_op, u8 ext_op)
{
	if (args.size() != 2 && args.size() != 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 2 or 3 operands"}; return false; }

	// write the op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;
	// if it had an explicit mask and we were told not to allow that, it's an error
	if (mask != nullptr && !maskable) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Instruction does not support masking"}; return false; }

	// vreg, *
	u64 dest, dest_sizecode;
	if (TryParseVPURegister(args[0], dest, dest_sizecode))
	{
		u64 elem_count = scalar ? 1 : Size(dest_sizecode) >> elem_sizecode;
		bool mask_present = VPUMaskPresent(mask.get(), elem_count);

		// 2 args case
		if (args.size() == 2)
		{
			// vreg, vreg
			u64 src, src_sizecode;
			if (TryParseVPURegister(args[1], src, src_sizecode))
			{
				if (dest_sizecode != src_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

				if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
				if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 0)) return false;
				if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
				if (!TryAppendVal(1, dest)) return false;
				if (!TryAppendVal(1, src)) return false;
			}
			// vreg, mem
			else if (args[1][args[1].size() - 1] == ']')
			{
				u64 a, b;
				Expr ptr_base;
				bool src_explicit;
				if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

				if (src_explicit && src_sizecode != (scalar ? elem_sizecode : dest_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

				if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
				if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
				if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
				if (!TryAppendVal(1, dest)) return false;
				if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
			}
			// vreg, imm
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register or memory value as second operand"}; return false; }
		}
		// 3 args case
		else
		{
			// vreg, vreg, *
			u64 src1, src1_sizecode;
			if (TryParseVPURegister(args[1], src1, src1_sizecode))
			{
				// vreg, vreg, vreg
				u64 src2, src2_sizecode;
				if (TryParseVPURegister(args[2], src2, src2_sizecode))
				{
					if (dest_sizecode != src1_sizecode || src1_sizecode != src2_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

					if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
					if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 0)) return false;
					if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
					if (!TryAppendVal(1, src1)) return false;
					if (!TryAppendVal(1, src2)) return false;
				}
				// vreg, vreg, mem
				else if (args[2][args[2].size() - 1] == ']')
				{
					u64 a, b;
					Expr ptr_base;
					bool src2_explicit;
					if (!TryParseAddress(args[2], a, b, ptr_base, src2_sizecode, src2_explicit)) return false;

					if (dest_sizecode != src1_sizecode || (src2_explicit && src2_sizecode != (scalar ? elem_sizecode : dest_sizecode))) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

					if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
					if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
					if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
					if (!TryAppendVal(1, src1)) return false;
					if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
				}
				// vreg, imm
				else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register or memory value as third operand"}; return false; }
			}
			// vreg, mem/imm, *
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register as second operand"}; return false; }
		}
	}
	// mem/imm, *
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register as first operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUUnary(OPCode opcode, u64 elem_sizecode, bool maskable, bool aligned, bool scalar, bool has_ext_op, u8 ext_op)
{
	if (args.size() != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write the op code
	if (!TryAppendByte((u8)opcode)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;
	// if it had an explicit mask and we were told not to allow that, it's an error
	if (mask != nullptr && !maskable) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Instruction does not support masking"}; return false; }

	// vreg, *
	u64 dest, dest_sizecode;
	if (TryParseVPURegister(args[0], dest, dest_sizecode))
	{
		u64 elem_count = scalar ? 1 : Size(dest_sizecode) >> elem_sizecode;
		bool mask_present = VPUMaskPresent(mask.get(), elem_count);

		// vreg, vreg
		u64 src, src_sizecode;
		if (TryParseVPURegister(args[1], src, src_sizecode))
		{
			if (dest_sizecode != src_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

			if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
			if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 0)) return false;
			if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
			if (!TryAppendVal(1, src)) return false;
		}
		// vreg, mem
		else if (args[1].back() == ']')
		{
			u64 a, b;
			Expr ptr_base;
			bool src_explicit;
			if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

			if (src_explicit && src_sizecode != (scalar ? elem_sizecode : dest_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"}; return false; }

			if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
			if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
			if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// vreg, imm
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register or memory value as second operand"}; return false; }
	}
	// mem/imm, *
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register as first operand"}; return false; }

	return true;
}

bool AssembleArgs::TryProcessVPUBinary_2arg(OPCode opcode, u64 elem_sizecode, bool maskable, bool aligned, bool scalar, bool has_ext_op, u8 ext_op)
{
	// ensure we select the 2 arg pathway
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }
	// then refer to VPUBinary
	return TryProcessVPUBinary(opcode, elem_sizecode, maskable, aligned, scalar, has_ext_op, ext_op);
}

bool AssembleArgs::TryProcessVPU_FCMP(OPCode opcode, u64 elem_sizecode, bool maskable, bool aligned, bool scalar)
{
	// has the 2-3 args + 1 for the comparison predicate
	if (args.size() != 3 && args.size() != 4) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Expected 3 or 4 operands" }; return false; }

	// last arg must be the comarison predicate imm - instant integral imm [0-31]
	u64 pred, sizecode;
	bool floating, explicit_sizecode, strict;
	if (!TryParseInstantImm(args.back(), pred, floating, sizecode, explicit_sizecode, strict)) return false;

	// we're forcing this quantity to be of a specific encoding format, so it doesn't make sense to allow specifying size information to the predicate expression
	if (explicit_sizecode) { res = { AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed" }; return false; }
	if (strict) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A STRICT specifier in this context is not allowed"}; return false; }

	// must be integral (and in bounds)
	if (floating) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Comparison predicate must be an integer"}; return false; }
	if (pred >= 32) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Comparison predicate out of range"}; return false; }

	// remove the comparison predicate arg
	args.pop_back();

	// now refer to binary vpu formatter
	return TryProcessVPUBinary(opcode, elem_sizecode, maskable, aligned, scalar, true, (u8)pred);
}

bool AssembleArgs::TryProcessVPUCVT_scalar_f2i(OPCode opcode, bool trunc, bool single)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write the opcode
	if (!TryAppendByte((u8)opcode)) return false;

	u8 mode = trunc ? 8 : 0; // decoded vpucvt mode
	if (single) mode += 4;

	// dest must be cpu register
	u64 dest, dest_sizecode;
	bool dest_high;
	if (!TryParseCPURegister(args[0], dest, dest_sizecode, dest_high)) return false;
	
	// account for dest size
	if (dest_sizecode == 3) mode += 2;
	else if (dest_sizecode != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified destination size not supported"}; return false; }

	// if source is xmm register
	u64 src, src_sizecode;
	if (TryParseVPURegister(args[1], src, src_sizecode))
	{
		// only xmm registers are supported
		if (src_sizecode != 4) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified source size not supported"}; return false; }
		// this check /should/ be redundant, but better safe than sorry
		if (src > 15) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Source register must be xmm0-15"}; return false; }

		// write the data
		if (!TryAppendByte(mode)) return false;
		if (!TryAppendByte((u8)((dest << 4) | src))) return false;
	}
	// if source is memory
	else if (args[1].back() == ']')
	{
		u64 a, b;
		bool src_explicit;
		Expr ptr_base;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

		// make sure the size matches what we're expecting
		if(src_explicit && src_sizecode != (single ? 2 : 3)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified source size not supported"}; return false; }

		++mode; // account for the memory mode case

		// write the data
		if (!TryAppendByte(mode)) return false;
		if (!TryAppendByte((u8)(dest << 4))) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected xmm register or memory value as second operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUCVT_scalar_i2f(OPCode opcode, bool single)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write the opcode
	if (!TryAppendByte((u8)opcode)) return false;

	u8 mode = single ? 20 : 16; // decoded vpucvt mode

	// dest must be xmm register
	u64 dest, dest_sizecode;
	if (!TryParseVPURegister(args[0], dest, dest_sizecode)) return false;

	// only xmm registers are supported
	if (dest_sizecode != 4) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified destination size not supported"}; return false; }
	// this check /should/ be redundant, but better safe than sorry
	if (dest > 15) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Destination register must be xmm0-15"}; return false; }

	// if source is reg
	u64 src, src_sizecode;
	bool src_high;
	if (TryParseCPURegister(args[1], src, src_sizecode, src_high))
	{
		// account for size case
		if (src_sizecode == 3) mode += 2;
		else if (src_sizecode != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified source size not supported"}; return false; }

		// write the data
		if (!TryAppendByte(mode)) return false;
		if (!TryAppendByte((u8)((dest << 4) | src))) return false;
	}
	// if source is mem
	else if (args[1].back() == ']')
	{
		u64 a, b;
		bool src_explicit;
		Expr ptr_base;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

		// we need to know what format we're converting from
		if (!src_explicit) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// account for size case
		if (src_sizecode == 3) mode += 2;
		else if (src_sizecode != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified source size not supported"}; return false; }

		++mode; // account for memory mode case

		// write the data
		if (!TryAppendByte(mode)) return false;
		if (!TryAppendByte((u8)(dest << 4))) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register or memory value as second operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUCVT_scalar_f2f(OPCode opcode, bool extend)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	// write the opcode
	if (!TryAppendByte((u8)opcode)) return false;

	u8 mode = extend ? 26 : 24; // decoded vpucvt mode

	// dest must be xmm register
	u64 dest, dest_sizecode;
	if (!TryParseVPURegister(args[0], dest, dest_sizecode)) return false;

	// only xmm registers are supported
	if (dest_sizecode != 4) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified destination size not supported"}; return false; }
	// this check /should/ be redundant, but better safe than sorry
	if (dest > 15) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Destination register must be xmm0-15"}; return false; }

	// if source is xmm register
	u64 src, src_sizecode;
	if (TryParseVPURegister(args[1], src, src_sizecode))
	{
		// only xmm registers are supported
		if (src_sizecode != 4) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified source size not supported"}; return false; }
		// this check /should/ be redundant, but better safe than sorry
		if (src > 15) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Source register must be xmm0-15"}; return false; }

		// write the data
		if (!TryAppendByte(mode)) return false;
		if (!TryAppendByte((u8)((dest << 4) | src))) return false;
	}
	// if source is memory
	else if (args[1].back() == ']')
	{
		u64 a, b;
		bool src_explicit;
		Expr ptr_base;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

		// make sure the size matches what we're expecting
		if (src_explicit && src_sizecode != (extend ? 2 : 3)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified source size not supported"}; return false; }

		++mode; // account for the memory mode case

		// write the data
		if (!TryAppendByte(mode)) return false;
		if (!TryAppendByte((u8)(dest << 4))) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected xmm register or memory value as second operand"}; return false; }

	return true;
}

bool AssembleArgs::__TryProcessVPUCVT_packed_formatter_reg(OPCode opcode, u8 mode, u64 elem_count, u64 dest, Expr *mask, bool zmask, u64 src)
{
	bool mask_present = VPUMaskPresent(mask, elem_count);

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte(mode)) return false;
	if (!TryAppendByte((u8)(dest << 3) | (0) | (mask_present ? 2 : 0) | (zmask ? 1 : 0))) return false;
	if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
	if (!TryAppendByte((u8)src)) return false;

	return true;
}
bool AssembleArgs::__TryProcessVPUCVT_packed_formatter_mem(OPCode opcode, u8 mode, u64 elem_count, u64 dest, Expr *mask, bool zmask, u64 a, u64 b, Expr &&ptr_base)
{
	bool mask_present = VPUMaskPresent(mask, elem_count);

	if (!TryAppendByte((u8)opcode)) return false;
	if (!TryAppendByte(mode)) return false;
	if (!TryAppendByte((u8)(dest << 3) | (4) | (mask_present ? 2 : 0) | (zmask ? 1 : 0))) return false;
	if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
	if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;

	return true;
}

bool AssembleArgs::TryProcessVPUCVT_packed_f2i(OPCode opcode, bool trunc, bool single)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	u8 mode = trunc ? 34 : 28; // decoded vpucvt mode
	if (single) mode += 3;

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;

	// dest must be vpu register
	u64 dest, dest_sizecode;
	if (!TryParseVPURegister(args[0], dest, dest_sizecode)) return false;

	// if src is a vpu register
	u64 src, src_sizecode;
	if (TryParseVPURegister(args[1], src, src_sizecode))
	{
		// validate operand sizes
		if (dest_sizecode != (single ? src_sizecode : src_sizecode == 4 ? 4 : src_sizecode - 1)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported"}; return false; }

		// decode size and get number of elements
		mode += (u8)src_sizecode - 4;
		u64 elem_count = (single ? 4 : 2) << (src_sizecode - 4);

		// write the data
		if (!__TryProcessVPUCVT_packed_formatter_reg(opcode, mode, elem_count, dest, mask.get(), zmask, src)) return false;
	}
	// if src is mem
	else if (args[1].back() == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool src_explicit;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

		// validate operand sizes
		if (src_explicit && dest_sizecode != (single ? src_sizecode : src_sizecode - 1)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported"}; return false; }

		// decode size and get number of elements
		u64 elem_count;
		// single is converting to same size, so we can use dest_sz instead
		if (single)
		{
			mode += (u8)dest_sizecode - 4;
			elem_count = 4 << (dest_sizecode - 4);
		}
		// otherwise we can't tell 2 vs 4 doubles since they both use xmm dest - we need explicit source size
		else if (src_explicit)
		{
			mode += (u8)src_sizecode - 4;
			elem_count = 2 << (src_sizecode - 4);
		}
		// however, if dest is ymm, we know source is zmm
		else if (dest_sizecode == 5)
		{
			mode += 2;
			elem_count = 8;
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"}; return false; }

		// write the data
		if (!__TryProcessVPUCVT_packed_formatter_mem(opcode, mode, elem_count, dest, mask.get(), zmask, a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a VPU register or memory value as second operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUCVT_packed_i2f(OPCode opcode, bool single)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"}; return false; }

	u8 mode = single ? 43 : 40; // decoded vpucvt mode

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;

	// dest must be vpu register
	u64 dest, dest_sizecode;
	if (!TryParseVPURegister(args[0], dest, dest_sizecode)) return false;

	// if src is a vpu register
	u64 src, src_sizecode;
	if (TryParseVPURegister(args[1], src, src_sizecode))
	{
		// validate operand sizes
		if (src_sizecode != (single ? dest_sizecode : dest_sizecode == 4 ? 4 : dest_sizecode - 1)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported"}; return false; }

		// decode size and get number of elements
		mode += (u8)dest_sizecode - 4;
		u64 elem_count = (single ? 4 : 2) << (dest_sizecode - 4);

		// write the data
		if (!__TryProcessVPUCVT_packed_formatter_reg(opcode, mode, elem_count, dest, mask.get(), zmask, src)) return false;
	}
	// if src is mem
	else if (args[1].back() == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool src_explicit;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

		// validate operand sizes
		if (src_explicit && src_sizecode != (single ? dest_sizecode : dest_sizecode - 1)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported"}; return false; }

		// decode size and get number of elements
		mode += (u8)dest_sizecode - 4;
		u64 elem_count = (single ? 4 : 2) << (dest_sizecode - 4);

		// write the data
		if (!__TryProcessVPUCVT_packed_formatter_mem(opcode, mode, elem_count, dest, mask.get(), zmask, a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a VPU register or memory value as second operand"}; return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUCVT_packed_f2f(OPCode opcode, bool extend)
{
	if (args.size() != 2) { res = { AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands" }; return false; }

	u8 mode = extend ? 49 : 46; // decoded vpucvt mode

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;

	// dest must be vpu register
	u64 dest, dest_sizecode;
	if (!TryParseVPURegister(args[0], dest, dest_sizecode)) return false;

	// if src is a vpu register
	u64 src, src_sizecode;
	if (TryParseVPURegister(args[1], src, src_sizecode))
	{
		u64 elem_count;
		if (extend)
		{
			// validate operand sizes
			if (src_sizecode != (dest_sizecode == 4 ? 4 : dest_sizecode - 1)) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported" }; return false; }

			// decode size and get number of elements
			mode += (u8)dest_sizecode - 4;
			elem_count = 2 << (dest_sizecode - 4);
		}
		else
		{
			// validate operand sizes
			if (dest_sizecode != (src_sizecode == 4 ? 4 : src_sizecode - 1)) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported" }; return false; }

			// decode size and get number of elements
			mode += (u8)src_sizecode - 4;
			elem_count = 2 << (src_sizecode - 4);
		}

		// write the data
		if (!__TryProcessVPUCVT_packed_formatter_reg(opcode, mode, elem_count, dest, mask.get(), zmask, src)) return false;
	}
	// if src is mem
	else if (args[1].back() == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool src_explicit;
		if (!TryParseAddress(args[1], a, b, ptr_base, src_sizecode, src_explicit)) return false;

		u64 elem_count;
		if (extend)
		{
			// validate operand sizes
			if (src_explicit && src_sizecode != dest_sizecode - 1) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported" }; return false; }

			// decode size and get number of elements
			mode += (u8)dest_sizecode - 4;
			elem_count = 2 << (dest_sizecode - 4);
		}
		else
		{
			// validate operand sizes
			if (src_explicit && dest_sizecode != src_sizecode - 1) { res = { AssembleError::UsageError, "line " + tostr(line) + ": Specified operand size combination not supported" }; return false; }

			// we can't tell 2 vs 4 elements since they both use xmm dest - we need explicit source size
			if (src_explicit)
			{
				mode += (u8)src_sizecode - 4;
				elem_count = 2 << (src_sizecode - 4);
			}
			// however, if dest is ymm, we know source is zmm
			else if (dest_sizecode == 5)
			{
				mode += 2;
				elem_count = 8;
			}
			else { res = { AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size" }; return false; }
		}

		// write the data
		if (!__TryProcessVPUCVT_packed_formatter_mem(opcode, mode, elem_count, dest, mask.get(), zmask, a, b, std::move(ptr_base))) return false;
	}
	else { res = { AssembleError::UsageError, "line " + tostr(line) + ": Expected a VPU register or memory value as second operand" }; return false; }

	return true;
}
}
