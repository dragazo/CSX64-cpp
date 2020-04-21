#ifndef CSX64_ASSEMBLY_H
#define CSX64_ASSEMBLY_H

#include <string>
#include <vector>
#include <list>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include "CoreTypes.h"
#include "Expr.h"
#include "Executable.h"

namespace CSX64
{
	enum class AssembleError
	{
		None, ArgCount, MissingSize, ArgError, FormatError, UsageError, UnknownOp, EmptyFile, InvalidLabel, SymbolRedefinition, UnknownSymbol, NotImplemented, Assertion, Failure
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
	/// Holds information on the location and value of missing pieces of information in an object file
	/// </summary>
	class HoleData
	{
	public:

		// The local address of the hole in the file
		u64 Address;
		// The size of the hole
		u8 Size;

		// The line where this hole was created
		u32 Line;
		// The expression that represents this hole's value
		Expr expr;

	public: // -- IO -- //

		/// <summary>
		/// Writes a binary representation of a hole to the stream
		/// </summary>
		/// <param name="writer">the binary writer to use</param>
		/// <param name="hole">the hole to write</param>
		static std::ostream &WriteTo(std::ostream &writer, const HoleData &hole);
		/// <summary>
		/// Reads a binary representation of a hole from the stream
		/// </summary>
		/// <param name="reader">the binary reader to use</param>
		/// <param name="hole">the resulting hole</param>
		static std::istream &ReadFrom(std::istream &reader, HoleData &hole);
	};

	// -----------------------------
	
	class ObjectFile;

	// represents a collection of (shared) binary literals.
	// they are stored in the most compact way possible with maximum sharing.
	class BinaryLiteralCollection
	{
	private: // -- private types -- //

		struct BinaryLiteral
		{
			std::size_t top_level_index; // index in top_level_literals
			std::size_t start;  // starting index of the binary value in the top level literal
			std::size_t length; // length of the binary value (in bytes)

			friend bool operator==(const BinaryLiteral &a, const BinaryLiteral &b) noexcept { return a.top_level_index == b.top_level_index && a.start == b.start && a.length == b.length; }
			friend bool operator!=(const BinaryLiteral &a, const BinaryLiteral &b) noexcept { return !(a == b); }
		};

	private: // -- private data -- //

		std::vector<BinaryLiteral> literals;
		std::vector<std::vector<u8>> top_level_literals;

		friend LinkResult Link(Executable &exe, std::list<std::pair<std::string, ObjectFile>> &objs, const std::string &entry_point);

	private: // -- utility -- //

		// inserts info into literals (if it doesn't already exist) and returns its index in the literals array
		std::size_t _insert(const BinaryLiteral &info);

	public: // -- ctor / dtor / asgn -- //

		BinaryLiteralCollection() = default;

		BinaryLiteralCollection(const BinaryLiteralCollection&) = default;
		BinaryLiteralCollection(BinaryLiteralCollection &&other) noexcept = default;

		BinaryLiteralCollection &operator=(const BinaryLiteralCollection&) = default;
		BinaryLiteralCollection &operator=(BinaryLiteralCollection&&) = default;

	public: // -- interface -- //

		// adds a new binary literal to the collection - returns the index of the literal added
		std::size_t add(std::vector<u8> &&value);

		// erases the contents of the collection
		void clear() noexcept;

	public: // -- IO -- //

		// writes the contents of this binary literal collection to the stream.
		// success can be checked by testing the steram after writing.
		std::ostream &write_to(std::ostream &writer) const;
		// discards the current contents of this collection and reads the contents in from the stream.
		// the contents must have been written via write_to().
		// success can be checked by testing the stream after reading.
		std::istream &read_from(std::istream &reader);
	};
	
	// -----------------------------

	// represents a flag used to denote a clean state in e.g. an object file
	class clean_flag_t
	{
	private: // -- data -- //

		bool val;

	public: // -- interface -- //

		// provide conversions to bool
		explicit operator bool() const noexcept { return val; }
		bool operator!() const noexcept { return !val; }

		// enable direct ctor/asgn for bool
		clean_flag_t(bool v) noexcept : val(v) {}
		clean_flag_t &operator=(bool v) noexcept { val = v; return *this; }

		// copying copies the state
		clean_flag_t(const clean_flag_t &other) noexcept : val(other.val) {}
		clean_flag_t &operator=(const clean_flag_t &other) noexcept { val = other.val; return *this; }

		// moving copies the state and makes the moved-from flag dirty
		clean_flag_t(clean_flag_t &&other) noexcept : val(std::exchange(other.val, false)) {}
		clean_flag_t &operator=(clean_flag_t &&other) noexcept { val = std::exchange(other.val, false); return *this; }
	};

	// Represents an assembled object file used to create an executable
	class ObjectFile
	{
	private: // -- private data -- //

		clean_flag_t _Clean = false;

		friend AssembleResult Assemble(std::istream &code, ObjectFile &file);

	public: // -- public data -- //

		std::unordered_set<std::string> GlobalSymbols;   // The list of exported symbol names
		std::unordered_set<std::string> ExternalSymbols; // The list of imported symbol names

		std::unordered_map<std::string, Expr> Symbols; // The symbols defined in the file

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

		BinaryLiteralCollection Literals;

	public: // -- ctor / dtor / asgn -- //

		// constructs an empty object file that is ready for use
		ObjectFile() = default;

		ObjectFile(const ObjectFile&) = default;
		ObjectFile &operator=(const ObjectFile&) = default;

		// move constructs a new object file. the contents of moved-from object are valid, but dirty and undefined.
		ObjectFile(ObjectFile&&) noexcept = default;
		// move assigns an object file. the contents of moved-from object are valid, but dirty and undefined.
		// self-assignment is safe.
		ObjectFile &operator=(ObjectFile&&) noexcept = default;

	public: // -- state -- //

		// checks if this object file is in a valid, usable state
		bool is_clean() const noexcept { return (bool)_Clean; }
		// marks the object file as dirty
		void make_dirty() noexcept { _Clean = false; }

		// checks if this object file is clean
		explicit operator bool() const noexcept { return is_clean(); }
		// checks if this object file is dirty (not clean)
		bool operator!() const noexcept { return !is_clean(); }

		// empties the contents of this file and effectively sets it to the newly-constructed state
		void clear() noexcept;

	public: // -- IO -- //

		// saves this object file to a file located at (path).
		// throws FileOpenError if the file cannot be opened (for writing).
		// throws IOError if any write operation fails.
		void save(const std::string &path) const;
		// loads this object file wit the content of a file located at (path).
		// throws FileOpenError if the file cannot be opened (for reading).
		// throws TypeError if the file is not a CSX64 object file.
		// throws VersionError if the file is of an incompatible version.
		// throws FormatError if the object file is corrupted.
		// throws any exception resulting from failed memory allocation.
		// if an exception is throw, this object is left in an valid, but unclean and undefined state.
		void load(const std::string &path);
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
	bool TryGetOp(const std::string &token, std::size_t pos, Expr::OPs &op, int &oplen);

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
	LinkResult Link(Executable &exe, std::list<std::pair<std::string, ObjectFile>> &objs, const std::string &entry_point = "main");
}

#endif
