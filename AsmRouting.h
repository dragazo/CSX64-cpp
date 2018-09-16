#ifndef CSX64_ASM_ROUTING_H
#define CSX64_ASM_ROUTING_H

#include <vector>
#include <string>

#include "CoreTypes.h"
#include "AsmArgs.h"
#include "AsmTables.h"

namespace CSX64
{
	// ---------------- //

	// -- directives -- //

	// ---------------- //

	inline bool asm_router_GLOBAL(AssembleArgs &args) { return args.TryProcessGlobal(); }
	inline bool asm_router_EXTERN(AssembleArgs &args) { return args.TryProcessExtern(); }

	inline bool asm_router_ALIGN(AssembleArgs &args) { return args.TryProcessAlign(); }

	inline bool asm_router_ALIGNB(AssembleArgs &args) { return true; }
	inline bool asm_router_ALIGNW(AssembleArgs &args) { return args.TryProcessAlignXX(2); }
	inline bool asm_router_ALIGND(AssembleArgs &args) { return args.TryProcessAlignXX(4); }
	inline bool asm_router_ALIGNQ(AssembleArgs &args) { return args.TryProcessAlignXX(8); }
	inline bool asm_router_ALIGNX(AssembleArgs &args) { return args.TryProcessAlignXX(16); }
	inline bool asm_router_ALIGNY(AssembleArgs &args) { return args.TryProcessAlignXX(32); }
	inline bool asm_router_ALIGNZ(AssembleArgs &args) { return args.TryProcessAlignXX(64); }

	inline bool asm_router_DB(AssembleArgs &args) { return args.TryProcessDeclare(1); }
	inline bool asm_router_DW(AssembleArgs &args) { return args.TryProcessDeclare(2); }
	inline bool asm_router_DD(AssembleArgs &args) { return args.TryProcessDeclare(4); }
	inline bool asm_router_DQ(AssembleArgs &args) { return args.TryProcessDeclare(8); }
	inline bool asm_router_DX(AssembleArgs &args) { return args.TryProcessDeclare(16); }
	inline bool asm_router_DY(AssembleArgs &args) { return args.TryProcessDeclare(32); }
	inline bool asm_router_DZ(AssembleArgs &args) { return args.TryProcessDeclare(64); }

	inline bool asm_router_RESB(AssembleArgs &args) { return args.TryProcessReserve(1); }
	inline bool asm_router_RESW(AssembleArgs &args) { return args.TryProcessReserve(2); }
	inline bool asm_router_RESD(AssembleArgs &args) { return args.TryProcessReserve(4); }
	inline bool asm_router_RESQ(AssembleArgs &args) { return args.TryProcessReserve(8); }
	inline bool asm_router_RESX(AssembleArgs &args) { return args.TryProcessReserve(16); }
	inline bool asm_router_RESY(AssembleArgs &args) { return args.TryProcessReserve(32); }
	inline bool asm_router_RESZ(AssembleArgs &args) { return args.TryProcessReserve(64); }

	inline bool asm_router_EQU(AssembleArgs &args) { return args.TryProcessEQU(); }

	inline bool asm_router_SEGMENT(AssembleArgs &args) { return args.TryProcessSegment(); } // SECTION

	// -------------- //

	// -- unmapped -- //

	// -------------- //

	inline bool asm_router_LFENCE(AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }
	inline bool asm_router_SFENCE(AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }
	inline bool asm_router_MFENCE(AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }

	inline bool asm_router_PAUSE(AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }

	// --------- //

	// -- x86 -- //

	// --------- //

