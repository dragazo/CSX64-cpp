#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../include/AsmTables.h"
#include "../include/Expr.h"
#include "../include/Assembly.h"
#include "../include/AsmArgs.h"

namespace CSX64
{
	const char CommentChar = ';';
	const char LabelDefChar = ':';

	const std::string CurrentLineMacro = "$";
	const std::string StartOfSegMacro = "$$";
	const std::string TimesIterIdMacro = "$I";

	const std::unordered_map<Expr::OPs, int> Precedence
	{
		{Expr::OPs::Mul, 5},

	{Expr::OPs::UDiv, 5},
	{Expr::OPs::UMod, 5},

	{Expr::OPs::SDiv, 5},
	{Expr::OPs::SMod, 5},

	{Expr::OPs::Add, 6},
	{Expr::OPs::Sub, 6},

	{Expr::OPs::SL, 7},
	{Expr::OPs::SR, 7},

	{Expr::OPs::Less, 9},
	{Expr::OPs::LessE, 9},
	{Expr::OPs::Great, 9},
	{Expr::OPs::GreatE, 9},

	{Expr::OPs::Eq, 10},
	{Expr::OPs::Neq, 10},

	{Expr::OPs::BitAnd, 11},
	{Expr::OPs::BitXor, 12},
	{Expr::OPs::BitOr, 13},
	{Expr::OPs::LogAnd, 14},
	{Expr::OPs::LogOr, 15},

	{Expr::OPs::NullCoalesce, 99},
	{Expr::OPs::Pair, 100},
	{Expr::OPs::Condition, 100}
	};

	const std::unordered_map<std::string, Expr::OPs> FunctionOperator_to_OP
	{
		{ "$INT", Expr::OPs::Int },
	{ "$FLOAT", Expr::OPs::Float },

	{ "$FLOOR", Expr::OPs::Floor },
	{ "$CEIL", Expr::OPs::Ceil },
	{ "$ROUND", Expr::OPs::Round },
	{ "$TRUNC", Expr::OPs::Trunc },

	{ "$REPR64", Expr::OPs::Repr64 },
	{ "$REPR32", Expr::OPs::Repr32 },

	{ "$FLOAT64", Expr::OPs::Float64 },
	{ "$FLOAT32", Expr::OPs::Float32 },

	{ "$PREC64", Expr::OPs::Prec64 },
	{ "$PREC32", Expr::OPs::Prec32 },
	};

	const std::unordered_map<AsmSegment, std::string> SegOffsets
	{
		{AsmSegment::TEXT, "#t"},
	{AsmSegment::RODATA, "#r"},
	{AsmSegment::DATA, "#d"},
	{AsmSegment::BSS, "#b"}
	};
	const std::unordered_map<AsmSegment, std::string> SegOrigins
	{
		{AsmSegment::TEXT, "#T"},
	{AsmSegment::RODATA, "#R"},
	{AsmSegment::DATA, "#D"},
	{AsmSegment::BSS, "#B"}
	};

	const std::unordered_set<std::string> PtrdiffIDs
	{
		"#t", "#r", "#d", "#b",
		"#T", "#R", "#D", "#B",

		"__heap__",
	};

	const std::unordered_set<std::string> VerifyLegalExpressionIgnores
	{
		"__heap__"
	};

	const std::unordered_set<std::string> AdditionalReservedSymbols
	{
		"BYTE", "WORD", "DWORD", "QWORD", "XMMWORD", "YMMWORD", "ZMMWORD",
		"OWORD", "TWORD"
	};

	// Maps CPU register names (all caps) to tuples of (id, sizecode, high)
	const std::unordered_map<std::string, std::tuple<u8, u8, bool>> CPURegisterInfo
	{
		{"RAX", std::tuple<u8, u8, bool>(0, 3, false)},
	{"RBX", std::tuple<u8, u8, bool>(1, 3, false)},
	{"RCX", std::tuple<u8, u8, bool>(2, 3, false)},
	{"RDX", std::tuple<u8, u8, bool>(3, 3, false)},
	{"RSI", std::tuple<u8, u8, bool>(4, 3, false)},
	{"RDI", std::tuple<u8, u8, bool>(5, 3, false)},
	{"RBP", std::tuple<u8, u8, bool>(6, 3, false)},
	{"RSP", std::tuple<u8, u8, bool>(7, 3, false)},
	{"R8", std::tuple<u8, u8, bool>(8, 3, false)},
	{"R9", std::tuple<u8, u8, bool>(9, 3, false)},
	{"R10", std::tuple<u8, u8, bool>(10, 3, false)},
	{"R11", std::tuple<u8, u8, bool>(11, 3, false)},
	{"R12", std::tuple<u8, u8, bool>(12, 3, false)},
	{"R13", std::tuple<u8, u8, bool>(13, 3, false)},
	{"R14", std::tuple<u8, u8, bool>(14, 3, false)},
	{"R15", std::tuple<u8, u8, bool>(15, 3, false)},

	{"EAX", std::tuple<u8, u8, bool>(0, 2, false)},
	{"EBX", std::tuple<u8, u8, bool>(1, 2, false)},
	{"ECX", std::tuple<u8, u8, bool>(2, 2, false)},
	{"EDX", std::tuple<u8, u8, bool>(3, 2, false)},
	{"ESI", std::tuple<u8, u8, bool>(4, 2, false)},
	{"EDI", std::tuple<u8, u8, bool>(5, 2, false)},
	{"EBP", std::tuple<u8, u8, bool>(6, 2, false)},
	{"ESP", std::tuple<u8, u8, bool>(7, 2, false)},
	{"R8D", std::tuple<u8, u8, bool>(8, 2, false)},
	{"R9D", std::tuple<u8, u8, bool>(9, 2, false)},
	{"R10D", std::tuple<u8, u8, bool>(10, 2, false)},
	{"R11D", std::tuple<u8, u8, bool>(11, 2, false)},
	{"R12D", std::tuple<u8, u8, bool>(12, 2, false)},
	{"R13D", std::tuple<u8, u8, bool>(13, 2, false)},
	{"R14D", std::tuple<u8, u8, bool>(14, 2, false)},
	{"R15D", std::tuple<u8, u8, bool>(15, 2, false)},

	{"AX", std::tuple<u8, u8, bool>(0, 1, false)},
	{"BX", std::tuple<u8, u8, bool>(1, 1, false)},
	{"CX", std::tuple<u8, u8, bool>(2, 1, false)},
	{"DX", std::tuple<u8, u8, bool>(3, 1, false)},
	{"SI", std::tuple<u8, u8, bool>(4, 1, false)},
	{"DI", std::tuple<u8, u8, bool>(5, 1, false)},
	{"BP", std::tuple<u8, u8, bool>(6, 1, false)},
	{"SP", std::tuple<u8, u8, bool>(7, 1, false)},
	{"R8W", std::tuple<u8, u8, bool>(8, 1, false)},
	{"R9W", std::tuple<u8, u8, bool>(9, 1, false)},
	{"R10W", std::tuple<u8, u8, bool>(10, 1, false)},
	{"R11W", std::tuple<u8, u8, bool>(11, 1, false)},
	{"R12W", std::tuple<u8, u8, bool>(12, 1, false)},
	{"R13W", std::tuple<u8, u8, bool>(13, 1, false)},
	{"R14W", std::tuple<u8, u8, bool>(14, 1, false)},
	{"R15W", std::tuple<u8, u8, bool>(15, 1, false)},

	{"AL", std::tuple<u8, u8, bool>(0, 0, false)},
	{"BL", std::tuple<u8, u8, bool>(1, 0, false)},
	{"CL", std::tuple<u8, u8, bool>(2, 0, false)},
	{"DL", std::tuple<u8, u8, bool>(3, 0, false)},
	{"SIL", std::tuple<u8, u8, bool>(4, 0, false)},
	{"DIL", std::tuple<u8, u8, bool>(5, 0, false)},
	{"BPL", std::tuple<u8, u8, bool>(6, 0, false)},
	{"SPL", std::tuple<u8, u8, bool>(7, 0, false)},
	{"R8B", std::tuple<u8, u8, bool>(8, 0, false)},
	{"R9B", std::tuple<u8, u8, bool>(9, 0, false)},
	{"R10B", std::tuple<u8, u8, bool>(10, 0, false)},
	{"R11B", std::tuple<u8, u8, bool>(11, 0, false)},
	{"R12B", std::tuple<u8, u8, bool>(12, 0, false)},
	{"R13B", std::tuple<u8, u8, bool>(13, 0, false)},
	{"R14B", std::tuple<u8, u8, bool>(14, 0, false)},
	{"R15B", std::tuple<u8, u8, bool>(15, 0, false)},

	{"AH", std::tuple<u8, u8, bool>(0, 0, true)},
	{"BH", std::tuple<u8, u8, bool>(1, 0, true)},
	{"CH", std::tuple<u8, u8, bool>(2, 0, true)},
	{"DH", std::tuple<u8, u8, bool>(3, 0, true)}
	};

