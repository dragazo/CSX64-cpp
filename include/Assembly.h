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

	class ObjectFile;

	// represents a collection of symbols that can be passed to the assembler to create pre-defined assembly symbols
	class AssemblerPredefines
	{
	private: // -- data -- //

		std::unordered_map<std::string, detail::Expr> symbols;

		friend AssembleResult assemble(std::istream&, ObjectFile&, const AssemblerPredefines*);

	public: // -- interface -- //
 
		void define_u64(std::string name, u64 value);
		void define_f64(std::string name, f64 value);
	};

	// assembles the code into an object file.
	AssembleResult assemble(std::istream &code, ObjectFile &file, const AssemblerPredefines *predefines = nullptr);
	// links object files together into an executable. throws DirtyError if any of the object files is dirty.
	// object files may be rendered dirty after this process (regardless of success). Any objects files that are still clean may be reused.
	// exe         - the resulting executable.
	// objs        - the object files to link. should all be clean. the first item in this array is the _start file.
	// entry_point - the user-level main entry point.
	LinkResult link(Executable &exe, std::list<std::pair<std::string, ObjectFile>> &objs, const std::string &entry_point = "main");

	namespace detail
	{
		enum class PatchError
		{
			None, Unevaluated, Error
		};
		enum class AsmSegment
		{
			INVALID = 0, TEXT = 1, RODATA = 2, DATA = 4, BSS = 8
		};

		// holds information on the location and value of missing pieces of information in an object file
		class HoleData
		{
		public: // -- data -- //

			u64 address; // The local address of the hole in the file
			u8 size;     // The size of the hole

			std::size_t line; // The line where this hole was created
			Expr expr;        // The expression that represents this hole's value

		public: // -- io -- //

			static std::ostream &WriteTo(std::ostream &writer, const HoleData &hole);
			static std::istream &ReadFrom(std::istream &reader, HoleData &hole);
		};

		// represents a collection of (shared) binary literals.
		// they are stored in the most compact way possible with maximum sharing.
		class BinaryLiteralCollection
		{
		private: // -- private types -- //

			struct BinaryLiteral
			{
				std::size_t top_level_index; // index in top_level_literals
				std::size_t start;           // starting index of the binary value in the top level literal
				std::size_t length;          // length of the binary value (in bytes)

				friend bool operator==(const BinaryLiteral &a, const BinaryLiteral &b) noexcept { return a.top_level_index == b.top_level_index && a.start == b.start && a.length == b.length; }
				friend bool operator!=(const BinaryLiteral &a, const BinaryLiteral &b) noexcept { return !(a == b); }
			};

		private: // -- private data -- //

			std::vector<BinaryLiteral> literals;
			std::vector<std::vector<u8>> top_level_literals;

			friend LinkResult CSX64::link(Executable &exe, std::list<std::pair<std::string, ObjectFile>> &objs, const std::string &entry_point);

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

			std::ostream &write_to(std::ostream &writer) const;
			std::istream &read_from(std::istream &reader);
		};

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
	}

	// Represents an assembled object file used to create an executable
	class ObjectFile
	{
	private: // -- private stuff -- //

		detail::clean_flag_t _Clean = false;

		friend AssembleResult CSX64::assemble(std::istream&, ObjectFile&, const AssemblerPredefines*);
		friend LinkResult CSX64::link(Executable&, std::list<std::pair<std::string, ObjectFile>>&, const std::string&);

		// renames "from" to "to" in the object file. the symbol to rename may be internal or external. the symbol must not have already been evaluated.
		// throws std::invalid_argument if "to" already exists, or if "from" has already been evaluated (because it may have already been linked to other expressions).
		void RenameSymbol(const std::string &from, const std::string &to);

	public: // -- public data -- //

		std::unordered_set<std::string> GlobalSymbols;   // The list of exported symbol names
		std::unordered_set<std::string> ExternalSymbols; // The list of imported symbol names

		std::unordered_map<std::string, detail::Expr> Symbols; // The symbols defined in the file

		u32 TextAlign = 1;
		u32 RodataAlign = 1;
		u32 DataAlign = 1;
		u32 BSSAlign = 1;

		std::vector<detail::HoleData> TextHoles;
		std::vector<detail::HoleData> RodataHoles;
		std::vector<detail::HoleData> DataHoles;

		std::vector<u8> Text;
		std::vector<u8> Rodata;
		std::vector<u8> Data;
		u64 BssLen = 0;

		detail::BinaryLiteralCollection Literals;

	public: // -- ctor / dtor / asgn -- //

		// constructs an empty object file that is ready for use
		ObjectFile() = default;

		ObjectFile(const ObjectFile&) = default;
		ObjectFile &operator=(const ObjectFile&) = default;

		// move constructs a new object file. the contents of moved-from object is valid, but dirty and undefined.
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

	// helper for imm parser
	bool TryGetOp(const std::string &token, std::size_t pos, detail::Expr::OPs &op, int &oplen);

	// -----------------------------------

	detail::PatchError TryPatchHole(std::vector<u8> &res, std::unordered_map<std::string, detail::Expr> &symbols, detail::HoleData &data, std::string &err);
}

#endif
