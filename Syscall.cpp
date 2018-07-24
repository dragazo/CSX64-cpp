#include <filesystem>

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
		if (RCX() >= MemorySize() || RCX() + RDX() > MemorySize()) { Terminate(ErrorCode::OutOfBounds); return false; }
		// make sure we're not in the readonly segment
		if (RCX() < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

		// read from the file
		fd.BaseStream->read((char*)Memory.data() + RCX(), RDX());
		u64 n = fd.BaseStream->gcount();

		// if we got nothing but it's interactive
		if (n == 0 && fd.Interactive)
		{
			--RIP();                // await further data by repeating the syscall
			SuspendedRead = true; // suspend execution until there's more data
		}
		// otherwise return num chars read from file
		else
		{
			RAX() = n; // count in RAX
		}

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
		if (RCX() >= MemorySize() || RCX() + RDX() > MemorySize()) { Terminate(ErrorCode::OutOfBounds); return false; }

		// attempt to write from memory to the file
		fd.BaseStream->write((char*)Memory.data() + RCX(), RDX());
	}

	bool Computer::Process_sys_open()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get an available file descriptor
		int fd_index;
		FileDescriptor *fd = FindAvailableFD(&fd_index);
		if (fd == nullptr) { Terminate(ErrorCode::InsufficientFDs); return false; }

		// get path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		std::fstream *f = new std::fstream(path);

		// store in the file descriptor
		fd->Open(f, true, false);
		// return file descriptor index in RAX
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

		// close the file
		if (!fd.Close()) { Terminate(ErrorCode::IOFailure); return false; }

		return true;
	}

	bool Computer::Process_sys_lseek()
	{
		// get fd index
		u64 fd_index = RBX();
		if (fd_index >= FDCount) { Terminate(ErrorCode::OutOfBounds); return false; }

		// get fd
		FileDescriptor fd = FileDescriptors[fd_index];
		if (!fd.InUse()) { Terminate(ErrorCode::FDNotInUse); return false; }

		// attempt to seek in the file
		if (!fd.Seek(RCX(), RDX())) { Terminate(ErrorCode::IOFailure); return false; }

		return true;
	}

	bool Computer::Process_sys_rename()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the paths
		std::string from, to;
		if (!GetCString(RBX(), from) || !GetCString(RCX(), to)) return false;

		// attempt the move operation
		try { std::rename(from.c_str(), to.c_str()); return true; }
		catch (...) { Terminate(ErrorCode::IOFailure); return false; }
	}
	bool Computer::Process_sys_unlink()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		// attempt the move operation
		try { std::remove(path.c_str()); return true; }
		catch (...) { Terminate(ErrorCode::IOFailure); return false; }
	}

	bool Computer::Process_sys_mkdir()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		// attempt the move operation
		try { fs::create_directory(path); return true; }
		catch (...) { Terminate(ErrorCode::IOFailure); return false; }
	}
	bool Computer::Process_sys_rmdir()
	{
		// make sure we're allowed to do this
		if (!FSF()) { Terminate(ErrorCode::FSDisabled); return false; }

		// get the path
		std::string path;
		if (!GetCString(RBX(), path)) return false;

		// attempt the move operation
		try { fs::remove(path); return true; }
		catch (...) { Terminate(ErrorCode::IOFailure); return false; }
	}

	bool Computer::Process_sys_brk()
	{
		// special request of 0 returns current break
		if (RBX() == 0) RAX() = MemorySize();
		// if the request is too high or goes below init size, don't do it
		else if (RBX() > MaxMemory || RBX() < InitMemorySize) RAX() = ~(u64)0; // RAX = -1
		// otherwise perform the reallocation
		else
		{
			Memory.resize(RBX());
			RAX() = 0;
		}

		return true;
	}
}
