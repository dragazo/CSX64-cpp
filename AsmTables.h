#ifndef CSX64_ASM_TABLES_H
#define CSX64_ASM_TABLES_H

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Expr.h"
#include "Assembly.h"
#include "AsmArgs.h"

namespace CSX64
{
	extern const char CommentChar;
	extern const char LabelDefChar;

	extern const std::string CurrentLineMacro;
	extern const std::string StartOfSegMacro;

	extern const std::unordered_map<Expr::OPs, int> Precedence;

	extern const std::unordered_set<char> UnaryOps;

	extern const std::unordered_map<AsmSegment, std::string> SegOffsets;
	extern const std::unordered_map<AsmSegment, std::string> SegOrigins;

	extern const std::unordered_set<std::string> VerifyLegalExpressionIgnores;

	extern const std::unordered_set<std::string> AdditionalReservedSymbols;

	// Maps CPU register names (all caps) to tuples of (id, sizecode, high)
	extern const std::unordered_map<std::string, std::tuple<u8, u8, bool>> CPURegisterInfo;

	// Maps FPU register names (all caps) to their ids
	extern const std::unordered_map<std::string, u8> FPURegisterInfo;

	// Maps VPU register names (all caps) to tuples of (id, sizecode)
	extern const std::unordered_map<std::string, std::tuple<u8, u8>> VPURegisterInfo;

	typedef bool(*asm_router)(AssembleArgs &args);

	extern const std::unordered_map<std::string, asm_router> asm_routing_table;
}

#endif
