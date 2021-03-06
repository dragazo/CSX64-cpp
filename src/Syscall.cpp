#include <experimental/filesystem>
#include <memory>
#include <cstdio>
#include <sstream>

#include "../include/Computer.h"

// !! CHANGE THIS ONCE VISUAL STUDIO IS UPDATED !! //
namespace fs = std::experimental::filesystem;

// -- Syscall -- //

namespace CSX64
{
	bool Computer::ProcessSYSCALL()
	{
		switch ((SyscallCode)RAX())
		{
		case SyscallCode::sys_exit: Exit((int)RBX()); return true;

		case SyscallCode::sys_read: return Process_sys_read();
		case SyscallCode::sys_write: return Process_sys_write();
		case SyscallCode::sys_open: return Process_sys_open();
		case SyscallCode::sys_close: return Process_sys_close();
		case SyscallCode::sys_lseek: return Process_sys_lseek();

		case SyscallCode::sys_brk: return Process_sys_brk();

		case SyscallCode::sys_rename: return Process_sys_rename();
		case SyscallCode::sys_unlink: return Process_sys_unlink();
		case SyscallCode::sys_mkdir: return Process_sys_mkdir();
		case SyscallCode::sys_rmdir: return Process_sys_rmdir();

			// otherwise syscall not found
		default: Terminate(ErrorCode::UnhandledSyscall); return false;
		}
	}

	bool Computer::Process_sys_read()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		IFileWrapper *const fd = FileDescriptors[fd_index].get();
		if (fd == nullptr) { Terminate(ErrorCode::FDNotInUse); return false; }

		// make sure we can read from it
		if (!fd->CanRead()) { Terminate(ErrorCode::FilePermissions); return false; }

		// make sure we're in bounds
		if (RCX() >= mem_size || RDX() >= mem_size || RCX() + RDX() > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
		// make sure we're not in the readonly segment
		if (RCX() < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

		// read from the file
		try
		{
			i64 n = fd->Read(reinterpret_cast<char*>(mem) + RCX(), (i64)RDX()); // aliasing is ok because casting to char type

			// if we got nothing but the wrapper is interactive
			if (n == 0 && fd->IsInteractive())
			{
				--RIP();               // await further data by repeating the syscall
				suspended_read = true; // suspend execution until there's more data
			}
			// otherwise success - return num chars read from file
			else RAX() = (u64)n;
		}
		// errors are failures - return -1
		catch (...) { RAX() = ~(u64)0; }

		return true;
	}
	bool Computer::Process_sys_write()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		IFileWrapper *const fd = FileDescriptors[fd_index].get();
		if (fd == nullptr) { Terminate(ErrorCode::FDNotInUse); return false; }

		// make sure we can write
		if (!fd->CanWrite()) { Terminate(ErrorCode::FilePermissions); return false; }

		// make sure we're in bounds
		if (RCX() >= mem_size || RDX() >= mem_size || RCX() + RDX() > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

		// attempt to write from memory to the file - success = num written, fail = -1
		try { RAX() = (u64)fd->Write(reinterpret_cast<char*>(mem) + RCX(), (i64)RDX()) ? RDX() : ~(u64)0; } // aliasing is ok because casting to char type
		catch (...) { RAX() = ~(u64)0; }
		
		return true;
	}

	bool Computer::Process_sys_open()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get an available file descriptor
		int fd_index = FindAvailableFD();
		// if we couldn't get one, return -1
		if (fd_index < 0) { RAX() = ~(u64)0; return true; }

		// get the file descriptor handle
		std::unique_ptr<IFileWrapper> &fd = FileDescriptors[fd_index];

		// get path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		int                raw_flags = (int)RCX();       // flags provided by user
		std::ios::openmode cpp_flags = std::ios::binary; // flags to provide to C++ (always open in binary mode)
		
		// alias permissions flags for convenience
		bool can_read = raw_flags & (int)OpenFlags::read;
		bool can_write = raw_flags & (int)OpenFlags::write;

		// process raw flags
		if (can_read) cpp_flags |= std::ios::in;
		if (can_write) cpp_flags |= std::ios::out;

		if (raw_flags & (int)OpenFlags::trunc) cpp_flags |= std::ios::trunc;
		
		if (raw_flags & (int)OpenFlags::append) cpp_flags |= std::ios::app;
		
		// handle creation mode flags
		if (raw_flags & (int)OpenFlags::temp)
		{
			// get the temp name (std::tmpnam is deprecated so we'll do something less fancy)
			std::string tmp_path;
			std::ostringstream ostr;
			ostr << std::hex << std::setfill('0');

			// get a tmp file path
			do
			{
				ostr.str(""); // make sure ostr is empty
				ostr << path << '/' << std::setw(16) << Rand() << ".tmp";
				tmp_path = ostr.str();
			}
			// repeat while that already exists
			while (std::ifstream {tmp_path});
			
			// update path and create it
			path = std::move(tmp_path);
			std::ofstream {path};
		}
		else if (raw_flags & (int)OpenFlags::create)
		{
			// if the file does not exist
			std::error_code err;
			if (!fs::exists(path, err))
			{
				// create it
				std::ofstream {path};
			}
		}

		// open the file
		auto f = std::make_unique<std::fstream>();
		try { f->open(path, cpp_flags); }
		catch (...) { RAX() = ~(u64)0; return true; }

		// if it didn't open, fail with -1
		if (!*f) { RAX() = ~(u64)0; return true; }

		// store in the file descriptor
		fd = std::make_unique<BasicFileWrapper>(f.get(), true, false, can_read, can_write, true); // provide the wrapper with the file pointer
		f.release(); // only if it succeeded should we release the unique_ptr on said file
		RAX() = fd_index;

		return true;
	}
	bool Computer::Process_sys_close()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		std::unique_ptr<IFileWrapper> &fd = FileDescriptors[fd_index];