	// Maps FPU register names (all caps) to their ids
	const std::unordered_map<std::string, u8> FPURegisterInfo
	{
		{"ST", 0},

	{"ST0", 0},
	{"ST1", 1},
	{"ST2", 2},
	{"ST3", 3},
	{"ST4", 4},
	{"ST5", 5},
	{"ST6", 6},
	{"ST7", 7},

	{"ST(0)", 0},
	{"ST(1)", 1},
	{"ST(2)", 2},
	{"ST(3)", 3},
	{"ST(4)", 4},
	{"ST(5)", 5},
	{"ST(6)", 6},
	{"ST(7)", 7}
	};

	// Maps VPU register names (all caps) to tuples of (id, sizecode)
	const std::unordered_map<std::string, std::tuple<u8, u8>> VPURegisterInfo
	{
		{"XMM0", std::tuple<u8, u8>(0, 4)},
	{"XMM1", std::tuple<u8, u8>(1, 4)},
	{"XMM2", std::tuple<u8, u8>(2, 4)},
	{"XMM3", std::tuple<u8, u8>(3, 4)},
	{"XMM4", std::tuple<u8, u8>(4, 4)},
	{"XMM5", std::tuple<u8, u8>(5, 4)},
	{"XMM6", std::tuple<u8, u8>(6, 4)},
	{"XMM7", std::tuple<u8, u8>(7, 4)},
	{"XMM8", std::tuple<u8, u8>(8, 4)},
	{"XMM9", std::tuple<u8, u8>(9, 4)},
	{"XMM10", std::tuple<u8, u8>(10, 4)},
	{"XMM11", std::tuple<u8, u8>(11, 4)},
	{"XMM12", std::tuple<u8, u8>(12, 4)},
	{"XMM13", std::tuple<u8, u8>(13, 4)},
	{"XMM14", std::tuple<u8, u8>(14, 4)},
	{"XMM15", std::tuple<u8, u8>(15, 4)},

	{"YMM0", std::tuple<u8, u8>(0, 5)},
	{"YMM1", std::tuple<u8, u8>(1, 5)},
	{"YMM2", std::tuple<u8, u8>(2, 5)},
	{"YMM3", std::tuple<u8, u8>(3, 5)},
	{"YMM4", std::tuple<u8, u8>(4, 5)},
	{"YMM5", std::tuple<u8, u8>(5, 5)},
	{"YMM6", std::tuple<u8, u8>(6, 5)},
	{"YMM7", std::tuple<u8, u8>(7, 5)},
	{"YMM8", std::tuple<u8, u8>(8, 5)},
	{"YMM9", std::tuple<u8, u8>(9, 5)},
	{"YMM10", std::tuple<u8, u8>(10, 5)},
	{"YMM11", std::tuple<u8, u8>(11, 5)},
	{"YMM12", std::tuple<u8, u8>(12, 5)},
	{"YMM13", std::tuple<u8, u8>(13, 5)},
	{"YMM14", std::tuple<u8, u8>(14, 5)},
	{"YMM15", std::tuple<u8, u8>(15, 5)},

	{"ZMM0", std::tuple<u8, u8>(0, 6)},
	{"ZMM1", std::tuple<u8, u8>(1, 6)},
	{"ZMM2", std::tuple<u8, u8>(2, 6)},
	{"ZMM3", std::tuple<u8, u8>(3, 6)},
	{"ZMM4", std::tuple<u8, u8>(4, 6)},
	{"ZMM5", std::tuple<u8, u8>(5, 6)},
	{"ZMM6", std::tuple<u8, u8>(6, 6)},
	{"ZMM7", std::tuple<u8, u8>(7, 6)},
	{"ZMM8", std::tuple<u8, u8>(8, 6)},
	{"ZMM9", std::tuple<u8, u8>(9, 6)},
	{"ZMM10", std::tuple<u8, u8>(10, 6)},
	{"ZMM11", std::tuple<u8, u8>(11, 6)},
	{"ZMM12", std::tuple<u8, u8>(12, 6)},
	{"ZMM13", std::tuple<u8, u8>(13, 6)},
	{"ZMM14", std::tuple<u8, u8>(14, 6)},
	{"ZMM15", std::tuple<u8, u8>(15, 6)},
	{"ZMM16", std::tuple<u8, u8>(16, 6)},
	{"ZMM17", std::tuple<u8, u8>(17, 6)},
	{"ZMM18", std::tuple<u8, u8>(18, 6)},
	{"ZMM19", std::tuple<u8, u8>(19, 6)},
	{"ZMM20", std::tuple<u8, u8>(20, 6)},
	{"ZMM21", std::tuple<u8, u8>(21, 6)},
	{"ZMM22", std::tuple<u8, u8>(22, 6)},
	{"ZMM23", std::tuple<u8, u8>(23, 6)},
	{"ZMM24", std::tuple<u8, u8>(24, 6)},
	{"ZMM25", std::tuple<u8, u8>(25, 6)},
	{"ZMM26", std::tuple<u8, u8>(26, 6)},
	{"ZMM27", std::tuple<u8, u8>(27, 6)},
	{"ZMM28", std::tuple<u8, u8>(28, 6)},
	{"ZMM29", std::tuple<u8, u8>(29, 6)},
	{"ZMM30", std::tuple<u8, u8>(30, 6)},
	{"ZMM31", std::tuple<u8, u8>(31, 6)}
	};

