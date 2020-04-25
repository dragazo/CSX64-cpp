#include <iostream>
#include <string>
#include <cmath>
#include <utility>
#include <cstdlib>
#include <cstring>
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
#include <limits>
#include <memory>
#include <fstream>

#include "../include/Assembly.h"
#include "../include/Utility.h"
#include "../include/Expr.h"
#include "../include/AsmTables.h"
#include "../include/Executable.h"
#include "../include/csx_exceptions.h"

using namespace CSX64::detail;

namespace CSX64
{
	// -- hole data -- //

	std::ostream &HoleData::WriteTo(std::ostream &writer, const HoleData &hole)
	{
		write<u64>(writer, hole.address);
		write<u8>(writer, hole.size);

		write<u32>(writer, (u32)hole.line);
		Expr::WriteTo(writer, hole.expr);

		return writer;
	}
	std::istream &HoleData::ReadFrom(std::istream &reader, HoleData &hole)
	{
		read<u64>(reader, hole.address);
		read<u8>(reader, hole.size);

		{
			u32 t;
			read<u32>(reader, t);
			hole.line = (std::size_t)t;
		}
		Expr::ReadFrom(reader, hole.expr);

		return reader;
	}

	// -- object file -- //

	void ObjectFile::clear() noexcept
	{
		_Clean = false;

		GlobalSymbols.clear();
		ExternalSymbols.clear();

		Symbols.clear();

		TextAlign = 1;
		RodataAlign = 1;
		DataAlign = 1;
		BSSAlign = 1;

		TextHoles.clear();
		RodataHoles.clear();
		DataHoles.clear();

		Text.clear();
		Rodata.clear();
		Data.clear();
		BssLen = 0;

		Literals.clear();
	}

	static const u8 obj_header[] = { 'C', 'S', 'X', '6', '4', 'o', 'b', 'j' };

	void ObjectFile::save(const std::string &path) const
	{
		// ensure the object is clean
		if (!is_clean()) throw DirtyError("Attempt to save dirty object file");

		std::ofstream file(path, std::ios::binary);
		if (!file) throw FileOpenError("Failed to open file for saving object file");

		// -- write obj_header and CSX64 version number -- //

		write_bin(file, obj_header, sizeof(obj_header));
		write<u64>(file, Version);

		// -- write globals -- //

		write<u64>(file, (u64)GlobalSymbols.size());
		for (const std::string &symbol : GlobalSymbols)
			write_str(file, symbol);
		
		// -- write externals -- //

		write<u64>(file, (u64)ExternalSymbols.size());
		for (const std::string &symbol : ExternalSymbols)
			write_str(file, symbol);

		// -- write symbols -- //

		write<u64>(file, (u64)Symbols.size());
		for(const auto &entry : Symbols)
		{
			write_str(file, entry.first);
			Expr::WriteTo(file, entry.second);
		}
		
		// -- write alignments -- //

		write<u32>(file, TextAlign);
		write<u32>(file, RodataAlign);
		write<u32>(file, DataAlign);
		write<u32>(file, BSSAlign);

		// -- write segment holes -- //

		write<u64>(file, (u64)TextHoles.size());
		for (const HoleData &hole : TextHoles) HoleData::WriteTo(file, hole);

		write<u64>(file, (u64)RodataHoles.size());
		for (const HoleData &hole : RodataHoles) HoleData::WriteTo(file, hole);

		write<u64>(file, (u64)DataHoles.size());
		for (const HoleData &hole : DataHoles) HoleData::WriteTo(file, hole);
		
		// -- write segments -- //

		write<u64>(file, (u64)Text.size());
		write_bin(file, Text.data(), Text.size());

		write<u64>(file, (u64)Rodata.size());
		write_bin(file, Rodata.data(), Rodata.size());

		write<u64>(file, (u64)Data.size());
		write_bin(file, Data.data(), Data.size());
		
		write<u64>(file, (u64)BssLen);

		// -- write binary literals -- //

		Literals.write_to(file);

		// -- validation -- //

		// make sure the writes succeeded
		if (!file) throw IOError("Failed to write object file to file");
	}
	void ObjectFile::load(const std::string &path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file) throw FileOpenError("Failed to open file for loading object file");

