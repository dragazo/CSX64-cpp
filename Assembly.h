#ifndef CSX64_ASSEMBLY_H
#define CSX64_ASSEMBLY_H

#include <string>
#include <vector>
#include <unordered_map>

#include "CoreTypes.h"
#include "HoleData.h"
#include "OFile.h"
#include "Expr.h"

// -- Assembly -- //

// LIMITATIONS:
// HoleData assumes 32-bit addresses to cut down on memory/disk usage
// Assembler/linker use List<byte>, which uses 32-bit indexing

namespace CSX64
{
	enum class AssembleError
	{
		None, ArgCount, MissingSize, ArgError, FormatError, UsageError, UnknownOp, EmptyFile, InvalidLabel, SymbolRedefinition, UnknownSymbol, NotImplemented
	};
	enum class LinkError
	{
		None, EmptyResult, SymbolRedefinition, MissingSymbol, FormatError
	};

	struct AssembleResult
	{
		AssembleError Error;
		std::string ErrorMsg;
	};
	struct LinkResult
	{
		LinkError Error;
		std::string ErrorMsg;
	};

	enum class PatchError
	{
		None, Unevaluated, Error
	};
	enum class AsmSegment
	{
		INVALID = 0, TEXT = 1, RODATA = 2, DATA = 4, BSS = 8
	};

	// -----------------------------

	/// <summary>
	/// Converts a string token into its character internals (accouting for C-style escapes in the case of `backquotes`)
	/// </summary>
	/// <param name="token">the string token to process (with quotes around it)</param>
	/// <param name="chars">the resulting character internals (without quotes around it) - can be the same object as token</param>
	/// <param name="err">the error message if there was an error</param>
	bool TryExtractStringChars(const std::string &token, std::string &chars, std::string &err);

	/// <summary>
	/// Gets the smallest size code that will support the unsigned value
	/// </summary>
	/// <param name="val">the value to test</param>
	static u64 SmallestUnsignedSizeCode(u64 val);

	/// <summary>
	/// Renames "from" to "to" in the object file. The symbol to rename may be internal or external.
	/// The object file is assumed to be complete and verified. The symbol must not have already been evaluated.
	/// Throws <see cref="ArgumentException"/> if "to" already exists or if "from" has already been evaluated (because it may have already been linked to other expressions).
	/// </summary>
	/// <param name="file">the file to act on</param>
	/// <param name="from">the original name</param>
	/// <param name="to">the resulting name</param>
	/// <exception cref="ArgumentException"></exception>
	static void RenameSymbol(ObjectFile &file, const std::string &from, const std::string &to);

	// helper for imm parser
	bool TryGetOp(const std::string &token, int pos, Expr::OPs &op, int &oplen);

	// defines a symbol for the assembler
	void DefineSymbol(std::string key, std::string value);
	void DefineSymbol(std::string key, u64 value);
	void DefineSymbol(std::string key, double value);

	// -----------------------------------

	PatchError TryPatchHole(std::vector<u8> &res, std::unordered_map<std::string, Expr> &symbols, HoleData &data, std::string &err);

	/// <summary>
	/// Assembles the code into an object file
	/// </summary>
	/// <param name="code">the code to assemble</param>
	/// <param name="file">the resulting object file if no errors occur</param>
	AssembleResult Assemble(const std::string &code, ObjectFile &file);
	/// <summary>
	/// Links object files together into an executable. Throws <see cref="ArgumentException"/> if any of the object files are dirty.
	/// Object files may be rendered dirty after this process (regardless of success). Any files that are still clean may be reused.
	/// </summary>
	/// <param name="exe">the resulting executable</param>
	/// <param name="objs">the object files to link. should all be clean. the first item in this array is the _start file</param>
	/// <param name="entry_point">the raw starting file</param>
	/// <exception cref="ArgumentException"></exception>
	LinkResult Link(std::vector<u8> &exe, std::vector<ObjectFile> &objs, std::string entry_point = "main");
}


#endif
