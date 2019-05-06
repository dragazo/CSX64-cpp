#ifndef DRAGAZO_CSX64_CSX_EXCEPTIONS_H
#define DRAGAZO_CSX64_CSX_EXCEPTIONS_H

#include <exception>
#include <stdexcept>

namespace CSX64
{
	// exception type thrown when a file could not be opened
	struct FileOpenError : std::runtime_error { using std::runtime_error::runtime_error; };
	// exception type thrown when a read/write operation fails
	struct IOError : std::runtime_error { using std::runtime_error::runtime_error; };

	// exception type thrown when attempting to use an object of incompatible version
	struct VersionError : std::invalid_argument { using std::invalid_argument::invalid_argument; };
	// exception type thrown when attempting to use an object of the incorrect type
	struct TypeError : std::invalid_argument { using std::invalid_argument::invalid_argument; };

	// exception type thrown when attempting to use an object of invalid format
	struct FormatError : std::invalid_argument { using std::invalid_argument::invalid_argument; };
	// exception type thrown when attempting to use a dirty object
	struct DirtyError : std::invalid_argument { using std::invalid_argument::invalid_argument; };
	// exception type thrown when attempting to use an empty object
	struct EmptyError : std::invalid_argument { using std::invalid_argument::invalid_argument; };

	// exception type thrown when program code attempts to violate memory requirements
	struct MemoryAllocException : std::length_error { using std::length_error::length_error; };

	// exception type thrown when IFileWrapper permissions are violated
	struct FileWrapperPermissionsException : std::runtime_error { using std::runtime_error::runtime_error; };
}

#endif