	inline bool asm_router_NOP(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::NOP); }

	inline bool asm_router_HLT(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::HLT); }
	inline bool asm_router_SYSCALL(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::SYSCALL); }

	inline bool asm_router_PUSHF(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 0); }
	inline bool asm_router_PUSHFD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 1); }
	inline bool asm_router_PUSHFQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 2); }

	inline bool asm_router_POPF(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 3); }
	inline bool asm_router_POPFD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 4); }
	inline bool asm_router_POPFQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 5); }

	inline bool asm_router_SAHF(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 6); }
	inline bool asm_router_LAHF(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 7); }

	inline bool asm_router_STC(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 0); }
	inline bool asm_router_CLC(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 1); }
	inline bool asm_router_STI(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 2); }
	inline bool asm_router_CLI(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 3); }
	inline bool asm_router_STD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 4); }
	inline bool asm_router_CLD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 5); }
	inline bool asm_router_STAC(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 6); }
	inline bool asm_router_CLAC(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 7); }
	inline bool asm_router_CMC(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 8); }

	inline bool asm_router_SETZ(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 0, 1); } // SETE
	inline bool asm_router_SETNZ(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 1, 1); } // SETNE
	inline bool asm_router_SETS(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 2, 1); }
	inline bool asm_router_SETNS(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 3, 1); }
	inline bool asm_router_SETP(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 4, 1); } // SETPE
	inline bool asm_router_SETNP(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 5, 1); } // SETPO
	inline bool asm_router_SETO(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 6, 1); }
	inline bool asm_router_SETNO(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 7, 1); }
	inline bool asm_router_SETC(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 8, 1); }
	inline bool asm_router_SETNC(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 9, 1); }

	inline bool asm_router_SETB(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 10, 1); } // SETNAE
	inline bool asm_router_SETBE(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 11, 1); } // SETNA
	inline bool asm_router_SETA(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 12, 1); } // SETNBE
	inline bool asm_router_SETAE(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 13, 1); } // SETNB

	inline bool asm_router_SETL(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 14, 1); } // SETNGE
	inline bool asm_router_SETLE(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 15, 1); } // SETNG
	inline bool asm_router_SETG(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 16, 1); } // SETNLE
	inline bool asm_router_SETGE(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 17, 1); } // SETNL

	inline bool asm_router_MOV(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOV); }

	inline bool asm_router_MOVZ(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 0); } // MOVE
	inline bool asm_router_MOVNZ(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 1); } // MOVNE
	// MOVS (mov) requires disambiguation
	inline bool asm_router_MOVNS(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 3); }
	inline bool asm_router_MOVP(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 4); } // MOVPE
	inline bool asm_router_MOVNP(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 5); } // MOVPO
	inline bool asm_router_MOVO(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 6); }
	inline bool asm_router_MOVNO(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 7); }
	inline bool asm_router_MOVC(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 8); }
	inline bool asm_router_MOVNC(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 9); }

	inline bool asm_router_MOVB(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 10); } // MOVNAE
	inline bool asm_router_MOVBE(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 11); } // MOVNA
	inline bool asm_router_MOVA(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 12); } // MOVNBE
	inline bool asm_router_MOVAE(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 13); } // MOVNB

	inline bool asm_router_MOVL(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 14); } // MOVNGE
	inline bool asm_router_MOVLE(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 15); } // MOVNG
	inline bool asm_router_MOVG(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 16); } // MOVNLE
	inline bool asm_router_MOVGE(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 17); } // MOVNL

	inline bool asm_router_XCHG(AssembleArgs &args) { return args.TryProcessXCHG(OPCode::XCHG); }

	inline bool asm_router_JMP(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::JMP, false, 0, 14, 3); }

	inline bool asm_router_JZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 0, 14, 3); } // JE
	inline bool asm_router_JNZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 1, 14, 3); } // JNE
	inline bool asm_router_JS(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 2, 14, 3); }
	inline bool asm_router_JNS(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 3, 14, 3); }
	inline bool asm_router_JP(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 4, 14, 3); } // JPE
	inline bool asm_router_JNP(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 5, 14, 3); } // JPO
	inline bool asm_router_JO(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 6, 14, 3); }
	inline bool asm_router_JNO(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 7, 14, 3); }
	inline bool asm_router_JC(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 8, 14, 3); }
	inline bool asm_router_JNC(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 9, 14, 3); }

	inline bool asm_router_JB(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 10, 14, 3); } // JNAE
	inline bool asm_router_JBE(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 11, 14, 3); } // JNA
	inline bool asm_router_JA(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 12, 14, 3); } // JNBE
	inline bool asm_router_JAE(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 13, 14, 3); } // JNB

	inline bool asm_router_JL(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 14, 14, 3); } // JNGE
	inline bool asm_router_JLE(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 15, 14, 3); } // JNG
	inline bool asm_router_JG(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 16, 14, 3); } // JNLE
	inline bool asm_router_JGE(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 17, 14, 3); } // JNL

	inline bool asm_router_JCXZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 18, 2, 1); }
	inline bool asm_router_JECXZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 18, 4, 2); }
	inline bool asm_router_JRCXZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 18, 8, 3); }

	inline bool asm_router_LOOP(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 0, 14, 3); }
	inline bool asm_router_LOOPZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 1, 14, 3); } // LOOPE
	inline bool asm_router_LOOPNZ(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 2, 14, 3); } // LOOPNE

	inline bool asm_router_CALL(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::CALL, false, 0, 14, 3); }
	inline bool asm_router_RET(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::RET); }

	inline bool asm_router_PUSH(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::PUSH, false, 0, 14); }
	inline bool asm_router_POP(AssembleArgs &args) { return args.TryProcessPOP(OPCode::POP); }

	inline bool asm_router_LEA(AssembleArgs &args) { return args.TryProcessLEA(OPCode::LEA); }

	inline bool asm_router_ADD(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::ADD); }
	inline bool asm_router_SUB(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::SUB); }

	inline bool asm_router_MUL(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::MUL_x, true, 0); }
	inline bool asm_router_MULX(AssembleArgs &args) { return args.TryProcessRR_RM(OPCode::MUL_x, true, 1, 12); }
	inline bool asm_router_IMUL(AssembleArgs &args)
	{
		switch (args.args.size())
		{
		case 1: return args.TryProcessIMMRM(OPCode::IMUL, true, 0);
		case 2: return args.TryProcessBinaryOp(OPCode::IMUL, true, 1);
		case 3: return args.TryProcessTernaryOp(OPCode::IMUL, true, 2);

		default: args.res = {AssembleError::ArgCount, "line " + tostr(args.line) + ": IMUL expected 1, 2, or 3 args"}; return false;
		}
	}
	inline bool asm_router_DIV(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::DIV); }
	inline bool asm_router_IDIV(AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::IDIV); }

	inline bool asm_router_SHL(AssembleArgs &args) { return args.TryProcessShift(OPCode::SHL); }
	inline bool asm_router_SHR(AssembleArgs &args) { return args.TryProcessShift(OPCode::SHR); }
	inline bool asm_router_SAL(AssembleArgs &args) { return args.TryProcessShift(OPCode::SAL); }
	inline bool asm_router_SAR(AssembleArgs &args) { return args.TryProcessShift(OPCode::SAR); }
	inline bool asm_router_ROL(AssembleArgs &args) { return args.TryProcessShift(OPCode::ROL); }
	inline bool asm_router_ROR(AssembleArgs &args) { return args.TryProcessShift(OPCode::ROR); }
	inline bool asm_router_RCL(AssembleArgs &args) { return args.TryProcessShift(OPCode::RCL); }
	inline bool asm_router_RCR(AssembleArgs &args) { return args.TryProcessShift(OPCode::RCR); }

	inline bool asm_router_AND(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::AND); }
	inline bool asm_router_OR(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::OR); }
	inline bool asm_router_XOR(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::XOR); }

	inline bool asm_router_INC(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::INC); }
	inline bool asm_router_DEC(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::DEC); }
	inline bool asm_router_NEG(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::NEG); }
	inline bool asm_router_NOT(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::NOT); }

	inline bool asm_router_CMP(AssembleArgs &args)
	{
		// if there are 2 args and the second one is an instant 0, we can make this a CMPZ instruction
		u64 a, b;
		bool floating, btemp;
		if (args.args.size() == 2 && args.TryParseInstantImm(args.args[1], a, floating, b, btemp) && a == 0)
		{
			// set new args for the unary version
			args.args.resize(1);
			return args.TryProcessUnaryOp(OPCode::CMPZ);
		}
		// otherwise normal binary
		else return args.TryProcessBinaryOp(OPCode::CMP);
	}
	inline bool asm_router_TEST(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::TEST); }

	inline bool asm_router_BSWAP(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BSWAP); }
	inline bool asm_router_BEXTR(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::BEXTR, false, 0, 15, 1); }
	inline bool asm_router_BLSI(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BLSI); }
	inline bool asm_router_BLSMSK(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BLSMSK); }
	inline bool asm_router_BLSR(AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BLSR); }
	inline bool asm_router_ANDN(AssembleArgs &args) { return args.TryProcessRR_RM(OPCode::ANDN, false, 0, 12); }

	inline bool asm_router_BT(AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 0, 15, 0); }
	inline bool asm_router_BTS(AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 1, 15, 0); }
	inline bool asm_router_BTR(AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 2, 15, 0); }
	inline bool asm_router_BTC(AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 3, 15, 0); }

	inline bool asm_router_CWD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 0); }
	inline bool asm_router_CDQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 1); }
	inline bool asm_router_CQO(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 2); }

	inline bool asm_router_CBW(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 3); }
	inline bool asm_router_CWDE(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 4); }
	inline bool asm_router_CDQE(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 5); }

	inline bool asm_router_MOVZX(AssembleArgs &args) { return args.TryProcessMOVxX(OPCode::MOVxX, false); }
	inline bool asm_router_MOVSX(AssembleArgs &args) { return args.TryProcessMOVxX(OPCode::MOVxX, true); }

	inline bool asm_router_ADC(AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::ADXX, true, 0); }
	inline bool asm_router_ADCX(AssembleArgs &args) { return args.TryProcessBinaryOp_R_RM(OPCode::ADXX, true, 1, 12); }
	inline bool asm_router_ADOX(AssembleArgs &args) { return args.TryProcessBinaryOp_R_RM(OPCode::ADXX, true, 2, 12); }

	inline bool asm_router_AAA(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 0); }
	inline bool asm_router_AAS(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 1); }
	inline bool asm_router_DAA(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 2); }
	inline bool asm_router_DAS(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 3); }

	// MOVS (string) requires disambiguation
	inline bool asm_router_MOVSB(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, 0); }
	inline bool asm_router_MOVSW(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, 1); }
	// MOVSD (string) requires disambiguation
	inline bool asm_router_MOVSQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, 3); }

	inline bool asm_router_CMPS(AssembleArgs &args) { return args.TryProcessCMPS_string(OPCode::string, false, false); }
	inline bool asm_router_CMPSB(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 0); }
	inline bool asm_router_CMPSW(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 1); }
	// CMPSD (string) requires disambiguation
	inline bool asm_router_CMPSQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 3); }

	inline bool asm_router_LODS(AssembleArgs &args) { return args.TryProcessLODS_string(OPCode::string, false); }
	inline bool asm_router_LODSB(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 0); }
	inline bool asm_router_LODSW(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 1); }
	inline bool asm_router_LODSD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 2); }
	inline bool asm_router_LODSQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 3); }

	inline bool asm_router_STOS(AssembleArgs &args) { return args.TryProcessSTOS_string(OPCode::string, false); }
	inline bool asm_router_STOSB(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 0); }
	inline bool asm_router_STOSW(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 1); }
	inline bool asm_router_STOSD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 2); }
	inline bool asm_router_STOSQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 3); }

	inline bool asm_router_SCAS(AssembleArgs &args) { return args.TryProcessSCAS_string(OPCode::string, false, false); }
	inline bool asm_router_SCASB(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 0); }
	inline bool asm_router_SCASW(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 1); }
	inline bool asm_router_SCASD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 2); }
	inline bool asm_router_SCASQ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 3); }

	inline bool asm_router_REP(AssembleArgs &args) { return args.TryProcessREP(); }
	inline bool asm_router_REPE(AssembleArgs &args) { return args.TryProcessREPE(); }
	inline bool asm_router_REPNE(AssembleArgs &args) { return args.TryProcessREPNE(); }

	inline bool asm_router_BSF(AssembleArgs &args) { return args.TryProcessBSx(OPCode::BSx, true); }
	inline bool asm_router_BSR(AssembleArgs &args) { return args.TryProcessBSx(OPCode::BSx, false); }

	inline bool asm_router_TZCNT(AssembleArgs &args) { return args.TryProcessBSx(OPCode::TZCNT, true); } // forward flag doesn't matter

	inline bool asm_router_UD(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::UD); }

	// --------- //

	// -- x87 -- //

	// --------- //

	inline bool asm_router_FNOP(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::NOP); }

	inline bool asm_router_FWAIT(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FWAIT); }

	inline bool asm_router_FNINIT(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FINIT); }
	inline bool asm_router_FINIT(AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_router_FNINIT(args); }

	inline bool asm_router_FNCLEX(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FCLEX); }
	inline bool asm_router_FCLEX(AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_router_FNCLEX(args); }

	inline bool asm_router_FNSTSW(AssembleArgs &args)
	{
		if (args.args.size() == 1 && ToUpper(args.args[0]) == "AX") return args.TryAppendByte((u8)OPCode::FSTLD_WORD) && args.TryAppendByte(0);
		else return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 1, 1);
	}
	inline bool asm_router_FSTSW(AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_router_FNSTSW(args); }

	inline bool asm_router_FNSTCW(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 2, 1); }
	inline bool asm_router_FSTCW(AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_router_FNSTCW(args); }

	inline bool asm_router_FLDCW(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 3, 1); }

	inline bool asm_router_FLD1(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 0); }
	inline bool asm_router_FLDL2T(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 1); }
	inline bool asm_router_FLDL2E(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 2); }
	inline bool asm_router_FLDPI(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 3); }
	inline bool asm_router_FLDLG2(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 4); }
	inline bool asm_router_FLDLN2(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 5); }
	inline bool asm_router_FLDZ(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 6); }

	inline bool asm_router_FLD(AssembleArgs &args) { return args.TryProcessFLD(OPCode::FLD, false); }
	inline bool asm_router_FILD(AssembleArgs &args) { return args.TryProcessFLD(OPCode::FLD, true); }

	inline bool asm_router_FST(AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, false, false, false); }
	inline bool asm_router_FIST(AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, true, false, false); }
	inline bool asm_router_FSTP(AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, false, true, false); }
	inline bool asm_router_FISTP(AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, true, true, false); }
	inline bool asm_router_FISTTP(AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, true, true, true); }

	inline bool asm_router_FXCH(AssembleArgs &args)
	{
		if (args.args.size() == 0) return args.TryProcessNoArgOp(OPCode::FXCH, true, 1);
		else return args.TryProcessFPURegisterOp(OPCode::FXCH);
	}

	inline bool asm_router_FMOVE(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 0); }
	inline bool asm_router_FMOVNE(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 1); }
	inline bool asm_router_FMOVB(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 2); } // FMOVNAE
	inline bool asm_router_FMOVBE(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 3); } // FMOVNA
	inline bool asm_router_FMOVA(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 4); } // FMOVNBE
	inline bool asm_router_FMOVAE(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 5); } // FMOVNB
	inline bool asm_router_FMOVU(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 6); }
	inline bool asm_router_FMOVNU(AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 7); }

	inline bool asm_router_FADD(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FADD, false, false); }
	inline bool asm_router_FADDP(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FADD, false, true); }
	inline bool asm_router_FIADD(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FADD, true, false); }

	inline bool asm_router_FSUB(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUB, false, false); }
	inline bool asm_router_FSUBP(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUB, false, true); }
	inline bool asm_router_FISUB(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUB, true, false); }

	inline bool asm_router_FSUBR(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUBR, false, false); }
	inline bool asm_router_FSUBRP(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUBR, false, true); }
	inline bool asm_router_FISUBR(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUBR, true, false); }

	inline bool asm_router_FMUL(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FMUL, false, false); }
	inline bool asm_router_FMULP(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FMUL, false, true); }
	inline bool asm_router_FIMUL(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FMUL, true, false); }

	inline bool asm_router_FDIV(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIV, false, false); }
	inline bool asm_router_FDIVP(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIV, false, true); }
	inline bool asm_router_FIDIV(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIV, true, false); }

	inline bool asm_router_FDIVR(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIVR, false, false); }
	inline bool asm_router_FDIVRP(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIVR, false, true); }
	inline bool asm_router_FIDIVR(AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIVR, true, false); }

	inline bool asm_router_F2XM1(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::F2XM1); }
	inline bool asm_router_FABS(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FABS); }
	inline bool asm_router_FCHS(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FCHS); }
	inline bool asm_router_FPREM(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPREM); }
	inline bool asm_router_FPREM1(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPREM1); }
	inline bool asm_router_FRNDINT(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FRNDINT); }
	inline bool asm_router_FSQRT(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSQRT); }
	inline bool asm_router_FYL2X(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FYL2X); }
	inline bool asm_router_FYL2XP1(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FYL2XP1); }
	inline bool asm_router_FXTRACT(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FXTRACT); }
	inline bool asm_router_FSCALE(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSCALE); }

	inline bool asm_router_FXAM(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FXAM); }
	inline bool asm_router_FTST(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FTST); }

	inline bool asm_router_FCOM(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, false, false); }
	inline bool asm_router_FCOMP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, false, false); }
	inline bool asm_router_FCOMPP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, true, false, false); }

	inline bool asm_router_FUCOM(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, false, true); }
	inline bool asm_router_FUCOMP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, false, true); }
	inline bool asm_router_FUCOMPP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, true, false, true); }

	inline bool asm_router_FCOMI(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, true, false); }
	inline bool asm_router_FCOMIP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, true, false); }

	inline bool asm_router_FUCOMI(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, true, true); }
	inline bool asm_router_FUCOMIP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, true, true); }

	inline bool asm_router_FICOM(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, true, false, false, false, false); }
	inline bool asm_router_FICOMP(AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, true, true, false, false, false); }

	inline bool asm_router_FSIN(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSIN); }
	inline bool asm_router_FCOS(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FCOS); }
	inline bool asm_router_FSINCOS(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSINCOS); }
	inline bool asm_router_FPTAN(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPTAN); }
	inline bool asm_router_FPATAN(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPATAN); }

	inline bool asm_router_FINCSTP(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FINCDECSTP, true, 0); }
	inline bool asm_router_FDECSTP(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FINCDECSTP, true, 1); }

	inline bool asm_router_FFREE(AssembleArgs &args) { return args.TryProcessFPURegisterOp(OPCode::FFREE); }

	inline bool asm_router_FNSAVE(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 6, ~(u64)0); } // sizecode = 0xff.ff to ensure user can't use explicit size (since it's not a standard size)
	inline bool asm_router_FSAVE(AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_router_FNSAVE(args); }

	inline bool asm_router_FRSTOR(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 7, ~(u64)0); } // sizecode = 0xff.ff to ensure user can't use explicit size (since it's not a standard size)

	inline bool asm_router_FNSTENV(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 8, ~(u64)0); } // sizecode = 0xff.ff to ensure user can't use explicit size (since it's not a standard size)
	inline bool asm_router_FSTENV(AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_router_FNSTENV(args); }

	inline bool asm_router_FLDENV(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 9, ~(u64)0); } // sizecode = 0xff.ff to ensure user can't use explicit size (since it's not a standard size)

	// ------------- //

	// -- vpu ops -- //

	// ------------- //

	inline bool asm_router_MOVQ(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, false, true); }
	inline bool asm_router_MOVD(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, false, false, true); }

	// MOVSD (vec) requires disambiguation
	inline bool asm_router_MOVSS(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, false, false, true); }

	inline bool asm_router_MOVDQA(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, true, false); } // size codes for these 2 don't matter
	inline bool asm_router_MOVDQU(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, false, false); }

	inline bool asm_router_MOVDQA64(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, true, false); }
	inline bool asm_router_MOVDQA32(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, true, false); }
	inline bool asm_router_MOVDQA16(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 1, true, true, false); }
	inline bool asm_router_MOVDQA8(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 0, true, true, false); }

	inline bool asm_router_MOVDQU64(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, false, false); }
	inline bool asm_router_MOVDQU32(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, false, false); }
	inline bool asm_router_MOVDQU16(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 1, true, false, false); }
	inline bool asm_router_MOVDQU8(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 0, true, false, false); }

	inline bool asm_router_MOVAPD(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, true, false); }
	inline bool asm_router_MOVAPS(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, true, false); }

	inline bool asm_router_MOVUPD(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, false, false); }
	inline bool asm_router_MOVUPS(AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, false, false); }

	inline bool asm_router_ADDSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 3, false, false, true); }
	inline bool asm_router_SUBSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 3, false, false, true); }
	inline bool asm_router_MULSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 3, false, false, true); }
	inline bool asm_router_DIVSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 3, false, false, true); }

	inline bool asm_router_ADDSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 2, false, false, true); }
	inline bool asm_router_SUBSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 2, false, false, true); }
	inline bool asm_router_MULSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 2, false, false, true); }
	inline bool asm_router_DIVSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 2, false, false, true); }

	inline bool asm_router_ADDPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 3, true, true, false); }
	inline bool asm_router_SUBPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 3, true, true, false); }
	inline bool asm_router_MULPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 3, true, true, false); }
	inline bool asm_router_DIVPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 3, true, true, false); }

	inline bool asm_router_ADDPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 2, true, true, false); }
	inline bool asm_router_SUBPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 2, true, true, false); }
	inline bool asm_router_MULPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 2, true, true, false); }
	inline bool asm_router_DIVPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 2, true, true, false); }

	inline bool asm_router_PAND(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 3, false, true, false); }
	inline bool asm_router_POR(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 3, false, true, false); }
	inline bool asm_router_PXOR(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 3, false, true, false); }
	inline bool asm_router_PANDN(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 3, false, true, false); }

	inline bool asm_router_PANDQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 3, true, true, false); } // ANDPD
	inline bool asm_router_PORQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 3, true, true, false); } // ORPD
	inline bool asm_router_PXORQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 3, true, true, false); } // XORPD
	inline bool asm_router_PANDNQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 3, true, true, false); } // ANDNPD

	inline bool asm_router_PANDD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 2, true, true, false); } // ANDPS
	inline bool asm_router_PORD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 2, true, true, false); } // ORPS
	inline bool asm_router_PXORD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 2, true, true, false); } // XORPS
	inline bool asm_router_PANDND(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 2, true, true, false); } // ANDNPS

	inline bool asm_router_PADDQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 3, true, true, false); }
	inline bool asm_router_PADDD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 2, true, true, false); }
	inline bool asm_router_PADDW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 1, true, true, false); }
	inline bool asm_router_PADDB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 0, true, true, false); }

	inline bool asm_router_PADDSW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDS, 1, true, true, false); }
	inline bool asm_router_PADDSB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDS, 0, true, true, false); }

	inline bool asm_router_PADDUSW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDUS, 1, true, true, false); }
	inline bool asm_router_PADDUSB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDUS, 0, true, true, false); }

	inline bool asm_router_PSUBQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 3, true, true, false); }
	inline bool asm_router_PSUBD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 2, true, true, false); }
	inline bool asm_router_PSUBW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 1, true, true, false); }
	inline bool asm_router_PSUBB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 0, true, true, false); }

	inline bool asm_router_PSUBSW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBS, 1, true, true, false); }
	inline bool asm_router_PSUBSB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBS, 0, true, true, false); }

	inline bool asm_router_PSUBUSW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBUS, 1, true, true, false); }
	inline bool asm_router_PSUBUSB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBUS, 0, true, true, false); }

	inline bool asm_router_PMULLQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_MULL, 3, true, true, false); }
	inline bool asm_router_PMULLD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_MULL, 2, true, true, false); }
	inline bool asm_router_PMULLW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_MULL, 1, true, true, false); }

	inline bool asm_router_MINSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 3, false, false, true); }
	inline bool asm_router_MINSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 2, false, false, true); }

	inline bool asm_router_MINPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 3, true, true, false); }
	inline bool asm_router_MINPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 2, true, true, false); }

	inline bool asm_router_MAXSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 3, false, false, true); }
	inline bool asm_router_MAXSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 2, false, false, true); }

	inline bool asm_router_MAXPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 3, true, true, false); }
	inline bool asm_router_MAXPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 2, true, true, false); }

	inline bool asm_router_PMINUQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 3, true, true, false); }
	inline bool asm_router_PMINUD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 2, true, true, false); }
	inline bool asm_router_PMINUW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 1, true, true, false); }
	inline bool asm_router_PMINUB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 0, true, true, false); }

	inline bool asm_router_PMINSQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 3, true, true, false); }
	inline bool asm_router_PMINSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 2, true, true, false); }
	inline bool asm_router_PMINSW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 1, true, true, false); }
	inline bool asm_router_PMINSB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 0, true, true, false); }

	inline bool asm_router_PMAXUQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 3, true, true, false); }
	inline bool asm_router_PMAXUD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 2, true, true, false); }
	inline bool asm_router_PMAXUW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 1, true, true, false); }
	inline bool asm_router_PMAXUB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 0, true, true, false); }

	inline bool asm_router_PMAXSQ(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 3, true, true, false); }
	inline bool asm_router_PMAXSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 2, true, true, false); }
	inline bool asm_router_PMAXSW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 1, true, true, false); }
	inline bool asm_router_PMAXSB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 0, true, true, false); }

	inline bool asm_router_ADDSUBPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADDSUB, 3, true, true, false); }
	inline bool asm_router_ADDSUBPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADDSUB, 2, true, true, false); }

	inline bool asm_router_PAVGW(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AVG, 1, true, true, false); }
	inline bool asm_router_PAVGB(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AVG, 0, true, true, false); }

	inline bool asm_router_CMPPD(AssembleArgs &args) { return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 3, true, true, false); }
	inline bool asm_router_CMPPS(AssembleArgs &args) { return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 2, true, true, false); }

	// CMPSD (vec) requires disambiguation
	inline bool asm_router_CMPSS(AssembleArgs &args) { return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 2, false, false, true); }

	// packed double comparisons
	inline bool asm_router_CMPEQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 0); }
	inline bool asm_router_CMPLTPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 1); }
	inline bool asm_router_CMPLEPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 2); }
	inline bool asm_router_CMPUNORDPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 3); }
	inline bool asm_router_CMPNEQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 4); }
	inline bool asm_router_CMPNLTPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 5); }
	inline bool asm_router_CMPNLEPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 6); }
	inline bool asm_router_CMPORDPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 7); }
	inline bool asm_router_CMPEQ_UQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 8); }
	inline bool asm_router_CMPNGEPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 9); }
	inline bool asm_router_CMPNGTPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 10); }
	inline bool asm_router_CMPFALSEPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 11); }
	inline bool asm_router_CMPNEQ_OQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 12); }
	inline bool asm_router_CMPGEPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 13); }
	inline bool asm_router_CMPGTPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 14); }
	inline bool asm_router_CMPTRUEPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 15); }
	inline bool asm_router_CMPEQ_OSPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 16); }
	inline bool asm_router_CMPLT_OQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 17); }
	inline bool asm_router_CMPLE_OQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 18); }
	inline bool asm_router_CMPUNORD_SPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 19); }
	inline bool asm_router_CMPNEQ_USPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 20); }
	inline bool asm_router_CMPNLT_UQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 21); }
	inline bool asm_router_CMPNLE_UQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 22); }
	inline bool asm_router_CMPORD_SPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 23); }
	inline bool asm_router_CMPEQ_USPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 24); }
	inline bool asm_router_CMPNGE_UQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 25); }
	inline bool asm_router_CMPNGT_UQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 26); }
	inline bool asm_router_CMPFALSE_OSPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 27); }
	inline bool asm_router_CMPNEQ_OSPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 28); }
	inline bool asm_router_CMPGE_OQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 29); }
	inline bool asm_router_CMPGT_OQPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 30); }
	inline bool asm_router_CMPTRUE_USPD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 31); }
	
	// packed single comparisons
	inline bool asm_router_CMPEQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 0); }
	inline bool asm_router_CMPLTPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 1); }
	inline bool asm_router_CMPLEPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 2); }
	inline bool asm_router_CMPUNORDPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 3); }
	inline bool asm_router_CMPNEQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 4); }
	inline bool asm_router_CMPNLTPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 5); }
	inline bool asm_router_CMPNLEPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 6); }
	inline bool asm_router_CMPORDPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 7); }
	inline bool asm_router_CMPEQ_UQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 8); }
	inline bool asm_router_CMPNGEPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 9); }
	inline bool asm_router_CMPNGTPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 10); }
	inline bool asm_router_CMPFALSEPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 11); }
	inline bool asm_router_CMPNEQ_OQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 12); }
	inline bool asm_router_CMPGEPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 13); }
	inline bool asm_router_CMPGTPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 14); }
	inline bool asm_router_CMPTRUEPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 15); }
	inline bool asm_router_CMPEQ_OSPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 16); }
	inline bool asm_router_CMPLT_OQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 17); }
	inline bool asm_router_CMPLE_OQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 18); }
	inline bool asm_router_CMPUNORD_SPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 19); }
	inline bool asm_router_CMPNEQ_USPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 20); }
	inline bool asm_router_CMPNLT_UQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 21); }
	inline bool asm_router_CMPNLE_UQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 22); }
	inline bool asm_router_CMPORD_SPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 23); }
	inline bool asm_router_CMPEQ_USPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 24); }
	inline bool asm_router_CMPNGE_UQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 25); }
	inline bool asm_router_CMPNGT_UQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 26); }
	inline bool asm_router_CMPFALSE_OSPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 27); }
	inline bool asm_router_CMPNEQ_OSPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 28); }
	inline bool asm_router_CMPGE_OQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 29); }
	inline bool asm_router_CMPGT_OQPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 30); }
	inline bool asm_router_CMPTRUE_USPS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 31); }

	// scalar double comparisons
	inline bool asm_router_CMPEQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 0); }
	inline bool asm_router_CMPLTSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 1); }
	inline bool asm_router_CMPLESD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 2); }
	inline bool asm_router_CMPUNORDSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 3); }
	inline bool asm_router_CMPNEQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 4); }
	inline bool asm_router_CMPNLTSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 5); }
	inline bool asm_router_CMPNLESD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 6); }
	inline bool asm_router_CMPORDSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 7); }
	inline bool asm_router_CMPEQ_UQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 8); }
	inline bool asm_router_CMPNGESD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 9); }
	inline bool asm_router_CMPNGTSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 10); }
	inline bool asm_router_CMPFALSESD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 11); }
	inline bool asm_router_CMPNEQ_OQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 12); }
	inline bool asm_router_CMPGESD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 13); }
	inline bool asm_router_CMPGTSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 14); }
	inline bool asm_router_CMPTRUESD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 15); }
	inline bool asm_router_CMPEQ_OSSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 16); }
	inline bool asm_router_CMPLT_OQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 17); }
	inline bool asm_router_CMPLE_OQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 18); }
	inline bool asm_router_CMPUNORD_SSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 19); }
	inline bool asm_router_CMPNEQ_USSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 20); }
	inline bool asm_router_CMPNLT_UQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 21); }
	inline bool asm_router_CMPNLE_UQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 22); }
	inline bool asm_router_CMPORD_SSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 23); }
	inline bool asm_router_CMPEQ_USSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 24); }
	inline bool asm_router_CMPNGE_UQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 25); }
	inline bool asm_router_CMPNGT_UQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 26); }
	inline bool asm_router_CMPFALSE_OSSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 27); }
	inline bool asm_router_CMPNEQ_OSSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 28); }
	inline bool asm_router_CMPGE_OQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 29); }
	inline bool asm_router_CMPGT_OQSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 30); }
	inline bool asm_router_CMPTRUE_USSD(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 31); }

	// scalar single comparisons
	inline bool asm_router_CMPEQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 0); }
	inline bool asm_router_CMPLTSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 1); }
	inline bool asm_router_CMPLESS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 2); }
	inline bool asm_router_CMPUNORDSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 3); }
	inline bool asm_router_CMPNEQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 4); }
	inline bool asm_router_CMPNLTSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 5); }
	inline bool asm_router_CMPNLESS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 6); }
	inline bool asm_router_CMPORDSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 7); }
	inline bool asm_router_CMPEQ_UQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 8); }
	inline bool asm_router_CMPNGESS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 9); }
	inline bool asm_router_CMPNGTSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 10); }
	inline bool asm_router_CMPFALSESS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 11); }
	inline bool asm_router_CMPNEQ_OQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 12); }
	inline bool asm_router_CMPGESS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 13); }
	inline bool asm_router_CMPGTSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 14); }
	inline bool asm_router_CMPTRUESS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 15); }
	inline bool asm_router_CMPEQ_OSSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 16); }
	inline bool asm_router_CMPLT_OQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 17); }
	inline bool asm_router_CMPLE_OQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 18); }
	inline bool asm_router_CMPUNORD_SSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 19); }
	inline bool asm_router_CMPNEQ_USSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 20); }
	inline bool asm_router_CMPNLT_UQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 21); }
	inline bool asm_router_CMPNLE_UQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 22); }
	inline bool asm_router_CMPORD_SSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 23); }
	inline bool asm_router_CMPEQ_USSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 24); }
	inline bool asm_router_CMPNGE_UQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 25); }
	inline bool asm_router_CMPNGT_UQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 26); }
	inline bool asm_router_CMPFALSE_OSSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 27); }
	inline bool asm_router_CMPNEQ_OSSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 28); }
	inline bool asm_router_CMPGE_OQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 29); }
	inline bool asm_router_CMPGT_OQSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 30); }
	inline bool asm_router_CMPTRUE_USSS(AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 31); }

	inline bool asm_router_COMISD(AssembleArgs &args) { return args.TryProcessVPUBinary_2arg(OPCode::VPU_FCOMI, 3, false, false, true); }
	inline bool asm_router_COMISS(AssembleArgs &args) { return args.TryProcessVPUBinary_2arg(OPCode::VPU_FCOMI, 2, false, false, true); }

	inline bool asm_router_SQRTPD(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 3, true, true, false); }
	inline bool asm_router_SQRTPS(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 2, true, true, false); }

	inline bool asm_router_SQRTSD(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 3, false, false, true); }
	inline bool asm_router_SQRTSS(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 2, false, false, true); }

	inline bool asm_router_RSQRTPD(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 3, true, true, false); }
	inline bool asm_router_RSQRTPS(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 2, true, true, false); }

	inline bool asm_router_RSQRTSD(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 3, false, false, true); }
	inline bool asm_router_RSQRTSS(AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 2, false, false, true); }

	inline bool asm_router_STMXCSR(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 4, 2); }
	inline bool asm_router_LDMXCSR(AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 5, 2); }

	inline bool asm_router_CVTSD2SI(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, false, false); }
	inline bool asm_router_CVTSS2SI(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, false, true); }
	inline bool asm_router_CVTTSD2SI(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, true, false); }
	inline bool asm_router_CVTTSS2SI(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, true, true); }

	inline bool asm_router_CVTSI2SD(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_i2f(OPCode::VPU_CVT, false); }
	inline bool asm_router_CVTSI2SS(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_i2f(OPCode::VPU_CVT, true); }

	inline bool asm_router_CVTSD2SS(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2f(OPCode::VPU_CVT, false); }
	inline bool asm_router_CVTSS2SD(AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2f(OPCode::VPU_CVT, true); }

	inline bool asm_router_CVTPD2DQ(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, false, false); }
	inline bool asm_router_CVTPS2DQ(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, false, true); }
	inline bool asm_router_CVTTPD2DQ(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, true, false); }
	inline bool asm_router_CVTTPS2DQ(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, true, true); }

	inline bool asm_router_CVTDQ2PD(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_i2f(OPCode::VPU_CVT, false); }
	inline bool asm_router_CVTDQ2PS(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_i2f(OPCode::VPU_CVT, true); }

	inline bool asm_router_CVTPD2PS(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2f(OPCode::VPU_CVT, false); }
	inline bool asm_router_CVTPS2PD(AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2f(OPCode::VPU_CVT, true); }

	// ---------------- //

	// -- CSX64 misc -- //

	// ---------------- //

	inline bool asm_router_DEBUG_CPU(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::DEBUG, true, 0); }
	inline bool asm_router_DEBUG_VPU(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::DEBUG, true, 1); }
	inline bool asm_router_DEBUG_FULL(AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::DEBUG, true, 2); }

	// -------------------- //

	// -- disambiguators -- //

	// -------------------- //

	inline bool asm_router_MOVS_disambig(AssembleArgs &args)
	{
		// MOVS (string) has 2 memory operands
		if (args.args.size() == 2 && args.args[0].back() == ']' && args.args[1].back() == ']')
		{
			return args.TryProcessMOVS_string(OPCode::string, false);
		}
		// otherwise is MOVS (MOVcc)
		else
		{
			return args.TryProcessBinaryOp(OPCode::MOVcc, true, 2);
		}
	}

	inline bool asm_router_MOVSD_disambig(AssembleArgs &args)
	{
		// MOVSD (string) takes no operands
		if (args.args.size() == 0)
		{
			return args.TryProcessNoArgOp(OPCode::string, true, 2);
		}
		// otherwise is MOVSD (vec)
		else
		{
			return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, false, true);
		}
	}

	inline bool asm_router_CMPSD_disambig(AssembleArgs &args)
	{
		// CMPSD (string) takes no operands
		if (args.args.size() == 0)
		{
			return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 2);
		}
		// otherwise is CMPSD (vec)
		else
		{
			return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 3, false, false, true);
		}
	}
}

#endif
