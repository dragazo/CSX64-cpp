#ifndef CSX64_ASSEMBLY_H
#define CSX64_ASSEMBLY_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "CoreTypes.h"
#include "HoleData.h"
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

	// Represents an assembled object file used to create an executable
	class ObjectFile
	{
	private:
		bool _Clean = false;

		friend AssembleResult Assemble(std::istream &code, ObjectFile &file);

	public:

		// The list of exported symbol names
		std::unordered_set<std::string> GlobalSymbols;
		// The list of imported symbol names
		std::unordered_set<std::string> ExternalSymbols;

		// The symbols defined in the file
		std::unordered_map<std::string, Expr> Symbols;

		u32 TextAlign = 1;
		u32 RodataAlign = 1;
		u32 DataAlign = 1;
		u32 BSSAlign = 1;

		std::vector<HoleData> TextHoles;
		std::vector<HoleData> RodataHoles;
		std::vector<HoleData> DataHoles;

		std::vector<u8> Text;
		std::vector<u8> Rodata;
		std::vector<u8> Data;
		u64 BssLen = 0;

		// Marks that this object file is in a valid, usable state
		inline constexpr bool Clean() const noexcept { return _Clean; }
		// marks the object file as dirty
		void MakeDirty() { _Clean = false; }

		// clears the contents of the object file (result is dirty)
		void Clear();

		// ---------------------------

		/// <summary>
		/// Writes a binary representation of an object file to the stream. Throws <see cref="std::invalid_argument"/> if file is dirty
		/// </summary>
		/// <param name="writer">the binary writer to use. must be clean</param>
		/// <param name="obj">the object file to write</param>
		static std::ostream &WriteTo(std::ostream &writer, const ObjectFile &obj);
		/// <summary>
		/// Reads a binary representation of an object file from the stream
		/// </summary>
		/// <param name="reader">the binary reader to use</param>
		/// <param name="hole">the resulting object file</param>
		static std::istream &ReadFrom(std::istream &reader, ObjectFile &obj);
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
	u64 SmallestUnsignedSizeCode(u64 val);

	/// <summary>
	/// Renames "from" to "to" in the object file. The symbol to rename may be internal or external.
	/// The object file is assumed to be complete and verified. The symbol must not have already been evaluated.
	/// Throws <see cref="ArgumentException"/> if "to" already exists or if "from" has already been evaluated (because it may have already been linked to other expressions).
	/// </summary>
	/// <param name="file">the file to act on</param>
	/// <param name="from">the original name</param>
	/// <param name="to">the resulting name</param>
	/// <exception cref="ArgumentException"></exception>
	void RenameSymbol(ObjectFile &file, std::string from, std::string to);

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
	AssembleResult Assemble(std::istream &code, ObjectFile &file);
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
