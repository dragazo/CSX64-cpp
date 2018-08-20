#include "Computer.h"

namespace CSX64
{
	#define p2(b) b, !b, !b, b
	#define p4(b) p2(b), p2(!b), p2(!b), p2(b)
	#define p6(b) p4(b), p4(!b), p4(!b), p4(b)
	const bool Computer::parity_table[256] = {p6(true), p6(false), p6(false), p6(true)};

	bool(Computer::* const Computer::opcode_handlers[256])() = 
	{
		&Computer::ProcessNOP,

		&Computer::ProcessHLT,
		&Computer::ProcessSYSCALL,

		&Computer::ProcessSTLDF,

		&Computer::ProcessFlagManip,

		&Computer::ProcessSETcc,

		&Computer::ProcessMOV,
		&Computer::ProcessMOVcc,

		&Computer::ProcessXCHG,

		&Computer::ProcessJMP,
		&Computer::ProcessJcc,
		&Computer::ProcessLOOPcc,

		&Computer::ProcessCALL,
		&Computer::ProcessRET,

		&Computer::ProcessPUSH,
		&Computer::ProcessPOP,

		&Computer::ProcessLEA,

		&Computer::ProcessADD,
		&Computer::ProcessSUB,

		&Computer::ProcessMUL_x,
		&Computer::ProcessIMUL,
		&Computer::ProcessDIV,
		&Computer::ProcessIDIV,

		&Computer::ProcessSHL,
		&Computer::ProcessSHR,
		&Computer::ProcessSAL,
		&Computer::ProcessSAR,
		&Computer::ProcessROL,
		&Computer::ProcessROR,
		&Computer::ProcessRCL,
		&Computer::ProcessRCR,

		&Computer::ProcessAND,
		&Computer::ProcessOR,
		&Computer::ProcessXOR,

		&Computer::ProcessINC,
		&Computer::ProcessDEC,
		&Computer::ProcessNEG,
		&Computer::ProcessNOT,

		&Computer::ProcessCMP,
		&Computer::ProcessCMPZ,
		&Computer::ProcessTEST,

		&Computer::ProcessBSWAP,
		&Computer::ProcessBEXTR,
		&Computer::ProcessBLSI,
		&Computer::ProcessBLSMSK,
		&Computer::ProcessBLSR,
		&Computer::ProcessANDN,
		&Computer::ProcessBTx,

		&Computer::ProcessCxy,
		&Computer::ProcessMOVxX,

		&Computer::ProcessADXX,
		&Computer::ProcessAAX,

		&Computer::ProcessSTRING,

		// x87 instructions

		&Computer::ProcessNOP,

		&Computer::FINIT,
		&Computer::ProcessFCLEX,

		&Computer::ProcessFSTLD_WORD,

		&Computer::ProcessFLD_const,
		&Computer::ProcessFLD,
		&Computer::ProcessFST,
		&Computer::ProcessFXCH,
		&Computer::ProcessFMOVcc,

		&Computer::ProcessFADD,
		&Computer::ProcessFSUB,
		&Computer::ProcessFSUBR,

		&Computer::ProcessFMUL,
		&Computer::ProcessFDIV,
		&Computer::ProcessFDIVR,

		&Computer::ProcessF2XM1,
		&Computer::ProcessFABS,
		&Computer::ProcessFCHS,
		&Computer::ProcessFPREM,
		&Computer::ProcessFPREM1,
		&Computer::ProcessFRNDINT,
		&Computer::ProcessFSQRT,
		&Computer::ProcessFYL2X,
		&Computer::ProcessFYL2XP1,
		&Computer::ProcessFXTRACT,
		&Computer::ProcessFSCALE,

		&Computer::ProcessFXAM,
		&Computer::ProcessFTST,
		&Computer::ProcessFCOM,

		&Computer::ProcessFSIN,
		&Computer::ProcessFCOS,
		&Computer::ProcessFSINCOS,
		&Computer::ProcessFPTAN,
		&Computer::ProcessFPATAN,

		&Computer::ProcessFINCDECSTP,
		&Computer::ProcessFFREE,

		// vpu instructions

		&Computer::ProcessVPUMove,

		&Computer::TryProcessVEC_FADD,
		&Computer::TryProcessVEC_FSUB,
		&Computer::TryProcessVEC_FMUL,
		&Computer::TryProcessVEC_FDIV,

		&Computer::TryProcessVEC_AND,
		&Computer::TryProcessVEC_OR,
		&Computer::TryProcessVEC_XOR,
		&Computer::TryProcessVEC_ANDN,

		&Computer::TryProcessVEC_ADD,
		&Computer::TryProcessVEC_ADDS,
		&Computer::TryProcessVEC_ADDUS,

		&Computer::TryProcessVEC_SUB,
		&Computer::TryProcessVEC_SUBS,
		&Computer::TryProcessVEC_SUBUS,

		&Computer::TryProcessVEC_MULL,

		&Computer::TryProcessVEC_FMIN,
		&Computer::TryProcessVEC_FMAX,

		&Computer::TryProcessVEC_UMIN,
		&Computer::TryProcessVEC_SMIN,
		&Computer::TryProcessVEC_UMAX,
		&Computer::TryProcessVEC_SMAX,

		&Computer::TryProcessVEC_FADDSUB,
		&Computer::TryProcessVEC_AVG,

			// -- unused opcodes -- //

		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,
		&Computer::ProcessUNKNOWN,

		&Computer::ProcessDEBUG
	};
}
