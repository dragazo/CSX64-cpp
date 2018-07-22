#include <iostream>
#include <string>
#include <cmath>
#include <utility>
#include <cstdlib>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <exception>
#include <tuple>
#include <queue>
#include <algorithm>
#include <memory>

#include "Assembly.h"
#include "Utility.h"
#include "Expr.h"
#include "OFile.h"
#include "AsmRouting.h"
#include "AsmTables.h"

namespace CSX64
{
	// -- implementation constants -- //

	

	// -- parsing utilities -- //

	bool TryExtractStringChars(const std::string &token, std::string &chars, std::string &err)
	{
		std::string b; // string builder (so token and chars can safely refer to the same object)

		// make sure it starts with a quote and is terminated
		if (token.empty() || token[0] != '"' && token[0] != '\'' && token[0] != '`' || token[0] != token[token.size() - 1]) { err = "Ill-formed string: " + token; return false; }

		// read all the characters inside
		for (int i = 1; i < token.size() - 1; ++i)
		{
			// if this is a `backquote` literal, it allows \escapes
			if (token[0] == '`' && token[i] == '\\')
			{
				// bump up i and make sure it's still good
				if (++i >= token.size() - 1) { err = "Ill-formed string (ends with beginning of an escape sequence): " + token; return false; }

				int temp, temp2;
				switch (token[i])
				{
				case '\'': temp = '\''; break;
				case '"': temp = '"'; break;
				case '`': temp = '`'; break;
				case '\\': temp = '\\'; break;
				case '?': temp = '?'; break;
				case 'a': temp = '\a'; break;
				case 'b': temp = '\b'; break;
				case 't': temp = '\t'; break;
				case 'n': temp = '\n'; break;
				case 'v': temp = '\v'; break;
				case 'f': temp = '\f'; break;
				case 'r': temp = '\r'; break;
				case 'e': temp = 27; break;

				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					temp = 0;
					// read the octal value into temp (up to 3 octal digits)
					for (int oct_count = 0; oct_count < 3 && token[i] >= '0' && token[i] <= '7'; ++i, ++oct_count)
						temp = (temp << 3) | (token[i] - '0');
					--i; // undo the last i increment (otherwise outer loop will skip a char)
					break;

				case 'x':
					// bump up i and make sure it's a hex digit
					if (!GetHexValue(token[++i], temp)) { err = "Ill-formed string (invalid hexadecimal escape): " + token; return false; }
					// if the next char is also a hex digit
					if (GetHexValue(token[i + 1], temp2))
					{
						// read it into the escape value as well
						++i;
						temp = (temp << 4) | temp2;
					}
					break;

				case 'u': case 'U': err = "Unicode character escapes are not yet supported: " + token; return false;

				default: err = "Ill-formed string (escape sequence not recognized): " + token; return false;
				}

				// append the character
				b.push_back(temp & 0xff);
			}
			// otherwise just read the character verbatim
			else b.push_back(token[i]);
		}

		// return extracted chars
		chars = std::move(b);
		return true;
	}

	u64 SmallestUnsignedSizeCode(u64 val)
	{
		// filter through till we get a size that will contain it
		if (val <= 0xff) return 0;
		else if (val <= 0xffff) return 1;
		else if (val <= 0xffffffff) return 2;
		else return 3;
	}

	static void RenameSymbol(ObjectFile &file, const std::string &from, const std::string &to)
	{
		// make sure "to" doesn't already exist
		if (ContainsKey(file.Symbols, to) || Contains(file.ExternalSymbols, to))
			throw std::invalid_argument("Attempt to rename symbol \"" + from + "\" to \"" + to + "\" (already exists)");
		
		// if it's a symbol defined in this file
		const Expr *expr;
		if (TryGetValue(file.Symbols, from, expr))
		{
			// make sure it hasn't already been evaluated (because it may have already been linked to other expressions)
			if (expr->IsEvaluated()) throw std::runtime_error("Attempt to rename symbol \"" + from + "\" to \"" + to + "\" (already evaluated)");

			// rename the symbol
			file.Symbols.emplace(to, std::move(file.Symbols.at(from)));
			file.Symbols.erase(from);

			// find and replace in global table (may not be global - that's ok)
			if (Contains(file.GlobalSymbols, from))
			{
				file.GlobalSymbols.emplace(to);
				file.GlobalSymbols.erase(from);
			}
		}
		// if it's a symbol defined externally
		else if (Contains(file.ExternalSymbols, from))
		{
			// replace
			file.ExternalSymbols.emplace(to);
			file.ExternalSymbols.erase(from);
		}
		// otherwise we don't know what it is
		else throw std::runtime_error("Attempt to rename symbol \"" + from + "\" to \"" + to + "\" (does not exist)");

		// -- now the easy part -- //

		// find and replace in symbol table expressions
		for(auto &entry : file.Symbols) entry.second.Resolve(from, to);

		// find and replace in hole expressions
		for(auto &entry : file.TextHoles) entry.Expr.Resolve(from, to);
		for (auto &entry : file.RodataHoles) entry.Expr.Resolve(from, to);
		for (auto &entry : file.DataHoles) entry.Expr.Resolve(from, to);
	}
	
