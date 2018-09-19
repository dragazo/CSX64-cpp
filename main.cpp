#include <list>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <filesystem>
#include <utility>
#include <sstream>

#include "CoreTypes.h"
#include "Computer.h"
#include "Assembly.h"
#include "Utility.h"
#include "ExeDir.h"

using namespace CSX64;

// !! REPLACE THIS WITH std::filesystem WHEN VISUAL STUDIO IS UPDATED !! //
namespace fs = std::experimental::filesystem;

namespace chrono = std::chrono;
typedef std::chrono::high_resolution_clock hrc;

// Represents a desired action
enum class ProgramAction
{
	ExecuteConsole,
	Assemble, Link
};

// An extension of assembly and link error codes for use specifically in <see cref="Main(string[])"/>
enum AsmLnkErrorExt
{
	FailOpen = 100,
	NullPath,
	InvalidPath,
	DirectoryNotFound,
	AccessViolation,
	FileNotFound,
	PathFormatUnsupported,
	IOError,
	FormatError,

	UnknownError = 199
};

// ---------------------------------

// converts time in nanoseconds to a more convenient human form
std::string FormatTime(long long ns)
{
	long long hr, min;
	long double sec;

	hr = ns / 3600000000000;
	ns %= 3600000000000;

	min = ns / 60000000000;
	ns %= 60000000000;

	sec = ns / (long double)1e9;

	// print result
	std::ostringstream ostr;
	ostr << hr << ':' << min << ':' << sec;
	return ostr.str();
}

// ---------------------------------

// The return value to use in the case of error during execution
const int ExecErrorReturnCode = -1;

const char *HelpMessage =
R"(
usage: csx[<options>][--] <pathspec>...
- h, --help             shows help info
- a, --assemble         assembe files into object files
- l, --link             link object files into an executable
--entry <entry>         main entry point for linker
- o, --out <pathspec>   specifies explicit output path
--fs                    sets the file system flag
--time                  after execution, display elapsed time
--end                   remaining args are pathspec

if no - g / -a / -l provided, executes a console program

report bugs to https://github.com/dragazo/CSX64-cpp/issues
)";

// adds standard symbols to the assembler predefine table
void AddPredefines()
{
	// -- syscall codes -- //

	DefineSymbol("sys_exit", (u64)SyscallCode::sys_exit);

	DefineSymbol("sys_read", (u64)SyscallCode::sys_read);
	DefineSymbol("sys_write", (u64)SyscallCode::sys_write);
	DefineSymbol("sys_open", (u64)SyscallCode::sys_open);
	DefineSymbol("sys_close", (u64)SyscallCode::sys_close);
	DefineSymbol("sys_lseek", (u64)SyscallCode::sys_lseek);

	DefineSymbol("sys_brk", (u64)SyscallCode::sys_brk);

	DefineSymbol("sys_rename", (u64)SyscallCode::sys_rename);
	DefineSymbol("sys_unlink", (u64)SyscallCode::sys_unlink);
	DefineSymbol("sys_mkdir", (u64)SyscallCode::sys_mkdir);
	DefineSymbol("sys_rmdir", (u64)SyscallCode::sys_rmdir);

	// -- error codes -- //

	DefineSymbol("err_none", (u64)ErrorCode::None);
	DefineSymbol("err_outofbounds", (u64)ErrorCode::OutOfBounds);
	DefineSymbol("err_unhandledsyscall", (u64)ErrorCode::UnhandledSyscall);
	DefineSymbol("err_undefinedbehavior", (u64)ErrorCode::UndefinedBehavior);
	DefineSymbol("err_arithmeticerror", (u64)ErrorCode::ArithmeticError);
	DefineSymbol("err_abort", (u64)ErrorCode::Abort);
	DefineSymbol("err_iofailure", (u64)ErrorCode::IOFailure);
	DefineSymbol("err_fsdisabled", (u64)ErrorCode::FSDisabled);
	DefineSymbol("err_accessviolation", (u64)ErrorCode::AccessViolation);
	DefineSymbol("err_insufficientfds", (u64)ErrorCode::InsufficientFDs);
	DefineSymbol("err_fdnotinuse", (u64)ErrorCode::FDNotInUse);
	DefineSymbol("err_notimplemented", (u64)ErrorCode::NotImplemented);
	DefineSymbol("err_stackoverflow", (u64)ErrorCode::StackOverflow);
	DefineSymbol("err_fpustackoverflow", (u64)ErrorCode::FPUStackOverflow);
	DefineSymbol("err_fpustackunderflow", (u64)ErrorCode::FPUStackUnderflow);
	DefineSymbol("err_fpuerror", (u64)ErrorCode::FPUError);
	DefineSymbol("err_fpuaccessviolation", (u64)ErrorCode::FPUAccessViolation);
	DefineSymbol("err_alignmentviolation", (u64)ErrorCode::AlignmentViolation);
	DefineSymbol("err_unknownop", (u64)ErrorCode::UnknownOp);

	// -- file open modes -- //

	DefineSymbol("O_RDONLY", (u64)OpenFlags::read);
	DefineSymbol("O_WRONLY", (u64)OpenFlags::write);
	DefineSymbol("O_RDWR", (u64)OpenFlags::read_write);

	DefineSymbol("O_CREAT", (u64)OpenFlags::create);
	DefineSymbol("O_TMPFILE", (u64)OpenFlags::temp);
	DefineSymbol("O_TRUNC", (u64)OpenFlags::trunc);

	DefineSymbol("O_APPEND", (u64)OpenFlags::append);

	// -- file seek modes -- //

	DefineSymbol("SEEK_SET", (u64)SeekMode::set);
	DefineSymbol("SEEK_CUR", (u64)SeekMode::cur);
	DefineSymbol("SEEK_END", (u64)SeekMode::end);
}

