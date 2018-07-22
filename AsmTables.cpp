#include <string>
#include <unordered_map>
#include <unordered_set>

#include "AsmTables.h"
#include "AsmRouting.h"
#include "Expr.h"
#include "Assembly.h"
#include "AsmArgs.h"

namespace CSX64
{
	const char CommentChar = ';';
	const char LabelDefChar = ':';

	const std::string CurrentLineMacro = "$";
	const std::string StartOfSegMacro = "$$";

	const std::unordered_map<Expr::OPs, int> Precedence
	{
		{Expr::OPs::Mul, 5},
	{Expr::OPs::Div, 5},
	{Expr::OPs::Mod, 5},

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

	const std::unordered_set<char> UnaryOps{'+', '-', '~', '!', '*', '/'};

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
		// -- directives -- //

		{"GLOBAL", asm_router_GLOBAL},
	{"EXTERN", asm_router_EXTERN},

	{"ALIGN", asm_router_ALIGN},

	{"ALIGNB", asm_router_ALIGNB},
	{"ALIGNW", asm_router_ALIGNW},
	{"ALIGND", asm_router_ALIGND},
	{"ALIGNQ", asm_router_ALIGNQ},
	{"ALIGNX", asm_router_ALIGNX},
	{"ALIGNY", asm_router_ALIGNY},
	{"ALIGNZ", asm_router_ALIGNZ},

	{"DB", asm_router_DB},
	{"DW", asm_router_DW},
	{"DD", asm_router_DD},
	{"DQ", asm_router_DQ},
	{"DX", asm_router_DX},
	{"DY", asm_router_DY},
	{"DZ", asm_router_DZ},

	{"RESB", asm_router_RESB},
	{"RESW", asm_router_RESW},
	{"RESD", asm_router_RESD},
	{"RESQ", asm_router_RESQ},
	{"RESX", asm_router_RESX},
	{"RESY", asm_router_RESY},
	{"RESZ", asm_router_RESZ},

	{"EQU", asm_router_EQU},

	{"SEGMENT", asm_router_SEGMENT},
	{"SECTION", asm_router_SEGMENT},

	// -- x86 -- //

	{"NOP", asm_router_NOP},

	{"HLT", asm_router_HLT},
	{"SYSCALL", asm_router_SYSCALL},

	{"PUSHF", asm_router_PUSHF},
	{"PUSHFD", asm_router_PUSHFD},
	{"PUSHFQ", asm_router_PUSHFQ},

	{"POPF", asm_router_POPF},
	{"POPFD", asm_router_POPFD},
	{"POPFQ", asm_router_POPFQ},

	{"SAHF", asm_router_SAHF},
	{"LAHF", asm_router_LAHF},

	{"STC", asm_router_STC},
	{"CLC", asm_router_CLC},
	{"STI", asm_router_STI},
	{"CLI", asm_router_CLI},
	{"STD", asm_router_STD},
	{"CLD", asm_router_CLD},
	{"STAC", asm_router_STAC},
	{"CLAC", asm_router_CLAC},
	{"CMC", asm_router_CMC},

	{"SETZ", asm_router_SETZ},
	{"SETE", asm_router_SETZ},
	{"SETNZ", asm_router_SETNZ},
	{"SETNE", asm_router_SETNZ},
	{"SETS", asm_router_SETS},
	{"SETNS", asm_router_SETNS},
	{"SETP", asm_router_SETP},
	{"SETPE", asm_router_SETP},
	{"SETNP", asm_router_SETNP},
	{"SETPO", asm_router_SETNP},
	{"SETO", asm_router_SETO},
	{"SETNO", asm_router_SETNO},
	{"SETC", asm_router_SETC},
	{"SETNC", asm_router_SETNC},

	{"SETB", asm_router_SETB},
	{"SETNAE", asm_router_SETB},
	{"SETBE", asm_router_SETBE},
	{"SETA", asm_router_SETBE},
	{"SETA", asm_router_SETA},
	{"SETNBE", asm_router_SETA},
	{"SETAE", asm_router_SETAE},
	{"SETNB", asm_router_SETAE},

	{"SETL", asm_router_SETL},
	{"SETNGE", asm_router_SETL},
	{"SETLE", asm_router_SETLE},
	{"SETNG", asm_router_SETLE},
	{"SETG", asm_router_SETG},
	{"SETNLE", asm_router_SETG},
	{"SETGE", asm_router_SETGE},
	{"SETNL", asm_router_SETGE},

	{"MOV", asm_router_MOV},

	{"MOVZ", asm_router_MOVZ},
	{"MOVE", asm_router_MOVZ},
	{"MOVNZ", asm_router_MOVNZ},
	{"MOVNE", asm_router_MOVNZ},
	{"MOVS", asm_router_MOVS},
	{"MOVNS", asm_router_MOVNS},
	{"MOVP", asm_router_MOVP},
	{"MOVPE", asm_router_MOVP},
	{"MOVNP", asm_router_MOVNP},
	{"MOVPO", asm_router_MOVNP},
	{"MOVO", asm_router_MOVO},
	{"MOVNO", asm_router_MOVNO},
	{"MOVC", asm_router_MOVC},
	{"MOVNC", asm_router_MOVNC},

	{"MOVB", asm_router_MOVB},
	{"MOVNAE", asm_router_MOVB},
	{"MOVBE", asm_router_MOVBE},
	{"MOVA", asm_router_MOVBE},
	{"MOVA", asm_router_MOVA},
	{"MOVNBE", asm_router_MOVA},
	{"MOVAE", asm_router_MOVAE},
	{"MOVNB", asm_router_MOVAE},

	{"MOVL", asm_router_MOVL},
	{"MOVNGE", asm_router_MOVL},
	{"MOVLE", asm_router_MOVLE},
	{"MOVNG", asm_router_MOVLE},
	{"MOVG", asm_router_MOVG},
	{"MOVNLE", asm_router_MOVG},
	{"MOVGE", asm_router_MOVGE},
	{"MOVNL", asm_router_MOVGE},

	{"XCHG", asm_router_XCHG},

	{"JMP", asm_router_JMP},

	{"JZ", asm_router_JZ},
	{"JE", asm_router_JZ},
	{"JNZ", asm_router_JNZ},
	{"JNE", asm_router_JNZ},
	{"JS", asm_router_JS},
	{"JNS", asm_router_JNS},
	{"JP", asm_router_JP},
	{"JPE", asm_router_JP},
	{"JNP", asm_router_JNP},
	{"JPO", asm_router_JNP},
	{"JO", asm_router_JO},
	{"JNO", asm_router_JNO},
	{"JC", asm_router_JC},
	{"JNC", asm_router_JNC},

	{"JB", asm_router_JB},
	{"JNAE", asm_router_JB},
	{"JBE", asm_router_JBE},
	{"JNA", asm_router_JBE},
	{"JA", asm_router_JA},
	{"JNBE", asm_router_JA},
	{"JAE", asm_router_JAE},
	{"JNB", asm_router_JAE},

	{"JL", asm_router_JL},
	{"JNGE", asm_router_JL},
	{"JLE", asm_router_JLE},
	{"JNG", asm_router_JLE},
	{"JG", asm_router_JG},
	{"JNLE", asm_router_JG},
	{"JGE", asm_router_JGE},
	{"JNL", asm_router_JGE},

	{"JCXZ", asm_router_JCXZ},
	{"JECXZ", asm_router_JECXZ},
	{"JRCXZ", asm_router_JRCXZ},

	{"LOOP", asm_router_LOOP},
	{"LOOPZ", asm_router_LOOPZ},
	{"LOOPE", asm_router_LOOPZ},
	{"LOOPNZ", asm_router_LOOPNZ},
	{"LOOPNE", asm_router_LOOPNZ},

	{"CALL", asm_router_CALL},
	{"RET", asm_router_RET},

	{"PUSH", asm_router_PUSH},
	{"POP", asm_router_POP},

	{"LEA", asm_router_LEA},

	{"ADD", asm_router_ADD},
	{"SUB", asm_router_SUB},

	{"MUL", asm_router_MUL},
	{"MULX", asm_router_MULX},
	{"IMUL", asm_router_IMUL},
	{"DIV", asm_router_DIV},
	{"IDIV", asm_router_IDIV},

	{"SHL", asm_router_SHL},
	{"SHR", asm_router_SHR},
	{"SAL", asm_router_SAL},
	{"SAR", asm_router_SAR},
	{"ROL", asm_router_ROL},
	{"ROR", asm_router_ROR},
	{"RCL", asm_router_RCL},
	{"RCR", asm_router_RCR},

	{"AND", asm_router_AND},
	{"OR", asm_router_OR},
	{"XOR", asm_router_XOR},

	{"INC", asm_router_INC},
	{"DEC", asm_router_DEC},
	{"NEG", asm_router_NEG},
	{"NOT", asm_router_NOT},

	{"CMP", asm_router_CMP},
	{"TEST", asm_router_TEST},

	{"BSWAP", asm_router_BSWAP},
	{"BEXTR", asm_router_BEXTR},
	{"BLSI", asm_router_BLSI},
	{"BLSMSK", asm_router_BLSMSK},
	{"BLSR", asm_router_BLSR},
	{"ANDN", asm_router_ANDN},

	{"BT", asm_router_BT},
	{"BTS", asm_router_BTS},
	{"BTR", asm_router_BTR},
	{"BTC", asm_router_BTC},

	{"CWD", asm_router_CWD},
	{"CDQ", asm_router_CDQ},
	{"CQO", asm_router_CQO},

	{"CBW", asm_router_CBW},
	{"CWDE", asm_router_CWDE},
	{"CDQE", asm_router_CDQE},

	{"MOVZX", asm_router_MOVZX},
	{"MOVSX", asm_router_MOVSX},

	{"ADC", asm_router_ADC},
	{"ADCX", asm_router_ADCX},
	{"ADOX", asm_router_ADOX},

	{"AAA", asm_router_AAA},
	{"AAS", asm_router_AAS},

	// -- x87 -- //

	{"FNOP", asm_router_FNOP},

	{"FWAIT", asm_router_FWAIT},

	{"FNINIT", asm_router_FNINIT},
	{"FINIT", asm_router_FINIT},

	{"FNCLEX", asm_router_FNCLEX},
	{"FCLEX", asm_router_FCLEX},

	{"FNSTSW", asm_router_FNSTSW},
	{"FSTSW", asm_router_FSTSW},

	{"FNSTCW", asm_router_FNSTCW},
	{"FSTCW", asm_router_FSTCW},

	{"FLDCW", asm_router_FLDCW},

	{"FLD1", asm_router_FLD1},
	{"FLDL2T", asm_router_FLDL2T},
	{"FLDL2E", asm_router_FLDL2E},
	{"FLDPI", asm_router_FLDPI},
	{"FLDLG2", asm_router_FLDLG2},
	{"FLDLN2", asm_router_FLDLN2},
	{"FLDZ", asm_router_FLDZ},

	{"FLD", asm_router_FLD},
	{"FILD", asm_router_FILD},

	{"FST", asm_router_FST},
	{"FIST", asm_router_FIST},
	{"FSTP", asm_router_FSTP},
	{"FISTP", asm_router_FISTP},
	{"FISTTP", asm_router_FISTTP},

	{"FXCH", asm_router_FXCH},

	{"FMOVE", asm_router_FMOVE},
	{"FMOVNE", asm_router_FMOVNE},
	{"FMOVB", asm_router_FMOVB},
	{"FMOVNAE", asm_router_FMOVB},
	{"FMOVBE", asm_router_FMOVBE},
	{"FMOVNA", asm_router_FMOVBE},
	{"FMOVA", asm_router_FMOVA},
	{"FMOVNBE", asm_router_FMOVA},
	{"FMOVAE", asm_router_FMOVAE},
	{"FMOVB", asm_router_FMOVAE},
	{"FMOVU", asm_router_FMOVU},
	{"FMOVNU", asm_router_FMOVNU},

	{"FADD", asm_router_FADD},
	{"FADDP", asm_router_FADDP},
	{"FIADD", asm_router_FIADD},

	{"FSUB", asm_router_FSUB},
	{"FSUBP", asm_router_FSUBP},
	{"FISUB", asm_router_FISUB},

	{"FSUBR", asm_router_FSUBR},
	{"FSUBRP", asm_router_FSUBRP},
	{"FISUBR", asm_router_FISUBR},

	{"FMUL", asm_router_FMUL},
	{"FMULP", asm_router_FMULP},
	{"FIMUL", asm_router_FIMUL},

	{"FDIV", asm_router_FDIV},
	{"FDIVP", asm_router_FDIVP},
	{"FIDIV", asm_router_FIDIV},

	{"FDIVR", asm_router_FDIVR},
	{"FDIVRP", asm_router_FDIVRP},
	{"FIDIVR", asm_router_FIDIVR},

	{"F2XM1", asm_router_F2XM1},
	{"FABS", asm_router_FABS},
	{"FCHS", asm_router_FCHS},
	{"FPREM", asm_router_FPREM},
	{"FPREM1", asm_router_FPREM1},
	{"FRNDINT", asm_router_FRNDINT},
	{"FSQRT", asm_router_FSQRT},
	{"FYL2X", asm_router_FYL2X},
	{"FYL2XP1", asm_router_FYL2XP1},
	{"FXTRACT", asm_router_FXTRACT},
	{"FSCALE", asm_router_FSCALE},

	{"FXAM", asm_router_FXAM},
	{"FTST", asm_router_FTST},

	{"FCOM", asm_router_FCOM},
	{"FCOMP", asm_router_FCOMP},
	{"FCOMPP", asm_router_FCOMPP},

	{"FUCOM", asm_router_FUCOM},
	{"FUCOMP", asm_router_FUCOMP},
	{"FUCOMPP", asm_router_FUCOMPP},

	{"FCOMI", asm_router_FCOMI},
	{"FCOMIP", asm_router_FCOMIP},

	{"FUCOMI", asm_router_FUCOMI},
	{"FUCOMIP", asm_router_FUCOMIP},

	{"FICOM", asm_router_FICOM},
	{"FICOMP", asm_router_FICOMP},

	{"FSIN", asm_router_FSIN},
	{"FCOS", asm_router_FCOS},
	{"FSINCOS", asm_router_FSINCOS},
	{"FPTAN", asm_router_FPTAN},
	{"FPATAN", asm_router_FPATAN},

	{"FINCSTP", asm_router_FINCSTP},
	{"FDECSTP", asm_router_FDECSTP},

	{"FFREE", asm_router_FFREE},

	{"MOVQ", asm_router_MOVQ},
	{"MOVD", asm_router_MOVD},

	{"MOVSD", asm_router_MOVSD},
	{"MOVSS", asm_router_MOVSS},

	{"MOVDQA", asm_router_MOVDQA},
	{"MOVDQU", asm_router_MOVDQU},

	{"MOVDQA64", asm_router_MOVDQA64},
	{"MOVDQA32", asm_router_MOVDQA32},
	{"MOVDQA16", asm_router_MOVDQA16},
	{"MOVDQA8", asm_router_MOVDQA8},

	{"MOVDQU64", asm_router_MOVDQU64},
	{"MOVDQU32", asm_router_MOVDQU32},
	{"MOVDQU16", asm_router_MOVDQU16},
	{"MOVDQU8", asm_router_MOVDQU8},

	{"MOVAPD", asm_router_MOVAPD},
	{"MOVAPS", asm_router_MOVAPS},

	{"MOVUPD", asm_router_MOVUPD},
	{"MOVUPS", asm_router_MOVUPS},

	{"ADDSD", asm_router_ADDSD},
	{"SUBSD", asm_router_SUBSD},
	{"MULSD", asm_router_MULSD},
	{"DIVSD", asm_router_DIVSD},

	{"ADDSS", asm_router_ADDSS},
	{"SUBSS", asm_router_SUBSS},
	{"MULSS", asm_router_MULSS},
	{"DIVSS", asm_router_DIVSS},

	{"ADDPD", asm_router_ADDPD},
	{"SUBPD", asm_router_SUBPD},
	{"MULPD", asm_router_MULPD},
	{"DIVPD", asm_router_DIVPD},

	{"ADDPS", asm_router_ADDPS},
	{"SUBPS", asm_router_SUBPS},
	{"MULPS", asm_router_MULPS},
	{"DIVPS", asm_router_DIVPS},

	{"PAND", asm_router_PAND},
	{"POR", asm_router_POR},
	{"PXOR", asm_router_PXOR},
	{"PANDN", asm_router_PANDN},

	{"PANDQ", asm_router_PANDQ},
	{"ANDPD", asm_router_PANDQ},
	{"PORQ", asm_router_PORQ},
	{"ORPD", asm_router_PORQ},
	{"PXORQ", asm_router_PXORQ},
	{"XORPD", asm_router_PXORQ},
	{"PANDNQ", asm_router_PANDNQ},
	{"ANDNPD", asm_router_PANDNQ},

	{"PANDD", asm_router_PANDD},
	{"ANDPS", asm_router_PANDD},
	{"PORD", asm_router_PORD},
	{"ORPS", asm_router_PORD},
	{"PXORD", asm_router_PXORD},
	{"XORPS", asm_router_PXORD},
	{"PANDND", asm_router_PANDND},
	{"ANDNPS", asm_router_PANDND},

	{"PADDQ", asm_router_PADDQ},
	{"PADDD", asm_router_PADDD},
	{"PADDW", asm_router_PADDW},
	{"PADDB", asm_router_PADDB},

	{"PADDSW", asm_router_PADDSW},
	{"PADDSB", asm_router_PADDSB},

	{"PADDUSW", asm_router_PADDUSW},
	{"PADDUSB", asm_router_PADDUSB},

	{"PSUBQ", asm_router_PSUBQ},
	{"PSUBD", asm_router_PSUBD},
	{"PSUBW", asm_router_PSUBW},
	{"PSUBB", asm_router_PSUBB},

	{"PSUBSW", asm_router_PSUBSW},
	{"PSUBSB", asm_router_PSUBSB},

	{"PSUBUSW", asm_router_PSUBUSW},
	{"PSUBUSB", asm_router_PSUBUSB},

	{"PMULLQ", asm_router_PMULLQ},
	{"PMULLD", asm_router_PMULLD},
	{"PMULLW", asm_router_PMULLW},

	{"MINSD", asm_router_MINSD},
	{"MINSS", asm_router_MINSS},

	{"MINPD", asm_router_MINPD},
	{"MINPS", asm_router_MINPS},

	{"MAXSD", asm_router_MAXSD},
	{"MAXSS", asm_router_MAXSS},

	{"MAXPD", asm_router_MAXPD},
	{"MAXPS", asm_router_MAXPS},

	{"PMINUQ", asm_router_PMINUQ},
	{"PMINUD", asm_router_PMINUD},
	{"PMINUW", asm_router_PMINUW},
	{"PMINUB", asm_router_PMINUB},

	{"PMINSQ", asm_router_PMINSQ},
	{"PMINSD", asm_router_PMINSD},
	{"PMINSW", asm_router_PMINSW},
	{"PMINSB", asm_router_PMINSB},

	{"PMAXUQ", asm_router_PMAXUQ},
	{"PMAXUD", asm_router_PMAXUD},
	{"PMAXUW", asm_router_PMAXUW},
	{"PMAXUB", asm_router_PMAXUB},

	{"PMAXSQ", asm_router_PMAXSQ},
	{"PMAXSD", asm_router_PMAXSD},
	{"PMAXSW", asm_router_PMAXSW},
	{"PMAXSB", asm_router_PMAXSB},

	{"ADDSUBPD", asm_router_ADDSUBPD},
	{"ADDSUBPS", asm_router_ADDSUBPS},

	{"PAVGW", asm_router_PAVGW},
	{"PAVGB", asm_router_PAVGB},

	// -- CSX64 misc -- //

	{"DEBUG_CPU", asm_router_DEBUG_CPU},
	{"DEBUG_VPU", asm_router_DEBUG_VPU},
	{"DEBUG_FULL", asm_router_DEBUG_FULL},

	};
}