	// helper for imm parser
	bool TryGetOp(const std::string &token, int pos, Expr::OPs &op, int &oplen)
	{
		// default to invalid op
		op = Expr::OPs::None;
		oplen = 0;

		// try to take as many characters as possible (greedy)
		if (pos + 2 <= token.size())
		{
			oplen = 2; // record oplen

					   // do ops where both chars are the same
			if (token[pos] == token[pos + 1])
			{
				switch (token[pos])
				{
				case '<': op = Expr::OPs::SL; return true;
				case '>': op = Expr::OPs::SR; return true;
				case '=': op = Expr::OPs::Eq; return true;
				case '&': op = Expr::OPs::LogAnd; return true;
				case '|': op = Expr::OPs::LogOr; return true;
				case '?': op = Expr::OPs::NullCoalesce; return true;
				}
			}
			// otherwise second must be =
			else if (token[pos + 1] == '=')
			{
				switch (token[pos])
				{
				case '<': op = Expr::OPs::LessE; return true;
				case '>': op = Expr::OPs::GreatE; return true;
				case '!': op = Expr::OPs::Neq; return true;
				}
			}
		}
		if (pos + 1 <= token.size())
		{
			oplen = 1; // record oplen
			switch (token[pos])
			{
			case '*': op = Expr::OPs::Mul; return true;
			case '/': op = Expr::OPs::Div; return true;
			case '%': op = Expr::OPs::Mod; return true;

			case '+': op = Expr::OPs::Add; return true;
			case '-': op = Expr::OPs::Sub; return true;

			case '<': op = Expr::OPs::Less; return true;
			case '>': op = Expr::OPs::Great; return true;

			case '&': op = Expr::OPs::BitAnd; return true;
			case '^': op = Expr::OPs::BitXor; return true;
			case '|': op = Expr::OPs::BitOr; return true;

			case '?': op = Expr::OPs::Condition; return true;
			case ':': op = Expr::OPs::Pair; return true;
			}
		}

		// if nothing found, fail
		return false;
	}

	// -- predefined symbols -- //

	// Stores all the predefined symbols that are not defined by the assembler itself
	std::unordered_map<std::string, Expr> PredefinedSymbols;

	void DefineSymbol(std::string key, std::string value)
	{
		Expr expr;
		expr.Token(std::move(value));
		PredefinedSymbols.emplace(std::move(key), std::move(expr));
	}
	void DefineSymbol(std::string key, u64 value)
	{
		Expr expr;
		expr.IntResult(value);
		PredefinedSymbols.emplace(std::move(key), std::move(expr));
	}
	void DefineSymbol(std::string key, double value)
	{
		Expr expr;
		expr.FloatResult(value);
		PredefinedSymbols.emplace(std::move(key), std::move(expr));
	}

	// !! PUT SYSCALL DEFINES SOMEWHERE !!

	/*
	static Assembly()
	{
		// create definitions for all the syscall codes
		foreach(SyscallCode item in Enum.GetValues(typeof(SyscallCode)))
			DefineSymbol($"sys_{item.ToString().ToLower()}", (u64)item);

		// create definitions for all the error codes
		foreach(ErrorCode item in Enum.GetValues(typeof(ErrorCode)))
			DefineSymbol($"err_{item.ToString().ToLower()}", (u64)item);
	}
	*/
	// -- patching -- //

