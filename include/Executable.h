#ifndef DRAGAZO_CSX64_EXECUTABLE_H
#define DRAGAZO_CSX64_EXECUTABLE_H

#include <array>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>

#include "CoreTypes.h"

namespace CSX64
{
	// represents a CSX64 executable.
	// Executable instances are at all times either empty or filled with well-formed information (refered to collectively as "valid").
	// provides utilities to save and load Executables from standard streams (with additional format validation built-in).
	class Executable
	{
	private: // -- data -- //

		std::array<std::size_t, 4> _seglens; // segment lengths (text, rodata, data, bss - respectively)

		std::vector<u8> _content; // executable contents (actually loaded into memory for execution)

	public: // -- ctor / dtor / asgn -- //

		// creates an empty executable
		Executable();

		Executable(const Executable&) = default;
		Executable &operator=(const Executable&) = default;

		// move-constructs an executable - other is guaranteed to be empty after this operation.
		Executable(Executable &&other) noexcept;
		// move-assigns an executable - other is left in a valid but undefined state.
		// self-assignment is safe.
		Executable &operator=(Executable &&other) noexcept;

	public: // -- swapping -- //

		friend void swap(Executable &a, Executable &b) noexcept;

	public: // -- construction -- //

		// assigns this executable a value constructed from component segments.
		// throws std::overflow_error if the sum of all the segments overflows std::size_t.
		// if an exception is thrown, this executable is left in the empty state.
		void construct(const std::vector<u8> &text, const std::vector<u8> &rodata, const std::vector<u8> &data, std::size_t bsslen);

	public: // -- state -- //

		// returns true iff the executable is empty (i.e. all segment lengths are zero)
		bool empty() const noexcept;
		// changes the executable to the empty state (i.e. all segment lengths are zero)
		void clear() noexcept;

		// returns true iff the executable is non-empty
		explicit operator bool() const noexcept { return !empty(); }
		// returns true iff the executable is empty
		bool operator!() const noexcept { return empty(); }

	public: // -- access -- //

		// gets the length of each segment
		std::size_t text_seglen() const noexcept { return _seglens[0]; }
		std::size_t rodata_seglen() const noexcept { return _seglens[1]; }
		std::size_t data_seglen() const noexcept { return _seglens[2]; }
		std::size_t bss_seglen() const noexcept { return _seglens[3]; }

		// gets the content of the executable in proper segment order (und if empty) (does not include bss) (i.e. holds text, rodata, and data in that order).
		const u8 *content() const& noexcept { return _content.data(); }
		const u8 *content() && noexcept = delete;
		// gets the size of the content array
		std::size_t content_size() const noexcept { return _content.size(); }

		// returns the total size of all segments (including bss) (>= content_size)
		std::size_t total_size() const noexcept { return _content.size() + _seglens[3]; }

	public: // -- IO -- //

		// saves this executable to a file located at (path).
		// throws FileOpenError if the file cannot be opened (for writing).
		// throws IOError if any write operation fails.
		void save(const std::string &path) const;
		// loads this executable with the content of a file located at (path).
		// throws FileOpenError if the file cannot be opened (for reading).
		// throws TypeError if the file is not a CSX64 executable.
		// throws VersionError if the file is of an incompatible version.
		// throws FormatError if the executable file is corrupted.
		// throws any exception resulting from failed memory allocation.
		// if an exception is thrown, this executable is left in the empty state.
		void load(const std::string &path);
	};
}

#endif
