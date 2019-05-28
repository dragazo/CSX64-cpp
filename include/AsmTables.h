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
	extern const std::string TimesIterIdMacro;

	extern const std::unordered_map<Expr::OPs, int> Precedence;

	extern const std::unordered_map<std::string, Expr::OPs> FunctionOperator_to_OP;

	extern const std::unordered_map<AsmSegment, std::string> SegOffsets;
	extern const std::unordered_map<AsmSegment, std::string> SegOrigins;

	// a set of all the ids to use for ptrdiff logic
	extern const std::unordered_set<std::string> PtrdiffIDs;

	extern const std::unordered_set<std::string> VerifyLegalExpressionIgnores;

	extern const std::unordered_set<std::string> AdditionalReservedSymbols;

	// Maps CPU register names (all caps) to tuples of (id, sizecode, high)
	extern const std::unordered_map<std::string, std::tuple<u8, u8, bool>> CPURegisterInfo;

	// Maps FPU register names (all caps) to their ids
	extern const std::unordered_map<std::string, u8> FPURegisterInfo;

	// Maps VPU register names (all caps) to tuples of (id, sizecode)
	extern const std::unordered_map<std::string, std::tuple<u8, u8>> VPURegisterInfo;

	// represents an assembly router function - calling this selects that syntax branch for assembly (one for each instruction)
	typedef bool(*asm_router)(AssembleArgs &args);

	// maps (uppercase) instructions to their associated assembly router
	extern const std::unordered_map<std::string, asm_router> asm_routing_table;

	// set of all (uppercase) instructions that can legally be modified by the lock prefix
	extern const std::unordered_set<std::string> valid_lock_instructions;
}

#endif