	/// <summary>
	/// Attempts to patch the hole. returns PatchError::None on success
	/// </summary>
	/// <param name="res">data array to patch</param>
	/// <param name="symbols">the symbols used for lookup</param>
	/// <param name="data">the hole's data</param>
	/// <param name="err">the resulting error on failure</param>
	PatchError TryPatchHole(std::vector<u8> &res, std::unordered_map<std::string, Expr> &symbols, HoleData &data, std::string &err)
	{
		// if we can fill it immediately, do so
		u64 val;
		bool floating;
		if (data.Expr.Evaluate(symbols, val, floating, err))
		{
			// if it's floating-point
			if (floating)
			{
				// only 64-bit and 32-bit are supported
				switch (data.Size)
				{
				case 8: if (!Write(res, data.Address, 8, val)) { err = "line " + tostr(data.Line) + ": Error writing value"; return PatchError::Error; } break;
				case 4: if (!Write(res, data.Address, 4, FloatAsUInt64((float)AsDouble(val)))) { err = "line " + tostr(data.Line) + ": Error writing value"; return PatchError::Error; } break;

				default: err = "line " + tostr(data.Line) + ": Attempt to use unsupported floating-point format"; return PatchError::Error;
				}
			}
			// otherwise it's integral
			else if (!Write(res, data.Address, data.Size, val)) { err = "line " + tostr(data.Line) + ": Error writing value"; return PatchError::Error; }

			// successfully patched
			return PatchError::None;
		}
		// otherwise it's unevaluated
		else { err = "line " + tostr(data.Line) + ": Failed to evaluate expression\n-> " + err; return PatchError::Unevaluated; }
	}

	// -- let the fun begin -- //

	// helper function for assembler
	bool _ElimHoles(std::unordered_map<std::string, Expr> &symbols, std::vector<HoleData> &holes, std::vector<u8> &seg, std::string &err)
	{
		using std::swap;

		for (int i = holes.size() - 1; i >= 0; --i)
		{
			switch (TryPatchHole(seg, symbols, holes[i], err))
			{
			case PatchError::None: if (i != holes.size() - 1) swap(holes[i], holes.back()); holes.pop_back(); break; // remove the hole if we solved it
			case PatchError::Unevaluated: break;
			case PatchError::Error: err = err; return false;

			default: throw std::runtime_error("Unknown patch error encountered");
			}
		}
	}

