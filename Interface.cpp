#include "Computer.h"

bool CSX64::Computer::Initialize(std::vector<u8> &exe, std::vector<std::string> args, u64 stacksize)
{
	// read header
	u64 text_seglen;
	u64 rodata_seglen;
	u64 data_seglen;
	u64 bss_seglen;
	if (!Read(exe, 0, 8, text_seglen) || !Read(exe, 8, 8, rodata_seglen) || !Read(exe, 16, 8, data_seglen) || !Read(exe, 24, 8, bss_seglen)) return false;

	// get size of memory and make sure it's within limits
	u64 size = exe.size() - 32 + bss_seglen + stacksize;
	// make sure it's within limits
	if (size > MaxMemory) return false;

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
	//for (int i = 0; i < 16; ++i) CPURegisters[i].x64() = Rand64(Rand);

	// set up fpu registers
	FINIT();

	// set up vpu registers
	//for (int i = 0; i < 32; ++i)
		//for (int j = 0; j < 8; ++j) ZMMRegisters[i].uint64(j) = Rand64(Rand);

	// set execution state
	RIP() = 0;
	EFLAGS() = 2; // x86 standard dictates this initial state
	Running = true;
	SuspendedRead = false;
	Error = ErrorCode::None;

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

CSX64::u64 CSX64::Computer::Tick(u64 count)
{
	u64 ticks, op;
	for (ticks = 0; ticks < count; ++ticks)
	{
		// fail if terminated or awaiting data
		if (!Running || SuspendedRead) break;

		// make sure we're before the executable barrier
		if (RIP() >= ExeBarrier) { Terminate(ErrorCode::AccessViolation); break; }

		// fetch the instruction
		if (!GetMemAdv<u8>(op)) break;

		//std::cout << op << '\n';

		(this->*opcode_handlers[op])();

		// switch through the opcodes
		
	}

	return ticks;
}
