// this file adds implementations for platform-specific driver functions.
// CSX64 itself is cross-platform, but the driver may need certain key pieces of information about the system to function properly.

// this file needs language extensions enabled in VisualStudio (i.e. no /Za build option).
// language extension are disabled for all the other files so that they only use standard-conforming C++.
// if you start getting mysterious compile errors from library code, make sure this file has them enabled.

#if defined(_WIN32)

// ------------- //

// -- WINDOWS -- //

// ------------- //

#include <Windows.h>

const char *exe_dir()
{
	static struct obj_t
	{
        const char *res = nullptr; // the resulting path
		char        buf[MAX_PATH]; // buffer for storing exe dir
        
		obj_t()
		{
			// get absolute path to exe
			auto len = GetModuleFileNameA(NULL, buf, sizeof(buf));
            
            // if that succeeded
            if (len != 0)
            {
			    // slide back len to remove the file name
			    while (len > 0 && buf[len - 1] != '\\' && buf[len - 1] != '/') --len;
                
			    // place the terminator at the resulting len
			    buf[len] = 0;
                
                // point result at the buffer
                res = buf;
            }
		}
	} obj;
    
	return obj.res;
}

#elif defined(unix) || defined(__unix__) || defined(__unix)

// ---------- //

// -- UNIX -- //

// ---------- //

#include <unistd.h>

const char *exe_dir()
{
    static struct obj_t
    {
        const char *res = nullptr; // the resulting path
        char        buf[1024];     // buffer for storing exe dir
        
        obj_t()
        {
            // get absolute path to exe
            auto len = readlink("/proc/self/exe", buf, sizeof(buf));
            
            // if that succeeded
            if (len != -1)
            {
                // slide back len to remove the file name
			    while (len > 0 && buf[len - 1] != '\\' && buf[len - 1] != '/') --len;
                
			    // place the terminator at the resulting len
			    buf[len] = 0;

                // point result at the buffer
                res = buf;
            }
        }
    } obj;
    
    return obj.res;
}

#else

// --------------------- //

// -- NO LOCALIZATION -- //

// --------------------- //

const char *exe_dir()
{
	return nullptr;
}

#endif