		// mark as initially dirty
		_Clean = false;

		u64 val;
		std::string str;
		Expr expr;
		HoleData hole;
		u8 header_temp[sizeof(obj_header)];

		// -- file validation -- //

		// read obj_header and make sure it matches - match failure is type error, not format error.
		if (!read_bin(file, header_temp, sizeof(obj_header))) goto err;
		if (std::memcmp(header_temp, obj_header, sizeof(obj_header)))
		{
			throw TypeError("File was not a CSX64 executable");
		}

		// read the version number and make sure it matches - match failure is a version error, not a format error
		if (!read<u64>(file, val)) goto err;
		if (val != Version)
		{
			throw VersionError("File was from an incompatible version of CSX64");
		}

		// -- read globals -- //

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		GlobalSymbols.clear();
		GlobalSymbols.reserve((std::size_t)val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!read_str(file, str)) goto err;
			GlobalSymbols.emplace(std::move(str));
		}

		// -- read externals -- //

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		ExternalSymbols.clear();
		ExternalSymbols.reserve((std::size_t)val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!read_str(file, str)) goto err;
			ExternalSymbols.emplace(std::move(str));
		}

		// -- read symbols -- //

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		Symbols.clear();
		Symbols.reserve((std::size_t)val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!read_str(file, str)) goto err;
			if (!Expr::ReadFrom(file, expr)) goto err;
			Symbols.emplace(std::move(str), std::move(expr));
		}

		// -- read alignments -- //

		if (!read<u32>(file, TextAlign) || !is_pow2(TextAlign)) goto err;
		if (!read<u32>(file, RodataAlign) || !is_pow2(RodataAlign)) goto err;
		if (!read<u32>(file, DataAlign) || !is_pow2(DataAlign)) goto err;
		if (!read<u32>(file, BSSAlign) || !is_pow2(BSSAlign)) goto err;

		// -- read segment holes -- //

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		TextHoles.clear();
		TextHoles.reserve((std::size_t)val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!HoleData::ReadFrom(file, hole)) goto err;
			TextHoles.emplace_back(std::move(hole));
		}

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		RodataHoles.clear();
		RodataHoles.reserve((std::size_t)val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!HoleData::ReadFrom(file, hole)) goto err;
			RodataHoles.emplace_back(std::move(hole));
		}

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		DataHoles.clear();
		DataHoles.reserve((std::size_t)val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!HoleData::ReadFrom(file, hole)) goto err;
			DataHoles.emplace_back(std::move(hole));
		}

		// -- read segments -- //

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		Text.clear();
		Text.resize((std::size_t)val);
		if (!read_bin(file, Text.data(), (std::size_t)val)) goto err;

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		Rodata.clear();
		Rodata.resize((std::size_t)val);
		if (!read_bin(file, Rodata.data(), (std::size_t)val)) goto err;

		if (!read<u64>(file, val)) goto err;
		if constexpr (std::numeric_limits<std::size_t>::max() < std::numeric_limits<u64>::max()) { if (val != (std::size_t)val) throw MemoryAllocException("File contents too large"); }
		Data.clear();
		Data.resize((std::size_t)val);
		if (!read_bin(file, Data.data(), (std::size_t)val)) goto err;

		if (!read<u64>(file, BssLen)) goto err;

		// -- read binary literals -- //

		if (!Literals.read_from(file)) goto err;

		// -- done -- //

		// validate the object
		_Clean = true;

		return;

	err:
		throw FormatError("Object file was corrupted");
	}

	// -- parsing utilities -- //

	bool TryExtractStringChars(const std::string &token, std::string &chars, std::string &err)
	{
		std::string b; // string builder (so token and chars can safely refer to the same object)

		// make sure it starts with a quote and is terminated
		if (token.empty() || (token[0] != '"' && token[0] != '\'' && token[0] != '`') || token[0] != token[token.size() - 1]) { err = "Ill-formed string: " + token; return false; }

		// read all the characters inside
		for (int i = 1; i < (int)token.size() - 1; ++i)
		{
			// if this is a `backquote` literal, it allows \escapes
			if (token[0] == '`' && token[i] == '\\')
			{
				// bump up i and make sure it's still good
				if (++i >= (int)token.size() - 1) { err = "Ill-formed string (ends with beginning of an escape sequence): " + token; return false; }

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

	void ObjectFile::RenameSymbol(const std::string &from, const std::string &to)
	{
		// make sure "to" doesn't already exist
		if (contains_key(Symbols, to) || contains(ExternalSymbols, to))
			throw std::invalid_argument("Attempt to rename symbol \"" + from + "\" to \"" + to + "\" (already exists)");
		
		// if it's a symbol defined in this file
		Expr *expr;
		if (try_get_value(Symbols, from, expr))
		{
			// make sure it hasn't already been evaluated (because it may have already been linked to other expressions)
			if (expr->IsEvaluated()) throw std::runtime_error("Attempt to rename symbol \"" + from + "\" to \"" + to + "\" (already evaluated)");
			
			// rename the symbol
			Symbols.emplace(to, std::move(*expr));
			Symbols.erase(from);

			// find and replace in global table (may not be global - that's ok)
			if (contains(GlobalSymbols, from))
			{
				
				GlobalSymbols.emplace(to);
				GlobalSymbols.erase(from);
			}
		}
		// if it's a symbol defined externally
		else if (contains(ExternalSymbols, from))
		{
			// replace
			ExternalSymbols.emplace(to);
			ExternalSymbols.erase(from);
		}
		// otherwise we don't know what it is
		else throw std::runtime_error("Attempt to rename symbol \"" + from + "\" to \"" + to + "\" (does not exist)");

		// -- now the easy part -- //
		
		// find and replace in symbol table expressions
		for (auto &entry : Symbols) entry.second.Resolve(from, to);

		// find and replace in hole expressions
		for (auto &entry : TextHoles) entry.expr.Resolve(from, to);
		for (auto &entry : RodataHoles) entry.expr.Resolve(from, to);
		for (auto &entry : DataHoles) entry.expr.Resolve(from, to);
	}
	
	// helper for imm parser
	bool TryGetOp(const std::string &token, std::size_t pos, Expr::OPs &op, int &oplen)
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
				case '/': op = Expr::OPs::SDiv; return true;
				case '%': op = Expr::OPs::SMod; return true;
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
			case '/': op = Expr::OPs::UDiv; return true;
			case '%': op = Expr::OPs::UMod; return true;

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

	void AssemblerPredefines::define_u64(std::string key, u64 value)
	{
		Expr expr;
		expr.IntResult(value);
		symbols.emplace(std::move(key), std::move(expr));
	}
	void AssemblerPredefines::define_f64(std::string key, f64 value)
	{
		Expr expr;
		expr.FloatResult(value);
		symbols.emplace(std::move(key), std::move(expr));
	}

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
		if (data.expr.Evaluate(symbols, val, floating, err))
		{
			// if it's floating-point
			if (floating)
			{
				// only 64-bit and 32-bit are supported
				switch (data.size)
				{
				case 8: if (!Write(res, data.address, 8, val)) { err = "line " + tostr(data.line) + ": Error writing value"; return PatchError::Error; } break;
				case 4: if (!Write(res, data.address, 4, transmute<u32>((f32)transmute<f64>(val)))) { err = "line " + tostr(data.line) + ": Error writing value"; return PatchError::Error; } break;

				default: err = "line " + tostr(data.line) + ": Attempt to use unsupported floating-point format"; return PatchError::Error;
				}
			}
			// otherwise it's integral
			else if (!Write(res, data.address, data.size, val)) { err = "line " + tostr(data.line) + ": Error writing value"; return PatchError::Error; }

			// successfully patched
			return PatchError::None;
		}
		// otherwise it's unevaluated
		else { err = "line " + tostr(data.line) + ": Failed to evaluate expression\n-> " + err; return PatchError::Unevaluated; }
	}

	// -- let the fun begin -- //

	// tries to patch and eliminate as many holes as possible. returns true unless there was a hard error during evaluation (e.g. unsupported floating-point format).
	bool _ElimHoles(std::unordered_map<std::string, Expr> &symbols, std::vector<HoleData> &holes, std::vector<u8> &seg, AssembleResult &res)
	{
		using std::swap;

		for (int i = (int)holes.size() - 1; i >= 0; --i)
		{
			switch (TryPatchHole(seg, symbols, holes[i], res.ErrorMsg))
			{
			case PatchError::None: if (i != (int)holes.size() - 1) swap(holes[i], holes.back()); holes.pop_back(); break; // remove the hole if we solved it
			case PatchError::Unevaluated: break;
			case PatchError::Error: res.Error = AssembleError::ArgError; return false;

			default: throw std::runtime_error("Unknown patch error encountered");
			}
		}

		return true;
	}
	// tries to patch all holes. returns true only if all holes were patched.
	bool _FixAllHoles(std::unordered_map<std::string, Expr> &symbols, std::vector<HoleData> &holes, std::vector<u8> &seg, LinkResult &res)
	{
		for (std::size_t i = 0; i < holes.size(); ++i)
		{
			switch (TryPatchHole(seg, symbols, holes[i], res.ErrorMsg))
			{
			case PatchError::None: break;
			case PatchError::Unevaluated: res.Error = LinkError::MissingSymbol; return false;
			case PatchError::Error: res.Error = LinkError::FormatError; return false;

			default: throw std::runtime_error("Unknown patch error encountered");
			}
		}

		return true;
	}
	
	AssembleResult assemble(std::istream &code, ObjectFile &_file_, const AssemblerPredefines *predefines)
	{
		// clear out the destination object file
		_file_.clear();

		// create the asm args for parsing - reference the (cleared) destination object file
		AssembleArgs args(_file_);

		// add all the predefined symbols
		if (predefines)
		{
			args.file.Symbols.insert(predefines->symbols.begin(), predefines->symbols.end());
		}
		
		// -- add a few more that we create -- //
		
		args.file.Symbols.insert(std::make_pair("__version__", Expr::CreateInt(Version)));

		args.file.Symbols.insert(std::make_pair("__pinf__", Expr::CreateFloat(std::numeric_limits<f64>::infinity())));
		args.file.Symbols.insert(std::make_pair("__ninf__", Expr::CreateFloat(-std::numeric_limits<f64>::infinity())));
		args.file.Symbols.insert(std::make_pair("__nan__", Expr::CreateFloat(std::numeric_limits<f64>::quiet_NaN())));

		args.file.Symbols.insert(std::make_pair("__fmax__", Expr::CreateFloat(std::numeric_limits<f64>::max())));
		args.file.Symbols.insert(std::make_pair("__fmin__", Expr::CreateFloat(std::numeric_limits<f64>::min())));
		args.file.Symbols.insert(std::make_pair("__fepsilon__", Expr::CreateFloat(std::numeric_limits<f64>::denorm_min())));

		args.file.Symbols.insert(std::make_pair("__pi__", Expr::CreateFloat(3.141592653589793238462643383279502884197169399375105820974)));
		args.file.Symbols.insert(std::make_pair("__e__", Expr::CreateFloat(2.718281828459045235360287471352662497757247093699959574966)));

		// potential parsing args for an instruction
		u64 a = 0;
		bool floating;

		std::string err; // error location for evaluation

		std::string rawline; // raw line to parse
		int ch;              // character extracted

		const asm_router *router; // router temporary

		while (code)
		{
			// get the line to parse
			rawline.clear();
			while ((ch = code.get()) != EOF)
			{
				// if we hit a comment char, discard it and the rest of the line
				if (ch == CommentChar)
				{
					code.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
					break; // we extracted a new line char, so stop line processing loop
				}
				// if we hit a new line char, we're done (don't append it to the string)
				else if (ch == '\n') break;
				// otherwise it's a normal char - add it to the raw line
				else rawline.push_back(ch);
			}
			++args.line;

			// if this is a shebang line (must have "#!" at the start of line 1)
			if (args.line == 1 && rawline.size() >= 2 && rawline[0] == '#' && rawline[1] == '!')
			{
				// ignore it - do this by pretending the line was empty
				rawline.clear();
			}

			// must update current line pos before TryExtractLineHeader() (because it may introduce labels)
			args.UpdateLinePos();

			// extract line header info
			if (!args.TryExtractLineHeader(rawline)) return args.res;

			// assemble this line a number of times equal to args.times (updated by calling TryExtractLineHeader() above)
			for (args.times_i = 0; args.times_i < args.times; ++args.times_i)
			{
				// must update current line pos before each TIMES assembly iteration (because each TIMES iteration is like a new line)
				args.UpdateLinePos();

				// split the line and prepare for parsing
				if (!args.SplitLine(rawline)) return args.res;

				// empty lines are ignored
				if (!args.op.empty())
				{
					// try to get the router
					if (!try_get_value(asm_routing_table, to_upper(args.op), router)) return { AssembleError::UnknownOp, "line " + tostr(args.line) + ": Unknown instruction" };

					// perform the assembly action
					if (!(*router)(args)) return args.res;
				}
			}
		}

		// -- minimize symbols and holes -- //

		// link each symbol to internal symbols (minimizes file size)
		for(auto &entry : args.file.Symbols) entry.second.Evaluate(args.file.Symbols, a, floating, err);

		// eliminate as many holes as possible
		if (!_ElimHoles(args.file.Symbols, args.file.TextHoles, args.file.Text, args.res)) return args.res;
		if (!_ElimHoles(args.file.Symbols, args.file.RodataHoles, args.file.Rodata, args.res)) return args.res;
		if (!_ElimHoles(args.file.Symbols, args.file.DataHoles, args.file.Data, args.res)) return args.res;

		// -- eliminate as many unnecessary symbols as we can -- //
		
		// these are copies to ensure removing/renaming in the symbol table doesn't make them dangling pointers
		std::vector<std::string> elim_symbols;   // symbol names to be eliminated
		std::vector<std::string> rename_symbols; // symbol names that we can rename to be shorter

		// for each symbol
		for(auto &entry : args.file.Symbols)
		{
			// if this symbol is non-global
			if (!contains(args.file.GlobalSymbols, entry.first))
			{
				// if this symbol has already been evaluated
				if (entry.second.IsEvaluated())
				{
					// we can eliminate it (because it's already been linked internally and won't be needed externally)
					elim_symbols.push_back(entry.first);
				}
				// otherwise we can rename it to something shorter (because it's still needed internally, but not needed externally)
				else rename_symbols.push_back(entry.first);
			}
		}
		// remove all the symbols we can eliminate
		for (const auto &elim : elim_symbols) args.file.Symbols.erase(elim);

		// -- finalize -- //

		// verify integrity of file
		if (!args.VerifyIntegrity()) return args.res;

		// rename all the symbols we can shorten (done after verify to ensure there's no verify error messages with the renamed symbols)
		for (std::size_t i = 0; i < rename_symbols.size(); ++i) args.file.RenameSymbol(rename_symbols[i], "^" + tohex(i));
		
		// validate assembled result (referentially-bound to _file_ argument)
		args.file._Clean = true;

		// return no error
		return {AssembleError::None, ""};
	}
	LinkResult link(Executable &exe, std::list<std::pair<std::string, ObjectFile>> &objs, const std::string &entry_point)
	{
		// parsing locations for evaluation
		u64 _res;
		bool _floating;
		std::string _err;
		LinkResult res;

		// -- ensure args are good -- //

		// ensure entry point is legal
		if (!AssembleArgs::IsValidName(entry_point, _err)) return LinkResult{LinkError::FormatError, "Entry point \"" + entry_point + "\" is not a legal symbol name"};

		// ensure we got at least 1 object file
		if (objs.empty()) return {LinkError::EmptyResult, "Got no object files"};

		// make sure all object files are starting out clean
		for (const auto &obj : objs) if (!obj.second.is_clean()) throw DirtyError("Attempt to use dirty object file");

		// -- validate _start file -- //

		// _start file must declare an external named "_start"
		if (!contains(objs.front().second.ExternalSymbols, (std::string)"_start")) return LinkResult{LinkError::FormatError, "_start file must declare an external named \"_start\""};

		// rename "_start" symbol in _start file to whatever the entry point is (makes _start dirty)
		try { objs.front().second.make_dirty(); objs.front().second.RenameSymbol("_start", entry_point); }
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
		std::unordered_map<std::string, std::pair<std::string, ObjectFile>*> global_to_obj;

		// the queue of object files that need to be added to the executable
		std::deque<std::pair<std::string, ObjectFile>*> include_queue;
		// a table for relating included object files to their beginning positions in the resulting binary (text, rodata, data, bss) tuples
		std::unordered_map<std::pair<std::string, ObjectFile>*, std::tuple<u64, u64, u64, u64>> included;

		BinaryLiteralCollection total_literals; // destination to merge all binary literals throughout all included object files
		std::unordered_map<std::pair<std::string, ObjectFile>*, std::vector<std::size_t>> obj_to_total_literals_locations; // maps from an included object file to its literal index map in total_literals

		std::size_t literals_size; // the total size of all (top level) literals merged together
		std::vector<std::size_t> rodata_top_level_literal_offsets; // offsets of each top level literal in the rodata segment

		// -- populate things -- //

		// populate global_to_obj with ALL global symbols
		for (auto &obj : objs)
		{
			const Expr *value;

			for (const auto &global : obj.second.GlobalSymbols)
			{
				// make sure source actually defined this symbol (just in case of corrupted object file)
				if (!try_get_value(obj.second.Symbols, global, value)) return LinkResult{LinkError::MissingSymbol, obj.first + ": Global symbol \"" + global + "\" was not defined"};
				// make sure it wasn't already defined
				if (contains_key(global_to_obj, global)) return LinkResult{LinkError::SymbolRedefinition, obj.first + ": Global symbol \"" + global + "\" was defined by " + global_to_obj[global]->first};

				// add to the table
				global_to_obj.emplace(global, &obj);
			}
		}

		// -- verify things -- //

		// make sure no one defined over reserved symbol names
		for (auto &obj : objs)
		{
			// only the verify ignores are a problem (because we'll be defining those)
			for (const std::string &reserved : VerifyLegalExpressionIgnores)
				if (contains_key(obj.second.Symbols, reserved)) return LinkResult{LinkError::SymbolRedefinition, obj.first + ": defined symbol with name \"" + reserved + "\" (reserved)"};
		}

		// start the merge process with the _start file
		include_queue.push_back(&objs.front()); // (by this point [0] is guaranteed to exist)

		// -- merge things -- //

		// while there are still things in queue
		while (!include_queue.empty())
		{
			// get the object file we need to incorporate
			std::pair<std::string, ObjectFile> *item = include_queue.front();
			include_queue.pop_front();
			ObjectFile &obj = item->second;

			// all included files are dirty
			obj.make_dirty();

			// account for alignment requirements
			Align(text, obj.TextAlign);
			Align(rodata, obj.RodataAlign);
			Align(data, obj.DataAlign);
			bsslen = Align(bsslen, obj.BSSAlign);

			// update segment alignments
			textalign = std::max<u64>(textalign, obj.TextAlign);
			rodataalign = std::max<u64>(rodataalign, obj.RodataAlign);
			dataalign = std::max<u64>(dataalign, obj.DataAlign);
			bssalign = std::max<u64>(bssalign, obj.BSSAlign);

			// add it to the set of included files
			included.emplace(item, std::tuple<u64, u64, u64, u64>(text.size(), rodata.size(), data.size(), bsslen));

			// offset holes to be relative to the start of their total segment (not relative to resulting file)
			for (HoleData &hole : obj.TextHoles) hole.address += text.size();
			for (HoleData &hole : obj.RodataHoles) hole.address += rodata.size();
			for (HoleData &hole : obj.DataHoles) hole.address += data.size();

			// reserve space for segments
			text.resize(text.size() + obj.Text.size());
			rodata.resize(rodata.size() + obj.Rodata.size());
			data.resize(data.size() + obj.Data.size());

			// append segments
			if (obj.Text.size() > 0) std::memcpy(text.data() + (text.size() - obj.Text.size()), obj.Text.data(), obj.Text.size());
			if (obj.Rodata.size() > 0) std::memcpy(rodata.data() + (rodata.size() - obj.Rodata.size()), obj.Rodata.data(), obj.Rodata.size());
			if (obj.Data.size() > 0) std::memcpy(data.data() + (data.size() - obj.Data.size()), obj.Data.data(), obj.Data.size());
			bsslen += obj.BssLen;

			// for each external symbol
			for (const std::string &external : obj.ExternalSymbols)
			{
				std::pair<std::string, ObjectFile> **global_source;

				// if this is a global symbol somewhere
				if (try_get_value(global_to_obj, external, global_source))
				{
					// if the source isn't already included and it isn't already in queue to be included
					if (!contains_key(included, *global_source) && !contains(include_queue, *global_source))
					{
						// add it to the queue
						include_queue.push_back(*global_source);
					}
				}
				// otherwise it wasn't defined
				else return LinkResult{LinkError::MissingSymbol, item->first + ": No global symbol found to match external symbol \"" + external + "\""};
			}

			// merge top level binary literals for this include file and fix their aliasing literal ranges
			std::vector<std::pair<std::size_t, std::size_t>> literal_fix_map(obj.Literals.top_level_literals.size());
			for (std::size_t i = 0; i < obj.Literals.top_level_literals.size(); ++i)
			{
				// add the top level literal and get its literal index
				std::size_t literal_pos = total_literals.add(std::move(obj.Literals.top_level_literals[i]));
				// extract all the info we need to adjust and merge the literal ranges for this object file
				std::size_t referenced_top_level_index = total_literals.literals[literal_pos].top_level_index;
				std::size_t start = total_literals.literals[literal_pos].start;

				// add a fix entry for this top level literal
				literal_fix_map[i] = {referenced_top_level_index, start};
			}
			// update all the literals for this file
			for (auto &lit : obj.Literals.literals)
			{
				// get the fix info for this literal's old top level index
				const std::pair<std::size_t, std::size_t> &fix_info = literal_fix_map[lit.top_level_index];

				lit.top_level_index = fix_info.first;
				lit.start += fix_info.second;
			}

			// insert a new literal locations map (pre-sized) and alias it
			std::vector<std::size_t> &literal_locations = obj_to_total_literals_locations.emplace(item, obj.Literals.literals.size()).first->second;

			// merge the (fixed) aliasing literal ranges
			for (std::size_t i = 0; i < obj.Literals.literals.size(); ++i)
			{
				// insert the aliasing literal range (aliased top level already added and range info fixed)
				// store the insertion index in the literal locations map for this object file
				literal_locations[i] = total_literals._insert(obj.Literals.literals[i]);
			}

			// this object file's literals collection is now invalid - just clear it all out
			obj.Literals.literals.clear();
			obj.Literals.top_level_literals.clear();
		}

		// after merging, but before alignment, we need to handle all the provisioned binary literals.
		// we'll just dump them all in the rodata segment (they're required to be read-only from a user perspective esp since they can alias)
		literals_size = 0;
		rodata_top_level_literal_offsets.resize(total_literals.top_level_literals.size());
		for (std::size_t i = 0; i < total_literals.top_level_literals.size(); ++i)
		{
			// for each top level literal, compute its (future) offset in the rodata segment and while we're at it calculate the total size of all top level literals
			rodata_top_level_literal_offsets[i] = rodata.size() + literals_size;
			literals_size += total_literals.top_level_literals[i].size();
		}

		// now that we know where all the literals should go, resize the rodata segment and copy them to their appropriate places
		rodata.resize(rodata.size() + literals_size);
		for (std::size_t i = 0; i < total_literals.top_level_literals.size(); ++i)
		{
			std::memcpy(&rodata[0] + rodata_top_level_literal_offsets[i], total_literals.top_level_literals[i].data(), total_literals.top_level_literals[i].size());
		}

		// and now go ahead and resolve all the missing binary literal symbols with relative address from the rodata origin
		for (const auto &entry : obj_to_total_literals_locations)
		{
			for (std::size_t i = 0; i < entry.second.size(); ++i)
			{
				// alias the literal range in total_literals
				const BinaryLiteralCollection::BinaryLiteral &lit = total_literals.literals[entry.second[i]];

				// construct the expr to alias the correct location in the rodata segment
				Expr expr;
				expr.OP = Expr::OPs::Add;
				expr.Left = Expr::NewToken(SegOrigins.at(AsmSegment::RODATA));
				expr.Right = Expr::NewInt(rodata_top_level_literal_offsets[lit.top_level_index] + lit.start);

				// inject the symbol definition into the object file's symbol table
				entry.first->second.Symbols.emplace(BinaryLiteralSymbolPrefix + tohex(i), std::move(expr));
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
			ObjectFile &obj = entry.first->second;

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
				if (!obj.Symbols.at(global).Evaluate(obj.Symbols, _res, _floating, _err))
					return LinkResult{LinkError::MissingSymbol, entry.first->first + ": Global symbol \"" + global + "\" could not be evaluated internally"};
			}
		}

		// this needs to be done after the previous loop to ensure globals have been evaluated before copying them
		for (auto &entry : included)
		{
			// alias the object file
			ObjectFile &obj = entry.first->second;

			// for each external symbol
			for (const std::string &external : obj.ExternalSymbols)
			{
				// add externals to local scope //

				// if obj already has a symbol of the same name
				if (contains_key(obj.Symbols, external)) return {LinkError::SymbolRedefinition, entry.first->first + ": defined external symbol \"" + external + "\""};
				// otherwise define it as a local in obj
				else obj.Symbols.emplace(external, global_to_obj.at(external)->second.Symbols.at(external));
			}
		}
		
		// -- patch things -- //

		// for each object file
		for (auto &entry : included)
		{
			// alias object file
			ObjectFile &obj = entry.first->second;

			// patch all the holes
			if (!_FixAllHoles(obj.Symbols, obj.TextHoles, text, res)) return {res.Error, entry.first->first + ":\n" + res.ErrorMsg};
			if (!_FixAllHoles(obj.Symbols, obj.RodataHoles, rodata, res)) return { res.Error, entry.first->first + ":\n" + res.ErrorMsg };
			if (!_FixAllHoles(obj.Symbols, obj.DataHoles, data, res)) return { res.Error, entry.first->first + ":\n" + res.ErrorMsg };
		}

		// -- finalize things -- //

		// construct the executable and we're good to go
		try
		{
			exe.construct(text, rodata, data, bsslen);
			return { LinkError::None, "" };
		}
		catch (const std::overflow_error&)
		{
			return { LinkError::FormatError, "Sum of segment sizes exceeded maximum get_size" };
		}
	}
}
