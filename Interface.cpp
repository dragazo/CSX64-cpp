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
    Memory.clear();
	Memory.resize(size);
	InitMemorySize = size;

	// copy over the text/rodata/data segments (not including header)
	std::memcpy(Memory.data(), exe.data() + 32, exe.size() - 32);
	// zero the bss segment
	std::memset(Memory.data() + (exe.size() - 32), 0, bss_seglen);
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
	u64 stack = Memory.size();
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
		if (!GetMemAdv(1, op)) break;

		//Console.WriteLine($"{RIP:x8} - {(OPCode)op}");

		// switch through the opcodes
		switch ((OPCode)op)
		{
		case OPCode::NOP: break;

		case OPCode::HLT: Terminate(ErrorCode::Abort); break;
		case OPCode::SYSCALL: Syscall(); break;

		case OPCode::STLDF: ProcessSTLDF(); break;

		case OPCode::FlagManip: ProcessFlagManip(); break;

		case OPCode::SETcc: ProcessSETcc(); break;

		case OPCode::MOV: ProcessMOV(); break;
		case OPCode::MOVcc: ProcessMOVcc(); break;

		case OPCode::XCHG: ProcessXCHG(); break;

		case OPCode::JMP: ProcessJMP(op); break;
		case OPCode::Jcc: ProcessJcc(); break;
		case OPCode::LOOPcc: ProcessLOOPcc(); break;

		case OPCode::CALL: if (!ProcessJMP(op)) break; PushRaw(8, op); break;
		case OPCode::RET: if (!PopRaw(8, op)) break; RIP() = op; break;

		case OPCode::PUSH: ProcessPUSH(); break;
		case OPCode::POP: ProcessPOP(); break;

		case OPCode::LEA: ProcessLEA(); break;

		case OPCode::ADD: ProcessADD(); break;
		case OPCode::SUB: ProcessSUB(); break;

		case OPCode::MUL_x: ProcessMUL_x(); break;
		case OPCode::IMUL: ProcessIMUL(); break;
		case OPCode::DIV: ProcessDIV(); break;
		case OPCode::IDIV: ProcessIDIV(); break;

		case OPCode::SHL: ProcessSHL(); break;
		case OPCode::SHR: ProcessSHR(); break;
		case OPCode::SAL: ProcessSAL(); break;
		case OPCode::SAR: ProcessSAR(); break;
		case OPCode::ROL: ProcessROL(); break;
		case OPCode::ROR: ProcessROR(); break;
		case OPCode::RCL: ProcessRCL(); break;
		case OPCode::RCR: ProcessRCR(); break;

		case OPCode::AND: ProcessAND(); break;
		case OPCode::OR: ProcessOR(); break;
		case OPCode::XOR: ProcessXOR(); break;

		case OPCode::INC: ProcessINC(); break;
		case OPCode::DEC: ProcessDEC(); break;
		case OPCode::NEG: ProcessNEG(); break;
		case OPCode::NOT: ProcessNOT(); break;

		case OPCode::CMP: ProcessSUB(false); break;
		case OPCode::CMPZ: ProcessCMPZ(); break;
		case OPCode::TEST: ProcessAND(false); break;

		case OPCode::BSWAP: ProcessBSWAP(); break;
		case OPCode::BEXTR: ProcessBEXTR(); break;
		case OPCode::BLSI: ProcessBLSI(); break;
		case OPCode::BLSMSK: ProcessBLSMSK(); break;
		case OPCode::BLSR: ProcessBLSR(); break;
		case OPCode::ANDN: ProcessANDN(); break;
		case OPCode::BTx: ProcessBTx(); break;

		case OPCode::Cxy: ProcessCxy(); break;
		case OPCode::MOVxX: ProcessMOVxX(); break;

		case OPCode::ADXX: ProcessADXX(); break;
		case OPCode::AAX: ProcessAAX(); break;

			// x87 instructions

		case OPCode::FWAIT: break; // thus far fpu ops are synchronous with cpu ops

		case OPCode::FINIT: FINIT(); break;
		case OPCode::FCLEX: FPU_status &= 0xff00; break;

		case OPCode::FSTLD_WORD: ProcessFSTLD_WORD(); break;

		case OPCode::FLD_const: ProcessFLD_const(); break;
		case OPCode::FLD: ProcessFLD(); break;
		case OPCode::FST: ProcessFST(); break;
		case OPCode::FXCH: ProcessFXCH(); break;
		case OPCode::FMOVcc: ProcessFMOVcc(); break;

		case OPCode::FADD: ProcessFADD(); break;
		case OPCode::FSUB: ProcessFSUB(); break;
		case OPCode::FSUBR: ProcessFSUBR(); break;

		case OPCode::FMUL: ProcessFMUL(); break;
		case OPCode::FDIV: ProcessFDIV(); break;
		case OPCode::FDIVR: ProcessFDIVR(); break;

		case OPCode::F2XM1: ProcessF2XM1(); break;
		case OPCode::FABS: ProcessFABS(); break;
		case OPCode::FCHS: ProcessFCHS(); break;
		case OPCode::FPREM: ProcessFPREM(); break;
		case OPCode::FPREM1: ProcessFPREM1(); break;
		case OPCode::FRNDINT: ProcessFRNDINT(); break;
		case OPCode::FSQRT: ProcessFSQRT(); break;
		case OPCode::FYL2X: ProcessFYL2X(); break;
		case OPCode::FYL2XP1: ProcessFYL2XP1(); break;
		case OPCode::FXTRACT: ProcessFXTRACT(); break;
		case OPCode::FSCALE: ProcessFSCALE(); break;

		case OPCode::FXAM: ProcessFXAM(); break;
		case OPCode::FTST: ProcessFTST(); break;
		case OPCode::FCOM: ProcessFCOM(); break;

		case OPCode::FSIN: ProcessFSIN(); break;
		case OPCode::FCOS: ProcessFCOS(); break;
		case OPCode::FSINCOS: ProcessFSINCOS(); break;
		case OPCode::FPTAN: ProcessFPTAN(); break;
		case OPCode::FPATAN: ProcessFPATAN(); break;

		case OPCode::FINCDECSTP: ProcessFINCDECSTP(); break;
		case OPCode::FFREE: ProcessFFREE(); break;

			// vpu instructions

		case OPCode::VPU_MOV: ProcessVPUMove(); break;

		case OPCode::VPU_FADD: TryProcessVEC_FADD(); break;
		case OPCode::VPU_FSUB: TryProcessVEC_FSUB(); break;
		case OPCode::VPU_FMUL: TryProcessVEC_FMUL(); break;
		case OPCode::VPU_FDIV: TryProcessVEC_FDIV(); break;

		case OPCode::VPU_AND: TryProcessVEC_AND(); break;
		case OPCode::VPU_OR: TryProcessVEC_OR(); break;
		case OPCode::VPU_XOR: TryProcessVEC_XOR(); break;
		case OPCode::VPU_ANDN: TryProcessVEC_ANDN(); break;

		case OPCode::VPU_ADD: TryProcessVEC_ADD(); break;
		case OPCode::VPU_ADDS: TryProcessVEC_ADDS(); break;
		case OPCode::VPU_ADDUS: TryProcessVEC_ADDUS(); break;

		case OPCode::VPU_SUB: TryProcessVEC_SUB(); break;
		case OPCode::VPU_SUBS: TryProcessVEC_SUBS(); break;
		case OPCode::VPU_SUBUS: TryProcessVEC_SUBUS(); break;

		case OPCode::VPU_MULL: TryProcessVEC_MULL(); break;

		case OPCode::VPU_FMIN: TryProcessVEC_FMIN(); break;
		case OPCode::VPU_FMAX: TryProcessVEC_FMAX(); break;

		case OPCode::VPU_UMIN: TryProcessVEC_UMIN(); break;
		case OPCode::VPU_SMIN: TryProcessVEC_SMIN(); break;
		case OPCode::VPU_UMAX: TryProcessVEC_UMAX(); break;
		case OPCode::VPU_SMAX: TryProcessVEC_SMAX(); break;

		case OPCode::VPU_FADDSUB: TryProcessVEC_FADDSUB(); break;
		case OPCode::VPU_AVG: TryProcessVEC_AVG(); break;

			// misc instructions

		case OPCode::DEBUG: // all debugging features
			if (!GetMemAdv(1, op)) break;
			switch (op)
			{
			case 0: WriteCPUDebugString(std::cout); break;
			case 1: WriteVPUDebugString(std::cout); break;
			case 2: WriteFullDebugString(std::cout); break;

			default: Terminate(ErrorCode::UndefinedBehavior); break;
			}
			break;

			// otherwise, unknown opcode
		default: Terminate(ErrorCode::UnknownOp); break;
		}
	}

	return ticks;
}
