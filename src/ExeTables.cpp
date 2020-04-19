#include "../include/Computer.h"

namespace CSX64
{
	const std::unordered_map<ErrorCode, std::string> ErrorCodeToString
	{
		{ErrorCode::None, ""},
		{ErrorCode::OutOfBounds, "Out of Bounds"},
		{ErrorCode::UnhandledSyscall, "Unhandled Syscall"},
		{ErrorCode::UndefinedBehavior, "Undefined Behavior"},
		{ErrorCode::ArithmeticError, "Arithmetic Error"},
		{ErrorCode::Abort, "Abort"},
		{ErrorCode::IOFailure, "IO Failure"},
		{ErrorCode::FSDisabled, "FS Disabled"},
		{ErrorCode::AccessViolation, "Access Violation"},
		{ErrorCode::InsufficientFDs, "Insufficient FDs"},
		{ErrorCode::FDNotInUse, "FD not in use"},
		{ErrorCode::NotImplemented, "Not Implemented"},
		{ErrorCode::StackOverflow, "Stack Overflow"},
		{ErrorCode::FPUStackOverflow, "FPU Stack Overflow"},
		{ErrorCode::FPUStackUnderflow, "FPU Stack Underflow"},
		{ErrorCode::FPUError, "FPU Error"},
		{ErrorCode::FPUAccessViolation, "FPU Access Violation"},
		{ErrorCode::AlignmentViolation, "Alignment Violation"},
		{ErrorCode::UnknownOp, "Unknown Operation"},
		{ErrorCode::FilePermissions, "File Permissions Error"},
	};

	#define p2(b) b, !b, !b, b
	#define p4(b) p2(b), p2(!b), p2(!b), p2(b)
	#define p6(b) p4(b), p4(!b), p4(!b), p4(b)
	const bool Computer::parity_table[256] = {p6(true), p6(false), p6(false), p6(true)};

	bool(Computer::* const Computer::opcode_handlers[256])() = 
	{
		// -- x86 instructions -- //

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

		&Computer::ProcessBSx,
		&Computer::ProcessTZCNT,

		&Computer::ProcessUD,

		&Computer::ProcessIO,

		// -- x87 instructions -- //

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

		// -- vpu instructions -- //

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

		&Computer::TryProcessVEC_FCMP,
		&Computer::TryProcessVEC_FCOMI,

		&Computer::TryProcessVEC_FSQRT,
		&Computer::TryProcessVEC_FRSQRT,

		&Computer::TryProcessVEC_CVT,

		// -- misc -- //

		&Computer::TryProcessTRANS,

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

		&Computer::ProcessDEBUG
	};

	const Computer::VPUBinaryDelegate Computer::__TryProcessVEC_FCMP_lookup[32] =
	{
		&Computer::__TryProcessVEC_FCMP_EQ_OQ,
		&Computer::__TryProcessVEC_FCMP_LT_OS,
		&Computer::__TryProcessVEC_FCMP_LE_OS,
		&Computer::__TryProcessVEC_FCMP_UNORD_Q,
		&Computer::__TryProcessVEC_FCMP_NEQ_UQ,
		&Computer::__TryProcessVEC_FCMP_NLT_US,
		&Computer::__TryProcessVEC_FCMP_NLE_US,
		&Computer::__TryProcessVEC_FCMP_ORD_Q,
		&Computer::__TryProcessVEC_FCMP_EQ_UQ,
		&Computer::__TryProcessVEC_FCMP_NGE_US,
		&Computer::__TryProcessVEC_FCMP_NGT_US,
		&Computer::__TryProcessVEC_FCMP_FALSE_OQ,
		&Computer::__TryProcessVEC_FCMP_NEQ_OQ,
		&Computer::__TryProcessVEC_FCMP_GE_OS,
		&Computer::__TryProcessVEC_FCMP_GT_OS,
		&Computer::__TryProcessVEC_FCMP_TRUE_UQ,
		&Computer::__TryProcessVEC_FCMP_EQ_OS,
		&Computer::__TryProcessVEC_FCMP_LT_OQ,
		&Computer::__TryProcessVEC_FCMP_LE_OQ,
		&Computer::__TryProcessVEC_FCMP_UNORD_S,
		&Computer::__TryProcessVEC_FCMP_NEQ_US,
		&Computer::__TryProcessVEC_FCMP_NLT_UQ,
		&Computer::__TryProcessVEC_FCMP_NLE_UQ,
		&Computer::__TryProcessVEC_FCMP_ORD_S,
		&Computer::__TryProcessVEC_FCMP_EQ_US,
		&Computer::__TryProcessVEC_FCMP_NGE_UQ,
		&Computer::__TryProcessVEC_FCMP_NGT_UQ,
		&Computer::__TryProcessVEC_FCMP_FALSE_OS,
		&Computer::__TryProcessVEC_FCMP_NEQ_OS,
		&Computer::__TryProcessVEC_FCMP_GE_OQ,
		&Computer::__TryProcessVEC_FCMP_GT_OQ,
		&Computer::__TryProcessVEC_FCMP_TRUE_US,
	};
}
