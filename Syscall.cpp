#include <filesystem>
#include <memory>
#include <cstdio>
#include <sstream>

#include "Computer.h"

// !! CHANGE THIS ONCE VISUAL STUDIO IS UPDATED !! //
namespace fs = std::experimental::filesystem;

// -- Syscall -- //

namespace CSX64
{
	bool Computer::Process_sys_read()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		FileDescriptor &fd = FileDescriptors[fd_index];
		if (!fd.InUse()) { Terminate(ErrorCode::FDNotInUse); return false; }

		// make sure we're in bounds
		if (RCX() >= mem_size || RDX() >= mem_size || RCX() + RDX() > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
		// make sure we're not in the readonly segment
		if (RCX() < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

		// read from the file
		try
		{
			u64 n = smart_readsome(*fd.Stream(), (char*)mem + RCX(), RDX());

			// if stream is bad now, return -1
			if (!*fd.Stream()) RAX() = ~(u64)0;
			// if we got nothing but it's interactive
			else if (n == 0 && fd.Interactive())
			{
				--RIP();              // await further data by repeating the syscall
				suspended_read = true; // suspend execution until there's more data
			}
			// otherwise success - return num chars read from file
			else RAX() = n;
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
		FileDescriptor &fd = FileDescriptors[fd_index];
		if (!fd.InUse()) { Terminate(ErrorCode::FDNotInUse); return false; }

		// make sure we're in bounds
		if (RCX() >= mem_size || RDX() >= mem_size || RCX() + RDX() > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

		// attempt to write from memory to the file - success = num written, fail = -1
		try { RAX() = fd.Stream()->write((char*)mem + RCX(), RDX()) ? RDX() : ~(u64)0; }
		catch (...) { RAX() = ~(u64)0; }

		return true;
	}

	bool Computer::Process_sys_open()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get an available file descriptor
		int fd_index;
		FileDescriptor *fd = FindAvailableFD(&fd_index);

		// if we couldn't get one, return -1
		if (fd == nullptr) { RAX() = ~(u64)0; return true; }

		// get path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		int raw_flags = RCX(); // flags provided by user
		int cpp_flags = 0;     // flags to provide to C++
		
		// process raw flags
		if (raw_flags & (int)OpenFlags::read) cpp_flags |= std::ios::in;
		if (raw_flags & (int)OpenFlags::write) cpp_flags |= std::ios::out;

		if (raw_flags & (int)OpenFlags::trunc) cpp_flags |= std::ios::trunc;
		
		if (raw_flags & (int)OpenFlags::append) cpp_flags |= std::ios::app;
		if (raw_flags & (int)OpenFlags::binary) cpp_flags |= std::ios::binary;
		
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
				ostr << path << '/' << std::setw(16) << Rand64(Rand) << ".tmp";
				tmp_path = ostr.str();
			}
			// repeat while that already exists
			while (std::ifstream(tmp_path));
			
			// update path and create it
			path = std::move(tmp_path);
			std::ofstream(path);
		}
		else if (raw_flags & (int)OpenFlags::create)
		{
			// if file does not exist, create it
			std::ifstream(path) || std::ofstream(path);
		}

		// open the file - held by smart pointer for convenience
		std::unique_ptr<std::fstream> f(new std::fstream);
		try { f->open(path, cpp_flags); }
		catch (...) { RAX() = ~(u64)0; return true; }

		// if it didn't open, fail with -1
		if (!*f) { RAX() = ~(u64)0; return true; }

		// store in the file descriptor (make sure smart pointer is released)
		fd->Open(f.release(), true, false);
		RAX() = fd_index;

		return true;
	}
	bool Computer::Process_sys_close()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		FileDescriptor &fd = FileDescriptors[fd_index];

		// close the file - success = 0, fail = -1
		RAX() = fd.Close() ? (u64)0 : ~(u64)0;
		return true;
	}

	bool Computer::Process_sys_lseek()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		FileDescriptor &fd = FileDescriptors[fd_index];
		if (!fd.InUse()) { Terminate(ErrorCode::FDNotInUse); return false; }

		int raw_mode = RDX();
		int cpp_mode;

		// process raw mode
		if (raw_mode == (int)SeekMode::set) cpp_mode = std::ios::beg;
		else if (raw_mode == (int)SeekMode::cur) cpp_mode = std::ios::cur;
		else if (raw_mode == (int)SeekMode::end) cpp_mode = std::ios::end;
		// otherwise unknown seek mode - return -1
		else { RAX() = ~(u64)0; return true; }

		// attempt the seek
		try
		{
			// seek on both pointers (may be different depending on object)
			fd.Stream()->seekg((i64)RCX(), cpp_mode);
			fd.Stream()->seekp((i64)RCX(), cpp_mode);

			// return position (we return the get pointer) - because the underlying stream can be modified from multiple sources, we can't be sure which to use
			RAX() = fd.Stream()->tellg();
		}
		catch (...) { RAX() = ~(u64)0; }
		
		return true;
	}

	bool Computer::Process_sys_brk()
	{
		// special request of 0 returns current break
		if (RBX() == 0) RAX() = mem_size;
		// if the request is too high or goes below init size, don't do it - return -1
		else if (RBX() > max_mem_size || RBX() < min_mem_size) RAX() = ~(u64)0;
		// otherwise perform the reallocation
		else
		{
			realloc(RBX());
			RAX() = 0;
		}

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