// -- file io -- //

// Saves binary data to a file. Returns true if there were no errors
// path - the file to save to
// exe  - the binary data to save
// len  - the length of the executable
int SaveBinaryFile(const std::string &path, const std::vector<u8> &exe)
{
	std::ofstream f(path, std::ios::binary);
	if (!f) { std::cout << "Failed to open \"" << path << "\" for writing\n"; return FailOpen; }

	// write the data
	f.write((char*)exe.data(), exe.size());

	if (!f) { std::cout << "Failed to write to \"" << path << "\"\n"; return IOError; }
	return 0;
}
// Loads the contents of a binary file. Returns true if there were no errors
// path - the file to read
// exe  - the resulting binary data
int LoadBinaryFile(const std::string &path, std::vector<u8> &exe)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) { std::cout << "Failed to open \"" << path << "\"\n"; return FailOpen; }

	// get length and go back to start
	exe.clear();
	exe.resize(f.tellg());
	f.seekg(0);

	// read the contents
	f.read((char*)exe.data(), exe.size());
	if (f.gcount() != exe.size()) { std::cout << "IO error while reading from " << path; return IOError; }

	if (!f) { std::cout << "Failed to read from \"" << path << "\"\n"; return IOError; }
	return 0;
}

/// <summary>
/// Serializes an object file to a file. Returns true if there were no errors
/// </summary>
/// <param name="path">the destination file to save to
/// <param name="obj">the object file to serialize
int SaveObjectFile(const std::string &path, const ObjectFile &obj)
{
	std::ofstream f(path, std::ios::binary);
	if (!f) { std::cout << "Failed to open \"" << path << "\"\n"; return FailOpen; }

	ObjectFile::WriteTo(f, obj);

	if (!f) { std::cout << "Failed to write to \"" << path << "\"\n"; return IOError; }
	return 0;
}
/// <summary>
/// Deserializes an object file from a file. Returns true if there were no errors
/// </summary>
/// <param name="path">the source file to read from
/// <param name="obj">the resulting object file
int LoadObjectFile(const std::string &path, ObjectFile &obj)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { std::cout << "Failed to open \"" << path << "\"\n"; return FailOpen; }

	ObjectFile::ReadFrom(f, obj);

	if (!f) { std::cout << "Failed to write to \"" << path << "\"\n"; return IOError; }
	return 0;
}

// Loads an object file and adds it to the list
// objs - the list of object files
// path - the file to to load
int LoadObjectFile(std::vector<ObjectFile> &objs, const std::string &path)
{
	ObjectFile obj;
	int ret = LoadObjectFile(path, obj);
	if (ret != 0) return ret;

	objs.emplace_back(std::move(obj));

	return 0;
}
// Loads the .o object files from a directory and adds them to the list
// objs - the list of object files
// path - the directory to load
int LoadObjectFileDir(std::vector<ObjectFile> &objs, const std::string &path)
{
	std::error_code err;
	fs::file_status stat = fs::status(path, err);
		
	if (!fs::exists(stat) || !fs::is_directory(stat))
	{
		std::cout << "no directory found: " << path << '\n';
		return (int)AsmLnkErrorExt::DirectoryNotFound;
	}

	ObjectFile obj;
	std::string f_path;
	for (auto &entry : fs::directory_iterator(path, err))
	{
		f_path = entry.path().string();
		if (EndsWith(ToUpper(f_path), ".O"))
		{
			int ret = LoadObjectFile(f_path, obj);
			if (ret != 0) return ret;

			objs.emplace_back(std::move(obj));
		}
	}

	return 0;
}

// -- assembly / linking -- //

