#ifndef CSX64_EXE_DIR_H
#define CSX64_EXE_DIR_H

#include <string>

// on success, returns a c-style string - path to the directory containing this executable.
// on failure, returns null.
const char *exe_dir();

#endif
