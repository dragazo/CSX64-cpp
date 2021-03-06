#ifndef CSX64_ASMARGS_H
#define CSX64_ASMARGS_H

#include <string>
#include <vector>
#include <memory>

#include "CoreTypes.h"
#include "Assembly.h"
#include "Expr.h"

namespace CSX64
{
	// Holds all the variables used during assembly
	class AssembleArgs
	{
	public: // -- data -- //

		ObjectFile &file;

		AsmSegment current_seg = AsmSegment::INVALID;
		AsmSegment done_segs = AsmSegment::INVALID;

		int line = 0;
		u64 line_pos_in_seg = 0;

		std::string last_nonlocal_label;

		std::string label_def;
		i64 times, times_i; // the TIMES upper bound and current loop index (i)
		std::string op;
		std::vector<std::string> args; // must be array for ref params

		AssembleResult res;

	public: // -- ctor/dtor -- //

		AssembleArgs(ObjectFile &dest) : file(dest) {}
		
		AssembleArgs(const AssembleArgs&) = delete;
		AssembleArgs(AssembleArgs&&) = delete;

		AssembleArgs &operator=(const AssembleArgs&) = delete;
		AssembleArgs &operator=(AssembleArgs&&) = delete;

	public: // -- Assembly Functions -- //

		// updates current line positioning variables.
		// must be called before TryExtractLineHeader() and at the start of each TIMES assembly iteration (before SplitLine()).
		void UpdateLinePos();

		/// <summary>
		/// Attempts to extract the header information from a raw line of source code. Should be called after UpdateLinePos() and before SplitLine().
		/// this->label_def - receives the label definition for this line (if any)
		/// this->times     - recieves the number of times to repeat the instruction (or 1 if not specified).
		/// </summary>
		/// <param name="rawline">the raw line to extract header information from</param>
		bool TryExtractLineHeader(std::string &rawline);

		// given a string region defined by str[str_begin..str_end) attempts to find the matching close paren for a given open paren located at str_begin.
		// if such a closing paren is found, it is stored to match.
		bool TryFindMatchingParen(const std::string &str, std::size_t str_begin, std::size_t str_end, std::size_t &match);

		// splits the string into an array of (non-empty) comma-separated values which are also trimmed of whitespace.
		// if no values are present (i.e. string region is empty or white space), no args are added to the array (no args present).
		// vec       - the location to store the indivual values (added to the end of the array in order)
		// str       - the string to inspect
		// str_begin - the starting index of the region to inspect (inclusive)
		// str_end   - the ending index of the region to inspect (exclusive)
		bool TrySplitOnCommas(std::vector<std::string> &vec, const std::string &str, std::size_t str_begin, std::size_t str_end);

		/// <summary>
		/// Splits the raw line into its separate components. The raw line should not have a comment section.
		/// </summary>
		/// <param name="rawline">the raw line to parse</param>
		bool SplitLine(const std::string &rawline);

		static bool IsValidName(const std::string &token, std::string &err);
		bool MutateName(std::string &label);

		bool TryReserve(u64 size);
		bool TryAppendVal(u64 size, u64 val);
		bool TryAppendByte(u8 val);

		// gets the compact imm prefix for a 64-bit bit quantity (val).
		static u8 GetCompactImmPrefix(u64 val);

		// attempts to append a compact imm to the current segment.
		// expr     - the value to append in the compact imm format.
		// sizecode - if strict is true, this is the size to use, otherwise is the default size if expr cannot be immediately evaluated.
		// strict   - denotes if sizecode MUST be obeyed - inhibits further compactness optimizations.
		bool TryAppendCompactImm(Expr &&expr, u64 sizecode, bool strict);

		// private
		bool TryAppendExpr(u64 size, Expr &&expr, std::vector<HoleData> &holes, std::vector<u8> &segment);
		bool TryAppendExpr(u64 size, Expr &&expr);
		bool TryAppendAddress(u64 a, u64 b, Expr &&hole);

		bool TryAlign(u64 size);
		bool TryPad(u64 size);