	const std::unordered_map<std::string, asm_router> asm_routing_table
	{
		// ---------------- //

		// -- directives -- //

		// ---------------- //

		{"GLOBAL", [](AssembleArgs &args) { return args.TryProcessGlobal(); }},
	{"EXTERN", [](AssembleArgs &args) { return args.TryProcessExtern(); }},

	{"ALIGN", [](AssembleArgs &args) { return args.TryProcessAlign(); }},

	{"ALIGNB", [](AssembleArgs&) { return true; }},
	{"ALIGNW", [](AssembleArgs &args) { return args.TryProcessAlignXX(2); }},
	{"ALIGND", [](AssembleArgs &args) { return args.TryProcessAlignXX(4); }},
	{"ALIGNQ", [](AssembleArgs &args) { return args.TryProcessAlignXX(8); }},
	{"ALIGNX", [](AssembleArgs &args) { return args.TryProcessAlignXX(16); }},
	{"ALIGNY", [](AssembleArgs &args) { return args.TryProcessAlignXX(32); }},
	{"ALIGNZ", [](AssembleArgs &args) { return args.TryProcessAlignXX(64); }},

	{"DB", [](AssembleArgs &args) { return args.TryProcessDeclare(1); }},
	{"DW", [](AssembleArgs &args) { return args.TryProcessDeclare(2); }},
	{"DD", [](AssembleArgs &args) { return args.TryProcessDeclare(4); }},
	{"DQ", [](AssembleArgs &args) { return args.TryProcessDeclare(8); }},
	{"DX", [](AssembleArgs &args) { return args.TryProcessDeclare(16); }},
	{"DY", [](AssembleArgs &args) { return args.TryProcessDeclare(32); }},
	{"DZ", [](AssembleArgs &args) { return args.TryProcessDeclare(64); }},

	{"RESB", [](AssembleArgs &args) { return args.TryProcessReserve(1); }},
	{"RESW", [](AssembleArgs &args) { return args.TryProcessReserve(2); }},
	{"RESD", [](AssembleArgs &args) { return args.TryProcessReserve(4); }},
	{"RESQ", [](AssembleArgs &args) { return args.TryProcessReserve(8); }},
	{"RESX", [](AssembleArgs &args) { return args.TryProcessReserve(16); }},
	{"RESY", [](AssembleArgs &args) { return args.TryProcessReserve(32); }},
	{"RESZ", [](AssembleArgs &args) { return args.TryProcessReserve(64); }},

	{"EQU", [](AssembleArgs &args) { return args.TryProcessEQU(); }},

	{"STATIC_ASSERT", [](AssembleArgs &args) { return args.TryProcessStaticAssert(); }},

	{"SEGMENT", [](AssembleArgs &args) { return args.TryProcessSegment(); }},
	{"SECTION", [](AssembleArgs &args) { return args.TryProcessSegment(); }},

	{"INCBIN", [](AssembleArgs &args) { return args.TryProcessINCBIN(); }},

	// -------------- //

	// -- unmapped -- //

	// -------------- //

	{"LFENCE", [](AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }},
	{"SFENCE", [](AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }},
	{"MFENCE", [](AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }},

	{"PAUSE", [](AssembleArgs &args) { return args.TryProcessNoArgOp_no_write(); }},

	// --------- //

	// -- x86 -- //

	// --------- //

	{"NOP", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::NOP); }},

	{"HLT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::HLT); }},
	{"SYSCALL", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::SYSCALL); }},

	{"PUSHF", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 0); }},
	{"PUSHFD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 1); }},
	{"PUSHFQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 2); }},

	{"POPF", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 3); }},
	{"POPFD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 4); }},
	{"POPFQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 5); }},

	{"SAHF", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 6); }},
	{"LAHF", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::STLDF, true, 7); }},

	{"STC", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 0); }},
	{"CLC", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 1); }},
	{"STI", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 2); }},
	{"CLI", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 3); }},
	{"STD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 4); }},
	{"CLD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 5); }},
	{"STAC", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 6); }},
	{"CLAC", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 7); }},
	{"CMC", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FlagManip, true, 8); }},

	{"SETZ", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 0, 1); }},
	{"SETE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 0, 1); }},
	{"SETNZ", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 1, 1); }},
	{"SETNE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 1, 1); }},
	{"SETS", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 2, 1); }},
	{"SETNS", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 3, 1); }},
	{"SETP", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 4, 1); }},
	{"SETPE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 4, 1); }},
	{"SETNP", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 5, 1); }},
	{"SETPO", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 5, 1); }},
	{"SETO", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 6, 1); }},
	{"SETNO", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 7, 1); }},
	{"SETC", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 8, 1); }},
	{"SETNC", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 9, 1); }},

	{"SETB", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 10, 1); }},
	{"SETNAE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 10, 1); }},
	{"SETBE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 11, 1); }},
	{"SETA", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 11, 1); }},
	{"SETA", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 12, 1); }},
	{"SETNBE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 12, 1); }},
	{"SETAE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 13, 1); }},
	{"SETNB", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 13, 1); }},

	{"SETL", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 14, 1); }},
	{"SETNGE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 14, 1); }},
	{"SETLE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 15, 1); }},
	{"SETNG", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 15, 1); }},
	{"SETG", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 16, 1); }},
	{"SETNLE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 16, 1); }},
	{"SETGE", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 17, 1); }},
	{"SETNL", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::SETcc, true, 17, 1); }},

	{"MOV", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOV); }},

	{"MOVZ", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 0); }},
	{"MOVE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 0); }},
	{"MOVNZ", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 1); }},
	{"MOVNE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 1); }},
	// MOVS (mov) requires disambiguation
	{"MOVNS", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 3); }},
	{"MOVP", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 4); }},
	{"MOVPE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 4); }},
	{"MOVNP", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 5); }},
	{"MOVPO", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 5); }},
	{"MOVO", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 6); }},
	{"MOVNO", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 7); }},
	{"MOVC", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 8); }},
	{"MOVNC", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 9); }},

	{"MOVB", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 10); }},
	{"MOVNAE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 10); }},
	{"MOVBE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 11); }},
	{"MOVA", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 11); }},
	{"MOVA", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 12); }},
	{"MOVNBE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 12); }},
	{"MOVAE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 13); }},
	{"MOVNB", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 13); }},

	{"MOVL", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 14); }},
	{"MOVNGE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 14); }},
	{"MOVLE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 15); }},
	{"MOVNG", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 15); }},
	{"MOVG", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 16); }},
	{"MOVNLE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 16); }},
	{"MOVGE", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 17); }},
	{"MOVNL", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::MOVcc, true, 17); }},

	{"XCHG", [](AssembleArgs &args) { return args.TryProcessXCHG(OPCode::XCHG); }},

	{"JMP", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::JMP, false, 0, 14, 3); }},

	{"JZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 0, 14, 3); }},
	{"JE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 0, 14, 3); }},
	{"JNZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 1, 14, 3); }},
	{"JNE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 1, 14, 3); }},
	{"JS", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 2, 14, 3); }},
	{"JNS", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 3, 14, 3); }},
	{"JP", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 4, 14, 3); }},
	{"JPE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 4, 14, 3); }},
	{"JNP", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 5, 14, 3); }},
	{"JPO", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 5, 14, 3); }},
	{"JO", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 6, 14, 3); }},
	{"JNO", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 7, 14, 3); }},
	{"JC", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 8, 14, 3); }},
	{"JNC", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 9, 14, 3); }},

	{"JB", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 10, 14, 3); }},
	{"JNAE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 10, 14, 3); }},
	{"JBE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 11, 14, 3); }},
	{"JNA", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 11, 14, 3); }},
	{"JA", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 12, 14, 3); }},
	{"JNBE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 12, 14, 3); }},
	{"JAE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 13, 14, 3); }},
	{"JNB", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 13, 14, 3); }},

	{"JL", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 14, 14, 3); }},
	{"JNGE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 14, 14, 3); }},
	{"JLE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 15, 14, 3); }},
	{"JNG", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 15, 14, 3); }},
	{"JG", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 16, 14, 3); }},
	{"JNLE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 16, 14, 3); }},
	{"JGE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 17, 14, 3); }},
	{"JNL", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 17, 14, 3); }},

	{"JCXZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 18, 2, 1); }},
	{"JECXZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 18, 4, 2); }},
	{"JRCXZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::Jcc, true, 18, 8, 3); }},

	{"LOOP", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 0, 14, 3); }},
	{"LOOPZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 1, 14, 3); }},
	{"LOOPE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 1, 14, 3); }},
	{"LOOPNZ", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 2, 14, 3); }},
	{"LOOPNE", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::LOOPcc, true, 2, 14, 3); }},

	{"CALL", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::CALL, false, 0, 14, 3); }},
	{"RET", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::RET); }},

	{"PUSH", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::PUSH, false, 0, 14); }},
	{"POP", [](AssembleArgs &args) { return args.TryProcessPOP(OPCode::POP); }},

	{"LEA", [](AssembleArgs &args) { return args.TryProcessLEA(OPCode::LEA); }},

	{"ADD", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::ADD); }},
	{"SUB", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::SUB); }},

	{"MUL", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::MUL_x, true, 0); }},
	{"MULX", [](AssembleArgs &args) { return args.TryProcessRR_RM(OPCode::MUL_x, true, 1, 12); }},
	{"IMUL", [](AssembleArgs &args)
		{
			switch (args.args.size())
			{
			case 1: return args.TryProcessIMMRM(OPCode::IMUL, true, 0);
			case 2: return args.TryProcessBinaryOp(OPCode::IMUL, true, 1);
			case 3: return args.TryProcessTernaryOp(OPCode::IMUL, true, 2);

			default: args.res = {AssembleError::ArgCount, "line " + tostr(args.line) + ": IMUL expected 1, 2, or 3 args"}; return false;
			}
		}},
	{"DIV", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::DIV); }},
	{"IDIV", [](AssembleArgs &args) { return args.TryProcessIMMRM(OPCode::IDIV); }},

	{"SHL", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::SHL); }},
	{"SHR", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::SHR); }},
	{"SAL", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::SAL); }},
	{"SAR", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::SAR); }},
	{"ROL", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::ROL); }},
	{"ROR", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::ROR); }},
	{"RCL", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::RCL); }},
	{"RCR", [](AssembleArgs &args) { return args.TryProcessShift(OPCode::RCR); }},

	{"AND", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::AND); }},
	{"OR", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::OR); }},
	{"XOR", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::XOR); }},

	{"INC", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::INC); }},
	{"DEC", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::DEC); }},
	{"NEG", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::NEG); }},
	{"NOT", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::NOT); }},

	{"CMP", [](AssembleArgs &args)
		{
			// if there are 2 args and the second one is an instant imm 0 and it doesn't have a strict specifier, we can make this a CMPZ instruction
			u64 a, b;
			bool floating, btemp, strict;
			if (args.args.size() == 2 && args.TryParseInstantImm(args.args[1], a, floating, b, btemp, strict) && a == 0 && !strict)
			{
				// set new args for the unary version
				args.args.resize(1);
				return args.TryProcessUnaryOp(OPCode::CMPZ);
			}
			// otherwise normal binary
			else return args.TryProcessBinaryOp(OPCode::CMP);
		}},
	{"TEST", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::TEST); }},

	{"BSWAP", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BSWAP); }},
	{"BEXTR", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::BEXTR, false, 0, 15, 1); }},
	{"BLSI", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BLSI); }},
	{"BLSMSK", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BLSMSK); }},
	{"BLSR", [](AssembleArgs &args) { return args.TryProcessUnaryOp(OPCode::BLSR); }},
	{"ANDN", [](AssembleArgs &args) { return args.TryProcessRR_RM(OPCode::ANDN, false, 0, 12); }},

	{"BT", [](AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 0, 15, 0); }},
	{"BTS", [](AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 1, 15, 0); }},
	{"BTR", [](AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 2, 15, 0); }},
	{"BTC", [](AssembleArgs &args) { return args.TryProcessBinaryOp_NoBMem(OPCode::BTx, true, 3, 15, 0); }},

	{"CWD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 0); }},
	{"CDQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 1); }},
	{"CQO", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 2); }},

	{"CBW", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 3); }},
	{"CWDE", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 4); }},
	{"CDQE", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::Cxy, true, 5); }},

	{"MOVZX", [](AssembleArgs &args) { return args.TryProcessMOVxX(OPCode::MOVxX, false); }},
	{"MOVSX", [](AssembleArgs &args) { return args.TryProcessMOVxX(OPCode::MOVxX, true); }},

	{"ADC", [](AssembleArgs &args) { return args.TryProcessBinaryOp(OPCode::ADXX, true, 0); }},
	{"ADCX", [](AssembleArgs &args) { return args.TryProcessBinaryOp_R_RM(OPCode::ADXX, true, 1, 12); }},
	{"ADOX", [](AssembleArgs &args) { return args.TryProcessBinaryOp_R_RM(OPCode::ADXX, true, 2, 12); }},

	{"AAA", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 0); }},
	{"AAS", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 1); }},
	{"DAA", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 2); }},
	{"DAS", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::AAX, true, 3); }},

	// MOVS (string) requires disambiguation
	{"MOVSB", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, 0); }},
	{"MOVSW", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, 1); }},
	// MOVSD (string) requires disambiguation
	{"MOVSQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, 3); }},

	{"CMPS", [](AssembleArgs &args) { return args.TryProcessCMPS_string(OPCode::string, false, false); }},
	{"CMPSB", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 0); }},
	{"CMPSW", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 1); }},
	// CMPSD (string) requires disambiguation
	{"CMPSQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (2 << 2) | 3); }},

	{"LODS", [](AssembleArgs &args) { return args.TryProcessLODS_string(OPCode::string, false); }},
	{"LODSB", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 0); }},
	{"LODSW", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 1); }},
	{"LODSD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 2); }},
	{"LODSQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (5 << 2) | 3); }},

	{"STOS", [](AssembleArgs &args) { return args.TryProcessSTOS_string(OPCode::string, false); }},
	{"STOSB", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 0); }},
	{"STOSW", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 1); }},
	{"STOSD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 2); }},
	{"STOSQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (7 << 2) | 3); }},

	{"SCAS", [](AssembleArgs &args) { return args.TryProcessSCAS_string(OPCode::string, false, false); }},
	{"SCASB", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 0); }},
	{"SCASW", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 1); }},
	{"SCASD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 2); }},
	{"SCASQ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::string, true, (9 << 2) | 3); }},

	// REP pseudo-instructions - will extract the actual instruction internally
	{"REP", [](AssembleArgs &args) { return args.TryProcessREP(); }},

	{"REPE", [](AssembleArgs &args) { return args.TryProcessREPE(); }},
	{"REPZ", [](AssembleArgs &args) { return args.TryProcessREPE(); }},

	{"REPNE", [](AssembleArgs &args) { return args.TryProcessREPNE(); }},
	{"REPNZ", [](AssembleArgs &args) { return args.TryProcessREPNE(); }},

	{"LOCK", [](AssembleArgs &args) { return args.TryProcessLOCK(); }},

	{"BSF", [](AssembleArgs &args) { return args.TryProcessBSx(OPCode::BSx, true); }},
	{"BSR", [](AssembleArgs &args) { return args.TryProcessBSx(OPCode::BSx, false); }},

	{"TZCNT", [](AssembleArgs &args) { return args.TryProcessBSx(OPCode::TZCNT, true); }},

	{"UD", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::UD); }},

	// --------- //

	// -- x87 -- //

	// --------- //

	{"FNOP", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::NOP); }},

	{"FWAIT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FWAIT); }},
	{"WAIT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FWAIT); }}, 
	{"FNINIT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FINIT); }},
	{"FINIT", [](AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_routing_table.at("FNINIT")(args); }},

	{"FNCLEX", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FCLEX); }},
	{"FCLEX", [](AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_routing_table.at("FNCLEX")(args); }},

	{"FNSTSW", [](AssembleArgs &args)
		{
			if (args.args.size() == 1 && ToUpper(args.args[0]) == "AX") return args.TryAppendByte((u8)OPCode::FSTLD_WORD) && args.TryAppendByte(0);
			else return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 1, 1);
		}},
	{"FSTSW", [](AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_routing_table.at("FNSTSW")(args); }},

	{"FNSTCW", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 2, 1); }},
	{"FSTCW", [](AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_routing_table.at("FNSTCW")(args); }},

	{"FLDCW", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 3, 1); }},

	{"FLD1", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 0); }},
	{"FLDL2T", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 1); }},
	{"FLDL2E", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 2); }},
	{"FLDPI", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 3); }},
	{"FLDLG2", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 4); }},
	{"FLDLN2", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 5); }},
	{"FLDZ", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FLD_const, true, 6); }},

	{"FLD", [](AssembleArgs &args) { return args.TryProcessFLD(OPCode::FLD, false); }},
	{"FILD", [](AssembleArgs &args) { return args.TryProcessFLD(OPCode::FLD, true); }},

	{"FST", [](AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, false, false, false); }},
	{"FIST", [](AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, true, false, false); }},
	{"FSTP", [](AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, false, true, false); }},
	{"FISTP", [](AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, true, true, false); }},
	{"FISTTP", [](AssembleArgs &args) { return args.TryProcessFST(OPCode::FST, true, true, true); }},

	{"FXCH", [](AssembleArgs &args)
		{
			if (args.args.size() == 0) return args.TryProcessNoArgOp(OPCode::FXCH, true, 1);
			else return args.TryProcessFPURegisterOp(OPCode::FXCH);
		}},

	{"FMOVE", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 0); }},
	{"FMOVNE", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 1); }},
	{"FMOVB", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 2); }},
	{"FMOVNAE", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 2); }},
	{"FMOVBE", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 3); }},
	{"FMOVNA", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 3); }},
	{"FMOVA", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 4); }},
	{"FMOVNBE", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 4); }},
	{"FMOVAE", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 5); }},
	{"FMOVB", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 5); }},
	{"FMOVU", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 6); }},
	{"FMOVNU", [](AssembleArgs &args) { return args.TryProcessFMOVcc(OPCode::FMOVcc, 7); }},

	{"FADD", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FADD, false, false); }},
	{"FADDP", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FADD, false, true); }},
	{"FIADD", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FADD, true, false); }},

	{"FSUB", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUB, false, false); }},
	{"FSUBP", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUB, false, true); }},
	{"FISUB", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUB, true, false); }},

	{"FSUBR", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUBR, false, false); }},
	{"FSUBRP", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUBR, false, true); }},
	{"FISUBR", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FSUBR, true, false); }},

	{"FMUL", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FMUL, false, false); }},
	{"FMULP", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FMUL, false, true); }},
	{"FIMUL", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FMUL, true, false); }},

	{"FDIV", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIV, false, false); }},
	{"FDIVP", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIV, false, true); }},
	{"FIDIV", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIV, true, false); }},

	{"FDIVR", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIVR, false, false); }},
	{"FDIVRP", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIVR, false, true); }},
	{"FIDIVR", [](AssembleArgs &args) { return args.TryProcessFPUBinaryOp(OPCode::FDIVR, true, false); }},

	{"F2XM1", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::F2XM1); }},
	{"FABS", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FABS); }},
	{"FCHS", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FCHS); }},
	{"FPREM", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPREM); }},
	{"FPREM1", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPREM1); }},
	{"FRNDINT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FRNDINT); }},
	{"FSQRT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSQRT); }},
	{"FYL2X", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FYL2X); }},
	{"FYL2XP1", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FYL2XP1); }},
	{"FXTRACT", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FXTRACT); }},
	{"FSCALE", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSCALE); }},

	{"FXAM", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FXAM); }},
	{"FTST", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FTST); }},

	{"FCOM", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, false, false); }},
	{"FCOMP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, false, false); }},
	{"FCOMPP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, true, false, false); }},

	{"FUCOM", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, false, true); }},
	{"FUCOMP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, false, true); }},
	{"FUCOMPP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, true, false, true); }},

	{"FCOMI", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, true, false); }},
	{"FCOMIP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, true, false); }},

	{"FUCOMI", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, false, false, true, true); }},
	{"FUCOMIP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, false, true, false, true, true); }},

	{"FICOM", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, true, false, false, false, false); }},
	{"FICOMP", [](AssembleArgs &args) { return args.TryProcessFCOM(OPCode::FCOM, true, true, false, false, false); }},

	{"FSIN", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSIN); }},
	{"FCOS", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FCOS); }},
	{"FSINCOS", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FSINCOS); }},
	{"FPTAN", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPTAN); }},
	{"FPATAN", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FPATAN); }},

	{"FINCSTP", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FINCDECSTP, true, 0); }},
	{"FDECSTP", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::FINCDECSTP, true, 1); }},

	{"FFREE", [](AssembleArgs &args) { return args.TryProcessFPURegisterOp(OPCode::FFREE); }},

	{"FNSAVE", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 6, ~(u64)0); }},
	{"FSAVE", [](AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_routing_table.at("FNSAVE")(args); }},

	{"FRSTOR", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 7, ~(u64)0); }},

	{"FNSTENV", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 8, ~(u64)0); }},
	{"FSTENV", [](AssembleArgs &args) { return args.TryAppendByte((u8)OPCode::FWAIT) && asm_routing_table.at("FNSTENV")(args); }},

	{"FLDENV", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 9, ~(u64)0); }},

	// ------------- //

	// -- vec ops -- //

	// ------------- //

	{"MOVQ", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, false, true); }},
	{"MOVD", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, false, false, true); }},

	// MOVSD (vec) requires disambiguation
	{"MOVSS", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, false, false, true); }},

	{"MOVDQA", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, true, false); }},
	{"MOVDQU", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, false, false, false); }},

	{"MOVDQA64", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, true, false); }},
	{"MOVDQA32", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, true, false); }},
	{"MOVDQA16", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 1, true, true, false); }},
	{"MOVDQA8", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 0, true, true, false); }},

	{"MOVDQU64", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, false, false); }},
	{"MOVDQU32", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, false, false); }},
	{"MOVDQU16", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 1, true, false, false); }},
	{"MOVDQU8", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 0, true, false, false); }},

	{"MOVAPD", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, true, false); }},
	{"MOVAPS", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, true, false); }},

	{"MOVUPD", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 3, true, false, false); }},
	{"MOVUPS", [](AssembleArgs &args) { return args.TryProcessVPUMove(OPCode::VPU_MOV, 2, true, false, false); }},

	{"ADDSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 3, false, false, true); }},
	{"SUBSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 3, false, false, true); }},
	{"MULSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 3, false, false, true); }},
	{"DIVSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 3, false, false, true); }},

	{"ADDSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 2, false, false, true); }},
	{"SUBSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 2, false, false, true); }},
	{"MULSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 2, false, false, true); }},
	{"DIVSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 2, false, false, true); }},

	{"ADDPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 3, true, true, false); }},
	{"SUBPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 3, true, true, false); }},
	{"MULPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 3, true, true, false); }},
	{"DIVPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 3, true, true, false); }},

	{"ADDPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADD, 2, true, true, false); }},
	{"SUBPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FSUB, 2, true, true, false); }},
	{"MULPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMUL, 2, true, true, false); }},
	{"DIVPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FDIV, 2, true, true, false); }},

	{"PAND", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 3, false, true, false); }},
	{"POR", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 3, false, true, false); }},
	{"PXOR", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 3, false, true, false); }},
	{"PANDN", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 3, false, true, false); }},

	{"PANDQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 3, true, true, false); }},
	{"ANDPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 3, true, true, false); }},
	{"PORQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 3, true, true, false); }},
	{"ORPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 3, true, true, false); }},
	{"PXORQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 3, true, true, false); }},
	{"XORPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 3, true, true, false); }},
	{"PANDNQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 3, true, true, false); }},
	{"ANDNPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 3, true, true, false); }},

	{"PANDD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 2, true, true, false); }},
	{"ANDPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AND, 2, true, true, false); }},
	{"PORD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 2, true, true, false); }},
	{"ORPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_OR, 2, true, true, false); }},
	{"PXORD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 2, true, true, false); }},
	{"XORPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_XOR, 2, true, true, false); }},
	{"PANDND", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 2, true, true, false); }},
	{"ANDNPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ANDN, 2, true, true, false); }},

	{"PADDQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 3, true, true, false); }},
	{"PADDD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 2, true, true, false); }},
	{"PADDW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 1, true, true, false); }},
	{"PADDB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADD, 0, true, true, false); }},

	{"PADDSW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDS, 1, true, true, false); }},
	{"PADDSB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDS, 0, true, true, false); }},

	{"PADDUSW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDUS, 1, true, true, false); }},
	{"PADDUSB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_ADDUS, 0, true, true, false); }},

	{"PSUBQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 3, true, true, false); }},
	{"PSUBD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 2, true, true, false); }},
	{"PSUBW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 1, true, true, false); }},
	{"PSUBB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUB, 0, true, true, false); }},

	{"PSUBSW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBS, 1, true, true, false); }},
	{"PSUBSB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBS, 0, true, true, false); }},

	{"PSUBUSW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBUS, 1, true, true, false); }},
	{"PSUBUSB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SUBUS, 0, true, true, false); }},

	{"PMULLQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_MULL, 3, true, true, false); }},
	{"PMULLD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_MULL, 2, true, true, false); }},
	{"PMULLW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_MULL, 1, true, true, false); }},

	{"MINSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 3, false, false, true); }},
	{"MINSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 2, false, false, true); }},

	{"MINPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 3, true, true, false); }},
	{"MINPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMIN, 2, true, true, false); }},

	{"MAXSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 3, false, false, true); }},
	{"MAXSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 2, false, false, true); }},

	{"MAXPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 3, true, true, false); }},
	{"MAXPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FMAX, 2, true, true, false); }},

	{"PMINUQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 3, true, true, false); }},
	{"PMINUD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 2, true, true, false); }},
	{"PMINUW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 1, true, true, false); }},
	{"PMINUB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMIN, 0, true, true, false); }},

	{"PMINSQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 3, true, true, false); }},
	{"PMINSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 2, true, true, false); }},
	{"PMINSW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 1, true, true, false); }},
	{"PMINSB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMIN, 0, true, true, false); }},

	{"PMAXUQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 3, true, true, false); }},
	{"PMAXUD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 2, true, true, false); }},
	{"PMAXUW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 1, true, true, false); }},
	{"PMAXUB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_UMAX, 0, true, true, false); }},

	{"PMAXSQ", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 3, true, true, false); }},
	{"PMAXSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 2, true, true, false); }},
	{"PMAXSW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 1, true, true, false); }},
	{"PMAXSB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_SMAX, 0, true, true, false); }},

	{"ADDSUBPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADDSUB, 3, true, true, false); }},
	{"ADDSUBPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FADDSUB, 2, true, true, false); }},

	{"PAVGW", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AVG, 1, true, true, false); }},
	{"PAVGB", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_AVG, 0, true, true, false); }},

	{"CMPPD", [](AssembleArgs &args) { return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 3, true, true, false); }},
	{"CMPPS", [](AssembleArgs &args) { return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 2, true, true, false); }},

	// CMPSD (vec) requires disambiguation
	{"CMPSS", [](AssembleArgs &args) { return args.TryProcessVPU_FCMP(OPCode::VPU_FCMP, 2, false, false, true); }},

	// packed double comparisons
	{"CMPEQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 0); }},
	{"CMPLTPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 1); }},
	{"CMPLEPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 2); }},
	{"CMPUNORDPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 3); }},
	{"CMPNEQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 4); }},
	{"CMPNLTPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 5); }},
	{"CMPNLEPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 6); }},
	{"CMPORDPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 7); }},
	{"CMPEQ_UQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 8); }},
	{"CMPNGEPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 9); }},
	{"CMPNGTPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 10); }},
	{"CMPFALSEPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 11); }},
	{"CMPNEQ_OQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 12); }},
	{"CMPGEPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 13); }},
	{"CMPGTPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 14); }},
	{"CMPTRUEPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 15); }},
	{"CMPEQ_OSPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 16); }},
	{"CMPLT_OQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 17); }},
	{"CMPLE_OQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 18); }},
	{"CMPUNORD_SPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 19); }},
	{"CMPNEQ_USPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 20); }},
	{"CMPNLT_UQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 21); }},
	{"CMPNLE_UQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 22); }},
	{"CMPORD_SPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 23); }},
	{"CMPEQ_USPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 24); }},
	{"CMPNGE_UQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 25); }},
	{"CMPNGT_UQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 26); }},
	{"CMPFALSE_OSPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 27); }},
	{"CMPNEQ_OSPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 28); }},
	{"CMPGE_OQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 29); }},
	{"CMPGT_OQPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 30); }},
	{"CMPTRUE_USPD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, true, true, false, true, 31); }},

	// packed single comparisons
	{"CMPEQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 0); }},
	{"CMPLTPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 1); }},
	{"CMPLEPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 2); }},
	{"CMPUNORDPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 3); }},
	{"CMPNEQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 4); }},
	{"CMPNLTPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 5); }},
	{"CMPNLEPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 6); }},
	{"CMPORDPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 7); }},
	{"CMPEQ_UQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 8); }},
	{"CMPNGEPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 9); }},
	{"CMPNGTPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 10); }},
	{"CMPFALSEPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 11); }},
	{"CMPNEQ_OQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 12); }},
	{"CMPGEPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 13); }},
	{"CMPGTPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 14); }},
	{"CMPTRUEPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 15); }},
	{"CMPEQ_OSPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 16); }},
	{"CMPLT_OQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 17); }},
	{"CMPLE_OQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 18); }},
	{"CMPUNORD_SPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 19); }},
	{"CMPNEQ_USPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 20); }},
	{"CMPNLT_UQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 21); }},
	{"CMPNLE_UQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 22); }},
	{"CMPORD_SPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 23); }},
	{"CMPEQ_USPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 24); }},
	{"CMPNGE_UQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 25); }},
	{"CMPNGT_UQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 26); }},
	{"CMPFALSE_OSPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 27); }},
	{"CMPNEQ_OSPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 28); }},
	{"CMPGE_OQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 29); }},
	{"CMPGT_OQPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 30); }},
	{"CMPTRUE_USPS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, true, true, false, true, 31); }},

	// scalar double comparisons
	{"CMPEQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 0); }},
	{"CMPLTSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 1); }},
	{"CMPLESD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 2); }},
	{"CMPUNORDSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 3); }},
	{"CMPNEQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 4); }},
	{"CMPNLTSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 5); }},
	{"CMPNLESD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 6); }},
	{"CMPORDSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 7); }},
	{"CMPEQ_UQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 8); }},
	{"CMPNGESD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 9); }},
	{"CMPNGTSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 10); }},
	{"CMPFALSESD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 11); }},
	{"CMPNEQ_OQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 12); }},
	{"CMPGESD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 13); }},
	{"CMPGTSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 14); }},
	{"CMPTRUESD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 15); }},
	{"CMPEQ_OSSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 16); }},
	{"CMPLT_OQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 17); }},
	{"CMPLE_OQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 18); }},
	{"CMPUNORD_SSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 19); }},
	{"CMPNEQ_USSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 20); }},
	{"CMPNLT_UQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 21); }},
	{"CMPNLE_UQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 22); }},
	{"CMPORD_SSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 23); }},
	{"CMPEQ_USSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 24); }},
	{"CMPNGE_UQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 25); }},
	{"CMPNGT_UQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 26); }},
	{"CMPFALSE_OSSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 27); }},
	{"CMPNEQ_OSSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 28); }},
	{"CMPGE_OQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 29); }},
	{"CMPGT_OQSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 30); }},
	{"CMPTRUE_USSD", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 3, false, false, true, true, 31); }},

	// scalar single comparisons
	{"CMPEQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 0); }},
	{"CMPLTSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 1); }},
	{"CMPLESS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 2); }},
	{"CMPUNORDSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 3); }},
	{"CMPNEQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 4); }},
	{"CMPNLTSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 5); }},
	{"CMPNLESS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 6); }},
	{"CMPORDSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 7); }},
	{"CMPEQ_UQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 8); }},
	{"CMPNGESS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 9); }},
	{"CMPNGTSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 10); }},
	{"CMPFALSESS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 11); }},
	{"CMPNEQ_OQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 12); }},
	{"CMPGESS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 13); }},
	{"CMPGTSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 14); }},
	{"CMPTRUESS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 15); }},
	{"CMPEQ_OSSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 16); }},
	{"CMPLT_OQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 17); }},
	{"CMPLE_OQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 18); }},
	{"CMPUNORD_SSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 19); }},
	{"CMPNEQ_USSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 20); }},
	{"CMPNLT_UQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 21); }},
	{"CMPNLE_UQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 22); }},
	{"CMPORD_SSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 23); }},
	{"CMPEQ_USSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 24); }},
	{"CMPNGE_UQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 25); }},
	{"CMPNGT_UQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 26); }},
	{"CMPFALSE_OSSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 27); }},
	{"CMPNEQ_OSSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 28); }},
	{"CMPGE_OQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 29); }},
	{"CMPGT_OQSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 30); }},
	{"CMPTRUE_USSS", [](AssembleArgs &args) { return args.TryProcessVPUBinary(OPCode::VPU_FCMP, 2, false, false, true, true, 31); }},

	{"COMISD", [](AssembleArgs &args) { return args.TryProcessVPUBinary_2arg(OPCode::VPU_FCOMI, 3, false, false, true); }},
	{"COMISS", [](AssembleArgs &args) { return args.TryProcessVPUBinary_2arg(OPCode::VPU_FCOMI, 2, false, false, true); }},

	{"SQRTPD", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 3, true, true, false); }},
	{"SQRTPS", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 2, true, true, false); }},

	{"SQRTSD", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 3, false, false, true); }},
	{"SQRTSS", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FSQRT, 2, false, false, true); }},

	{"RSQRTPD", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 3, true, true, false); }},
	{"RSQRTPS", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 2, true, true, false); }},

	{"RSQRTSD", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 3, false, false, true); }},
	{"RSQRTSS", [](AssembleArgs &args) { return args.TryProcessVPUUnary(OPCode::VPU_FRSQRT, 2, false, false, true); }},

	{"STMXCSR", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 4, 2); }},
	{"LDMXCSR", [](AssembleArgs &args) { return args.TryProcessFSTLD_WORD(OPCode::FSTLD_WORD, 5, 2); }},

	{"CVTSD2SI", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, false, false); }},
	{"CVTSS2SI", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, false, true); }},
	{"CVTTSD2SI", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, true, false); }},
	{"CVTTSS2SI", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2i(OPCode::VPU_CVT, true, true); }},

	{"CVTSI2SD", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_i2f(OPCode::VPU_CVT, false); }},
	{"CVTSI2SS", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_i2f(OPCode::VPU_CVT, true); }},

	{"CVTSD2SS", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2f(OPCode::VPU_CVT, false); }},
	{"CVTSS2SD", [](AssembleArgs &args) { return args.TryProcessVPUCVT_scalar_f2f(OPCode::VPU_CVT, true); }},

	{"CVTPD2DQ", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, false, false); }},
	{"CVTPS2DQ", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, false, true); }},
	{"CVTTPD2DQ", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, true, false); }},
	{"CVTTPS2DQ", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2i(OPCode::VPU_CVT, true, true); }},

	{"CVTDQ2PD", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_i2f(OPCode::VPU_CVT, false); }},
	{"CVTDQ2PS", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_i2f(OPCode::VPU_CVT, true); }},

	{"CVTPD2PS", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2f(OPCode::VPU_CVT, false); }},
	{"CVTPS2PD", [](AssembleArgs &args) { return args.TryProcessVPUCVT_packed_f2f(OPCode::VPU_CVT, true); }},

	// ---------------- //

	// -- CSX64 misc -- //

	// ---------------- //

	{"DEBUG_CPU", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::DEBUG, true, 0); }},
	{"DEBUG_VPU", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::DEBUG, true, 1); }},
	{"DEBUG_FULL", [](AssembleArgs &args) { return args.TryProcessNoArgOp(OPCode::DEBUG, true, 2); }},

	// -------------------- //

	// -- disambiguators -- //

	// -------------------- //

	{"MOVS", [](AssembleArgs &args)
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
		}},

	{"MOVSD", [](AssembleArgs &args)
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
		}},

	{"CMPSD", [](AssembleArgs &args)
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
		}},

	};

	const std::unordered_set<std::string> valid_lock_instructions
	{
		"ADD", "ADC", "AND", "BTC", "BTR", "BTS", "CMPXCHG", "CMPXCH8B", "CMPXCHG16B", "DEC", "INC", "NEG", "NOT", "OR", "SBB", "SUB", "XOR", "XADD", "XCHG"
	};
}