	AssembleResult Assemble(const std::string &code, ObjectFile &file)
	{
		AssembleArgs args;

		/*
		// create the table of predefined symbols
		args.file.Symbols = new Dictionary<string, Expr>(PredefinedSymbols)
		{
		["__version__"] = new Expr(){IntResult = Computer.Version},

		["__pinf__"] = new Expr(){FloatResult = double.PositiveInfinity},
		["__ninf__"] = new Expr(){FloatResult = double.NegativeInfinity},
		["__nan__"] = new Expr(){FloatResult = double.NaN},

		["__fmax__"] = new Expr(){FloatResult = double.MaxValue},
		["__fmin__"] = new Expr(){FloatResult = double.MinValue},
		["__fepsilon__"] = new Expr(){FloatResult = double.Epsilon},

		["__pi__"] = new Expr(){FloatResult = Math.PI},
		["__e__"] = new Expr(){FloatResult = Math.E}
		};*/

		int pos = 0, end = 0; // position in code

		// potential parsing args for an instruction
		u64 a = 0, b = 0;
		bool floating, btemp;

		std::string err; // error location for evaluation
		
		const asm_router *router; // router temporary

		if (code.empty()) return AssembleResult{AssembleError::EmptyFile, "The file was empty"};

		while (pos < code.size())
		{
			// update current line pos
			switch (args.current_seg)
			{
			case AsmSegment::TEXT: args.line_pos_in_seg = args.file.Text.size(); break;
			case AsmSegment::RODATA: args.line_pos_in_seg = args.file.Rodata.size(); break;
			case AsmSegment::DATA: args.line_pos_in_seg = args.file.Data.size(); break;
			case AsmSegment::BSS: args.line_pos_in_seg = args.file.BssLen; break;

				// default does nothing - it is ill-formed to make an address outside of any segment
			}

			// find the next separator
			for (end = pos; end < code.size() && code[end] != '\n' && code[end] != CommentChar; ++end);

			// advance line counter
			++args.line;
			// split the line
			if (!args.SplitLine(code.substr(pos, end - pos))) return {AssembleError::FormatError, "line " + tostr(args.line) + ": Failed to parse line\n-> " + args.res.ErrorMsg};
			// if the separator was a comment character, consume the rest of the line as well as no-op
			if (end < code.size() && code[end] == CommentChar)
				for (; end < code.size() && code[end] != '\n'; ++end);

			// process marked label
			if (!args.TryProcessLabel()) return args.res;

			// empty lines are ignored
			if (!args.op.empty())
			{
				// try to get the router
				if (!TryGetValue(asm_routing_table, ToUpper(args.op), router)) return {AssembleError::UnknownOp, "line " + tostr(args.line) + ": Unknown instruction"};

				// perform the assembly action
				if (!(*router)(args)) return args.res;
			}

			// advance to after the new line
			pos = end + 1;
		}

		// -- minimize symbols and holes -- //

		// link each symbol to internal symbols (minimizes file size)
		for(auto &entry : args.file.Symbols) entry.second.Evaluate(args.file.Symbols, a, floating, err);

		// eliminate as many holes as possible
		if (!_ElimHoles(args.file.Symbols, args.file.TextHoles, args.file.Text, err)) return {AssembleError::ArgError, err};
		if (!_ElimHoles(args.file.Symbols, args.file.RodataHoles, args.file.Rodata, err)) return {AssembleError::ArgError, err};
		if (!_ElimHoles(args.file.Symbols, args.file.DataHoles, args.file.Data, err)) return {AssembleError::ArgError, err};

		// -- eliminate as many unnecessary symbols as we can -- //

		std::vector<const std::string*> elim_symbols;   // symbol names to be eliminated
		std::vector<const std::string*> rename_symbols; // symbol names that we can rename to be shorter

		// for each symbol
		for(auto &entry : args.file.Symbols)
		{
			// if this symbol is non-global
			if (!Contains(args.file.GlobalSymbols, entry.first))
			{
				// if this symbol has already been evaluated
				if (entry.second.IsEvaluated())
				{
					// we can eliminate it (because it's already been linked internally and won't be needed externally)
					elim_symbols.push_back(&entry.first);
				}
				// otherwise we can rename it to something shorter (because it's still needed internally, but not needed externally)
				else rename_symbols.push_back(&entry.first);
			}
		}
		// remove all the symbols we can eliminate
		for(const std::string *elim : elim_symbols) args.file.Symbols.erase(*elim);

		// -- finalize -- //

		// verify integrity of file
		if (!args.VerifyIntegrity()) return args.res;

		// rename all the symbols we can shorten (done after verify to ensure there's no verify error messages with the renamed symbols)
		for (int i = 0; i < rename_symbols.size(); ++i) RenameSymbol(args.file, *rename_symbols[i], "^" + tohex(i));

		// validate result
		file = std::move(args.file);
		file._Clean = true;

		// return no error
		return {AssembleError::None, ""};
	}
	LinkResult Link(std::vector<u8> &exe, std::vector<ObjectFile> &objs, std::string entry_point)
	{
		// parsing locations for evaluation
		u64 _res;
		bool _floating;
		std::string _err;

		// -- ensure args are good -- //

		// ensure entry point is legal
		if (!AssembleArgs::IsValidName(entry_point, _err)) return LinkResult{LinkError::FormatError, "Entry point \"" + entry_point + "\" is not a legal symbol name"};

		// ensure we got at least 1 object file
		if (objs.empty()) return {LinkError::EmptyResult, "Got no object files"};

		// make sure all object files are starting out clean
		for (std::size_t i = 0; i < objs.size(); ++i)
			if (!objs[i].Clean()) throw std::invalid_argument{"Attempt to use dirty object file"};

		// -- validate _start file -- //

		// _start file must declare an external named "_start"
		if (!Contains(objs[0].ExternalSymbols, "_start")) return LinkResult{LinkError::FormatError, "_start file must declare an external named \"_start\""};

		// rename "_start" symbol in _start file to whatever the entry point is (makes _start dirty)
		try { objs[0].MakeDirty(); RenameSymbol(objs[0], "_start", entry_point); }
		catch (...) { return {LinkError::FormatError, "an error occured while renaming \"_start\" in the _start file"}; }

		// -- define things -- //

		// create segments (we don't know how large the resulting file will be, so it needs to be expandable)
		std::vector<u8> text;
		std::vector<u8> rodata;
		std::vector<u8> data;
		u64 bsslen = 0;

		// segment alignments
		u64 textalign = 1;
		u64 rodataalign = 1;
		u64 dataalign = 1;
		u64 bssalign = 1;

		// a table for relating global symbols to their object file
		std::unordered_map<std::string, ObjectFile*> global_to_obj;

		// the queue of object files that need to be added to the executable
		std::list<ObjectFile*> include_queue;
		// a table for relating included object files to their beginning positions in the resulting binary (text, rodata, data, bss) tuples
		std::unordered_map<ObjectFile*, std::tuple<u64, u64, u64, u64>> included;

		// -- populate things -- //

		// populate global_to_obj with ALL global symbols
		for (ObjectFile &obj : objs)
		{
			const Expr *value;

			for (const std::string &global : obj.GlobalSymbols)
			{
				// make sure source actually defined this symbol (just in case of corrupted object file)
				if (!TryGetValue(obj.Symbols, global, value)) return LinkResult{LinkError::MissingSymbol, "Global symbol \"" + global + "\" was not defined"};
				// make sure it wasn't already defined
				if (ContainsKey(global_to_obj, global)) return LinkResult{LinkError::SymbolRedefinition, "Global symbol \"" + global + "\" was defined by multiple sources"};

				// add to the table
				global_to_obj.emplace(global, &obj);
			}
		}

		// -- verify things -- //

		// make sure no one defined over reserved symbol names
		for (ObjectFile &obj : objs)
		{
			// only the verify ignores are a problem (because we'll be defining those)
			for (const std::string &reserved : VerifyLegalExpressionIgnores)
				if (ContainsKey(obj.Symbols, reserved)) return LinkResult{LinkError::SymbolRedefinition, "Object file defined symbol with name \"" + reserved + "\" (reserved)"};
		}

		// start the merge process with the _start file
		include_queue.push_back(&objs[0]);

		// -- merge things -- //

		// while there are still things in queue
		while (include_queue.size() > 0)
		{
			// get the object file we need to incorporate
			ObjectFile *obj = include_queue.front();
			include_queue.pop_front();
			// all included files are dirty
			obj->MakeDirty();

			// account for alignment requirements
			Align(text, obj->TextAlign);
			Align(rodata, obj->RodataAlign);
			Align(data, obj->DataAlign);
			bsslen = Align(bsslen, obj->BSSAlign);

			// update segment alignments
			textalign = std::max<u64>(textalign, obj->TextAlign);
			rodataalign = std::max<u64>(rodataalign, obj->RodataAlign);
			dataalign = std::max<u64>(dataalign, obj->DataAlign);
			bssalign = std::max<u64>(bssalign, obj->BSSAlign);

			// add it to the set of included files
			included.emplace(obj, std::tuple<u64, u64, u64, u64>(text.size(), rodata.size(), data.size(), bsslen));

			// offset holes to be relative to the start of their total segment (not relative to resulting file)
			for (HoleData &hole : obj->TextHoles) hole.Address += text.size();
			for (HoleData &hole : obj->RodataHoles) hole.Address += rodata.size();
			for (HoleData &hole : obj->DataHoles) hole.Address += data.size();

			// reserve space for segments
			text.resize(text.size() + obj->Text.size());
			rodata.resize(rodata.size() + obj->Rodata.size());
			data.resize(data.size() + obj->Data.size());

			// append segments
			std::memcpy(&text[text.size() - obj->Text.size()], &obj->Text[0], obj->Text.size());
			std::memcpy(&text[rodata.size() - obj->Rodata.size()], &obj->Rodata[0], obj->Rodata.size());
			std::memcpy(&text[data.size() - obj->Data.size()], &obj->Data[0], obj->Data.size());
			bsslen += obj->BssLen;
			
			// for each external symbol
			for (const std::string &external : obj->ExternalSymbols)
			{
				ObjectFile **global_source;

				// if this is a global symbol somewhere
				if (TryGetValue(global_to_obj, external, global_source))
				{
					// if the source isn't already included and it isn't already in queue to be included
					if (!ContainsKey(included, *global_source) && !Contains(include_queue, *global_source))
					{
						// add it to the queue
						include_queue.push_back(*global_source);
					}
				}
				// otherwise it wasn't defined
				else return LinkResult{LinkError::MissingSymbol, "No global symbol found to match external symbol \"" + external + "\""};
			}
		}

		// account for segment alignments
		Pad(text, AlignOffset(text.size(), rodataalign));
		Pad(rodata, AlignOffset(text.size() + rodata.size(), dataalign));
		Pad(data, AlignOffset(text.size() + rodata.size() + data.size(), bssalign));
		bsslen += AlignOffset(text.size() + rodata.size() + data.size() + bsslen, 2); // the whole executable is 16-bit aligned (for stack)

		// now that we're done merging we need to define segment offsets in the result
		for (auto &entry : included)
		{
			// alias the object file
			ObjectFile &obj = *entry.first;

			// define the segment origins
			obj.Symbols.emplace(SegOrigins.at(AsmSegment::TEXT), Expr::CreateInt(0));
			obj.Symbols.emplace(SegOrigins.at(AsmSegment::RODATA), Expr::CreateInt(text.size()));
			obj.Symbols.emplace(SegOrigins.at(AsmSegment::DATA), Expr::CreateInt(text.size() + rodata.size()));
			obj.Symbols.emplace(SegOrigins.at(AsmSegment::BSS), Expr::CreateInt(text.size() + rodata.size() + data.size()));

			// and file-scope segment offsets
			obj.Symbols.emplace(SegOffsets.at(AsmSegment::TEXT), Expr::CreateInt(std::get<0>(entry.second)));
			obj.Symbols.emplace(SegOffsets.at(AsmSegment::RODATA), Expr::CreateInt(text.size() + std::get<1>(entry.second)));
			obj.Symbols.emplace(SegOffsets.at(AsmSegment::DATA), Expr::CreateInt(text.size() + rodata.size() + std::get<2>(entry.second)));
			obj.Symbols.emplace(SegOffsets.at(AsmSegment::BSS), Expr::CreateInt(text.size() + rodata.size() + data.size() + std::get<3>(entry.second)));

			// and everything else
			obj.Symbols.emplace("__heap__", Expr::CreateInt(text.size() + rodata.size() + data.size() + bsslen));

			// for each global symbol
			for (const std::string &global : obj.GlobalSymbols)
			{
				// if it can't be evaluated internally, it's an error (i.e. cannot define a global in terms of another file's globals)
				if (!obj.Symbols[global].Evaluate(obj.Symbols, _res, _floating, _err))
					return LinkResult{LinkError::MissingSymbol, "Global symbol \"" + global + "\" could not be evaluated internally"};
			}

			// for each external symbol
			for (const std::string &external : obj.ExternalSymbols)
			{
				// add externals to local scope //

				// if obj already has a symbol of the same name
				if (ContainsKey(obj.Symbols, external)) return {LinkError::SymbolRedefinition, "Object file defined external symbol \"" + external + "\""};
				// otherwise define it as a local in obj
				else obj.Symbols.emplace(external, global_to_obj[external]->Symbols[external]);
			}
		}

		// -- patch things -- //

		// for each object file
		for (auto &entry : included)
		{
			// alias object file
			ObjectFile &obj = *entry.first;

			// patch all the holes
			for (HoleData &hole : obj.TextHoles)
			{
				switch (TryPatchHole(text, obj.Symbols, hole, _err))
				{
				case PatchError::None: break;
				case PatchError::Unevaluated: return {LinkError::MissingSymbol, _err};
				case PatchError::Error: return {LinkError::FormatError, _err};

				default: throw std::runtime_error("Unknown patch error encountered");
				}
			}
			for (HoleData &hole : obj.RodataHoles)
			{
				switch (TryPatchHole(rodata, obj.Symbols, hole, _err))
				{
				case PatchError::None: break;
				case PatchError::Unevaluated: return {LinkError::MissingSymbol, _err};
				case PatchError::Error: return {LinkError::FormatError, _err};

				default: throw std::runtime_error("Unknown patch error encountered");
				}
			}
			for (HoleData &hole : obj.DataHoles)
			{
				switch (TryPatchHole(data, obj.Symbols, hole, _err))
				{
				case PatchError::None: break;
				case PatchError::Unevaluated: return {LinkError::MissingSymbol, _err};
				case PatchError::Error: return {LinkError::FormatError, _err};

				default: throw std::runtime_error("Unknown patch error encountered");
				}
			}
		}

		// -- finalize things -- //

		// allocate executable space (header + text + data)
		exe.clear();
		exe.resize(32 + text.size() + rodata.size() + data.size());

		// write header (length of each segment)
		Write(exe, 0, 8, text.size());
		Write(exe, 8, 8, rodata.size());
		Write(exe, 16, 8, data.size());
		Write(exe, 24, 8, bsslen);

		// copy text and data
		std::memcpy(&exe[32], &text[0], text.size());
		std::memcpy(&exe[32 + text.size()], &rodata[0], rodata.size());
		std::memcpy(&exe[32 + text.size() + rodata.size()], &data[0], data.size());

		// linked successfully
		return {LinkError::None, ""};
	}
}