		// attempts to extract an expression from str.
		// str      - the string containing an expression.
		// beg/end  - begin/end indicies in the string to examine (full string would be 0, len).
		// expr     - the extracted expression (on success).
		// aft      - the index immediately after the parsed expression (on success).
		bool TryExtractExpr(const std::string &str, std::size_t beg, std::size_t end, std::unique_ptr<Expr> &expr, std::size_t &aft);
		// attempts to extract an expression from str.
		// equivalent to calling TryExtractExpr() from 0,len and guaranteeing aft==len.
		bool TryExtractExpr(const std::string &str, std::unique_ptr<Expr> &expr);

		bool TryParseImm(const std::string &token, Expr &expr, u64 &sizecode, bool &explicit_size, bool &strict);
		bool TryParseInstantImm(const std::string &token, u64 &val, bool &floating, u64 &sizecode, bool &explicit_size, bool &strict);

		/// <summary>
		/// Attempts to extract the numeric portion of a standard label: val in (#base + val). Returns true on success
		/// </summary>
		/// <param name="expr">the expression representing the label (either the label itself or a token expression reference to it)</param>
		/// <param name="val">the resulting value portion or null if zero (e.g. seg origins)</param>
		bool TryExtractPtrVal(const Expr *expr, const Expr *&val, const std::string &_base);
		/// <summary>
		/// Performs pointer difference arithmetic on the expression tree and returns the result
		/// </summary>
		/// <param name="expr">the expression tree to operate on</param>
		std::unique_ptr<Expr> Ptrdiff(std::unique_ptr<Expr> expr);

		/// <summary>
		/// Attempts to parse an imm that has a prefix. If the imm is a compound expression, it must be parenthesized
		/// </summary>
		/// <param name="token">token to parse</param>
		/// <param name="prefix">the prefix the imm is required to have</param>
		/// <param name="val">resulting value</param>
		/// <param name="floating">results in true if val is floating-point</param>
		bool TryParseInstantPrefixedImm(const std::string &token, const std::string &prefix, u64 &val, bool &floating, u64 &sizecode, bool &explicit_size, bool &strict);

		bool TryParseCPURegister(const std::string &token, u64 &reg, u64 &sizecode, bool &high);
		bool TryParseFPURegister(const std::string &token, u64 &reg);
		bool TryParseVPURegister(const std::string &token, u64 &reg, u64 &sizecode);

		// computes the multiplier (not mult code) for <label> in <hole> and stores it in <mult>
		bool TryGetRegMult(const std::string &label, Expr &hole, u64 &mult);
		bool TryParseAddress(std::string token, u64 &a, u64 &b, Expr &ptr_base, u64 &sizecode, bool &explicit_size);

		bool VerifyLegalExpression(Expr &expr);
		/// <summary>
		/// Ensures that all is good in the hood. Returns true if the hood is good
		/// </summary>
		bool VerifyIntegrity();

		// -- misc -- //

		bool IsReservedSymbol(std::string symbol);

		bool TryProcessAlignXX(u64 size);
		bool TryProcessAlign();

		bool TryProcessGlobal();
		bool TryProcessExtern();

		bool TryProcessDeclare(u64 size);
		bool TryProcessReserve(u64 size);

		bool TryProcessEQU();

		bool TryProcessStaticAssert();

		bool TryProcessSegment();

		bool TryProcessINCBIN();

		bool TryProcessMOVx_trans(u64 sizecode);

		bool TryProcessDEBUG_mem();

		// -- x86 op formats -- //

		bool TryProcessTernaryOp(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15);
		bool TryProcessBinaryOp(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15, int _force_b_imm_sizecode = -1);
		bool TryProcessUnaryOp(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15);
		bool TryProcessIMMRM(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15, int default_sizecode = -1);
		bool TryProcessRR_RM(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15);

		bool TryProcessBinaryOp_NoBIMM(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15);
		bool TryProcessBinaryOp_NoBMem(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15, int _force_b_imm_sizecode = -1);
		bool TryProcessBinaryOp_R_RM(OPCode op, bool has_ext_op = false, u8 ext_op = 0, u64 sizemask = 15, int _force_b_imm_sizecode = -1);