		// close the file - success = 0, fail = -1
		fd = nullptr;
		RAX() = 0;
		return true;
	}

	bool Computer::Process_sys_lseek()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		IFileWrapper *const fd = FileDescriptors[fd_index].get();
		if (fd == nullptr) { Terminate(ErrorCode::FDNotInUse); return false; }

		// make sure it can seek
		if (!fd->CanSeek()) { Terminate(ErrorCode::FilePermissions); return false; }

		int               raw_mode = (int)RDX();
		std::ios::seekdir cpp_mode;

		// process raw mode
		if (raw_mode == (int)SeekMode::set) cpp_mode = std::ios::beg;
		else if (raw_mode == (int)SeekMode::cur) cpp_mode = std::ios::cur;
		else if (raw_mode == (int)SeekMode::end) cpp_mode = std::ios::end;
		// otherwise unknown seek mode - return -1
		else { RAX() = ~(u64)0; return true; }

		// attempt the seek
		try { RAX() = (u64)fd->Seek((i64)RCX(), cpp_mode); }
		catch (...) { RAX() = ~(u64)0; }
		
		return true;
	}

	bool Computer::Process_sys_brk()
	{
		// special request of 0 returns current break
		if (RBX() == 0) RAX() = mem_size;
		// if the request is too high or goes below init size, don't do it - return -1
		else if (RBX() > max_mem_size || RBX() < min_mem_size) { RAX() = ~(u64)0; }
		// otherwise perform the reallocation
		else RAX() = this->realloc(RBX(), true) ? 0 : ~(u64)0;

		return true;
	}

	bool Computer::Process_sys_rename()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the paths
		std::string from, to;
		if (!GetCString(RBX(), from) || !GetCString(RCX(), to)) return false;

		// attempt the move operation - success = 0, fail = -1
		try { fs::rename(from, to); RAX() = 0; }
		catch (...) { RAX() = ~(u64)0; }

		return true;
	}
	bool Computer::Process_sys_unlink()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the path
		std::string path;
		if (!GetCString(RBX(), path)) return false;
		
		// attempt the unlink operation - using remove, but it must not be a directory - success = 0, failure = -1
		try { RAX() = !fs::is_directory(path) && fs::remove(path) ? (u64)0 : ~(u64)0; }
		catch (...) { RAX() = ~(u64)0; }

		return true;
	}

	bool Computer::Process_sys_mkdir()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		// attempt the mkdir operation - success = 0, failure = -1
		try { RAX() = fs::create_directory(path) ? (u64)0 : ~(u64)0; }
		catch (...) { RAX() = ~(u64)0; }

		return true;
	}
	bool Computer::Process_sys_rmdir()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		// attempt the rmdir - using remove, but it must be a directory - success = 0, failure = -1
		try { RAX() = fs::is_directory(path) && fs::remove(path) ? (u64)0 : ~(u64)0; }
		catch (...) { RAX() = ~(u64)0; }

		return true;
	}
}
