#include "Computer.h"

namespace CSX64
{
	bool Computer::Initialize(std::vector<u8> &exe, std::vector<std::string> args, u64 stacksize)
	{
		// read header
		u64 text_seglen;
		u64 rodata_seglen;
		u64 data_seglen;
		u64 bss_seglen;
		if (!Read(exe, 0, 8, text_seglen) || !Read(exe, 8, 8, rodata_seglen) || !Read(exe, 16, 8, data_seglen) || !Read(exe, 24, 8, bss_seglen)) return false;

		// make sure exe is well-formed
		if (32 + text_seglen + rodata_seglen + data_seglen != exe.size()) return false;

		// get size of memory and make sure it's within limits
		u64 size = exe.size() - 32 + bss_seglen + stacksize;

		// make sure it's within limits
		if (size > max_mem_size) return false;

		// get new memory array (does not include header)
		alloc(size);

		// copy over the text/rodata/data segments (not including header)
		std::memcpy(mem, exe.data() + 32, exe.size() - 32);
		// zero the bss segment
		std::memset((char*)mem + (exe.size() - 32), 0, bss_seglen);
		// randomize the stack segment
		//for (u64 i = exe.size() - 32 + bss_seglen; i < Memory.size(); ++i) Memory[i] = Rand();

		// set up memory barriers
		ExeBarrier = text_seglen;
		ReadonlyBarrier = text_seglen + rodata_seglen;
		StackBarrier = text_seglen + rodata_seglen + data_seglen + bss_seglen;

		// set up cpu registers
		for (int i = 0; i < 16; ++i) CPURegisters[i].x64() = Rand64(Rand);

		// set up fpu registers
		FINIT();

		// set up vpu registers
		for (int i = 0; i < 32; ++i)
			for (int j = 0; j < 8; ++j) ZMMRegisters[i].uint64(j) = Rand64(Rand);

		// set execution state
		RIP() = 0;
		EFLAGS() = 2; // x86 standard dictates this initial state
		running = true;
		suspended_read = false;
		error = ErrorCode::None;

		// get the stack pointer
		u64 stack = size;
		RBP() = stack; // RBP points to before we start pushing args

		// if we have cmd line args, load them
		if (!args.empty())
		{
			std::vector<u64> pointers(args.size()); // an array of pointers to args in computer memory

			// for each arg (backwards to make more sense visually, but the order doesn't actually matter)
			for (int i = args.size() - 1; i >= 0; --i)
			{
				// push the arg onto the stack
				stack -= args[i].size() + 1;
				SetCString(stack, args[i]);

				// record pointer to this arg
				pointers[i] = stack;
			}

			// make room for the pointer array
			stack -= 8 * pointers.size();
			// write pointer array to memory
			for (int i = 0; i < pointers.size(); ++i) SetMem(stack + i * 8, pointers[i]);
		}

		// load arg count and arg array pointer to RDI, RSI
		RDI() = args.size();
		RSI() = stack;

		// initialize RSP
		RSP() = stack;

		// also push args to stack (RTL)
		Push(RSI());
		Push(RDI());

		return true;
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
		case SyscallCode::sys_exit: Exit(RBX()); return true;

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
