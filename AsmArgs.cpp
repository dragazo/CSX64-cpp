// -- AssembleArgs impl

#include <string>
#include <vector>

#include "AsmTables.h"
#include "AsmArgs.h"
#include "CoreTypes.h"
#include "Utility.h"

namespace CSX64
{
bool AssembleArgs::SplitLine(const std::string &rawline)
{
	// (label:) (op (arg, arg, ...))

	int pos = 0, end; // position in line parsing
	int quote;        // index of openning quote in args

	// empty args from last line
	args.clear();

	// -- parse label and op -- //

	// skip leading white space
	for (; pos < rawline.size() && std::isspace(rawline[pos]); ++pos);
	// get a white space-delimited token
	for (end = pos; end < rawline.size() && !std::isspace(rawline[end]); ++end);

	// if we got a label
	if (pos < rawline.size() && rawline[end - 1] == LabelDefChar)
	{
		// set as label def
		label_def = rawline.substr(pos, end - pos - 1);

		// get another token for op to use

		// skip leading white space
		for (pos = end; pos < rawline.size() && std::isspace(rawline[pos]); ++pos);
		// get a white space-delimited token
		for (end = pos; end < rawline.size() && !std::isspace(rawline[end]); ++end);
	}
	// otherwise there's no label for this line
	else label_def.clear();

	// if we got something, record as op, otherwise is empty string
	if (pos < rawline.size()) op = rawline.substr(pos, end - pos);
	else op.clear();

	// -- parse args -- //

	// parse the rest of the line as comma-separated tokens
	while (true)
	{
		// skip leading white space
		for (pos = end + 1; pos < rawline.size() && std::isspace(rawline[pos]); ++pos);
		// when pos reaches end of token, we're done parsing
		if (pos >= rawline.size()) break;

		// find the next terminator (comma-separated)
		for (end = pos, quote = -1; end < rawline.size(); ++end)
		{
			if (rawline[end] == '"' || rawline[end] == '\'' || rawline[end] == '`') quote = quote < 0 ? end : rawline[end] == rawline[quote] ? -1 : quote;
			else if (quote < 0 && rawline[end] == ',') break; // comma marks end of token
		}
		// make sure we closed any quotations
		if (quote >= 0) { res = {AssembleError::FormatError, std::string() + "line " + tostr(line) + ": Unmatched quotation encountered in argument list"}; return false; }

		// get the arg (remove leading/trailing white space - some logic requires them not be there e.g. address parser)
		std::string arg = Trim(rawline.substr(pos, end - pos));
		// make sure arg isn't empty
		if (arg.empty()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Empty operation argument encountered"}; return false; }
		// add this token
		args.emplace_back(std::move(arg));
	}

	// successfully parsed line
	return true;
}

bool AssembleArgs::IsValidName(const std::string &token, std::string &err)
{
	// can't be empty string
	if (token.empty()) { err = "Symbol name was empty string"; return false; }

	// first char is underscore or letter
	if (token[0] != '_' && !std::isalpha(token[0])) { err = "Symbol contained an illegal character"; return false; }
	// all other chars may additionally be numbers or periods
	for (int i = 1; i < token.size(); ++i)
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

bool AssembleArgs::TryAppendExpr(u64 size, Expr &&expr, std::vector<HoleData> &holes, std::vector<u8> &segment)
{
	std::string err; // evaluation error parsing location

	// create the hole data
	HoleData data;
	data.Address = segment.size();
	data.Size = size;
	data.Line = line;
	data.Expr = std::move(expr);

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
	if (!TryAppendByte(a)) return false;
	if (a & 3) { if (!TryAppendByte(b)) return false; }
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
		file.TextAlign = std::max<u32>(file.TextAlign, size);
		return true;
	case AsmSegment::RODATA:
		Align(file.Rodata, size);
		file.RodataAlign = std::max<u32>(file.RodataAlign, size);
		return true;
	case AsmSegment::DATA:
		Align(file.Data, size);
		file.DataAlign = std::max<u32>(file.DataAlign, size);
		return true;
	case AsmSegment::BSS:
		file.BssLen = Align(file.BssLen, size);
		file.BSSAlign = std::max<u32>(file.BSSAlign, size);
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
	case AsmSegment::DATA: Pad(file.Data, size); return false;
	case AsmSegment::BSS: file.BssLen += size; return true;

	default: res = AssembleResult{AssembleError::FormatError, "line " + tostr(line) + ": Cannot pad this segment"}; return false;
	}
}

bool AssembleArgs::__TryParseImm(const std::string &token, Expr *&expr)
{
	expr = nullptr; // initially-nulled result

	Expr *temp, *_temp; // temporary for node creation

	int pos = 0, end; // position in token

	bool binPair = false;          // marker if tree contains complete binary pairs (i.e. N+1 values and N binary ops)
	int unpaired_conditionals = 0; // number of unpaired conditional ops

	Expr::OPs op = Expr::OPs::None; // extracted binary op (initialized so compiler doesn't complain)
	int oplen = 0;                  // length of operator found (in characters)

	std::string err; // error location for hole evaluation

	std::vector<char> unaryOps; // holds unary ops for processing
	std::vector<Expr*> stack;   // the stack used to manage operator precedence rules

								// top of stack shall be refered to as current

	stack.push_back(nullptr); // stack will always have a null at its base (simplifies code slightly)

							  // skip white space
	for (; pos < token.size() && std::isspace(token[pos]); ++pos);
	// if we're past the end, token was empty
	if (pos >= token.size()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Empty expression encountered"}; return false; }

	while (true)
	{
		// -- read (unary op...)[operand](binary op) -- //

		// consume unary ops (allows white space)
		for (; pos < token.size(); ++pos)
		{
			if (Contains(UnaryOps, token[pos])) unaryOps.push_back(token[pos]); // absorb unary ops
			else if (!std::isspace(token[pos])) break; // non-white is start of operand
		}
		// if we're past the end, there were unary ops with no operand
		if (pos >= token.size()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Unary ops encountered without an operand"}; return false; }

		int depth = 0;  // parens depth - initially 0
		int quote = -1; // index of current quote char - initially not in one

		bool numeric = std::isdigit(token[pos]); // flag if this is a numeric literal

		// move end to next logical separator (white space or binary op)
		for (end = pos; end < token.size(); ++end)
		{
			// if we're not in a quote
			if (quote < 0)
			{
				// account for important characters
				if (token[end] == '(') ++depth;
				else if (token[end] == ')') --depth; // depth control
				else if (numeric && (token[end] == 'e' || token[end] == 'E') && end + 1 < token.size() && (token[end + 1] == '+' || token[end + 1] == '-')) ++end; // make sure an exponent sign won't be parsed as binary + or - by skipping it
				else if (token[end] == '"' || token[end] == '\'' || token[end] == '`') quote = end; // quotes mark start of a string
				else if (depth == 0 && (std::isspace(token[end]) || TryGetOp(token, end, op, oplen))) break; // break on white space or binary op

				// can't ever have negative depth
				if (depth < 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Mismatched parenthesis: {token}"}; return false; }
			}
			// otherwise we're in a quote
			else
			{
				// if we have a matching quote, break out of quote mode
				if (token[end] == token[quote]) quote = -1;
			}
		}
		// if depth isn't back to 0, there was a parens mismatch
		if (depth != 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Mismatched parenthesis: " + token}; return false; }
		// if quote isn't back to -1, there was a quote mismatch
		if (quote >= 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Mismatched quotation: " + token}; return false; }
		// if pos == end we'll have an empty token (e.g. expression was just a binary op)
		if (pos == end) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Empty token encountered in expression: " + token}; return false; }

		// -- convert to expression tree -- //

		// if sub-expression
		if (token[pos] == '(')
		{
			// parse the inside into temp
			if (!__TryParseImm(token.substr(pos + 1, end - pos - 2), temp)) return false;
		}
		// otherwise is value
		else
		{
			// get the value to insert
			std::string val = token.substr(pos, end - pos);

			// mutate it
			if (!MutateName(val)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse imm \"" + token + "\"\n-> " + res.ErrorMsg}; return false; }

			// if it's the current line macro
			if (val == CurrentLineMacro)
			{
				// must be in a segment
				if (current_seg == AsmSegment::INVALID) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Attempt to take an address outside of a segment"}; return false; }

				temp = new Expr;
				temp->OP = Expr::OPs::Add;
				temp->Left = Expr::NewToken(SegOffsets.at(current_seg));
				temp->Right = Expr::NewInt(line_pos_in_seg);
			}
			// if it's the start of segment macro
			else if (val == StartOfSegMacro)
			{
				// must be in a segment
				if (current_seg == AsmSegment::INVALID) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Attempt to take an address outside of a segment"}; return false; }

				temp = Expr::NewToken(SegOrigins.at(current_seg));
			}
			// otherwise it's a normal value/symbol
			else
			{
				// create the hole for it
				temp = Expr::NewToken(val);

				// it either needs to be evaluatable or a valid label name
				if (!temp->Evaluatable(file.Symbols) && !IsValidName(val, err))
				{
					res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to resolve token as a valid imm or symbol name: " + val + "\n-> " + err}; return false;
				}
			}
		}

		// handle parsed unary ops (stack provides right-to-left evaluation)
		while (unaryOps.size() > 0)
		{
			char uop = unaryOps.back(); unaryOps.pop_back();

			switch (uop)
			{
			case '+': break; // unary plus does nothing
			case '-': _temp = new Expr; _temp->OP = Expr::OPs::Neg; _temp->Left = temp; temp = _temp; break;
			case '~': _temp = new Expr; _temp->OP = Expr::OPs::BitNot; _temp->Left = temp; temp = _temp; break;
			case '!': _temp = new Expr; _temp->OP = Expr::OPs::LogNot; _temp->Left = temp; temp = _temp; break;
			case '*': _temp = new Expr; _temp->OP = Expr::OPs::Float; _temp->Left = temp; temp = _temp; break;
			case '/': _temp = new Expr; _temp->OP = Expr::OPs::Int; _temp->Left = temp; temp = _temp; break;

			default: throw std::runtime_error("unary op \'" + tostr(uop) + "}\' not implemented");
			}
		}

		// -- append subtree to main tree --

		// if no tree yet, use this one
		if (expr == nullptr) expr = temp;
		// otherwise append to current (guaranteed to be defined by second pass)
		else
		{
			// put it in the right (guaranteed by this algorithm to be empty)
			stack.back()->Right = temp;
		}

		// flag as a valid binary pair (i.e. every binary op now has 2 operands)
		binPair = true;

		// -- get binary op -- //

		// we may have stopped token parsing on white space, so wind up to find a binary op
		for (; end < token.size(); ++end)
		{
			if (TryGetOp(token, end, op, oplen)) break; // break when we find an op
														// if we hit a non-white character, there are tokens with no binary ops between them
			else if (!std::isspace(token[end])) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Encountered two tokens with no binary op between them: " + token}; return false; }
		}
		// if we didn't find any binary ops, we're done
		if (end >= token.size()) break;

		// -- process binary op -- //

		// ternary conditional has special rules
		if (op == Expr::OPs::Pair)
		{
			// seek out nearest conditional without a pair
			for (; stack.back() && (stack.back()->OP != Expr::OPs::Condition || stack.back()->Right->OP == Expr::OPs::Pair); stack.pop_back());
			// if we didn't find anywhere to put it, this is an error
			if (stack.back() == nullptr) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expression contained a ternary conditional pair without a corresponding condition: " + token}; return false; }
		}
		// right-to-left operators
		else if (op == Expr::OPs::Condition)
		{
			// wind current up to correct precedence (right-to-left evaluation, so don't skip equal precedence)
			for (; stack.back() && Precedence.at(stack.back()->OP) < Precedence.at(op); stack.pop_back());
		}
		// left-to-right operators
		else
		{
			// wind current up to correct precedence (left-to-right evaluation, so also skip equal precedence)
			for (; stack.back() && Precedence.at(stack.back()->OP) <= Precedence.at(op); stack.pop_back());
		}

		// if we have a valid current
		if (stack.back())
		{
			// splice in the new operator, moving current's right sub-tree to left of new node
			_temp = new Expr;
			_temp->OP = op;
			_temp->Left = stack.back()->Right;
			stack.push_back(stack.back()->Right = _temp);
		}
		// otherwise we'll have to move the root
		else
		{
			// splice in the new operator, moving entire tree to left of new node
			_temp = new Expr;
			_temp->OP = op;
			_temp->Left = expr;
			stack.push_back(expr = _temp);
		}

		binPair = false; // flag as invalid binary pair

						 // update unpaired conditionals
		if (op == Expr::OPs::Condition) ++unpaired_conditionals;
		else if (op == Expr::OPs::Pair) --unpaired_conditionals;

		// pass last delimiter
		pos = end + oplen;
	}

	// handle binary pair mismatch
	if (!binPair) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expression contained a mismatched binary op"}; return false; }
	// make sure all conditionals were matched
	if (unpaired_conditionals != 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expression contained incomplete ternary conditionals"}; return false; }

	// run ptrdiff logic on result
	_temp = Ptrdiff(expr);
	delete expr;
	expr = _temp;

	return true;
}
bool AssembleArgs::TryParseImm(std::string token, Expr &expr, u64 &sizecode, bool &explicit_size)
{
	sizecode = 3; explicit_size = false; // initially no explicit size

										 // handle explicit sizes directives
	std::string utoken = ToUpper(token);
	if (StartsWithToken(utoken, "BYTE")) { sizecode = 0; explicit_size = true; token = TrimStart(token.substr(4)); }
	else if (StartsWithToken(utoken, "WORD")) { sizecode = 1; explicit_size = true; token = TrimStart(token.substr(4)); }
	else if (StartsWithToken(utoken, "DWORD")) { sizecode = 2; explicit_size = true; token = TrimStart(token.substr(5)); }
	else if (StartsWithToken(utoken, "QWORD")) { sizecode = 3; explicit_size = true; token = TrimStart(token.substr(5)); }

	// refer to helper (and iron out the pointer stuff for the caller)
	Expr *ptr;
	if (!__TryParseImm(token, ptr)) { delete ptr; return false; }
	expr = std::move(*ptr);
	delete ptr;

	return true;
}
bool AssembleArgs::TryParseInstantImm(std::string token, u64 &val, bool &floating, u64 &sizecode, bool &explicit_size)
{
	std::string err; // error location for evaluation

	Expr hole;
	if (!TryParseImm(std::move(token), hole, sizecode, explicit_size)) return false;
	if (!hole.Evaluate(file.Symbols, val, floating, err)) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Failed to evaluate instant imm: " + token + "\n-> " + err}; return false; }

	return true;
}

bool AssembleArgs::TryExtractPtrVal(const Expr *expr, const Expr *&val, const std::string &_base)
{
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
	if (expr->OP != Expr::OPs::Add || expr->Left->Token() && *expr->Left->Token() != _base) return false;

	// return the value portion
	val = expr->Right;
	return true;
}
Expr *AssembleArgs::Ptrdiff(Expr *expr)
{
	// on null, return null as well
	if (expr == nullptr) return nullptr;

	std::vector<const Expr*> add; // list of added terms
	std::vector<const Expr*> sub; // list of subtracted terms

	const Expr *a = nullptr, *b = nullptr; // expression temporaries for lookup (initialized cause compiler yelled at me)
	Expr *temp;

	// populate lists
	expr->PopulateAddSub(add, sub);

	// perform ptrdiff reduction on anything defined by the linker
	for (const std::string &seg_name : VerifyLegalExpressionIgnores)
	{
		for (int i = 0, j = 0; ; ++i, ++j)
		{
			// wind i up to next add label
			for (; i < add.size() && !TryExtractPtrVal(add[i], a, seg_name); ++i);
			// if this exceeds bounds, break
			if (i >= add.size()) break;

			// wind j up to next sub label
			for (; j < sub.size() && !TryExtractPtrVal(sub[j], b, seg_name); ++j);
			// if this exceeds bounds, break
			if (j >= sub.size()) break;

			// we got a pair: replace items in add/sub with their pointer values
			// if extract succeeded but returned null, the "missing" value is zero - just remove from the list
			if (a) add[i] = a; else { std::swap(add[i], add.back()); add.pop_back(); }
			if (b) sub[j] = b; else { std::swap(sub[j], sub.back()); sub.pop_back(); }
		}
	}

	// for each add item
	for (int i = 0; i < add.size(); ++i)
	{
		// if it's not a leaf
		if (!add[i]->IsLeaf())
		{
			// recurse on children
			temp = new Expr;
			temp->OP = add[i]->OP;
			temp->Left = Ptrdiff(add[i]->Left);
			temp->Right = Ptrdiff(add[i]->Right);
			add[i] = temp;
		}
		// otherwise take a copy (so result won't be obliterated by deleting the source)
		else add[i] = new Expr(std::move(*add[i]));
	}
	// for each sub item
	for (int i = 0; i < sub.size(); ++i)
	{
		// if it's not a leaf
		if (!sub[i]->IsLeaf())
		{
			// recurse on children
			temp = new Expr;
			temp->OP = sub[i]->OP;
			temp->Left = Ptrdiff(sub[i]->Left);
			temp->Right = Ptrdiff(sub[i]->Right);
			sub[i] = temp;
		}
		// otherwise take a copy (so result won't be obliterated by deleting the source)
		else sub[i] = new Expr(std::move(*add[i]));
	}

	// stitch together the new tree
	if (sub.size() == 0) temp = new Expr(Expr::ChainAddition(add));
	else if (add.size() == 0)
	{
		temp = new Expr;
		temp->OP = Expr::OPs::Neg;
		temp->Left = new Expr(Expr::ChainAddition(sub));
		return temp;
	}
	else
	{
		temp = new Expr;
		temp->OP = Expr::OPs::Sub;
		temp->Left = new Expr(Expr::ChainAddition(add));
		temp->Right = new Expr(Expr::ChainAddition(sub));
		return temp;
	}

	return temp;
}

bool AssembleArgs::TryParseInstantPrefixedImm(const std::string &token, const std::string &prefix, u64 &val, bool &floating, u64 &sizecode, bool &explicit_size)
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
		for (end = prefix.size() + 1; end < token.size() && depth > 0; ++end)
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
		for (end = prefix.size(); end < token.size() && (std::isalnum(token[end]) || token[end] == '_' || token[end] == '.'); ++end);
	}

	// make sure we consumed the entire string
	if (end != token.size()) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Compound expressions used as prefixed expressions must be parenthesized \"" + token}; return false; }

	// prefix index must be instant imm
	if (!TryParseInstantImm(token.substr(prefix.size()), val, floating, sizecode, explicit_size)) { res = {AssembleError::ArgError, "line " + tostr(line) + ": Failed to parse instant prefixed imm \"" + token + "\"\n-> " + res.ErrorMsg}; return false; }

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

bool AssembleArgs::TryParseAddressReg(const std::string &label, Expr &hole, bool &present, u64 &m)
{
	m = 0; present = false; // initialize out params

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
		if (list.size() == 1 || list.size() > 1 && list[1]->OP != Expr::OPs::Mul)
		{
			// add in a multiplier of 1
			list[0]->OP = Expr::OPs::Mul;
			list[0]->Left = Expr::NewInt(1);
			list[0]->Right = Expr::NewToken(*list[0]->Token());

			// insert new register location as beginning of path
			list.insert(list.begin(), list[0]->Right);
		}

		// start 2 above (just above regular mult code)
		for (int i = 2; i < list.size();)
		{
			switch (list[i]->OP)
			{
			case Expr::OPs::Add: case Expr::OPs::Sub: case Expr::OPs::Neg: ++i; break;

			case Expr::OPs::Mul:
			{
				// toward leads to register, mult leads to mult value
				Expr *toward = list[i - 1], *mult = list[i]->Left == list[i - 1] ? list[i]->Right : list[i]->Left;

				// if pos is add/sub, we need to distribute
				if (toward->OP == Expr::OPs::Add || toward->OP == Expr::OPs::Sub)
				{
					// swap operators with toward
					list[i]->OP = toward->OP;
					toward->OP = Expr::OPs::Mul;

					// create the distribution node
					Expr *temp = new Expr;
					temp->OP = Expr::OPs::Mul;
					temp->Left = mult;

					// compute right and transfer mult to toward
					if (toward->Left == list[i - 2]) { temp->Right = toward->Right; toward->Right = mult; }
					else { temp->Right = toward->Left; toward->Left = mult; }

					// add it in
					if (list[i]->Left == mult) list[i]->Left = temp; else list[i]->Right = temp;
				}
				// if pos is mul, we need to combine with pre-existing mult code
				else if (toward->OP == Expr::OPs::Mul)
				{
					// create the combination node
					Expr *temp = new Expr;
					temp->OP = Expr::OPs::Mul;
					temp->Left = mult;
					temp->Right = toward->Left == list[i - 2] ? toward->Right : toward->Left;

					// add it in
					if (list[i]->Left == mult)
					{
						list[i]->Left = temp; // replace mult with combination
						list[i]->Right = list[i - 2]; // bump up toward
					}
					else
					{
						list[i]->Right = temp;
						list[i]->Left = list[i - 2];
					}

					// remove the skipped list[i - 1]
					list.erase(list.begin() + (i - 1));
				}
				// if pos is neg, we need to put the negative on the mult
				else if (toward->OP == Expr::OPs::Neg)
				{
					// create the combinartion node
					Expr *temp = new Expr;
					temp->OP = Expr::OPs::Neg;
					temp->Left = mult;

					// add it in
					if (list[i]->Left == mult)
					{
						list[i]->Left = temp; // replace mult with combination
						list[i]->Right = list[i - 2]; // bump up toward
					}
					else
					{
						list[i]->Right = temp;
						list[i]->Left = list[i - 2];
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
		if (!(list[1]->Left == list[0] ? list[1]->Right : list[1]->Left)->Evaluate(file.Symbols, val, floating, err))
		{
			res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to evaluate register multiplier as an instant imm\n-> " + err}; return false;
		}
		// make sure it's not floating-point
		if (floating) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Register multiplier may not be floating-point"}; return false; }

		// look through from top to bottom
		for (int i = list.size() - 1; i >= 2; --i)
		{
			// if this will negate the register
			if (list[i]->OP == Expr::OPs::Neg || list[i]->OP == Expr::OPs::Sub && list[i]->Right == list[i - 1])
			{
				// negate found partial mult
				val = ~val + 1;
			}
		}

		// remove the register section from the expression (replace with integral 0)
		list[1]->IntResult(0);

		m += val; // add extracted mult to total mult
		list.clear(); // clear list for next pass
	}

	// -- final task: get mult code and negative flag -- //

	// only other thing is transforming the multiplier into a mult code
	switch (m)
	{
		// 0 is not present
	case 0: m = 0; present = false; break;

		// if present, only 1, 2, 4, and 8 are allowed
	case 1: m = 0; present = true; break;
	case 2: m = 1; present = true; break;
	case 4: m = 2; present = true; break;
	case 8: m = 3; present = true; break;

	default: res = {AssembleError::ArgError, "line " + tostr(line) + ": Register multipliers may only be 1, 2, 4, or 8. Got: " + tostr((i64)m) + "*" + label}; return false;
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

					  // extract the address internals
	token = token.substr(1, token.size() - 2);

	// turn into an expression
	if (!TryParseImm(token, ptr_base, sz, explicit_sz)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to parse address expression\n-> " + res.ErrorMsg}; return false; }

	// look through all the register names
	for (const auto &entry : CPURegisterInfo)
	{
		// extract the register data
		bool present;
		u64 mult;
		if (!TryParseAddressReg(entry.first, ptr_base, present, mult)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Failed to extract register data\n-> " + res.ErrorMsg}; return false; }

		// if the register is present we need to do something with it
		if (present)
		{
			// if we have an explicit address component size to enforce
			if (explicit_sz)
			{
				// if this conflicts with the current one, it's an error
				if (sz != std::get<1>(entry.second)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Encountered address components of conflicting sizes"}; return false; }
			}
			// otherwise record this as the size to enforce
			else { sz = std::get<1>(entry.second); explicit_sz = true; }

			// if the multiplier is non-trivial (non-1) it has to go in r1
			if (mult != 0)
			{
				// if r1 already has a value, this is an error
				if (r1 != 666) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Only one register may have a (non-1) pre-multiplier"}; return false; }

				r1 = std::get<0>(entry.second);
				m1 = mult;
			}
			// otherwise multiplier is trivial, try to put it in r2 first
			else if (r2 == 666) r2 = std::get<0>(entry.second);
			// if that falied, try r1
			else if (r1 == 666) r1 = std::get<0>(entry.second);
			// if nothing worked, both are full
			else { res = {AssembleError::FormatError, "line " + tostr(line) + ": An address expression may use up to 2 registers"}; return false; }
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
			|| Contains(VerifyLegalExpressionIgnores, tok))
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
	for (HoleData &hole : file.TextHoles) if (!VerifyLegalExpression(hole.Expr)) return false;
	for (HoleData &hole : file.RodataHoles) if (!VerifyLegalExpression(hole.Expr)) return false;
	for (HoleData &hole : file.DataHoles) if (!VerifyLegalExpression(hole.Expr)) return false;

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
bool AssembleArgs::TryProcessLabel()
{
	if (!label_def.empty())
	{
		std::string err;

		// if it's not a local, mark as last non-local label
		if (label_def[0] != '.') last_nonlocal_label = label_def;

		// mutate and test result for legality
		if (!MutateName(label_def)) return false;
		if (!IsValidName(label_def, err)) { res = {AssembleError::InvalidLabel, "line " + tostr(line) + ": " + err}; return false; }

		// it can't be a reserved symbol
		if (IsReservedSymbol(label_def)) { res = {AssembleError::InvalidLabel, "line " + tostr(line) + ": Symbol name is reserved: " + label_def}; return false; }

		// ensure we don't redefine a symbol
		if (ContainsKey(file.Symbols, label_def)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Symbol was already defined: " + label_def}; return false; }
		// ensure we don't define an external
		if (Contains(file.ExternalSymbols, label_def)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define external symbol internally: " + label_def}; return false; }

		// if it's not an EQU expression, inject a label
		if (ToUpper(op) != "EQU")
		{
			// addresses must be in a valid segment
			if (current_seg == AsmSegment::INVALID) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Attempt to address outside of a segment"}; return false; }

			Expr temp;
			temp.OP = Expr::OPs::Add;
			temp.Left = Expr::NewToken(SegOffsets.at(current_seg));
			temp.Right = Expr::NewInt(line_pos_in_seg);
			file.Symbols.emplace(std::move(label_def), std::move(temp));
		}
	}

	return true;
}

bool AssembleArgs::TryProcessAlignXX(u64 size)
{
	if (args.size() != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected no operands"}; return false; }

	return TryAlign(size);
}
bool AssembleArgs::TryProcessAlign()
{
	if (args.size() != 1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 1 operand"}; return false; }

	u64 val, sizecode;
	bool floating, explicit_size;
	if (!TryParseInstantImm(args[0], val, floating, sizecode, explicit_size)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Alignment value must be instant\n-> " + res.ErrorMsg}; return false; }
	if (floating) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Alignment value cannot be floating-point"}; return false; }
	if (val == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Attempt to align to a multiple of zero"}; return false; }

	if (!IsPowerOf2(val)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Alignment value must be a power of 2. Got " + tostr(val)}; return false; }

	return TryAlign(val);
}

bool AssembleArgs::TryProcessGlobal()
{
	if (args.size() == 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected at least one symbol to export"};   return false; }

	std::string err;
	for (std::string &symbol : args)
	{
		// special error message for using global on local labels
		if (symbol[0] == '.') { res = {AssembleError::ArgError, "line " + tostr(line) + ": Cannot export local symbols without their full declaration"};   return false; }
		// test name for legality
		if (!IsValidName(symbol, err)) { res = {AssembleError::InvalidLabel, "line " + tostr(line) + ": {err}"};   return false; }

		// don't add to global list twice
		if (Contains(file.GlobalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Attempt to export \"" + symbol + "\" multiple times"};   return false; }
		// ensure we don't global an external
		if (Contains(file.ExternalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define external \"" + symbol + "\" as global"};   return false; }

		// add it to the globals list
		file.GlobalSymbols.emplace(std::move(symbol));
	}

	return true;
}
bool AssembleArgs::TryProcessExtern()
{
	if (args.size() == 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected at least one symbol to import"};   return false; }

	std::string err;
	for (std::string &symbol : args)
	{
		// special error message for using extern on local labels
		if (symbol[0] == '.') { res = {AssembleError::ArgError, "line " + tostr(line) + ": Cannot import local symbols"};   return false; }
		// test name for legality
		if (!IsValidName(symbol, err)) { res = {AssembleError::InvalidLabel, "line " + tostr(line) + ": {err}"};   return false; }

		// ensure we don't extern a symbol that already exists
		if (ContainsKey(file.Symbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define symbol \"{symbol}\" (defined internally) as external"};   return false; }

		// don't add to external list twice
		if (Contains(file.ExternalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Attempt to import \"{symbol}\" multiple times"};   return false; }
		// ensure we don't extern a global
		if (Contains(file.GlobalSymbols, symbol)) { res = {AssembleError::SymbolRedefinition, "line " + tostr(line) + ": Cannot define global \"{symbol}\" as external"};   return false; }

		// add it to the external list
		file.ExternalSymbols.emplace(std::move(symbol));
	}

	return true;
}

bool AssembleArgs::TryProcessDeclare(u64 size)
{
	if (args.size() == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected at least 1 value to write"};   return false; }

	Expr expr;
	std::string chars, err;

	// for each argument (not using foreach because order is incredibly important and i'm paranoid)
	for (int i = 0; i < args.size(); ++i)
	{
		// if it's a string
		if (args[i][0] == '"' || args[i][0] == '\'' || args[i][0] == '`')
		{
			// get its chars
			if (!TryExtractStringChars(args[i], chars, err)) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Invalid string literal: {args[i]}\n-> {err}"};   return false; }

			// dump into memory (one byte each)
			for (int j = 0; j < chars.size(); ++j) if (!TryAppendByte((u8)chars[j])) return false;
			// make sure we write a multiple of size
			if (!TryPad(AlignOffset((u64)chars.size(), size))) return false;
		}
		// otherwise it's a value
		else
		{
			// can only use standard sizes
			if (size > 8) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Attempt to write a numeric value in an unsuported format"};   return false; }

			// get the value
			u64 sizecode;
			bool explicit_size;
			if (!TryParseImm(args[i], expr, sizecode, explicit_size)) return false;
			if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"};   return false; }

			// write the value
			if (!TryAppendExpr(size, std::move(expr))) return false;
		}
	}

	return true;
}
bool AssembleArgs::TryProcessReserve(u64 size)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Reserve expected one arg"};   return false; }

	// parse the number to reserve
	u64 count, sizecode;
	bool floating, explicit_size;
	if (!TryParseInstantImm(args[0], count, floating, sizecode, explicit_size)) return false;
	if (floating) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Reserve count cannot be floating-point"};   return false; }
	if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"};   return false; }

	// reserve the space
	if (!TryReserve(count * size)) return false;

	return true;
}

bool AssembleArgs::TryProcessEQU()
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// make sure we have a label on this line
	if (label_def.empty()) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a label declaration to link to the value"};   return false; }

	// get the expression
	Expr expr;
	u64 sizecode;
	bool explicit_size;
	if (!TryParseImm(args[0], expr, sizecode, explicit_size)) return false;
	if (explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": A size directive in this context is not allowed"};   return false; }

	// inject the symbol
	file.Symbols.emplace(std::move(label_def), std::move(expr));

	return true;
}

bool AssembleArgs::TryProcessSegment()
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// get the segment we're going to
	std::string useg = ToUpper(std::move(args[0]));
	if (useg == ".TEXT") current_seg = AsmSegment::TEXT;
	else if (useg == ".RODATA") current_seg = AsmSegment::RODATA;
	else if (useg == ".DATA") current_seg = AsmSegment::DATA;
	else if (useg == ".BSS") current_seg = AsmSegment::BSS;
	else { res = {AssembleError::ArgError, "line " + tostr(line) + ": Unknown segment specified"}; return false; }

	// if this segment has already been done, fail
	if ((int)done_segs & (int)current_seg) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Attempt to redeclare segment {current_seg}"};   return false; }
	// add to list of completed segments
	done_segs = (AsmSegment)((int)done_segs | (int)current_seg);

	// we don't want to have cross-segment local symbols
	last_nonlocal_label.clear();

	return true;
}

bool AssembleArgs::TryProcessTernaryOp(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask)
{
	if (args.size() != 3) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 3 args"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	u64 dest, a_sizecode, imm_sz;
	bool dest_high, imm_sz_explicit;
	Expr imm;
	if (!TryParseCPURegister(args[0], dest, a_sizecode, dest_high)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register as first operand"};   return false; }
	if (!TryParseImm(args[2], imm, imm_sz, imm_sz_explicit)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected imm as third operand"};   return false; }

	if (imm_sz_explicit && imm_sz != a_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }
	if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

	// reg
	u64 reg, b_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[1], reg, b_sizecode, reg_high))
	{
		if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

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

		if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

		if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul) | 1)) return false;
		if (!TryAppendExpr(Size(a_sizecode), std::move(imm))) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	// imm
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register or memory value as second operand"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessBinaryOp(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask, int _force_b_imm_sizecode)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg, *
	u64 dest, a_sizecode;
	bool dest_high;
	if (TryParseCPURegister(args[0], dest, a_sizecode, dest_high))
	{
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

		// reg, reg
		u64 src, b_sizecode;
		bool src_high;
		if (TryParseCPURegister(args[1], src, b_sizecode, src_high))
		{
			if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

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

			if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

			if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2) | (dest_high ? 2 : 0ul))) return false;
			if (!TryAppendVal(1, (2 << 4))) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// reg, imm
		else
		{
			Expr imm;
			bool explicit_size;
			if (!TryParseImm(args[1], imm, b_sizecode, explicit_size)) return false;

			// fix up size codes
			if (_force_b_imm_sizecode == -1)
			{
				if (explicit_size) { if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; } }
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
			if ((Size(b_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

			if (a_explicit && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Argument size missmatch"};   return false; }

			if (!TryAppendVal(1, (b_sizecode << 2) | (src_high ? 1 : 0ul))) return false;
			if (!TryAppendVal(1, (3 << 4) | src)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// mem, mem
		else if (args[1][args[1].size() - 1] == ']') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Only one operand may be a memory value"};   return false; }
		// mem, imm
		else
		{
			Expr imm;
			bool b_explicit;
			if (!TryParseImm(args[1], imm, b_sizecode, b_explicit)) return false;

			// fix up the size codes
			if (_force_b_imm_sizecode == -1)
			{
				if (a_explicit && b_explicit) { if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; } }
				else if (b_explicit) a_sizecode = b_sizecode;
				else if (a_explicit) b_sizecode = a_sizecode;
				else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }
			}
			else b_sizecode = (u64)_force_b_imm_sizecode;

			if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

			if (!TryAppendVal(1, a_sizecode << 2)) return false;
			if (!TryAppendVal(1, 4 << 4)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
			if (!TryAppendExpr(Size(b_sizecode), std::move(imm))) return false;
		}
	}
	// imm, *
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected cpu register or memory value as first operand"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessUnaryOp(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg
	u64 reg, a_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, reg_high))
	{
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

		if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2) | (reg_high ? 2 : 0ul))) return false;
	}
	// mem
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 1)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	// imm
	else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessIMMRM(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask, int default_sizecode)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg
	u64 reg, a_sizecode;
	bool reg_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, reg_high))
	{
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

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
			if (default_sizecode == -1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }
			else a_sizecode = (u64)default_sizecode;
		}
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 3)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	// imm
	else
	{
		Expr imm;
		bool explicit_size;
		if (!TryParseImm(args[0], imm, a_sizecode, explicit_size)) return false;

		if (!explicit_size)
		{
			if (default_sizecode == -1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }
			else a_sizecode = (u64)default_sizecode;
		}
		if ((Size(a_sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 2)) return false;
		if (!TryAppendExpr(Size(a_sizecode), std::move(imm))) return false;
	}

	return true;
}
bool AssembleArgs::TryProcessRR_RM(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask)
{
	if (args.size() != 3) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 3 operands"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// reg, *, *
	u64 dest, sizecode;
	bool dest_high;
	if (TryParseCPURegister(args[0], dest, sizecode, dest_high))
	{
		// apply size mask
		if ((Size(sizecode) & sizemask) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size not supported"};   return false; }

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
				if (sizecode != src_1_sizecode || sizecode != src_2_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

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

				if (sizecode != src_1_sizecode || src_2_explicit_size && sizecode != src_2_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

				if (!TryAppendVal(1, (dest << 4) | (sizecode << 2) | (dest_high ? 2 : 0ul) | 1)) return false;
				if (!TryAppendVal(1, (src_1_high ? 128 : 0ul) | src_1)) return false;
				if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
			}
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Third operand must be a cpu register or memory value"};   return false; }
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand must be a cpu register"};   return false; }
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be a cpu register"};   return false; }

	return true;
}

bool AssembleArgs::TryProcessBinaryOp_NoBMem(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask, int _force_b_imm_sizecode)
{
	// b can't be memory
	if (args.size() > 1 && args[1][args[1].size() - 1] == ']') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand may not be a memory value"};   return false; }

	// otherwise refer to binary formatter
	return TryProcessBinaryOp(op, has_ext_op, ext_op, sizemask, _force_b_imm_sizecode);
}
bool AssembleArgs::TryProcessBinaryOp_R_RM(OPCode op, bool has_ext_op, u8 ext_op, u64 sizemask, int _force_b_imm_sizecode)
{
	// a must be register
	u64 reg, sz;
	bool high;
	if (args.size() > 0 && !TryParseCPURegister(args[0], reg, sz, high)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be a cpu register"};   return false; }
	// b must be register or memory
	if (args.size() > 1 && !TryParseCPURegister(args[1], reg, sz, high) && args[1][args[1].size() - 1] != ']') { res = {AssembleError::UsageError, "line " + tostr(line) + ": Second operand must be a cpu register or memory value"};   return false; }

	// otherwise refer to binary formatter
	return TryProcessBinaryOp(op, has_ext_op, ext_op, sizemask, _force_b_imm_sizecode);
}

bool AssembleArgs::TryProcessNoArgOp(OPCode op, bool has_ext_op, u8 ext_op)
{
	if (args.size() != 0) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	return true;
}

bool AssembleArgs::TryProcessXCHG(OPCode op)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;

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
			if (a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

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

			if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

			if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2) | (reg_high ? 2 : 0ul) | 1)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// reg, imm
		else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value as second operand"};   return false; }
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
			if (explicit_size && a_sizecode != b_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size missmatch"};   return false; }

			if (!TryAppendVal(1, (reg << 4) | (b_sizecode << 2) | (reg_high ? 2 : 0ul) | 1)) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// mem, mem
		else if (args[1][args[1].size() - 1] == ']') { res = {AssembleError::FormatError, "line " + tostr(line) + ": Only one operand may be a memory value"};   return false; }
		// mem, imm
		else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value as second operand"};   return false; }
	}
	// imm, *
	else { res = {AssembleError::FormatError, "line " + tostr(line) + ": Expected a cpu register or memory value as first operand"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessLEA(OPCode op)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	u64 dest, a_sizecode;
	bool dest_high;
	if (!TryParseCPURegister(args[0], dest, a_sizecode, dest_high)) return false;
	if (a_sizecode == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": 8-bit addressing is not supported"};   return false; }

	u64 a, b, b_sizecode;
	Expr ptr_base;
	bool explicit_size;
	if (!TryParseAddress(args[1], a, b, ptr_base, b_sizecode, explicit_size)) return false;

	if (!TryAppendByte((u8)op)) return false;
	if (!TryAppendVal(1, (dest << 4) | (a_sizecode << 2))) return false;
	if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;

	return true;
}
bool AssembleArgs::TryProcessPOP(OPCode op)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;

	// reg
	u64 reg, a_sizecode;
	bool a_high;
	if (TryParseCPURegister(args[0], reg, a_sizecode, a_high))
	{
		if ((Size(a_sizecode) & 14) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

		if (!TryAppendVal(1, (reg << 4) | (a_sizecode << 2))) return false;
	}
	// mem
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, a_sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		if ((Size(a_sizecode) & 14) == 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 1)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a cpu register or memory value"};   return false; }

	return true;
}

bool AssembleArgs::__TryProcessShift_mid()
{
	// reg/mem, reg
	u64 src, b_sizecode;
	bool b_high;
	if (TryParseCPURegister(args[1], src, b_sizecode, b_high))
	{
		if (src != 2 || b_sizecode != 0 || b_high) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Shifts using a register as count source must use CL"};   return false; }

		if (!TryAppendByte(0x80)) return false;
	}
	// reg/mem, imm
	else
	{
		Expr imm;
		bool explicit_size;
		if (!TryParseImm(args[1], imm, b_sizecode, explicit_size)) return false;

		// mask the shift count to 6 bits (we just need to make sure it can't set the CL flag)
		imm.Left = new Expr(std::move(imm));
		imm.Right = Expr::NewInt(0x3f);
		imm.OP = Expr::OPs::BitAnd;

		if (!TryAppendExpr(1, std::move(imm))) return false;
	}

	return true;
}
bool AssembleArgs::TryProcessShift(OPCode op)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;

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

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		// make sure we're using a normal word size
		if (a_sizecode > 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

		if (!TryAppendVal(1, (a_sizecode << 2) | 1)) return false;
		if (!__TryProcessShift_mid()) return false;
		if (!TryAppendAddress(1, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a cpu register or memory value as first operand"};   return false; }

	return true;
}

bool AssembleArgs::__TryProcessMOVxX_settings_byte(bool sign, u64 dest, u64 dest_sizecode, u64 src_sizecode)
{
	// 16, *
	if (dest_sizecode == 1)
	{
		// 16, 8
		if (src_sizecode == 0)
		{
			if (!TryAppendVal(1, (dest << 4) | (sign ? 1 : 0ul))) return false;
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size combination is not supported"};   return false; }
	}
	// 32/64, *
	else if (dest_sizecode == 2 || dest_sizecode == 3)
	{
		// 32/64, 8
		if (src_sizecode == 0)
		{
			if (!TryAppendVal(1, (dest << 4) | (dest_sizecode == 2 ? sign ? 4 : 2ul : sign ? 8 : 6ul))) return false;
		}
		// 32/64, 16
		else if (src_sizecode == 1)
		{
			if (!TryAppendVal(1, (dest << 4) | (dest_sizecode == 2 ? sign ? 5 : 3ul : sign ? 9 : 7ul))) return false;
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size combination is not supported"};   return false; }
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size combination is not supported"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessMOVxX(OPCode op, bool sign)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	u64 dest, dest_sizecode;
	bool __dest_high;
	if (!TryParseCPURegister(args[0], dest, dest_sizecode, __dest_high)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be a cpu register"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;

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

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		// write the settings byte
		if (!__TryProcessMOVxX_settings_byte(sign, dest, dest_sizecode, src_sizecode)) return false;

		// mark source as a memory value and append the address
		if (!TryAppendByte(0x80)) return false;
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a cpu register or memory value as second operand"};   return false; }

	return true;
}

bool AssembleArgs::TryProcessFPUBinaryOp(OPCode op, bool integral, bool pop)
{
	// write op code
	if (!TryAppendByte((u8)op)) return false;

	// make sure the programmer doesn't pull any funny business due to our arg-count-based approach
	if (integral && args.size() != 1) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Integral {op} requires 1 arg"};   return false; }
	if (pop && args.size() != 0 && args.size() != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Popping {op} requires 0 or 2 args"};   return false; }

	// handle arg count cases
	if (args.size() == 0)
	{
		// no args is st(1) <- f(st(1), st(0)), pop
		if (!pop) { res = {AssembleError::UsageError, "line " + tostr(line) + ": This form requires operands"};   return false; }

		if (!TryAppendByte(0x12)) return false;
	}
	else if (args.size() == 1)
	{
		u64 a, b, sizecode;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		// integral
		if (integral)
		{
			if (sizecode == 1) { if (!TryAppendByte(5)) return false; }
			else if (sizecode == 2) { if (!TryAppendByte(6)) return false; }
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }
		}
		// floatint-point
		else
		{
			if (sizecode == 2) { if (!TryAppendByte(3)) return false; }
			else if (sizecode == 3) { if (!TryAppendByte(4)) return false; }
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }
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
			if (!TryAppendVal(1, (a << 4) | (pop ? 2 : 1ul))) return false;
		}
		// if a is st(0)
		else if (a == 0)
		{
			if (pop) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected ST(0) as second operand"};   return false; }

			if (!TryAppendVal(1, b << 4)) return false;
		}
		// x87 requires one of them be st(0)
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": One operand must be ST(0)"};   return false; }
	}
	else { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Too many operands"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessFPURegisterOp(OPCode op, bool has_ext_op, u8 ext_op)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	u64 reg;
	if (!TryParseFPURegister(args[0], reg)) return false;

	// write op code
	if (!TryAppendByte((u8)op)) return false;
	if (has_ext_op) { if (!TryAppendByte(ext_op)) return false; }

	// write the register
	if (!TryAppendVal(1, reg)) return false;

	return true;
}

bool AssembleArgs::TryProcessFSTLD_WORD(OPCode op, u8 mode, u64 _sizecode)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// operand has to be mem
	u64 a, b, sizecode;
	Expr ptr_base;
	bool explicit_size;
	if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

	// must be the dictated size
	if (explicit_size && sizecode != _sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

	// write data
	if (!TryAppendByte((u8)op)) return false;
	if (!TryAppendByte(mode)) return false;
	if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;

	return true;
}

bool AssembleArgs::TryProcessFLD(OPCode op, bool integral)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// write op code
	if (!TryAppendByte((u8)op)) return false;

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

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		// handle integral cases
		if (integral)
		{
			if (sizecode != 1 && sizecode != 2 && sizecode != 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

			if (!TryAppendVal(1, sizecode + 2)) return false;
		}
		// otherwise floating-point
		else
		{
			if (sizecode != 2 && sizecode != 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

			if (!TryAppendVal(1, sizecode - 1)) return false;
		}

		// and write the address
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected an fpu register or a memory value"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessFST(OPCode op, bool integral, bool pop, bool trunc)
{
	if (args.size() != 1) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }

	// write the op code
	if (!TryAppendByte((u8)op)) return false;

	// if it's an fpu register
	u64 reg;
	if (TryParseFPURegister(args[0], reg))
	{
		// can't be an integral op
		if (integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a memory value"};   return false; }

		if (!TryAppendVal(1, (reg << 4) | (pop ? 1 : 0ul))) return false;
	}
	// if it's a memory destination
	else if (args[0][args[0].size() - 1] == ']')
	{
		u64 a, b, sizecode;
		Expr ptr_base;
		bool explicit_size;
		if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

		if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

		// if this is integral (i.e. truncation store)
		if (integral)
		{
			if (sizecode == 1) { if (!TryAppendVal(1, pop ? trunc ? 11 : 7ul : 6)) return false; }
			else if (sizecode == 2) { if (!TryAppendVal(1, pop ? trunc ? 12 : 9ul : 8)) return false; }
			else if (sizecode == 3)
			{
				// there isn't a non-popping 64-bit int store
				if (!pop) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

				if (!TryAppendVal(1, trunc ? 13 : 10ul)) return false;
			}
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }
		}
		// otherwise is floating-point
		else
		{
			if (sizecode == 2) { if (!TryAppendVal(1, pop ? 3 : 2ul)) return false; }
			else if (sizecode == 3) { if (!TryAppendVal(1, pop ? 5 : 4ul)) return false; }
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }
		}

		// and write the address
		if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected an fpu register or memory value"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessFCOM(OPCode op, bool integral, bool pop, bool pop2, bool eflags, bool unordered)
{
	// write the op code
	if (!TryAppendByte((u8)op)) return false;

	// handle arg count cases
	if (args.size() == 0)
	{
		if (integral) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }
		if (eflags) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

		// no args is same as using st(1) (plus additional case of double pop)
		if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (1 << 4) | (pop2 ? 2 : pop ? 1 : 0ul))) return false;
	}
	else if (args.size() == 1)
	{
		// double pop doesn't accept operands
		if (pop2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"};   return false; }
		if (eflags) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

		// register
		u64 reg;
		if (TryParseFPURegister(args[0], reg))
		{
			// integral forms only store to memory
			if (integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a memory value"};   return false; }

			if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (reg << 4) | (pop ? 1 : 0ul))) return false;
		}
		// memory
		else if (args[0][args[0].size() - 1] == ']')
		{
			u64 a, b, sizecode;
			Expr ptr_base;
			bool explicit_size;
			if (!TryParseAddress(args[0], a, b, ptr_base, sizecode, explicit_size)) return false;

			if (!explicit_size) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Could not deduce operand size"};   return false; }

			// handle size cases
			if (sizecode == 1)
			{
				// this mode only allows int
				if (!integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

				if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (pop ? 8 : 7ul))) return false;
			}
			else if (sizecode == 2)
			{
				if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (integral ? pop ? 10 : 9ul : pop ? 4 : 3ul))) return false;
			}
			else if (sizecode == 3)
			{
				// this mode only allows fp
				if (integral) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

				if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (pop ? 6 : 5ul))) return false;
			}
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Specified size is not supported"};   return false; }

			// and write the address
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected an fpu register or a memory value"};   return false; }
	}
	else if (args.size() == 2)
	{
		if (integral) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 1 operand"};   return false; }
		if (pop2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected no operands"};   return false; }

		u64 reg_a, reg_b;
		if (!TryParseFPURegister(args[0], reg_a) || !TryParseFPURegister(args[1], reg_b)) return false;

		// first arg must be st0
		if (reg_a != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be ST(0)"};   return false; }

		if (!TryAppendVal(1, (unordered ? 128 : 0ul) | (reg_b << 4) | (pop ? 12 : 11ul))) return false;
	}
	else { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Too many operands"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessFMOVcc(OPCode op, u64 condition)
{
	if (args.size() != 2) { res = {AssembleError::ArgCount, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	u64 reg;
	if (!TryParseFPURegister(args[0], reg) || reg != 0) { res = {AssembleError::UsageError, "line " + tostr(line) + ": First operand must be ST(0)"};   return false; }
	if (!TryParseFPURegister(args[1], reg)) return false;

	if (!TryAppendByte((u8)op)) return false;
	if (!TryAppendVal(1, (reg << 4) | condition)) return false;

	return true;
}

bool AssembleArgs::TryExtractVPUMask(std::string &arg, std::unique_ptr<Expr> &mask, bool &zmask)
{
	mask = nullptr; // no mask is denoted by null
	zmask = false; // by default, mask is not a zmask

				   // if it ends in z or Z, it's a zmask
	if (arg[arg.size() - 1] == 'z' || arg[arg.size() - 1] == 'Z')
	{
		// remove the z
		arg.pop_back();
		arg = TrimEnd(std::move(arg));

		// ensure validity - must be preceded by }
		if (arg.size() == 0 || arg[arg.size() - 1] != '}') { res = {AssembleError::FormatError, "line " + tostr(line) + ": Zmask declarator encountered without a corresponding mask"};   return false; }

		// mark as being a zmask
		zmask = true;
	}

	// if it ends in }, there's a white mask
	if (arg[arg.size() - 1] == '}')
	{
		// find the opening bracket
		std::size_t pos = arg.find('{');
		if (pos == std::string::npos) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Ill-formed vpu whitemask encountered"};   return false; }
		if (pos == 0) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Lone vpu whitemask encountered"}; ;  return false; }

		// extract the whitemask internals
		std::string innards = arg.substr(pos + 1, arg.size() - 2 - pos);
		// pop the whitemask off the arg
		arg = TrimEnd(arg.substr(0, pos));

		// parse the mask expression
		u64 sizecode;
		bool explicit_size;
		Expr _mask;
		if (!TryParseImm(innards, _mask, sizecode, explicit_size)) return false;

		// we now need to return it as a dynamic object - handle with care
		mask.reset(new Expr(std::move(_mask)));
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

bool AssembleArgs::TryProcessVPUMove(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar)
{
	if (args.size() != 2) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 2 operands"};   return false; }

	// write the op code
	if (!TryAppendByte((u8)op)) return false;

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;
	// if it had an explicit mask and we were told not to allow that, it's an error
	if (mask != nullptr && !maskable) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Instruction does not support masking"};   return false; }

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
			if (dest_sizecode != src_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

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

			if (src_explicit && src_sizecode != (scalar ? elem_sizecode : dest_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

			if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
			if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
			if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
			if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
		}
		// vreg, imm
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register or memory value as second operand"};   return false; }
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
			if (dest_explicit && dest_sizecode != (scalar ? elem_sizecode : src_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

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
		else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register as second operand"};   return false; }
	}
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register or a memory value as first operand"};   return false; }

	return true;
}
bool AssembleArgs::TryProcessVPUBinary(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar)
{
	if (args.size() != 2 && args.size() != 3) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected 2 or 3 operands"};   return false; }

	// write the op code
	if (!TryAppendByte((u8)op)) return false;

	// extract the mask
	std::unique_ptr<Expr> mask;
	bool zmask;
	if (!TryExtractVPUMask(args[0], mask, zmask)) return false;
	// if it had an explicit mask and we were told not to allow that, it's an error
	if (mask != nullptr && !maskable) { res = {AssembleError::FormatError, "line " + tostr(line) + ": Instruction does not support masking"};   return false; }

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
				if (dest_sizecode != src_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

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

				if (src_explicit && src_sizecode != (scalar ? elem_sizecode : dest_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

				if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
				if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
				if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
				if (!TryAppendVal(1, dest)) return false;
				if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
			}
			// vreg, imm
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register or memory value as second operand"};   return false; }
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
					if (dest_sizecode != src1_sizecode || src1_sizecode != src2_sizecode) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

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

					if (dest_sizecode != src1_sizecode || src2_explicit && src2_sizecode != (scalar ? elem_sizecode : dest_sizecode)) { res = {AssembleError::UsageError, "line " + tostr(line) + ": Operand size mismatch"};   return false; }

					if (!TryAppendVal(1, (dest << 3) | (aligned ? 4 : 0ul) | (dest_sizecode - 4))) return false;
					if (!TryAppendVal(1, (mask_present ? 128 : 0ul) | (zmask ? 64 : 0ul) | (scalar ? 32 : 0ul) | (elem_sizecode << 2) | 1)) return false;
					if (mask_present && !TryAppendExpr(BitsToBytes(elem_count), std::move(*mask))) return false;
					if (!TryAppendVal(1, src1)) return false;
					if (!TryAppendAddress(a, b, std::move(ptr_base))) return false;
				}
				// vreg, imm
				else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register or memory value as third operand"};   return false; }
			}
			// vreg, mem/imm, *
			else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected vpu register as second operand"};   return false; }
		}
	}
	// mem/imm, *
	else { res = {AssembleError::UsageError, "line " + tostr(line) + ": Expected a vpu register as first operand"};   return false; }

	return true;
}
}