/// <summary>
/// Assembles the (from) file into an object file and saves it to the (to) file. Returns true if successful
/// </summary>
/// <param name="from">source assembly file
/// <param name="to">destination for resulting object file
int Assemble(const std::string &from, const std::string &to)
{
	// open the file
	std::ifstream f(from);
	if (!f) { std::cout << "Failed to open \"" << from << "\"\n"; return FailOpen; }

	// assemble the program
	ObjectFile obj;
	AssembleResult res = Assemble(f, obj);
		
	// if there was no error
	if (res.Error == AssembleError::None)
	{
		// save result
		return SaveObjectFile(to, obj);
	}
	// otherwise show error message
	else { std::cout << "Assemble Error in " << from << ":\n" << res.ErrorMsg << '\n'; return (int)res.Error; }
}
/// <summary>
/// Assembles the (from) file into an object file and saves it as the same name but with a .o extension
/// </summary>
/// <param name="from">the source assembly file
int Assemble(const std::string &from)
{
	fs::path _path(from);
	_path.replace_extension(".o");
	return Assemble(from, _path.string());
}

/// <summary>
/// Links several object files to create an executable and saves it to the (to) file. Returns true if successful
/// </summary>
/// <param name="paths">the object files to link</param>
/// <param name="to">destination for the resulting executable</param>
/// <param name="entry_point">the main entry point</param>
/// <param name="rootdir">the root directory to use for core file lookup - null for default</param>
int Link(std::vector<std::string> &paths, const std::string &to, const std::string &entry_point, const char *rootdir)
{
	std::vector<ObjectFile> objs;

	// get exe directory - default to provided root dir if present
	const char *dir = rootdir ? rootdir : exe_dir();

	// if that failed, error
	if (!dir)
	{
		// print the error message and a hint of how to fix it
		std::cerr <<
R"(
Uhoh! Looks like I don't have full support for your system.
I couldn't find your install directory.
You can specify it explicitly with --rootdir <pathspec> to bypass this issue.
)";

		// return error code
		return -1;
	}

	// load the _start file
	int ret = LoadObjectFile(objs, dir + (std::string)"/_start.o");
	if (ret != 0) return ret;

	// load the stdlib files
	ret = LoadObjectFileDir(objs, dir + (std::string)"/stdlib");
	if (ret != 0) return ret;

	// load the user-defined pathspecs
	for(const std::string &path : paths)
	{
		ret = LoadObjectFile(objs, path);
		if (ret != 0) return ret;
	}

	// link the object files
	std::vector<u8> exe;
	LinkResult res = Link(exe, objs, entry_point);

	// if there was no error
	if (res.Error == LinkError::None)
	{
		// save result
		return SaveBinaryFile(to, exe);
	}
	// otherwise show error message
	else { std::cout << "Link Error:\n" << res.ErrorMsg << '\n'; return (int)res.Error; }
}

// -- execution -- //

/// <summary>
/// Executes a program via the console client. returns true if there were no errors
/// </summary>
/// <param name="exe">the code to execute
/// <param name="args">the command line arguments for the client program
int RunConsole(std::vector<u8> &exe, std::vector<std::string> &args, bool fsf, bool time)
{
	// create the computer
	Computer computer;

	// initialize program
	computer.Initialize(exe, args);

	// set private flags
	computer.FSF() = fsf;

	// this usage is just going for raw speed, so enable OTRF
	computer.OTRF() = true;

	// tie standard streams - stdin is non-interactive because we don't control it
	computer.GetFD(0).Open(static_cast<std::iostream*>(&std::cin), false, false);
	computer.GetFD(1).Open(static_cast<std::iostream*>(&std::cout), false, false);
	computer.GetFD(2).Open(static_cast<std::iostream*>(&std::cerr), false, false);

	// begin execution
	hrc::time_point start, stop;
	start = hrc::now();
	while (computer.Running()) computer.Tick(~(u64)0);
	stop = hrc::now();

	// if there was an error
	if (computer.Error() != ErrorCode::None)
	{
		// print error message
		std::cout << "\n\nError Encountered: " << (int)computer.Error() << '\n';
		// return execution error code
		return ExecErrorReturnCode;
	}
	// otherwise no error
	else
	{
		// print elapsed time
		if (time) std::cout << "\n\nElapsed Time: " << FormatTime(chrono::duration_cast<chrono::nanoseconds>(stop - start).count()) << '\n';
		// use return value
		return computer.ReturnValue();
	}
}

/// <summary>
/// Executes a program via the console client. returns true if there were no errors
/// </summary>
/// <param name="path">the file to execute
/// <param name="args">the command line arguments for the client program
int RunConsole(const std::string &path, std::vector<std::string> &args, bool fsf, bool time)
{
	// read the binary data
	std::vector<u8> exe;
	int ret = LoadBinaryFile(path, exe);
	if (ret != 0) return ret;

	// run as a console client and return success flag
	return RunConsole(exe, args, fsf, time);
}

