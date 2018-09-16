#ifndef CSX64_CORETYPES_H
#define CSX64_CORETYPES_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace CSX64
{
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

	static_assert(sizeof(f64) * CHAR_BIT == 64, "Uhoh!! double isn't 64-bit in this compiler!");
	static_assert(sizeof(f32) * CHAR_BIT == 32, "Uhoh!! float isn't 32-bit in this compiler!");

	// true if long double uses 80-bit extended precision
	constexpr bool f80_enabled = sizeof(long double) * CHAR_BIT == 80;

	// if f80_enabled, aliases long double, otherwise aliases void
	typedef std::conditional<f80_enabled, long double, void> f80;

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

		DEBUG = 255
	};
}

#endif
