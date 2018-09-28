// !! ADD INCLUDES/LOGIC FOR GETTING EXE DIRECTORY ON VARIOUS PLATFORMS !! //

// this file needs language extensions enabled (i.e. no /Za build option).
// language extension are disabled for all the other files so that they only use standard-conforming C++.
// if you start getting mysterious compile errors from library code, make sure this file has them enabled.

#ifdef _WIN32

#include <cstddef>
#include <Windows.h>

const char *exe_dir()
{
	// create a static instance of a helper type
	static struct obj_t
	{
		// buffer for storing exe dir
		char buf[MAX_PATH];

		// constructor of this object gets the dir name
		obj_t()
		{
			// get absolute path to ex e
			std::size_t len = (std::size_t)GetModuleFileNameA(NULL, buf, sizeof(buf));

			// slide back len to remove the file name
			while (len > 0 && buf[len - 1] != '\\' && buf[len - 1] != '/') --len;

			// place the terminator at the resulting len
			buf[len] = 0;
		}
	} obj;

	// return our exe dir buffer
	return obj.buf;
}

#else

// default implementation returns nullptr - we don't have localization for this platform
const char *exe_dir()
{
	return nullptr;
}

#endif