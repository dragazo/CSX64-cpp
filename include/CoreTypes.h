#ifndef CSX64_CORETYPES_H
#define CSX64_CORETYPES_H

#include <cstdint>
#include <climits>

namespace CSX64
{
	// -- core typedefs -- //

	typedef std::uint64_t u64;
	typedef std::uint32_t u32;
	typedef std::uint16_t u16;
	typedef std::uint8_t u8;

	typedef std::int64_t i64;
	typedef std::int32_t i32;
	typedef std::int16_t i16;
	typedef std::int8_t i8;

	typedef double f64;
	typedef float f32;

	typedef long double fext;

	static_assert(CHAR_BIT == 8, "Uhoh!! This system doesn't use 8-bit bytes!");

	static_assert(sizeof(u64) == 8, "Uhoh!! This compiler's std::uint64_t isn't really 64-bit!");
	static_assert(sizeof(u32) == 4, "Uhoh!! This compiler's std::uint32_t isn't really 32-bit!");
	static_assert(sizeof(u16) == 2, "Uhoh!! This compiler's std::uint64_t isn't really 16-bit!");
	static_assert(sizeof(u8) == 1, "Uhoh!! This compiler's std::uint64_t isn't really 8-bit!");

	static_assert(sizeof(i64) == 8, "Uhoh!! This compiler's std::int64_t isn't really 64-bit!");
	static_assert(sizeof(i32) == 4, "Uhoh!! This compiler's std::int32_t isn't really 32-bit!");
	static_assert(sizeof(i16) == 2, "Uhoh!! This compiler's std::int64_t isn't really 16-bit!");
	static_assert(sizeof(i8) == 1, "Uhoh!! This compiler's std::int64_t isn't really 8-bit!");

	static_assert(sizeof(f64) == 8, "Uhoh!! This compiler's double isn't 64-bit!");
	static_assert(sizeof(f32) == 4, "Uhoh!! This compiler's float isn't 32-bit!");

	static_assert(sizeof(fext) >= sizeof(f64), "Uhoh!! This compiler's long double is less precise than double!");

	// ---------------------------------------

	enum class OPCode
	{
		// x86 instructions

		NOP,
		HLT, SYSCALL,
		STLDF,
		FlagManip,

		SETcc, MOV, MOVcc, XCHG,

		JMP, Jcc, LOOPcc, CALL, RET,
		PUSH, POP,
		LEA,

		ADD, SUB,
		MUL_x, IMUL, DIV, IDIV,
		SHL, SHR, SAL, SAR, ROL, ROR, RCL, RCR,
		AND, OR, XOR,
		INC, DEC, NEG, NOT,

		CMP, CMPZ, TEST,

		BSWAP, BEXTR, BLSI, BLSMSK, BLSR, ANDN, BTx,
		Cxy, MOVxX,
		ADXX, AAX,

		string,

		BSx, TZCNT,

		UD,

		IO,

		// x87 instructions

		FWAIT,
		FINIT, FCLEX,
		FSTLD_WORD,
		FLD_const, FLD, FST, FXCH, FMOVcc,

		FADD, FSUB, FSUBR,
		FMUL, FDIV, FDIVR,

		F2XM1, FABS, FCHS, FPREM, FPREM1, FRNDINT, FSQRT, FYL2X, FYL2XP1, FXTRACT, FSCALE,
		FXAM, FTST, FCOM,
		FSIN, FCOS, FSINCOS, FPTAN, FPATAN,
		FINCDECSTP, FFREE,

		// SIMD instructions

		VPU_MOV,

		VPU_FADD, VPU_FSUB, VPU_FMUL, VPU_FDIV,
		VPU_AND, VPU_OR, VPU_XOR, VPU_ANDN,
		VPU_ADD, VPU_ADDS, VPU_ADDUS,
		VPU_SUB, VPU_SUBS, VPU_SUBUS,
		VPU_MULL,

		VPU_FMIN, VPU_FMAX,
		VPU_UMIN, VPU_SMIN, VPU_UMAX, VPU_SMAX,

		VPU_FADDSUB,
		VPU_AVG,

		VPU_FCMP, VPU_FCOMI,

		VPU_FSQRT, VPU_FRSQRT,

		VPU_CVT,

		// misc instructions

		TRANS,

		DEBUG = 255
	};
}

#endif
