#include <iostream>
#include <iomanip>

#include "Computer.h"

#define __OPCODE_COUNTS 0

namespace CSX64
{
	#if __OPCODE_COUNTS
	u64 op_exe_count[256];
	#endif

	void Computer::Initialize(const std::vector<u8> &exe, const std::vector<std::string> &args, u64 stacksize)
	{
		// read header
		u64 text_seglen;
		u64 rodata_seglen;
		u64 data_seglen;
		u64 bss_seglen;
		if (!Read(exe, 0, 8, text_seglen) || !Read(exe, 8, 8, rodata_seglen) || !Read(exe, 16, 8, data_seglen) || !Read(exe, 24, 8, bss_seglen))
			throw ExecutableFormatError("executable header missing");
		
		// make sure exe is well-formed
		if (32 + text_seglen + rodata_seglen + data_seglen != exe.size())
			throw ExecutableFormatError("executable header invalid");

		// get size of memory and make sure it's within limits
		u64 size = exe.size() - 32 + bss_seglen + stacksize;

		// mark the minimum memory size (so we can't truncate off program code/data/stack/etc.)
		min_mem_size = size;

		// make sure it's within max memory usage limits
		if (size > max_mem_size)
			throw MemoryAllocException("executable size exceeded max memory");
		
		// get new memory array (does not include header)
		if (!this->realloc(size, false))
			throw MemoryAllocException("memory allocation failed");

		// copy over the text/rodata/data segments (not including header)
		std::memcpy(mem, exe.data() + 32, exe.size() - 32);
		// zero the bss segment
		std::memset(reinterpret_cast<char*>(mem) + (exe.size() - 32), 0, bss_seglen);
		// randomize the stack segment
		//for (u64 i = exe.size() - 32 + bss_seglen; i < Memory.size(); ++i) Memory[i] = Rand();

		// set up memory barriers
		ExeBarrier = text_seglen;
		ReadonlyBarrier = text_seglen + rodata_seglen;
		StackBarrier = text_seglen + rodata_seglen + data_seglen + bss_seglen;

		// set up cpu registers
		for (int i = 0; i < 16; ++i) CPURegisters[i].x64() = Rand();

		// set up fpu registers
		FINIT();

		// set up vpu registers
		for (int i = 0; i < 32; ++i)
			for (int j = 0; j < 8; ++j) ZMMRegisters[i].get<u64>(j) = Rand();
		_MXCSR = 0x1f80;

		// set execution state
		RIP() = 0;
		RFLAGS() = 2; // x86 standard dictates this initial state
		running = true;
		suspended_read = false;
		error = ErrorCode::None;

		// get the stack pointer
		u64 stack = size;
		RBP() = stack; // RBP points to before we start pushing args

		// an array of pointers to command line args in computer memory.
		// one for each arg, plus a null terminator.
		std::vector<u64> cmdarg_pointers(args.size() + 1);

		// put each arg on the stack and get their addresses
		for (u64 i = 0; i < args.size(); ++i)
		{
			// push the arg onto the stack
			stack -= args[i].size() + 1;
			SetCString(stack, args[i]);

			// record pointer to this arg
			cmdarg_pointers[i] = stack;
		}
		// the last pointer is null (C guarantees this, so we will as well)
		cmdarg_pointers[args.size()] = 0;

		// make room for the command line pointer array
		stack -= 8 * cmdarg_pointers.size();
		// write pointer array to memory
		for (u64 i = 0; i < cmdarg_pointers.size(); ++i) SetMem(stack + i * 8, cmdarg_pointers[i]);

		// load arg count and arg array pointer to RDI, RSI
		RDI() = args.size();
		RSI() = stack;

		// initialize RSP
		RSP() = stack;

		// also push args to stack (RTL)
		Push(RSI());
		Push(RDI());

		#if __OPCODE_COUNTS
		// clear op_exe_count
		for (u64 i = 0; i < 256; ++i) op_exe_count[i] = 0;
		#endif
	}

	u64 Computer::Tick(u64 count)
	{
		u64 ticks, op;
		for (ticks = 0; ticks < count; ++ticks)
		{
			// fail if terminated or awaiting data
			if (!running || suspended_read) break;

			// make sure we're before the executable barrier
			if (RIP() >= ExeBarrier) { Terminate(ErrorCode::AccessViolation); break; }

			// fetch the instruction
			if (!GetMemAdv<u8>(op)) break;

			#if __OPCODE_COUNTS
			// update op exe count
			++op_exe_count[op];
			#endif

			//std::cout << op << '\n';

			// perform the instruction
			(this->*opcode_handlers[op])();
		}

		return ticks;
	}

	bool Computer::ProcessSYSCALL()
	{
		switch ((SyscallCode)RAX())
		{
		case SyscallCode::sys_exit:

			#if __OPCODE_COUNTS
			std::cout << "\n\nOPCode Counts:\n" << std::dec << std::setfill(' ');
			for (u64 i = 0; i < 256; ++i)
			{
				std::cout << std::setw(3) << i << ": " << std::setw(16) << op_exe_count[i] << "   ";
				if (i > 0 && i % 4 == 3) std::cout << '\n';
			}
			#endif

			Exit((int)RBX());
			return true;

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
}
