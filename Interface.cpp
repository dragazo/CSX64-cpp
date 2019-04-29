#include <iostream>
#include <iomanip>

#include "Computer.h"

#define __OPCODE_COUNTS 0

namespace CSX64
{
	#if __OPCODE_COUNTS
	u64 op_exe_count[256];
	#endif

	void Computer::Initialize(const Executable &exe, const std::vector<std::string> &args, u64 stacksize)
	{
		// get size of memory we need to allocate
		u64 size = exe.total_size() + stacksize;

		// make sure we catch overflow from adding stacksize
		if (size < stacksize) throw std::overflow_error("memory size overflow");
		// make sure it's within max memory usage limits
		if (size > max_mem_size) throw MemoryAllocException("executable size exceeded max memory");
		
		// allocate the required space (we can safely discard any previous values)
		if (!this->realloc(size, false)) throw MemoryAllocException("memory allocation failed");

		// mark the minimum memory size (so client code can't truncate off program code/data/stack/etc.)
		min_mem_size = size;

		// copy the executable content into our memory array
		std::memcpy(mem, exe.content(), exe.content_size());
		// zero the bss segment
		std::memset(reinterpret_cast<char*>(mem) + exe.content_size(), 0, exe.bss_seglen());

		// set up memory barriers
		ExeBarrier = exe.text_seglen();
		ReadonlyBarrier = exe.text_seglen() + exe.rodata_seglen();
		StackBarrier = exe.text_seglen() + exe.rodata_seglen() + exe.data_seglen() + exe.bss_seglen();

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