		bool TryProcessNoArgOp(OPCode op, bool has_ext_op = false, u8 ext_op = 0);
		bool TryProcessNoArgOp_no_write(); // this is the same as NoArgOp, but doesn't write an opcode

		bool TryProcessXCHG(OPCode op);
		bool TryProcessLEA(OPCode op);
		bool TryProcessPOP(OPCode op);

		bool __TryProcessShift_mid();
		bool TryProcessShift(OPCode op);

		bool __TryProcessMOVxX_settings_byte(bool sign, u64 dest, u64 dest_sizecode, u64 src_sizecode);
		bool TryProcessMOVxX(OPCode op, bool sign);

		// gets the sizecode specified by a binary string operation (e.g. MOVS (string))
		bool __TryGetBinaryStringOpSize(u64 &sizecode);

		bool TryProcessMOVS_string(OPCode op, bool rep);
		bool TryProcessCMPS_string(OPCode op, bool repe, bool repne);
		bool TryProcessLODS_string(OPCode op, bool rep);
		bool TryProcessSTOS_string(OPCode op, bool rep);
		bool TryProcessSCAS_string(OPCode op, bool repe, bool repne);

		// extracts a prefix from the args and returns the actual instruction token (as uppercase)
		bool TryProcessPrefixOp(std::string &actual);

		bool TryProcessREP();
		bool TryProcessREPE();
		bool TryProcessREPNE();

		bool TryProcessLOCK();

		bool TryProcessBSx(OPCode op, bool forward);

		// -- x87 op formats -- //

		bool TryProcessFPUBinaryOp(OPCode op, bool integral, bool pop);
		bool TryProcessFPURegisterOp(OPCode op, bool has_ext_op = false, u8 ext_op = 0);

		// _sizecode is only used for validating explicit memory operand sizes
		bool TryProcessFSTLD_WORD(OPCode op, u8 mode, u64 _sizecode);

		bool TryProcessFLD(OPCode op, bool integral);
		bool TryProcessFST(OPCode op, bool integral, bool pop, bool trunc);
		bool TryProcessFCOM(OPCode op, bool integral, bool pop, bool pop2, bool eflags, bool unordered);
		bool TryProcessFMOVcc(OPCode op, u64 condition);

		// -- SIMD op formats -- //

		bool TryExtractVPUMask(std::string &arg, std::unique_ptr<Expr> &mask, bool &zmask);
		bool VPUMaskPresent(Expr *mask, u64 elem_count);

		bool TryProcessVPUMove(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar);
		bool TryProcessVPUBinary(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar, bool has_ext_op = false, u8 ext_op = 0);
		bool TryProcessVPUUnary(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar, bool has_ext_op = false, u8 ext_op = 0);

		// same as TryProcessVPUBinary() except that it forces the 2 arg pathway
		bool TryProcessVPUBinary_2arg(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar, bool has_ext_op = false, u8 ext_op = 0);

		bool TryProcessVPU_FCMP(OPCode op, u64 elem_sizecode, bool maskable, bool aligned, bool scalar);

		// these internally handle all the vpu conversion encodings
		bool TryProcessVPUCVT_scalar_f2i(OPCode op, bool trunc, bool single);
		bool TryProcessVPUCVT_scalar_i2f(OPCode op, bool single);
		bool TryProcessVPUCVT_scalar_f2f(OPCode op, bool extend);

		// these are helpers for the packed cvt formatters to reduce code duplication
		bool __TryProcessVPUCVT_packed_formatter_reg(OPCode op, u8 mode, u64 elem_count, u64 dest, Expr *mask, bool zmask, u64 src);
		bool __TryProcessVPUCVT_packed_formatter_mem(OPCode op, u8 mode, u64 elem_count, u64 dest, Expr *mask, bool zmask, u64 a, u64 b, Expr &&ptr_base);

		// these internally handle all the vpu conversion encodings
		bool TryProcessVPUCVT_packed_f2i(OPCode op, bool trunc, bool single);
		bool TryProcessVPUCVT_packed_i2f(OPCode op, bool single);
		bool TryProcessVPUCVT_packed_f2f(OPCode op, bool extend);
	};
}

#endif
