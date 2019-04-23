#ifndef DRAGAZO_CSX64_CSX_EXCEPTIONS_H
#define DRAGAZO_CSX64_CSX_EXCEPTIONS_H

#include <exception>
#include <stdexcept>

namespace CSX64
{
	// exception type thrown when attempting to use an invalid executable
	struct ExecutableFormatError : std::invalid_argument { using std::invalid_argument::invalid_argument; };

	// exception type thrown when program code attempts to violate memory requirements
	struct MemoryAllocException : std::length_error { using std::length_error::length_error; };

	// exception type thrown when IFileWrapper permissions are violated
	struct FileWrapperPermissionsException : std::runtime_error { using std::runtime_error::runtime_error; };
}

#endif