// -- main -- //

int main(int argc, const char *argv[])
{
	using namespace CSX64;

	ProgramAction action = ProgramAction::ExecuteConsole; // requested action
	std::vector<std::string> pathspec;                    // input paths
	const char *entry_point = nullptr;                    // main entry point for linker
	const char *output = nullptr;                         // output path
	const char *rootdir = nullptr;                        // root directory to use for std lookup
	bool fsf = false;                                     // fsf flag
	bool time = false;                                    // time flag
	bool accepting_options = true;                        // marks that we're still accepting options

	// process the terminal args
	for (int i = 1; i < argc; ++i)
	{
		// if we're still accepting options
		if (accepting_options)
		{
			// do the long names first
			if (strcmp(argv[i], "--help") == 0) { std::system("\"https://github.com/dragazo/CSX64/blob/master/CSX64 Specification.pdf\""); return 0; }
			else if (strcmp(argv[i], "--assemble") == 0) { if (action != ProgramAction::ExecuteConsole) { std::cout << "usage error - see -h for help\n"; return 0; } action = ProgramAction::Assemble; }
			else if (strcmp(argv[i], "--link") == 0) { if (action != ProgramAction::ExecuteConsole) { std::cout << "usage error - see -h for help\n"; return 0; } action = ProgramAction::Link; }
			else if (strcmp(argv[i], "--output") == 0) { if (output != nullptr || i + 1 >= argc) { std::cout << "usage error - see -h for help\n"; return 0; } output = argv[++i]; }
			else if (strcmp(argv[i], "--entry") == 0) { if (entry_point != nullptr || i + 1 >= argc) { std::cout << "usage error - see -h for help\n"; return 0; } entry_point = argv[++i]; }
			else if (strcmp(argv[i], "--rootdir") == 0) { if (rootdir != nullptr || i + 1 >= argc) { std::cout << "usage error - see -h for help\n"; return 0; } rootdir = argv[++i]; }
			else if (strcmp(argv[i], "--end") == 0) { accepting_options = false; }
			else if (strcmp(argv[i], "--fs") == 0) { fsf = true; }
			else if (strcmp(argv[i], "--time") == 0) { time = true; }
			// then the short names
			else if (argv[i][0] == '-')
			{
				for (const char *arg = argv[i] + 1; *arg; ++arg)
				{
					switch (*arg)
					{
					case '-': break; // acts as a separator for short options and enables -- as a no-op arg spacer
					case 'h': std::cout << HelpMessage; return 0;
					case 'a': if (action != ProgramAction::ExecuteConsole) { std::cout << "usage error - see -h for help\n"; return 0; } action = ProgramAction::Assemble; break;
					case 'l': if (action != ProgramAction::ExecuteConsole) { std::cout << "usage error - see -h for help\n"; return 0; } action = ProgramAction::Link; break;
					case 'o': if (output != nullptr || i + 1 >= argc) { std::cout << "usage error - see -h for help\n"; return 0; } output = argv[++i]; break;

					default: std::cout << "unknown option '" << *arg << "' see -h for help\n"; return 0;
					}
				}
			}
			// otherwise it's part of pathspec
			else pathspec.emplace_back(argv[i]);
		}
		// if we're not accepting options, it's part of pathspec
		else pathspec.emplace_back(argv[i]);
	}	

	// perform the action
	switch (action)
	{
	case ProgramAction::ExecuteConsole:
		if (pathspec.empty()) { std::cout << "Expected a file to execute\n"; return 0; }
		return RunConsole(pathspec[0], pathspec, fsf, time);

	case ProgramAction::Assemble:
		if (pathspec.empty()) { std::cout << "Assembler expected at least 1 file to assemble\n"; return 0; }
		
		// add the assembler predefines now
		AddPredefines();
		
		if (output == nullptr) // if no output is provided, batch process each pathspec
		{
			for (const std::string &path : pathspec)
			{
				int res = Assemble(path);
				if (res != 0) return res;
			}
			return 0;
		}
		else // otherwise, we're expecting only one input with a named output
		{
			if (pathspec.size() != 1) { std::cout << "Assembler with an explicit output expected only one input\n"; return 0; }
			return Assemble(pathspec[0], output);
		}

	case ProgramAction::Link:
		if (pathspec.empty()) { std::cout << ("Linker expected at least 1 file to link\n"); return 0; }

		return Link(pathspec, output ? output : "a.exe", entry_point ? entry_point : "main", rootdir);
	}

	return 0;
}
