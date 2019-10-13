#include <iostream>
#include <fstream>
#include <utility>
#include <vector>
#include <string>
#include <cstdlib>
#include <chrono>
#include <unordered_map>
#include <experimental/filesystem>

#include "include/CoreTypes.h"
#include "include/Computer.h"
#include "include/Assembly.h"
#include "include/Utility.h"

using namespace CSX64;

namespace fs = std::experimental::filesystem;

// Represents a desired action
enum class ProgramAction
{
	ExecuteConsole, // takes 1 csx64 executable + args -- executes as a console application

	ExecuteConsoleScript, // takes 1 csx64 assembly file + args -- compiles, links, and executes in memory as a console application
	ExecuteConsoleMultiscript, // takes 1+ csx64 assembly files -- compiles, links, and executes in memory as a console application

	Assemble, // assembles 1+ csx64 assembly files into csx64 object files
	Link,     // links 1+ csx64 object files into a csx64 executable
};

// An extension of assembly and link error codes for use specifically in
enum class AsmLnkErrorExt
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

	MemoryAllocError = 197,
	ComputerInitError = 198,

	UnknownError = 199
};

// ---------------------------------

// on success, returns a (non-null) C-style string - path to the directory containing this executable.
// on failure, returns null.
const char *exe_dir();

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
R"(Usage: csx [OPTION]... [ARG]...
Assemble, link, or execute CSX64 files.

  -h, --help                print this help page and exit

  -a, --assemble            assemble CSX64 asm files into CSX64 obj files
  -l, --link                link CSX64 asm/obj files into a CSX64 executable
  -s, --script              assemble, link, and execute a CSX64 asm/obj file in memory
  -S, --multiscript         as --script, but takes multiple CSX64 asm/obj files
  otherwise                 execute a CSX64 executable with provided args

  -o, --out <path>          specify an explicit output path
      --entry <entry>       main entry point for linker
      --rootdir <dir>       specify an explicit rootdir (contains _start.o and stdlib/*.o)

      --fs                  sets the file system flag during execution
  -u, --unsafe              sets all unsafe flags during execution (those in this section)

  -t, --time                after execution display elapsed time
      --                    remaining args are not csx64 options (added to arg list)

Report bugs to: https://github.com/dragazo/CSX64-cpp/issues
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
	DefineSymbol("err_filepermissions", (u64)ErrorCode::FilePermissions);

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

// ------------------ //

// -- exeutable io -- //

// ------------------ //

// Saves a csx64 executable to a file (no format checking).
// path - the file to save to.
// exe  - the csx64 executable to save.
int SaveExecutable(const std::string &path, const Executable &exe)
{
	try
	{
		exe.save(path);
		return 0;
	}
	catch (const FileOpenError&)
	{
		std::cerr << "Failed to open " << path << " for writing\n";
		return (int)AsmLnkErrorExt::FailOpen;
	}
	catch (const IOError&)
	{
		std::cerr << "An IO error occurred while saving executable to " << path << '\n';
		return (int)AsmLnkErrorExt::IOError;
	}
}
// Loads a csx64 executable from a file (no format checking).
// path - the file to read.
// exe  - the resulting csx64 executable (on success).
int LoadExecutable(const std::string &path, Executable &exe)
{
	try
	{
		exe.load(path);
		return 0;
	}
	catch (const FileOpenError&)
	{
		std::cerr << "Failed to open " << path << " for reading\n";
		return (int)AsmLnkErrorExt::FailOpen;
	}
	catch (const TypeError&)
	{
		std::cerr << path << " is not a CSX64 executable\n";
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const VersionError&)
	{
		std::cerr << "Executable " << path << " is of an incompatible version of CSX64\n";
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const FormatError&)
	{
		std::cerr << "Executable " << path << " is of an unrecognized format\n";
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "Failed to allocate space for executable\n";
		return (int)AsmLnkErrorExt::MemoryAllocError;
	}
}

// -------------------- //

// -- object file io -- //

// -------------------- //

// Saves an object file to a file.
// path - the destination file to save to.
// obj  - the object file to save.
int SaveObjectFile(const std::string &path, const ObjectFile &obj)
{
	try
	{
		obj.save(path);
		return 0;
	}
	catch (const FileOpenError&)
	{
		std::cerr << "Failed to open " << path << " for writing\n";
		return (int)AsmLnkErrorExt::FailOpen;
	}
	catch (const IOError&)
	{
		std::cerr << "An IO error occurred while saving object file to " << path << '\n';
		return (int)AsmLnkErrorExt::IOError;
	}
}
// Loads an object file from a file.
// path - the source file to read from.
// obj  - the resulting object file (on success).
int LoadObjectFile(const std::string &path, ObjectFile &obj)
{
	try
	{
		obj.load(path);
		return 0;
	}
	catch (const FileOpenError&)
	{
		std::cerr << "Failed to open " << path << " for reading\n";
		return (int)AsmLnkErrorExt::FailOpen;
	}
	catch (const TypeError&)
	{
		std::cerr << path << " is not a CSX64 object file\n";
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const VersionError&)
	{
		std::cerr << "Object file " << path << " is of an incompatible version of CSX64\n";
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const FormatError&)
	{
		std::cerr << "Object file " << path << " is of an unrecognized format\n";
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "Failed to allocate space for object file\n";
		return (int)AsmLnkErrorExt::MemoryAllocError;
	}
}

// Loads the .o (obj) files from a directory and adds them to the list.
// objs - the resulting list of object files (new entries appended to the end).
// path - the directory to load.
int LoadObjectFileDir(std::vector<ObjectFile> &objs, const std::string &path)
{
	std::error_code err;
	fs::file_status stat = fs::status(path, err);

	// make sure path exists and is a directory
	if (!fs::exists(stat)) { std::cerr << path << " does not exist\n"; return (int)AsmLnkErrorExt::DirectoryNotFound; }
	if (!fs::is_directory(stat)) { std::cerr << path << " is not a directory\n"; return (int)AsmLnkErrorExt::DirectoryNotFound; }

	// for every item in the directory: if it ends with ".o" load it as an object file
	for (const auto &entry : fs::directory_iterator(path, err))
	{
		std::string f_path = entry.path().string();
		if (EndsWith(f_path, ".o"))
		{
			int ret = LoadObjectFile(f_path, objs.emplace_back());
			if (ret != 0) return ret;
		}
	}

	return 0;
}

// -------------- //

// -- assembly -- //

// -------------- //

// assembles assembly source code from a file (file) into an object file (dest).
// file - the assembly source file.
// dest - the resulting object file (on success).
int Assemble(const std::string &file, ObjectFile &dest)
{
	// open the file for reading - make sure it succeeds
	std::ifstream source(file);
	if (!source) { std::cerr << "Failed to open " << file << " for reading\n"; return (int)AsmLnkErrorExt::FailOpen; }

	// assemble the source
	AssembleResult res = CSX64::Assemble(source, dest);

	// if there was an error, show error message
	if (res.Error != AssembleError::None)
	{
		std::cerr << "Assemble Error in " << file << ":\n" << res.ErrorMsg << '\n';
		return (int)res.Error;
	}

	return 0;
}

// ------------- //

// -- linking -- //

// ------------- //

// Loads the stdlib object files and appends them to the end of the list.
// objs    - the destination for storing loaded stdlib object files (appended to end).
// rootdir - the root directory to use for core file lookup - null for default.
int LoadStdlibObjs(std::vector<ObjectFile> &objs, const char *rootdir)
{
	// get exe directory - default to provided root dir if present
	const char *dir = rootdir ? rootdir : exe_dir();
	
	// if that failed, error
	if (!dir)
	{
		std::cerr <<
			"Uhoh! Apparently CSX64 doesn't have full support for your system.\n"
			"Error:  Could not locate root directory.\n"
			"Bypass: Specify explicitly with --rootdir <pathspec>.\n\n"
			"Please also post an issue along with your system information to\n"
			"    https://github.com/dragazo/CSX64-cpp/issues.\n\n";

		return -1;
	}

	// load the _start file
	int ret = LoadObjectFile(dir + (std::string)"/_start.o", objs.emplace_back());
	if (ret != 0) return ret;

	// then load the stdlib files
	return LoadObjectFileDir(objs, dir + (std::string)"/stdlib");
}

// Links several files to create an executable (stored to dest).
// dest        - the resulting executable (on success).
// files       - the files to link. ".o" files are loaded as object files, otherwise treated as assembly source and assembled.
// entry_point - the main entry point.
// rootdir     - the root directory to use for core file lookup - null for default.
int Link(Executable &dest, const std::vector<std::string> &files, const std::string &entry_point, const char *rootdir)
{
	std::vector<ObjectFile> objs;

	// load the stdlib files
	int ret = LoadStdlibObjs(objs, rootdir);
	if (ret != 0) return ret;

	// load the provided files
	for (const std::string &file : files)
	{
		// treat ".o" as object file, otherwise as assembly source
		ret = EndsWith(file, ".o") ? LoadObjectFile(file, objs.emplace_back()) : Assemble(file, objs.emplace_back());
		if (ret != 0) return ret;
	}

	// link the resulting object files into an executable
	LinkResult res = CSX64::Link(dest, objs, entry_point);

	// if there was an error, show error message
	if (res.Error != LinkError::None)
	{
		std::cerr << "Link Error:\n" << res.ErrorMsg << '\n';
		return (int)res.Error;
	}

	return 0;
}

// --------------- //

// -- execution -- //

// --------------- //

// Executes a console program. Return value is either client program exit code or a csx64 execution error code (delineated in stderr).
// exe  - the client program to execute
// args - command line args for the client program.
// fsf  - value of FSF (file system flag) during client program execution.
// time - marks if the execution time should be measured.
int RunConsole(const Executable &exe, const std::vector<std::string> &args, bool fsf, bool time)
{
	// create the computer
	Computer computer;

	// for this usage, remove max memory restrictions
	computer.MaxMemory(~(u64)0);

	try
	{
		// initialize program
		computer.Initialize(exe, args);
	}
	catch (const FormatError &ex)
	{
		std::cerr << ex.what() << '\n';
		return (int)AsmLnkErrorExt::FormatError;
	}
	catch (const MemoryAllocException &ex)
	{
		std::cerr << ex.what() << '\n';
		return (int)AsmLnkErrorExt::MemoryAllocError;
	}

	// set private flags
	computer.FSF() = fsf;

	// this usage is just going for raw speed, so enable OTRF
	computer.OTRF() = true;

	// tie standard streams - stdin is non-interactive because we don't control it
	computer.OpenFileWrapper(0, std::make_unique<TerminalInputFileWrapper>(&std::cin, false, false));
	computer.OpenFileWrapper(1, std::make_unique<TerminalOutputFileWrapper>(&std::cout, false, false));
	computer.OpenFileWrapper(2, std::make_unique<TerminalOutputFileWrapper>(&std::cerr, false, false));

	// begin execution
	auto start = std::chrono::high_resolution_clock::now();
	while (computer.Running()) computer.Tick(~(u64)0);
	auto stop = std::chrono::high_resolution_clock::now();

	// if there was an error
	if (computer.Error() != ErrorCode::None)
	{
		std::cerr << "\n\nError Encountered: (" << (int)computer.Error() << ") " << ErrorCodeToString.at(computer.Error()) << '\n';
		return ExecErrorReturnCode;
	}
	// otherwise no error
	else
	{
		if (time) std::cout << "\n\nElapsed Time: " << FormatTime(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count()) << '\n';
		return computer.ReturnValue();
	}
}

// -------------------------- //

// -- cmd line arg parsing -- //

// -------------------------- //

// holds all the information needed to parse command line options for main().
struct cmdln_pack
{
	ProgramAction action = ProgramAction::ExecuteConsole; // requested action
	std::vector<std::string> pathspec;                    // input paths
	const char *entry_point = nullptr;                    // main entry point for linker
	const char *output = nullptr;                         // output path
	const char *rootdir = nullptr;                        // root directory to use for std lookup
	bool fsf = false;                                     // fsf flag
	bool time = false;                                    // time flag
	bool accepting_options = true;                        // marks that we're still accepting options

	// these are parsing helpers - ignore
	int argc, i;
	const char *const *argv;

	// parses the provided command line args and fills out this structure with the results.
	// returns true on success. failure returns false after printing an error message to stderr.
	// MUST BE CALLED ON A FRESHLY-CONSTRUCTED PACK.
	bool parse(int _argc, const char *const *_argv);
};

bool _help(cmdln_pack&) { std::cout << HelpMessage; return false; }

bool _assemble(cmdln_pack &p)
{
	if (p.action != ProgramAction::ExecuteConsole) { std::cerr << p.argv[p.i] << ": Already specified mode\n"; return false; }
	
	p.action = ProgramAction::Assemble;
	return true;
}
bool _link(cmdln_pack &p)
{
	if (p.action != ProgramAction::ExecuteConsole) { std::cerr << p.argv[p.i] << ": Already specified mode\n"; return false; }
	
	p.action = ProgramAction::Link;
	return true;
}
bool _script(cmdln_pack &p)
{
	if (p.action != ProgramAction::ExecuteConsole) { std::cerr << p.argv[p.i] << ": Already specified mode\n"; return false; }
	
	p.action = ProgramAction::ExecuteConsoleScript;
	return true;
}
bool _multiscript(cmdln_pack &p)
{
	if (p.action != ProgramAction::ExecuteConsole) { std::cerr << p.argv[p.i] << ": Already specified mode\n"; return false; }

	p.action = ProgramAction::ExecuteConsoleMultiscript;
	return true;
}

bool _out(cmdln_pack &p)
{
	if (p.output != nullptr) { std::cerr << p.argv[p.i] << ": Already specified output path\n"; return false; }
	if (p.i + 1 >= p.argc) { std::cerr << p.argv[p.i] << ": Expected output path\n"; return false; }

	p.output = p.argv[++p.i];
	return true;
}
bool _entry(cmdln_pack &p)
{
	if (p.entry_point != nullptr) { std::cerr << p.argv[p.i] << ": Already specified entry point\n"; return false; }
	if (p.i + 1 >= p.argc) { std::cerr << p.argv[p.i] << ": Expected entry point\n"; return false; }

	p.entry_point = p.argv[++p.i];
	return true;
}
bool _rootdir(cmdln_pack &p)
{
	if (p.rootdir != nullptr) { std::cerr << p.argv[p.i] << ": Already specified root directory\n"; return false; }
	if (p.i + 1 >= p.argc) { std::cerr << p.argv[p.i] << ": Expected root directory\n"; return false; }

	p.rootdir = p.argv[++p.i];
	return true;
}

bool _fs(cmdln_pack &p) { p.fsf = true; return true; }
bool _time(cmdln_pack &p) { p.time = true; return true; }
bool _end(cmdln_pack &p) { p.accepting_options = false; return true; }
bool _unsafe(cmdln_pack &p) { p.fsf = true; return true; }

// maps (long) options to their parsing handlers
const std::unordered_map<std::string, bool(*)(cmdln_pack&)> long_names
{
	{ "--help", _help },

{ "--assemble", _assemble },
{ "--link", _link },
{ "--script", _script },
{ "--multiscript", _multiscript },

{ "--output", _out },
{ "--entry", _entry },
{ "--rootdir", _rootdir },

{ "--fs", _fs },
{ "--unsafe", _unsafe },

{ "--time", _time },
{ "--", _end },
};
// maps (short) options to their parsing handlers
const std::unordered_map<char, bool(*)(cmdln_pack&)> short_names
{
{ 'h', _help },

{ 'a', _assemble },
{ 'l', _link },
{ 's', _script },
{ 'S', _multiscript },

{ 'o', _out },

{ 'u', _unsafe },

{ 't', _time },
};

bool cmdln_pack::parse(int _argc, const char *const *_argv)
{
	// set up argc/argv for the parsing pack
	argc = _argc;
	argv = _argv;

	// for each arg (store index in this->i for handlers)
	for (i = 1; i < argc; ++i)
	{
		if (accepting_options)
		{
			auto it_long_handler = long_names.find(argv[i]);

			// if we found a handler, call it
			if (it_long_handler != long_names.end()) { if (!it_long_handler->second(*this)) return false; }
			// otherwise if it starts with a '-' it's a list of short options
			else if (argv[i][0] == '-')
			{
				// hold on to the current arg (some options can change i)
				const char *arg = argv[i];
				
				for (const char *p = argv[i] + 1; *p; ++p)
				{
					auto it_short_handler = short_names.find(*p);

					// if we found a handler, call it
					if (it_short_handler != short_names.end()) { if (!it_short_handler->second(*this)) return false; }
					// otherwise it's an unknown option
					else { std::cerr << arg << ": Unknown option '" << *p << "'\n"; return false; }
				}
			}
			// otherwise it's a pathspec
			else pathspec.emplace_back(argv[i]);
		}
		else pathspec.emplace_back(argv[i]);
	}

	return true;
}

// -------------------- //

// -- main interface -- //

// -------------------- //

int main(int argc, char *argv[]) try
{
	// we make a lot of assumptions about the current platform being little-endian (in fact some code still artificially simulates it)
	// however, the majority isn't simulated since that'd be ridiculously-slow - so we need to make sure this system is really little-endian.

	if (!IsLittleEndian())
	{
		std::cerr <<
			"Uhoh!! Looks like this platform isn't little-endian!\n"
			"Most everything in CSX64 assumes little-endian,\n"
			"so most of it won't work on this system!\n\n";
		return -1;
	}

	// ------------------------------------ //

	cmdln_pack dat; // command line option parsing pack

	// parse our command line args - on failure a message is printed to stdout, just return 0.
	if (!dat.parse(argc, argv)) return 0;

	// ------------------------------------ //

	// perform the action
	switch (dat.action)
	{
	case ProgramAction::ExecuteConsole:

	{
		if (dat.pathspec.empty()) { std::cerr << "Expected a file to execute\n"; return 0; }

		Executable exe;
		
		int res = LoadExecutable(dat.pathspec[0], exe);
		return res != 0 ? res : RunConsole(exe, dat.pathspec, dat.fsf, dat.time);
	}

	case ProgramAction::ExecuteConsoleScript:

	{
		if (dat.pathspec.empty()) { std::cerr << "Expected a file to assemble, link, and execute\n"; return 0; }

		AddPredefines();
		Executable exe;
		
		int res = Link(exe, { dat.pathspec[0] }, dat.entry_point ? dat.entry_point : "main", dat.rootdir);
		return res != 0 ? res : RunConsole(exe, dat.pathspec, dat.fsf, dat.time);
	}

	case ProgramAction::ExecuteConsoleMultiscript:

	{
		if (dat.pathspec.empty()) { std::cerr << "Expected 1+ files to assemble, link, and execute\n"; return 0; }

		AddPredefines();
		Executable exe;
		
		int res = Link(exe, dat.pathspec, dat.entry_point ? dat.entry_point : "main", dat.rootdir);
		return res != 0 ? res : RunConsole(exe, { "<script>" }, dat.fsf, dat.time);
	}

	case ProgramAction::Assemble:

	{
		if (dat.pathspec.empty()) { std::cerr << "Expected 1+ files to assemble\n"; return 0; }

		AddPredefines();
		ObjectFile obj;

		// if no explicit output is provided, batch process each pathspec
		if (dat.output == nullptr)
		{
			for (const std::string &path : dat.pathspec)
			{
				fs::path dest(path);
				dest.replace_extension(".o");

				int res = Assemble(path, obj);
				if (res != 0) return res;

				res = SaveObjectFile(dest.string(), obj);
				if (res != 0) return res;
			}
			return 0;
		}
		// otherwise, we're expecting only one input with a named output
		else
		{
			if (dat.pathspec.size() != 1) { std::cerr << "Assembler with an explicit output expected only one input\n"; return 0; }

			int res = Assemble(dat.pathspec[0], obj);
			return res != 0 ? res : SaveObjectFile(dat.output, obj);
		}
	}

	case ProgramAction::Link:

	{
		if (dat.pathspec.empty()) { std::cerr << "Linker expected 1+ files to link\n"; return 0; }

		AddPredefines();
		Executable exe;

		int res = Link(exe, dat.pathspec, dat.entry_point ? dat.entry_point : "main", dat.rootdir);
		return res != 0 ? res : SaveExecutable(dat.output ? dat.output : "a.out", exe);
	}

	} // end switch

	return 0;
}
catch (const std::exception &ex)
{
	std::cerr << "UNHANDLED EXCEPTION:\n" << ex.what() << '\n';
	return -666;
}
catch (...)
{
	std::cerr << "UNHANDLED NON-STANDARD EXCEPTION\n";
	return -999;
}
