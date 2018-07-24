// !! ADD INCLUDES/LOGIC FOR GETTING EXE DIRECTORY ON VARIOUS PLATFORMS !! //

// this file needs language extensions enabled (i.e. no /Za build option).
// language extension are disabled for all the other files so that they only use standard-conforming C++.
// if you start getting mysterious compile errors from library code, make sure this file has them enabled.

#ifdef _WIN32

#include <string>
#include <Windows.h>

std::string __exe_dir;

void init_exe_dir()
{
	// get absolute path to exe
	char __exe_dir_buf[256];
	GetModuleFileNameA(NULL, __exe_dir_buf, sizeof(__exe_dir_buf));

	// load into the string
	__exe_dir = __exe_dir_buf;

	// chip off the file name
	while (!__exe_dir.empty() && __exe_dir.back() != '\\' && __exe_dir.back() != '/') __exe_dir.pop_back();
}
const std::string &exe_dir()
{
	return __exe_dir;
}

#endif