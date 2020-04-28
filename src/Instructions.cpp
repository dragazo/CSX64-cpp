#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <string>
#include <iostream>
#include <iomanip>
#include <memory>
#include <cfenv>
#include <cmath>
#include <limits>
#include <type_traits>
#include <ctime>

#include "../include/Computer.h"

#include "../ios-frstor/iosfrstor.h"
#include "../BiggerInts/BiggerInts.h"

#include "../include/FastRng.h"

// macros for generating flag union expressions for use with a Computer object
// __VA_ARGS__ was expanding non-standardly in VS, which is why the below macro recursion doesn't use it
#define MASK_UNION_0() (0)
#define MASK_UNION_1(f) (decltype(((::CSX64::Computer*)nullptr)->f())::mask)
#define MASK_UNION_2(a,b)                              (MASK_UNION_1(a) | MASK_UNION_1(b))
#define MASK_UNION_3(a,b,c)                            (MASK_UNION_1(a) | MASK_UNION_2(b,c))
#define MASK_UNION_4(a,b,c,d)                          (MASK_UNION_1(a) | MASK_UNION_3(b,c,d))
#define MASK_UNION_5(a,b,c,d,e)                        (MASK_UNION_1(a) | MASK_UNION_4(b,c,d,e))
#define MASK_UNION_6(a,b,c,d,e,f)                      (MASK_UNION_1(a) | MASK_UNION_5(b,c,d,e,f))
#define MASK_UNION_7(a,b,c,d,e,f,g)                    (MASK_UNION_1(a) | MASK_UNION_6(b,c,d,e,f,g))
#define MASK_UNION_8(a,b,c,d,e,f,g,h)                  (MASK_UNION_1(a) | MASK_UNION_7(b,c,d,e,f,g,h))
#define MASK_UNION_9(a,b,c,d,e,f,g,h,i)                (MASK_UNION_1(a) | MASK_UNION_8(b,c,d,e,f,g,h,i))
#define MASK_UNION_10(a,b,c,d,e,f,g,h,i,j)             (MASK_UNION_1(a) | MASK_UNION_9(b,c,d,e,f,g,h,i,j))
#define MASK_UNION_11(a,b,c,d,e,f,g,h,i,j,k)           (MASK_UNION_1(a) | MASK_UNION_10(b,c,d,e,f,g,h,i,j,k))
#define MASK_UNION_12(a,b,c,d,e,f,g,h,i,j,k,l)         (MASK_UNION_1(a) | MASK_UNION_11(b,c,d,e,f,g,h,i,j,k,l))
#define MASK_UNION_13(a,b,c,d,e,f,g,h,i,j,k,l,m)       (MASK_UNION_1(a) | MASK_UNION_12(b,c,d,e,f,g,h,i,j,k,l,m))
#define MASK_UNION_14(a,b,c,d,e,f,g,h,i,j,k,l,m,n)     (MASK_UNION_1(a) | MASK_UNION_13(b,c,d,e,f,g,h,i,j,k,l,m,n))
#define MASK_UNION_15(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o)   (MASK_UNION_1(a) | MASK_UNION_14(b,c,d,e,f,g,h,i,j,k,l,m,n,o))
#define MASK_UNION_16(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) (MASK_UNION_1(a) | MASK_UNION_15(b,c,d,e,f,g,h,i,j,k,l,m,n,o,p))

using namespace CSX64::detail;

namespace CSX64
{
    bool Computer::FetchTernaryOpFormat(u8 &s, u64 &a, u64 &b)
    {
        if (!GetMemAdv<u8>(s)) return false;
        u64 sizecode = (s >> 2) & 3;

        if constexpr (StrictUND)
        {
            // make sure dest will be valid for storing (high flag)
            if ((s & 2) != 0 && ((s & 0xc0) != 0 || sizecode != 0)) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        // get b (imm)
        if (!GetMemAdv(get_size(sizecode), b)) return false;

        // get a (reg or mem)
        if ((s & 1) == 0)
        {
            u8 t;
            if (!GetMemAdv<u8>(t)) return false;
            if ((t & 128) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in (ABCD)H
                    if ((t & 0x0c) != 0 || sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                a = CPURegisters[t & 15].x8h();
            }
            else a = CPURegisters[t & 15][sizecode];
            return true;
        }
        else return GetAddressAdv(a) && GetMemRaw(a, get_size(sizecode), a);
    }
    bool Computer::StoreTernaryOPFormat(u8 s, u64 res)
    {
        if ((s & 2) != 0) CPURegisters[s >> 4].x8h() = (u8)res;
        else CPURegisters[s >> 4][(s >> 2) & 3] = res;
        return true;
    }

    bool Computer::FetchBinaryOpFormat(u8 &s1, u8 &s2, u64 &m, u64 &a, u64 &b, bool get_a, int _a_sizecode, int _b_sizecode, bool allow_b_mem)
    {
        // read settings
        if (!GetMemAdv<u8>(s1) || !GetMemAdv<u8>(s2)) return false;

        // if they requested an explicit size for a, change it in the settings byte
        if (_a_sizecode != -1) s1 = (u8)((s1 & 0xf3) | (_a_sizecode << 2));

        // get size codes
        u64 a_sizecode = (s1 >> 2) & 3;
        u64 b_sizecode = _b_sizecode == -1 ? a_sizecode : (u64)_b_sizecode;

        // switch through mode
        switch (s2 >> 4)
        {
        case 0:
            // if dh is flagged
            if ((s1 & 2) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in registers 0-3 and 8-bit mode
                    if ((s1 & 0xc0) != 0 || a_sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                if (get_a) a = CPURegisters[s1 >> 4].x8h();
            }
            else if (get_a) a = CPURegisters[s1 >> 4][a_sizecode];
            // if sh is flagged
            if ((s1 & 1) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in registers 0-3 and 8-bit mode
                    if ((s2 & 0x0c) != 0 || b_sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                b = CPURegisters[s2 & 15].x8h();
            }
            else b = CPURegisters[s2 & 15][b_sizecode];
            return true;

        case 1:
            // if dh is flagged
            if ((s1 & 2) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in registers 0-3 and 8-bit mode
                    if ((s1 & 0xc0) != 0 || a_sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                if (get_a) a = CPURegisters[s1 >> 4].x8h();
            }
            else if (get_a) a = CPURegisters[s1 >> 4][a_sizecode];
            // get imm
            return GetMemAdv_szc(b_sizecode, b);

        case 2:
            if constexpr (StrictUND)
            {
                // handle allow_b_mem case
                if (!allow_b_mem) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            // if dh is flagged
            if ((s1 & 2) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in registers 0-3 and 8-bit mode
                    if ((s1 & 0xc0) != 0 || a_sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                if (get_a) a = CPURegisters[s1 >> 4].x8h();
            }
            else if (get_a) a = CPURegisters[s1 >> 4][a_sizecode];
            // get mem
            return GetAddressAdv(m) && GetMemRaw_szc(m, b_sizecode, b);

        case 3:
            // get mem
            if (!GetAddressAdv(m) || (get_a && !GetMemRaw_szc(m, a_sizecode, a))) return false;
            // if sh is flagged
            if ((s1 & 1) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in registers 0-3 and 8-bit mode
                    if ((s2 & 0x0c) != 0 || b_sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                b = CPURegisters[s2 & 15].x8h();
            }
            else b = CPURegisters[s2 & 15][b_sizecode];
            return true;

        case 4:
            // get mem
            if (!GetAddressAdv(m) || (get_a && !GetMemRaw_szc(m, a_sizecode, a))) return false;
            // get imm
            return GetMemAdv_szc(b_sizecode, b);

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
    bool Computer::StoreBinaryOpFormat(u8 s1, u8 s2, u64 m, u64 res)
    {
        u64 sizecode = (s1 >> 2) & 3;

        // modes 0-2 - this method avoids having to perform the shift
        if (s2 <= 0x2f)
        {
            if ((s1 & 2) != 0) CPURegisters[s1 >> 4].x8h() = (u8)res;
            else CPURegisters[s1 >> 4][sizecode] = res;
            return true;
        }
        // modes 3-4 - the fetch already validated mode was in the proper range
        else return SetMemRaw_szc(m, sizecode, res);
    }

    bool Computer::FetchUnaryOpFormat(u8 &s, u64 &m, u64 &a, bool get_a, int _a_sizecode)
    {
        // read settings
        if (!GetMemAdv<u8>(s)) return false;

        // if they requested an explicit size for a, change it in the settings byte
        if (_a_sizecode != -1) s = (u8)((s & 0xf3) | (_a_sizecode << 2));

        u64 a_sizecode = (s >> 2) & 3;

        // switch through mode
        switch (s & 1)
        {
        case 0:
            // if h is flagged
            if ((s & 2) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in registers 0-3 and 8-bit mode
                    if ((s & 0xc0) != 0 || a_sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                if (get_a) a = CPURegisters[s >> 4].x8h();
            }
            else if (get_a) a = CPURegisters[s >> 4][a_sizecode];
            return true;

        case 1:
            return GetAddressAdv(m) && (!get_a || GetMemRaw(m, get_size(a_sizecode), a));

        default: return true; // this should never happen but compiler is complainy
        }
    }
    bool Computer::StoreUnaryOpFormat(u8 s, u64 m, u64 res)
    {
        u64 sizecode = (s >> 2) & 3;

        // switch through mode
        switch (s & 1)
        {
        case 0:
            if ((s & 2) != 0) CPURegisters[s >> 4].x8h() = (u8)res;
            else CPURegisters[s >> 4][sizecode] = res;
            return true;

        case 1:
            return SetMemRaw(m, get_size(sizecode), res);

        default: return true; // this can't happen but compiler is stupid
        }
    }

    bool Computer::FetchShiftOpFormat(u8 &s, u64 &m, u64 &val, u8 &count)
    {
        // read settings byte
        if (!GetMemAdv<u8>(s) || !GetMemAdv<u8>(count)) return false;
        u64 sizecode = (s >> 2) & 3;

        // if count set CL flag, replace it with that
        if ((count & 0x80) != 0) count = (u8)CL();
        // mask count
        count = count & (sizecode == 3 ? 0x3f : 0x1f);

        // if dest is a register
        if ((s & 1) == 0)
        {
            // if high flag set
            if ((s & 2) != 0)
            {
                if constexpr (StrictUND)
                {
                    // need to be in (ABCD)H
                    if ((s & 0xc0) != 0 || sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                val = CPURegisters[s >> 4].x8h();
            }
            else val = CPURegisters[s >> 4][sizecode];

            return true;
        }
        // otherwise is memory value
        else return GetAddressAdv(m) && GetMemRaw(m, get_size(sizecode), val);
    }
    bool Computer::StoreShiftOpFormat(u8 s, u64 m, u64 res)
    {
        u64 sizecode = (s >> 2) & 3;

        // if dest is a register
        if ((s & 1) == 0)
        {
            // if high flag set
            if ((s & 2) != 0) CPURegisters[s >> 4].x8h() = (u8)res;
            else CPURegisters[s >> 4][sizecode] = res;

            return true;
        }
        // otherwise dest is memory
        else return SetMemRaw(m, get_size(sizecode), res);
    }

    bool Computer::FetchIMMRMFormat(u8 &s, u64 &a, int _a_sizecode)
    {
        if (!GetMemAdv<u8>(s)) return false;

        u64 a_sizecode = _a_sizecode == -1 ? (s >> 2) & 3 : (u64)_a_sizecode;

        // get the value into b
        switch (s & 3)
        {
        case 0:
            a = CPURegisters[s >> 4][a_sizecode];
            return true;

        case 1:
            if constexpr (StrictUND)
            {
                // make sure we're in (ABCD)H
                if ((s & 0xc0) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            a = CPURegisters[s >> 4].x8h();
            return true;

        case 2: return GetMemAdv(get_size(a_sizecode), a);

        case 3: return GetAddressAdv(a) && GetMemRaw(a, get_size(a_sizecode), a);
        }

        return true;
    }

    bool Computer::FetchRR_RMFormat(u8 &s1, u8 &s2, u64 &dest, u64 &a, u64 &b)
    {
        if (!GetMemAdv<u8>(s1) || !GetMemAdv<u8>(s2)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        // if dest is high
        if ((s1 & 2) != 0)
        {
            if constexpr (StrictUND)
            {
                // make sure we're in (ABCD)H
                if (sizecode != 0 || (s1 & 0xc0) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            dest = CPURegisters[s1 >> 4].x8h();
        }
        else dest = CPURegisters[s1 >> 4][sizecode];

        // if a is high
        if ((s2 & 128) != 0)
        {
            if constexpr (StrictUND)
            {
                // make sure we're in (ABCD)H
                if (sizecode != 0 || (s2 & 0x0c) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            a = CPURegisters[s2 & 15].x8h();
        }
        else a = CPURegisters[s2 & 15][sizecode];

        // if b is register
        if ((s1 & 1) == 0)
        {
            u8 t;
            if (!GetMemAdv<u8>(t)) return false;

            // if b is high
            if ((t & 128) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in (ABCD)H
                    if (sizecode != 0 || (t & 0x0c) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                b = CPURegisters[t & 15].x8h();
            }
            else b = CPURegisters[t & 15][sizecode];
        }
        // otherwise b is memory
        else
        {
            if (!GetAddressAdv(b) || !GetMemRaw(b, get_size(sizecode), b)) return false;
        }

        return true;
    }
    bool Computer::StoreRR_RMFormat(u8 s1, u64 res)
    {
        // if dest is high
        if ((s1 & 2) != 0) CPURegisters[s1 >> 4].x8h() = (u8)res;
        else CPURegisters[s1 >> 4][(s1 >> 2) & 3] = res;

        return true;
    }

    void Computer::UpdateFlagsZSP(u64 value, u64 sizecode)
    {
        if constexpr (FlagAccessMasking)
        {
            RFLAGS() &= ~MASK_UNION_3(ZF, SF, PF);
			RFLAGS() |= (value == 0 ? MASK_UNION_1(ZF) : 0) | (negative(value, sizecode) ? MASK_UNION_1(SF) : 0) | (parity_table[value & 0xff] ? MASK_UNION_1(PF) : 0);
        }
        else
        {
            ZF() = value == 0;
            SF() = negative(value, sizecode);
            PF() = parity_table[value & 0xff];
        }
    }

    // -------------------------------------------------

    /*
    [8: mode]
    mode = 0: pushf
    mode = 1: pushfd
    mode = 2: pushfq
    mode = 3: popf
    mode = 4: popfd
    mode = 5: popfq
    mode = 6: sahf
    mode = 7: lahf
    */
    bool Computer::ProcessSTLDF()
    {
        u8 ext;
        if (!GetMemAdv<u8>(ext)) return false;

        switch (ext)
        {
            // pushf
        case 0:
        case 1: // VM and RF flags are cleared in the stored image
        case 2:
            return PushRaw(get_size(ext + 1), RFLAGS() & ~0x30000ul);

            // popf
        case 3:
        case 4: // can't modify reserved flags
        case 5:
        {
            u64 t;
            if (!PopRaw(get_size(ext - 2), t)) return false;
            RFLAGS() = (RFLAGS() & ~ModifiableFlags) | (t & ModifiableFlags);
            return true;
        }

            // sahf
        case 6: RFLAGS() = (RFLAGS() & ~ModifiableFlags) | (AH() & ModifiableFlags); return true;
            // lahf
        case 7: AH() = (u8)RFLAGS(); return true;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }

    /*
    [8: ext]
    ext = 0: set   CF
    ext = 1: clear CF
    ext = 2: set   IF
    ext = 3: clear IF
    ext = 4: set   DF
    ext = 5: clear DF
    ext = 6: set   AC
    ext = 7: clear AC
    ext = 8: flip  CF
    elxe UND
    */
    bool Computer::ProcessFlagManip()
    {
        u8 ext;
        if (!GetMemAdv<u8>(ext)) return false;

        switch (ext)
        {
        case 0: CF() = true; return true;
        case 1: CF() = false; return true;
        case 2: IF() = true; return true;
        case 3: IF() = false; return true;
        case 4: DF() = true; return true;
        case 5: DF() = false; return true;
        case 6: AC() = true; return true;
        case 7: AC() = false; return true;
        case 8: CF() = !CF(); return true;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }

    /*
    [op][cnd]
    cnd = 0: Z
    cnd = 1: NZ
    cnd = 2: S
    cnd = 3: NS
    cnd = 4: P
    cnd = 5: NP
    cnd = 6: O
    cnd = 7: NO
    cnd = 8: C
    cnd = 9: NC
    cnd = 10: B
    cnd = 11: BE
    cnd = 12: A
    cnd = 13: AE
    cnd = 14: L
    cnd = 15: LE
    cnd = 16: G
    cnd = 17: GE
    */
    bool Computer::ProcessSETcc()
    {
        u8 ext, s;
        u64 m, _a;
        if (!GetMemAdv<u8>(ext)) return false;
        if (!FetchUnaryOpFormat(s, m, _a, false)) return false;

        // get the flag
        bool flag;
        switch (ext)
        {
        case 0: flag = ZF(); break;
        case 1: flag = !ZF(); break;
        case 2: flag = SF(); break;
        case 3: flag = !SF(); break;
        case 4: flag = PF(); break;
        case 5: flag = !PF(); break;
        case 6: flag = OF(); break;
        case 7: flag = !OF(); break;
        case 8: flag = CF(); break;
        case 9: flag = !CF(); break;
        case 10: flag = cc_b(); break;
        case 11: flag = cc_be(); break;
        case 12: flag = cc_a(); break;
        case 13: flag = cc_ae(); break;
        case 14: flag = cc_l(); break;
        case 15: flag = cc_le(); break;
        case 16: flag = cc_g(); break;
        case 17: flag = cc_ge(); break;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        return StoreUnaryOpFormat(s, m, flag);
    }

    bool Computer::ProcessMOV()
    {
        u8 s1, s2;
        u64 m, a, b;
        return FetchBinaryOpFormat(s1, s2, m, a, b, false) && StoreBinaryOpFormat(s1, s2, m, b);
    }
    /*
    [op][cnd]
    cnd = 0: Z
    cnd = 1: NZ
    cnd = 2: S
    cnd = 3: NS
    cnd = 4: P
    cnd = 5: NP
    cnd = 6: O
    cnd = 7: NO
    cnd = 8: C
    cnd = 9: NC
    cnd = 10: B
    cnd = 11: BE
    cnd = 12: A
    cnd = 13: AE
    cnd = 14: L
    cnd = 15: LE
    cnd = 16: G
    cnd = 17: GE
    */
    bool Computer::ProcessMOVcc()
    {
        u8 ext, s1, s2;
        u64 m, _dest, src;
        if (!GetMemAdv<u8>(ext)) return false;
        if (!FetchBinaryOpFormat(s1, s2, m, _dest, src, false)) return false;

        // get the flag
        bool flag;
        switch (ext)
        {
        case 0: flag = ZF(); break;
        case 1: flag = !ZF(); break;
        case 2: flag = SF(); break;
        case 3: flag = !SF(); break;
        case 4: flag = PF(); break;
        case 5: flag = !PF(); break;
        case 6: flag = OF(); break;
        case 7: flag = !OF(); break;
        case 8: flag = CF(); break;
        case 9: flag = !CF(); break;
        case 10: flag = cc_b(); break;
        case 11: flag = cc_be(); break;
        case 12: flag = cc_a(); break;
        case 13: flag = cc_ae(); break;
        case 14: flag = cc_l(); break;
        case 15: flag = cc_le(); break;
        case 16: flag = cc_g(); break;
        case 17: flag = cc_ge(); break;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        // if flag is true, store result
        if (flag) return StoreBinaryOpFormat(s1, s2, m, src);
        // even in false case upper 32 bits must be cleared in the case of a conditional 32-bit register load
        else
        {
            // if it's a 32-bit register load
            if (((s1 >> 2) & 3) == 2 && (s2 >> 4) <= 2)
            {
                // load 32-bit partition to itself (internally zeroes high bits)
                CPURegisters[s1 >> 4].x32() = CPURegisters[s1 >> 4].x32();
            }

            return true;
        }
    }

    /*
    [4: r1][2: size][1: r1h][1: mem]
    mem = 0: [1: r2h][3:][4: r2]
    r1 <- r2
    r2 <- r1
    mem = 1: [address]
    r1 <- M[address]
    M[address] <- r1
    (r1h and r2h mark AH, BH, CH, or DH for r1 or r2)
    */
    bool Computer::ProcessXCHG()
    {
        u8 a, b;
        u64 temp_1, temp_2;

        if (!GetMemAdv<u8>(a)) return false;
        u64 sizecode = (a >> 2) & 3;

        // if a is high
        if ((a & 2) != 0)
        {
            if constexpr (StrictUND)
            {
                // make sure we're in (ABCD)H
                if ((a & 0xc0) != 0 || sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            temp_1 = CPURegisters[a >> 4].x8h();
        }
        else temp_1 = CPURegisters[a >> 4][sizecode];

        // if b is reg
        if ((a & 1) == 0)
        {
            if (!GetMemAdv<u8>(b)) return false;

            // if b is high
            if ((b & 128) != 0)
            {
                if constexpr (StrictUND)
                {
                    // make sure we're in (ABCD)H
                    if ((b & 0x0c) != 0 || sizecode != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                }

                temp_2 = CPURegisters[b & 15].x8h();
                CPURegisters[b & 15].x8h() = (u8)temp_1;
            }
            else
            {
                temp_2 = CPURegisters[b & 15][sizecode];
                CPURegisters[b & 15][sizecode] = temp_1;
            }
        }
        // otherwise b is mem
        else
        {
            u64 t;

            // get mem value into temp_2 (address in b)
            if (!GetAddressAdv(t) || !GetMemRaw(t, get_size(sizecode), temp_2)) return false;
            // store b result
            if (!SetMemRaw(t, get_size(sizecode), temp_1)) return false;
        }

        // store a's result (b's was handled internally above)
        if ((a & 2) != 0) CPURegisters[a >> 4].x8h() = (u8)temp_2;
        else CPURegisters[a >> 4][sizecode] = temp_2;

        return true;
    }

    bool Computer::ProcessJMP_raw(u64 &aft)
    {
        u8 s;
        u64 val;
        if (!FetchIMMRMFormat(s, val)) return false;

        if constexpr (StrictUND)
        {
            u64 sizecode = (s >> 2) & 3;

            // 8-bit addressing not allowed
            if (sizecode == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        aft = RIP(); // record point immediately after reading (for CALL return address)
        RIP() = val; // jump

        return true;
    }
    bool Computer::ProcessJMP()
    {
        u64 temp;
        return ProcessJMP_raw(temp);
    }
    /*
    [op][cnd]
    cnd = 0: Z
    cnd = 1: NZ
    cnd = 2: S
    cnd = 3: NS
    cnd = 4: P
    cnd = 5: NP
    cnd = 6: O
    cnd = 7: NO
    cnd = 8: C
    cnd = 9: NC
    cnd = 10: B
    cnd = 11: BE
    cnd = 12: A
    cnd = 13: AE
    cnd = 14: L
    cnd = 15: LE
    cnd = 16: G
    cnd = 17: GE
    cnd = 18: CXZ/ECXZ/RCXZ
    */
    bool Computer::ProcessJcc()
    {
        u8 ext, s;
        u64 val;
        if (!GetMemAdv<u8>(ext)) return false;
        if (!FetchIMMRMFormat(s, val)) return false;
        u64 sizecode = (s >> 2) & 3;

        if constexpr (StrictUND)
        {
            // 8-bit addressing not allowed
            if (sizecode == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        // get the flag
        bool flag;
        switch (ext)
        {
        case 0: flag = ZF(); break;
        case 1: flag = !ZF(); break;
        case 2: flag = SF(); break;
        case 3: flag = !SF(); break;
        case 4: flag = PF(); break;
        case 5: flag = !PF(); break;
        case 6: flag = OF(); break;
        case 7: flag = !OF(); break;
        case 8: flag = CF(); break;
        case 9: flag = !CF(); break;
        case 10: flag = cc_b(); break;
        case 11: flag = cc_be(); break;
        case 12: flag = cc_a(); break;
        case 13: flag = cc_ae(); break;
        case 14: flag = cc_l(); break;
        case 15: flag = cc_le(); break;
        case 16: flag = cc_g(); break;
        case 17: flag = cc_ge(); break;
        case 18: flag = CPURegisters[2][sizecode] == 0; break;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        if (flag) RIP() = val; // jump

        return true;
    }
    bool Computer::ProcessLOOPcc()
    {
        u8 ext, s;
        u64 val;

        // get the cc continue flag
        bool continue_flag;
        if (!GetMemAdv<u8>(ext)) return false;
        switch (ext)
        {
        case 0: continue_flag = true; break; // LOOP
        case 1: continue_flag = ZF(); break;   // LOOPe
        case 2: continue_flag = !ZF(); break;  // LOOPne

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        if (!FetchIMMRMFormat(s, val)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 count;
        switch (sizecode)
        {
        case 3: count = --rcx(); break;
        case 2: count = --ecx(); break;
        case 1: count = --CX(); break;
        case 0: terminate_err(ErrorCode::UndefinedBehavior); return false; // 8-bit not allowed

        default: return true; // this can't happen but compiler is stupid
        }

        if (count != 0 && continue_flag) RIP() = val; // jump if nonzero count and continue flag set

        return true;
    }

    bool Computer::ProcessCALL()
    {
        u64 temp;
        return ProcessJMP_raw(temp) && push_mem<u64>(temp);
    }
    bool Computer::ProcessRET()
    {
        u64 temp;
        if (!pop_mem<u64>(temp)) return false;
        RIP() = temp;
        return true;
    }

    bool Computer::ProcessPUSH()
    {
        u8 s;
        u64 a;
        if (!FetchIMMRMFormat(s, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        if constexpr (StrictUND)
        {
            // 8-bit push not allowed
            if (sizecode == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        return PushRaw(get_size(sizecode), a);
    }
    /*
    [4: dest][2: size][1:][1: mem]
    mem = 0:             reg
    mem = 1: [address]   M[address]
    */
    bool Computer::ProcessPOP()
    {
        u8 s;
        u64 val;
        if (!GetMemAdv<u8>(s)) return false;
        u64 sizecode = (s >> 2) & 3;

        if constexpr (StrictUND)
        {
            // 8-bit pop not allowed
            if (sizecode == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        // get the value
        if (!PopRaw(get_size(sizecode), val)) return false;

        // if register
        if ((s & 1) == 0)
        {
            CPURegisters[s >> 4][sizecode] = val;
            return true;
        }
        // otherwise is memory
        else
        {
            u64 t;
            return GetAddressAdv(t) && SetMemRaw(t, get_size(sizecode), val);
        }
    }

    /*
    [4: dest][2: size][2:]   [address]
    dest <- address
    */
    bool Computer::ProcessLEA()
    {
        u8 s;
        u64 address;
        if (!GetMemAdv<u8>(s) || !GetAddressAdv(address)) return false;
        u64 sizecode = (s >> 2) & 3;

        if constexpr (StrictUND)
        {
            // LEA doesn't allow 8-bit addressing
            if (sizecode == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        CPURegisters[s >> 4][sizecode] = address;
        return true;
    }

    bool Computer::ProcessADD()
    {
        u8 s1, s2;
        u64 m, a, b;
        if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 res = truncate(a + b, sizecode);

        UpdateFlagsZSP(res, sizecode);
        CF() = res < a;
        AF() = (res & 0xf) < (a & 0xf); // AF is just like CF but only the low nibble
        OF() = positive(a ^ b, sizecode) && negative(a ^ res, sizecode); // overflow if sign(a)=sign(b) and sign(a)!=sign(res)

        return StoreBinaryOpFormat(s1, s2, m, res);
    }
    bool Computer::ProcessSUB()
    {
        return ProcessSUB_raw(true);
    }

    bool Computer::ProcessSUB_raw(bool apply)
    {
        u8 s1, s2;
        u64 m, a, b;
        if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 res = truncate(a - b, sizecode);

        if constexpr (FlagAccessMasking)
        {
			RFLAGS() &= ~MASK_UNION_6(ZF, SF, PF, CF, AF, OF);
			RFLAGS() |= (res == 0 ? MASK_UNION_1(ZF) : 0) | (negative(res, sizecode) ? MASK_UNION_1(SF) : 0) | (parity_table[res & 0xff] ? MASK_UNION_1(PF) : 0)
                | (a < b ? MASK_UNION_1(CF) : 0) | ((a & 0xf) < (b & 0xf) ? MASK_UNION_1(AF) : 0) | (negative((a ^ b) & (a ^ res), sizecode) ? MASK_UNION_1(OF) : 0);
        }
        else
        {
            UpdateFlagsZSP(res, sizecode);
            CF() = a < b; // if a < b, a borrow was taken from the highest bit
            AF() = (a & 0xf) < (b & 0xf); // AF is just like CF but only the low nibble
            OF() = negative((a ^ b) & (a ^ res), sizecode); // overflow if sign(a)!=sign(b) and sign(a)!=sign(res)
        }

        return !apply || StoreBinaryOpFormat(s1, s2, m, res);
    }

    bool Computer::ProcessMUL_x()
    {
        u8 ext;
        if (!GetMemAdv<u8>(ext)) return false;

        switch (ext)
        {
        case 0: return ProcessMUL();
        case 1: return ProcessMULX();

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
    bool Computer::ProcessMUL()
    {
        u8 s;
        u64 a, res;
        if (!FetchIMMRMFormat(s, a)) return false;

        // switch through register sizes
        switch ((s >> 2) & 3)
        {
        case 0:
            res = AL() * a;
            AX() = (u16)res;
            CF() = OF() = AH() != 0;
            break;
        case 1:
            res = AX() * a;
            DX() = (u16)(res >> 16); AX() = (u16)res;
            CF() = OF() = DX() != 0;
            break;
        case 2:
            res = eax() * a;
            edx() = (u32)(res >> 32); eax() = (u32)res;
            CF() = OF() = edx() != 0;
            break;
        case 3:
        {
            auto _res = (BiggerInts::uint_t<128>)rax() * (BiggerInts::uint_t<128>)a;
            rdx() = _res.blocks[1]; rax() = _res.blocks[0];
            CF() = OF() = rdx() != 0;
            break;
        }
        } // end switch
        
		RFLAGS() ^= Rand() & MASK_UNION_4(SF, ZF, AF, PF);

        return true;
    }
    bool Computer::ProcessMULX()
    {
        u8 s1, s2;
        u64 dest, a, b, res;
        if (!FetchRR_RMFormat(s1, s2, dest, a, b)) return false;

        // switch through register sizes
        switch ((s1 >> 2) & 3)
        {
        case 2:
            res = a * b;
            CPURegisters[s1 >> 4].x32() = (u32)(res >> 32); CPURegisters[s2 & 15].x32() = (u32)res;
            break;
        case 3:
        {
            auto _res = (BiggerInts::uint_t<128>)a * (BiggerInts::uint_t<128>)b;
            CPURegisters[s1 >> 4].x64() = _res.blocks[1]; CPURegisters[s2 & 15].x64() = _res.blocks[0];
            break;
        }

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        return true;
    }
    bool Computer::ProcessIMUL()
    {
        u8 mode;
        if (!GetMemAdv<u8>(mode)) return false;

        switch (mode)
        {
        case 0: return ProcessUnary_IMUL();
        case 1: return ProcessBinary_IMUL();
        case 2: return ProcessTernary_IMUL();

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
    bool Computer::ProcessUnary_IMUL()
    {
        u8 s;
        u64 _a;
        if (!FetchIMMRMFormat(s, _a)) return false;
        u64 sizecode = (s >> 2) & 3;

        // get val as sign extended
        i64 a = sign_extend(_a, sizecode);
        i64 res;

        // switch through register sizes
        switch (sizecode)
        {
        case 0:
            res = (i8)AL() * a;
            AX() = (u16)res;
            CF() = OF() = res != (i8)res;
            break;
        case 1:
            res = (i16)AX() * a;
            DX() = (u16)(res >> 16); AX() = (u16)res;
            CF() = OF() = res != (i16)res;
            break;
        case 2:
            res = (i32)eax() * a;
            edx() = (u32)(res >> 32); eax() = (u32)res;
            CF() = OF() = res != (i32)res;
            break;
        case 3:
        {
            auto _res = (BiggerInts::int_t<128>)(i64)rax() * (BiggerInts::int_t<128>)a;
            rdx() = _res.blocks[1]; rax() = _res.blocks[0];
            CF() = OF() = _res != (i64)_res;
            break;
        }
        } // end switch

		RFLAGS() ^= Rand() & MASK_UNION_4(SF, ZF, AF, PF);

        return true;
    }
    bool Computer::ProcessBinary_IMUL()
    {
        u8 s1, s2;
        u64 m, _a, _b;
        if (!FetchBinaryOpFormat(s1, s2, m, _a, _b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        // get vals as sign extended
        i64 a = sign_extend(_a, sizecode);
        i64 b = sign_extend(_b, sizecode);

        i64 res = 0;

        // switch through register sizes
        switch (sizecode)
        {
        case 0:
            res = a * b;
            CF() = OF() = res != (i8)res;
            break;
        case 1:
            res = a * b;
            CF() = OF() = res != (i16)res;
            break;
        case 2:
            res = a * b;
            CF() = OF() = res != (i32)res;
            break;
        case 3:
        {
            auto _res = (BiggerInts::int_t<128>)a * (BiggerInts::int_t<128>)b;
            res = (i64)_res;
            CF() = OF() = _res != res;
            break;
        }
        } // end switch

		RFLAGS() ^= Rand() & MASK_UNION_4(SF, ZF, AF, PF);

        return StoreBinaryOpFormat(s1, s2, m, (u64)res);
    }
    bool Computer::ProcessTernary_IMUL()
    {
        u8 s;
        u64 _a, _b;
        if (!FetchTernaryOpFormat(s, _a, _b)) return false;
        u64 sizecode = (s >> 2) & 3;

        // get vals as sign extended
        i64 a = sign_extend(_a, sizecode);
        i64 b = sign_extend(_b, sizecode);

        i64 res = 0;

        // switch through register sizes
        switch (sizecode)
        {
        case 0:
            res = a * b;
            CF() = OF() = res != (i8)res;
            break;
        case 1:
            res = a * b;
            CF() = OF() = res != (i16)res;
            break;
        case 2:
            res = a * b;
            CF() = OF() = res != (i32)res;
            break;
        case 3:
        {
            auto _res = (BiggerInts::int_t<128>)a * (BiggerInts::int_t<128>)b;
            res = (i64)_res;
            CF() = OF() = _res != res;
            break;
        }
        } // end switch

		RFLAGS() ^= Rand() & MASK_UNION_4(SF, ZF, AF, PF);

        return StoreTernaryOPFormat(s, (u64)res);
    }

    bool Computer::ProcessDIV()
    {
        u8 s;
        u64 a;
        if (!FetchIMMRMFormat(s, a)) return false;

        if (a == 0) { terminate_err(ErrorCode::ArithmeticError); return false; }

        u64 full, quo, rem;

        // switch through register sizes
        switch ((s >> 2) & 3)
        {
        case 0:
            full = AX();
            quo = full / a; rem = full % a;
            if (quo != (u8)quo) { terminate_err(ErrorCode::ArithmeticError); return false; }
            AL() = (u8)quo; AH() = (u8)rem;
            break;
        case 1:
            full = ((u64)DX() << 16) | AX();
            quo = full / a; rem = full % a;
            if (quo != (u16)quo) { terminate_err(ErrorCode::ArithmeticError); return false; }
            AX() = (u16)quo; DX() = (u16)rem;
            break;
        case 2:
            full = ((u64)edx() << 32) | eax();
            quo = full / a; rem = full % a;
            if (quo != (u32)quo) { terminate_err(ErrorCode::ArithmeticError); return false; }
            eax() = (u32)quo; edx() = (u32)rem;
            break;
        case 3:
        {
            BiggerInts::uint_t<128> Full; Full.blocks[1] = rdx(); Full.blocks[0] = rax();
            auto divmod = BiggerInts::divmod(Full, (BiggerInts::uint_t<128>)a);
            if (divmod.first != (u64)divmod.first) { terminate_err(ErrorCode::ArithmeticError); return false; }
            rax() = (u64)divmod.first; rdx() = (u64)divmod.second;
            break;
        }
        } // end switch

		RFLAGS() ^= Rand() & MASK_UNION_6(CF, OF, SF, ZF, AF, PF);

        return true;
    }
    bool Computer::ProcessIDIV()
    {
        u8 s;
        u64 _a;
        if (!FetchIMMRMFormat(s, _a)) return false;
        u64 sizecode = (s >> 2) & 3;

        if (_a == 0) { terminate_err(ErrorCode::ArithmeticError); return false; }

        // get val as signed
        i64 a = (i64)sign_extend(_a, sizecode);

        i64 full, quo, rem;

        // switch through register sizes
        switch ((s >> 2) & 3)
        {
        case 0:
            full = (i16)AX();
            quo = full / a; rem = full % a;
            if (quo != (i8)quo) { terminate_err(ErrorCode::ArithmeticError); return false; }
            AL() = (u8)quo; AH() = (u8)rem;
            break;
        case 1:
            full = ((i32)DX() << 16) | AX();
            quo = full / a; rem = full % a;
            if (quo != (i16)quo) { terminate_err(ErrorCode::ArithmeticError); return false; }
            AX() = (u16)quo; DX() = (u16)rem;
            break;
        case 2:
            full = ((i64)edx() << 32) | eax();
            quo = full / a; rem = full % a;
            if (quo != (i32)quo) { terminate_err(ErrorCode::ArithmeticError); return false; }
            eax() = (u32)quo; edx() = (u32)rem;
            break;
        case 3:
        {
            BiggerInts::int_t<128> Full; Full.blocks[1] = rdx(); Full.blocks[0] = rax();
            auto divmod = BiggerInts::divmod(Full, (BiggerInts::int_t<128>)a);
            if (divmod.first != (i64)divmod.first) { terminate_err(ErrorCode::ArithmeticError); return false; }
            rax() = (u64)divmod.first; rdx() = (u64)divmod.second;
            break;
        }
        } // end switch

		RFLAGS() ^= Rand() & MASK_UNION_6(CF, OF, SF, ZF, AF, PF);

        return true;
    }

    bool Computer::ProcessSHL()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = truncate(val << count, sizecode);

            UpdateFlagsZSP(res, sizecode);
            CF() = count < get_size_bits(sizecode) ? ((val >> (get_size_bits(sizecode) - count)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
            OF() = count == 1 ? negative(res, sizecode) != CF() : Rand() & 1; // OF is 1 if top 2 bits of original value were different (UND for sh != 1)
            AF() = Rand() & 1; // AF is undefined

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }
    bool Computer::ProcessSHR()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = val >> count;

            UpdateFlagsZSP(res, sizecode);
            CF() = count < get_size_bits(sizecode) ? ((val >> (count - 1)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
            OF() = count == 1 ? negative(val, sizecode) : Rand() & 1; // OF is high bit of original value (UND for sh != 1)
            AF() = Rand() & 1; // AF is undefined

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }

    bool Computer::ProcessSAL()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = truncate((u64)((i64)sign_extend(val, sizecode) << count), sizecode);

            UpdateFlagsZSP(res, sizecode);
            CF() = count < get_size_bits(sizecode) ? ((val >> (get_size_bits(sizecode) - count)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
            OF() = count == 1 ? negative(res, sizecode) != CF() : Rand() & 1; // OF is 1 if top 2 bits of original value were different (UND for sh != 1)
            AF() = Rand() & 1; // AF is undefined

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }
    bool Computer::ProcessSAR()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = truncate((u64)((i64)sign_extend(val, sizecode) >> count), sizecode);

            UpdateFlagsZSP(res, sizecode);
            CF() = count < get_size_bits(sizecode) ? ((val >> (count - 1)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
            OF() = count == 1 ? false : Rand() & 1; // OF is cleared (UND for sh != 1)
            AF() = Rand() & 1; // AF is undefined

            return StoreShiftOpFormat(s, m, res);
        }
        else return false;
    }

    bool Computer::ProcessROL()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        count %= get_size_bits(sizecode); // rotate performed modulo-n

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = truncate((val << count) | (val >> (get_size_bits(sizecode) - count)), sizecode);

            CF() = ((val >> (get_size_bits(sizecode) - count)) & 1) == 1; // CF holds last bit shifted around
            OF() = count == 1 ? CF() ^ negative(res, sizecode) : Rand() & 1; // OF is xor of CF (after rotate) and high bit of result (UND if sh != 1)

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }
    bool Computer::ProcessROR()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        count %= get_size_bits(sizecode); // rotate performed modulo-n

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = truncate((val >> count) | (val << (get_size_bits(sizecode) - count)), sizecode);

            CF() = ((val >> (count - 1)) & 1) == 1; // CF holds last bit shifted around
            OF() = count == 1 ? negative(res, sizecode) ^ (((res >> (get_size_bits(sizecode) - 2)) & 1) != 0) : Rand() & 1; // OF is xor of 2 highest bits of result

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }

    bool Computer::ProcessRCL()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        count %= get_size_bits(sizecode) + 1; // rotate performed modulo-n+1

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = val, temp;
            u64 high_mask = (u64)1 << (get_size_bits(sizecode) - 1); // mask for highest bit
            for (u16 i = 0; i < count; ++i)
            {
                temp = res << 1; // shift res left by 1, store in temp
                temp |= CF() ? 1 : 0ul; // or in CF to the lowest bit
                CF() = (res & high_mask) != 0; // get the previous highest bit into CF
                res = temp; // store back to res
            }

            OF() = count == 1 ? CF() ^ negative(res, sizecode) : Rand() & 1; // OF is xor of CF (after rotate) and high bit of result (UND if sh != 1)

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }
    bool Computer::ProcessRCR()
    {
        u8 s, count;
        u64 m, val;
        if (!FetchShiftOpFormat(s, m, val, count)) return false;
        u64 sizecode = (s >> 2) & 3;

        count %= get_size_bits(sizecode) + 1; // rotate performed modulo-n+1

        // shift of zero is no-op
        if (count != 0)
        {
            u64 res = val, temp;
            u64 high_mask = (u64)1 << (get_size_bits(sizecode) - 1); // mask for highest bit
            for (u16 i = 0; i < count; ++i)
            {
                temp = res >> 1; // shift res right by 1, store in temp
                temp |= CF() ? high_mask : 0ul; // or in CF to the highest bit
                CF() = (res & 1) != 0; // get the previous low bit into CF
                res = temp; // store back to res
            }

            OF() = count == 1 ? negative(res, sizecode) ^ (((res >> (get_size_bits(sizecode) - 2)) & 1) != 0) : Rand() & 1; // OF is xor of 2 highest bits of result

            return StoreShiftOpFormat(s, m, res);
        }
        else return true;
    }

    bool Computer::ProcessAND_raw(bool apply)
    {
        u8 s1, s2;
        u64 m, a, b;
        if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 res = a & b;

        UpdateFlagsZSP(res, sizecode);
        OF() = false;
        CF() = false;
        AF() = Rand() & 1;

        return !apply || StoreBinaryOpFormat(s1, s2, m, res);
    }

    bool Computer::ProcessAND()
    {
        return ProcessAND_raw(true);
    }
    bool Computer::ProcessOR()
    {
        u8 s1, s2;
        u64 m, a, b;
        if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 res = a | b;

        UpdateFlagsZSP(res, sizecode);
        OF() = false;
        CF() = false;
        AF() = Rand() & 1;

        return StoreBinaryOpFormat(s1, s2, m, res);
    }
    bool Computer::ProcessXOR()
    {
        u8 s1, s2;
        u64 m, a, b;
        if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 res = a ^ b;

        UpdateFlagsZSP(res, sizecode);
        OF() = false;
        CF() = false;
        AF() = Rand() & 1;

        return StoreBinaryOpFormat(s1, s2, m, res);
    }

    bool Computer::ProcessINC()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = truncate(a + 1, sizecode);

        if constexpr (FlagAccessMasking)
        {
			RFLAGS() &= ~MASK_UNION_5(ZF, SF, PF, AF, OF);
			RFLAGS() |= (res == 0 ? MASK_UNION_1(ZF) : 0) | (negative(res, sizecode) ? MASK_UNION_1(SF) : 0) | (parity_table[res & 0xff] ? MASK_UNION_1(PF) : 0)
                | ((res & 0xf) == 0 ? MASK_UNION_1(AF) : 0) | (positive(a, sizecode) && negative(res, sizecode) ? MASK_UNION_1(OF) : 0);
        }
        else
        {
            UpdateFlagsZSP(res, sizecode);
            AF() = (res & 0xf) == 0; // low nibble of 0 was a nibble overflow (TM)
            OF() = positive(a, sizecode) && negative(res, sizecode); // + -> - is overflow
        }

        return StoreUnaryOpFormat(s, m, res);
    }
    bool Computer::ProcessDEC()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = truncate(a - 1, sizecode);

        if constexpr (FlagAccessMasking)
        {
			RFLAGS() &= ~MASK_UNION_5(ZF, SF, PF, AF, OF);
			RFLAGS() |= (res == 0 ? MASK_UNION_1(ZF) : 0) | (negative(res, sizecode) ? MASK_UNION_1(SF) : 0) | (parity_table[res & 0xff] ? MASK_UNION_1(PF) : 0)
                | ((a & 0xf) == 0 ? MASK_UNION_1(AF) : 0) | (negative(a, sizecode) && positive(res, sizecode) ? MASK_UNION_1(OF) : 0);
        }
        else
        {
            UpdateFlagsZSP(res, sizecode);
            AF() = (a & 0xf) == 0; // nibble a = 0 results in borrow from the low nibble
            OF() = negative(a, sizecode) && positive(res, sizecode); // - -> + is overflow
        }

        return StoreUnaryOpFormat(s, m, res);
    }

    bool Computer::ProcessNEG()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = truncate(0 - a, sizecode);

        UpdateFlagsZSP(res, sizecode);
        CF() = 0 < a; // if 0 < a, a borrow was taken from the highest bit (see SUB code where a=0, b=a)
        AF() = 0 < (a & 0xf); // AF is just like CF but only the low nibble
        OF() = negative(a, sizecode) && negative(res, sizecode);

        return StoreUnaryOpFormat(s, m, res);
    }
    bool Computer::ProcessNOT()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = truncate(~a, sizecode);

        return StoreUnaryOpFormat(s, m, res);
    }

    bool Computer::ProcessCMP()
    {
        return ProcessSUB_raw(false);
    }
    bool Computer::ProcessTEST()
    {
        return ProcessAND_raw(false);
    }

    bool Computer::ProcessCMPZ()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        UpdateFlagsZSP(a, sizecode);
		RFLAGS() &= ~MASK_UNION_3(CF, OF, AF);

        return true;
    }

    bool Computer::ProcessBSWAP()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        return StoreUnaryOpFormat(s, m, bswap(a, sizecode));
    }
    bool Computer::ProcessBEXTR()
    {
        u8 s1, s2;
        u64 m, a, b;
        if (!FetchBinaryOpFormat(s1, s2, m, a, b, true, -1, 1)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        int pos = (int)((b >> 8) % get_size_bits(sizecode));
        int len = (int)((b & 0xff) % get_size_bits(sizecode));

        u64 res = (a >> pos) & (((u64)1 << len) - 1);

        EFLAGS() = 2; // clear all the (public) flags (flag 1 must always be set)
        ZF() = res == 0; // ZF is set on zero
		RFLAGS() ^= Rand() & MASK_UNION_3(AF, SF, PF); // AF, SF, PF undefined

        return StoreBinaryOpFormat(s1, s2, m, res);
    }

    bool Computer::ProcessBLSI()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = a & (~a + 1);

        ZF() = res == 0;
        SF() = negative(res, sizecode);
        CF() = a != 0;
        OF() = false;
		RFLAGS() ^= Rand() & MASK_UNION_2(AF, PF);

        return StoreUnaryOpFormat(s, m, res);
    }
    bool Computer::ProcessBLSMSK()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = truncate(a ^ (a - 1), sizecode);

        SF() = negative(res, sizecode);
        CF() = a == 0;
        ZF() = OF() = false;
		RFLAGS() ^= Rand() & MASK_UNION_2(AF, PF);

        return StoreUnaryOpFormat(s, m, res);
    }
    bool Computer::ProcessBLSR()
    {
        u8 s;
        u64 m, a;
        if (!FetchUnaryOpFormat(s, m, a)) return false;
        u64 sizecode = (s >> 2) & 3;

        u64 res = a & (a - 1);

        ZF() = res == 0;
        SF() = negative(res, sizecode);
        CF() = a == 0;
        OF() = false;
		RFLAGS() ^= Rand() & MASK_UNION_2(AF, PF);

        return StoreUnaryOpFormat(s, m, res);
    }
    bool Computer::ProcessANDN()
    {
        u8 s1, s2;
        u64 dest, a, b;
        if (!FetchRR_RMFormat(s1, s2, dest, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        if constexpr (StrictUND)
        {
            // only supports 32 and 64-bit operands
            if (sizecode != 2 && sizecode != 3) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        u64 res = ~a & b;

        ZF() = res == 0;
        SF() = negative(res, sizecode);
        OF() = false;
        CF() = false;
		RFLAGS() ^= Rand() & MASK_UNION_2(AF, PF);

        return StoreRR_RMFormat(s1, res);
    }

    /*
    [8: ext]   [binary]
    ext = 0: BT
    ext = 1: BTS
    ext = 2: BTR
    ext = 3: BTC
    else UND
    */
    bool Computer::ProcessBTx()
    {
        u8 ext, s1, s2;
        u64 m, a, b;
        if (!GetMemAdv<u8>(ext)) return false;

        if (!FetchBinaryOpFormat(s1, s2, m, a, b, true, -1, 0, false)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 mask = (u64)1 << (b % get_size_bits(sizecode)); // performed modulo-n

        CF() = (a & mask) != 0;
		RFLAGS() ^= Rand() & MASK_UNION_4(OF, SF, AF, PF);

        switch (ext)
        {
        case 0: return true;
        case 1: return StoreBinaryOpFormat(s1, s2, m, a | mask);
        case 2: return StoreBinaryOpFormat(s1, s2, m, a & ~mask);
        case 3: return StoreBinaryOpFormat(s1, s2, m, a ^ mask);

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }

    /*
    [8: ext]
    ext = 0: CWD
    ext = 1: CDQ
    ext = 2: CQO
    ext = 3: CBW
    ext = 4: CWDE
    ext = 5: CDQE
    */
    bool Computer::ProcessCxy()
    {
        u8 ext;
        if (!GetMemAdv<u8>(ext)) return false;

        switch (ext)
        {
        case 0: DX() = (i16)AX() >= 0 ? 0 : 0xffff; return true;
        case 1: edx() = (i32)eax() >= 0 ? 0 : 0xffffffff; return true;
        case 2: rdx() = (i64)rax() >= 0 ? 0 : 0xffffffffffffffff; return true;

        case 3: AX() = (u16)(i8)AL(); return true;
        case 4: eax() = (u32)(i16)AX(); return true;
        case 5: rax() = (u64)(i32)eax(); return true;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
    /*
    [4: dest][4: mode]   [1: mem][1: sh][2:][4: src]
    mode = 0:  16 <- 8  Zero
    mode = 1:  16 <- 8  Sign
    mode = 2:  32 <- 8  Zero
    mode = 3:  32 <- 16 Zero
    mode = 4:  32 <- 8  Sign
    mode = 5:  32 <- 16 Sign
    mode = 6:  64 <- 8  Zero
    mode = 7:  64 <- 16 Zero
    mode = 8:  64 <- 8  Sign
    mode = 9:  64 <- 16 Sign
    mode = 10: 64 <- 32 Sign
    else UND
    (sh marks that source is (ABCD)H)
    */
    bool Computer::ProcessMOVxX()
    {
        u8 s1, s2;
        u64 src;
        if (!GetMemAdv<u8>(s1) || !GetMemAdv<u8>(s2)) return false;

        // if source is register
        if ((s2 & 128) == 0)
        {
            switch (s1 & 15)
            {
            case 0:
            case 1:
            case 2:
            case 4:
            case 6:
            case 8:
                if ((s2 & 64) != 0) // if high register
                {
                    if constexpr (StrictUND)
                    {
                        // make sure we're in registers A-D
                        if ((s2 & 0x0c) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
                    }

                    src = CPURegisters[s2 & 15].x8h();
                }
                else src = CPURegisters[s2 & 15].x8();
                break;

            case 3: case 5: case 7: case 9: src = CPURegisters[s2 & 15].x16(); break;

            case 10: src = CPURegisters[s2 & 15].x32(); break;

            default: terminate_err(ErrorCode::UndefinedBehavior); return false;
            }
        }
        // otherwise is memory value
        else
        {
            if (!GetAddressAdv(src)) return false;
            switch (s1 & 15)
            {
            case 0: case 1: case 2: case 4: case 6: case 8: if (!GetMemRaw(src, 1, src)) return false; break;
            case 3: case 5: case 7: case 9: if (!GetMemRaw(src, 2, src)) return false; break;
            case 10: if (!GetMemRaw(src, 4, src)) return false; break;

            default: terminate_err(ErrorCode::UndefinedBehavior); return false;
            }
        }

        // store the value
        switch (s1 & 15)
        {
        case 0: CPURegisters[s1 >> 4].x16() = (u16)src; break;
        case 1: CPURegisters[s1 >> 4].x16() = (u16)sign_extend(src, 0); break;

        case 2: case 3: CPURegisters[s1 >> 4].x32() = (u32)src; break;
        case 4: CPURegisters[s1 >> 4].x32() = (u32)sign_extend(src, 0); break;
        case 5: CPURegisters[s1 >> 4].x32() = (u32)sign_extend(src, 1); break;

        case 6: case 7: CPURegisters[s1 >> 4].x64() = src; break;
        case 8: CPURegisters[s1 >> 4].x64() = sign_extend(src, 0); break;
        case 9: CPURegisters[s1 >> 4].x64() = sign_extend(src, 1); break;
        case 10: CPURegisters[s1 >> 4].x64() = sign_extend(src, 2); break;
        }

        return true;
    }

    /*
    [8: ext]   [binary]
    ext = 0: ADC
    ext = 1: ADCX
    ext = 2: ADOX
    else UND
    */
    bool Computer::ProcessADXX()
    {
        u8 ext, s1, s2;
        u64 m, a, b;
        if (!GetMemAdv<u8>(ext)) return false;

        if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
        u64 sizecode = (s1 >> 2) & 3;

        u64 res = a + b;

        switch (ext)
        {
        case 0: case 1: if (CF()) ++res; break;
        case 2: if (OF()) ++res; break;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        res = truncate(res, sizecode);

        switch (ext)
        {
        case 0:
            CF() = res < a;
            UpdateFlagsZSP(res, sizecode);
            AF() = (res & 0xf) < (a & 0xf); // AF is just like CF but only the low nibble
            OF() = positive(a, sizecode) == positive(b, sizecode) && positive(a, sizecode) != positive(res, sizecode);
            break;
        case 1:
            CF() = res < a;
            break;
        case 2:
            OF() = positive(a, sizecode) == positive(b, sizecode) && positive(a, sizecode) != positive(res, sizecode);
            break;
        }

        return StoreBinaryOpFormat(s1, s2, m, res);
    }

    /*
    [8: ext]
    ext = 0: AAA
    ext = 1: AAS
    ext = 2: DAA
    ext = 3: DAS
    else UND
    */
    bool Computer::ProcessAAX()
    {
        u8 ext;
        u8 temp_u8;
        bool temp_b;
        if (!GetMemAdv<u8>(ext)) return false;

        switch (ext)
        {
        case 0:
            if ((AL() & 0x0f) > 9 || AF())
            {
                AX() += 0x106;
                AF() = true;
                CF() = true;
            }
            else
            {
                AF() = false;
                CF() = false;
            }
            AL() &= 0x0f;

			RFLAGS() ^= Rand() & MASK_UNION_4(OF, SF, ZF, PF);

            return true;

        case 1:
            if ((AL() & 0x0f) > 9 || AF())
            {
                AX() -= 6;
                --AH();
                AF() = true;
                CF() = true;
            }
            else
            {
                AF() = false;
                CF() = false;
            }
            AL() &= 0x0f;

			RFLAGS() ^= Rand() & MASK_UNION_4(OF, SF, ZF, PF);

            return true;

        case 2:
            // Intel's reference has this instruction modify CF unnecessarily - leaving those lines in but commenting them out

            temp_u8 = (u8)AL();
            temp_b = CF();

            //CF() = false;

            if ((AL() & 0x0f) > 9 || AF())
            {
                AL() += 6;
                //CF() = temp_b || AL() < 6; // AL() < 6 gets the carry flag we need from the above addition
                AF() = true;
            }
            else AF() = false;

            if (temp_u8 > 0x99 || temp_b)
            {
                AL() += 0x60;
                CF() = true;
            }
            else CF() = false;

            // update flags
            UpdateFlagsZSP(AL(), 0);
			RFLAGS() ^= Rand() & MASK_UNION_1(OF);

            return true;

        case 3:
            temp_u8 = (u8)AL();
            temp_b = CF();

            CF() = false;
            
            if ((AL() & 0x0f) > 9 || AF())
            {
                CF() = temp_b || AL() < 6; // AL() < 6 gets the borrow flag we need from the next subtraction:
                AL() -= 6;
                AF() = true;
            }
            else AF() = false;

            if (temp_u8 > 0x99 || temp_b)
            {
                AL() -= 0x60;
                CF() = true;
            }

            // update flags
            UpdateFlagsZSP(AL(), 0);
			RFLAGS() ^= Rand() & MASK_UNION_1(OF);

            return true;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        return true;
    }

    // helper for MOVS - performs the actual move
    bool Computer::_ProcessSTRING_MOVS(u64 sizecode)
    {
        u64 size = get_size(sizecode);
        u64 temp;

        if (!GetMemRaw(rsi(), size, temp) || !SetMemRaw(rdi(), size, temp)) return false;

        if (DF()) { rsi() -= size; rdi() -= size; }
        else { rsi() += size; rdi() += size; }

        return true;
    }
    bool Computer::_ProcessSTRING_CMPS(u64 sizecode)
    {
        u64 size = get_size(sizecode);
        u64 a, b;

        if (!GetMemRaw(rsi(), size, a) || !GetMemRaw(rdi(), size, b)) return false;

        if (DF()) { rsi() -= size; rdi() -= size; }
        else { rsi() += size; rdi() += size; }

        u64 res = truncate(a - b, sizecode);

        // update flags
        UpdateFlagsZSP(res, sizecode);
        CF() = a < b; // if a < b, a borrow was taken from the highest bit
        AF() = (a & 0xf) < (b & 0xf); // AF is just like CF but only the low nibble
        OF() = negative(a ^ b, sizecode) && negative(a ^ res, sizecode); // overflow if sign(a)!=sign(b) and sign(a)!=sign(res)

        return true;
    }
    bool Computer::_ProcessSTRING_LODS(u64 sizecode)
    {
        u64 size = get_size(sizecode);
        u64 temp;

        if (!GetMemRaw(rsi(), size, temp)) return false;

        if (DF()) rsi() -= size;
        else rsi() += size;

        CPURegisters[0][sizecode] = temp;

        return true;
    }
    bool Computer::_ProcessSTRING_STOS(u64 sizecode)
    {
        u64 size = get_size(sizecode);

        if (!SetMemRaw(rdi(), size, CPURegisters[0][sizecode])) return false;

        if (DF()) rdi() -= size;
        else rdi() += size;

        return true;
    }
    bool Computer::_ProcessSTRING_SCAS(u64 sizecode)
    {
        u64 size = get_size(sizecode);
        u64 a = CPURegisters[0][sizecode];
        u64 b;

        if (!GetMemRaw(rdi(), size, b)) return false;

        u64 res = truncate(a - b, sizecode);

        // update flags
        UpdateFlagsZSP(res, sizecode);
        CF() = a < b; // if a < b, a borrow was taken from the highest bit
        AF() = (a & 0xf) < (b & 0xf); // AF is just like CF but only the low nibble
        OF() = negative(a ^ b, sizecode) && negative(a ^ res, sizecode); // overflow if sign(a)!=sign(b) and sign(a)!=sign(res)

        if (DF()) rdi() -= size;
        else rdi() += size;

        return true;
    }

    /*
    [6: mode][2: size]
        mode = 0:        MOVS
        mode = 1:  REP   MOVS
        mode = 2:        CMPS
        mode = 3:  REPE  CMPS
        mode = 4:  REPNE CMPS
        mode = 5:        LODS
        mode = 6:  REP   LODS
        mode = 7:        STOS
        mode = 8:  REP   STOS
        mode = 9:        SCAS
        mode = 10: REPE  SCAS
        mode = 11: REPNE SCAS
        else UND
    */
    bool Computer::ProcessSTRING()
    {
        u8 s;
        if (!GetMemAdv<u8>(s)) return false;
        u64 sizecode = s & 3;
        
        // switch through mode
        switch (s >> 2)
        {
        case 0: // MOVS
            if (!_ProcessSTRING_MOVS(sizecode)) return false;
            break;

        case 1: // REP MOVS

            // if we can do the whole thing in a single tick
            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_MOVS(sizecode)) return false;
                    --rcx();
                }
            }
            // otherwise perform a single iteration (if count is nonzero)
            else if (rcx())
            {
                if (!_ProcessSTRING_MOVS(sizecode)) return false;
                --rcx();
                RIP() -= 2; // reset RIP to repeat instruction
            }
            break;

        case 2: // CMPS
            if (!_ProcessSTRING_CMPS(sizecode)) return false;
            break;

        case 3: // REPE CMPS

            // if we can do the whole thing in a single tick
            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_CMPS(sizecode)) return false;
                    --rcx();
                    if (!ZF()) break;
                }
            }
            // otherwise perform a single iteration (if count is nonzero)
            else if (rcx())
            {
                if (!_ProcessSTRING_CMPS(sizecode)) return false;
                --rcx();
                if (ZF()) RIP() -= 2; // if condition met, reset RIP to repeat instruction
            }
            break;

        case 4: // REPNE CMPS

            // if we can do the whole thing in a single tick
            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_CMPS(sizecode)) return false;
                    --rcx();
                    if (ZF()) break;
                }
            }
            // otherwise perform a single iteration (if count is nonzero)
            else if (rcx())
            {
                if (!_ProcessSTRING_CMPS(sizecode)) return false;
                --rcx();
                if (!ZF()) RIP() -= 2; // if condition met, reset RIP to repeat instruction
            }
            break;

        case 5: // LODS
            if (!_ProcessSTRING_LODS(sizecode)) return false;
            break;

        case 6: // REP LODS

            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_LODS(sizecode)) return false;
                    --rcx();
                }
            }
            else if (rcx())
            {
                if (!_ProcessSTRING_LODS(sizecode)) return false;
                --rcx();
                RIP() -= 2; // reset RIP to repeat instruction
            }
            break;

        case 7: // STOS
            if (!_ProcessSTRING_STOS(sizecode)) return false;
            break;

        case 8: // REP STOS
            
            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_STOS(sizecode)) return false;
                    --rcx();
                }
            }
            else if (rcx())
            {
                if (!_ProcessSTRING_STOS(sizecode)) return false;
                --rcx();
                RIP() -= 2; // reset RIP to repeat instruction
            }
            break;

        case 9: // SCAS
            if (!_ProcessSTRING_SCAS(sizecode)) return false;
            break;

        case 10: // REPE SCAS

            // if we can do the whole thing in a single tick
            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_SCAS(sizecode)) return false;
                    --rcx();
                    if (!ZF()) break;
                }
            }
            // otherwise perform a single iteration (if count is nonzero)
            else if (rcx())
            {
                if (!_ProcessSTRING_SCAS(sizecode)) return false;
                --rcx();
                if (ZF()) RIP() -= 2; // if condition met, reset RIP to repeat instruction
            }
            break;

        case 11: // REPNE SCAS

            // if we can do the whole thing in a single tick
            if (OTRF())
            {
                while (rcx())
                {
                    if (!_ProcessSTRING_SCAS(sizecode)) return false;
                    --rcx();
                    if (ZF()) break;
                }
            }
            // otherwise perform a single iteration (if count is nonzero)
            else if (rcx())
            {
                if (!_ProcessSTRING_SCAS(sizecode)) return false;
                --rcx();
                if (!ZF()) RIP() -= 2; // if condition met, reset RIP to repeat instruction
            }
            break;

        // otherwise unknown mode
        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        return true;
    }

    /*
    [1: forward][1: mem][2: size][4: dest]
        mem = 0: [4:][4: src]
        mem = 1: [address]
    */
    bool Computer::_Process_BSx_common(u8 &s, u64 &src, u64 &sizecode)
    {
        if (!GetMemAdv<u8>(s)) return false;
        sizecode = (s >> 4) & 3;

        // if src is mem
        if (s & 64)
        {
            if (!GetAddressAdv(src) || !GetMemRaw(src, get_size(sizecode), src)) return false;
        }
        // otherwise src is reg
        else
        {
            u8 t;
            if (!GetMemAdv<u8>(t)) return false;
            src = CPURegisters[t & 15][sizecode];
        }

        return true;
    }
    bool Computer::ProcessBSx()
    {
        u8 s;
        u64 src, sizecode, res;
        if (!_Process_BSx_common(s, src, sizecode)) return false;

        // if src is zero
        if (src == 0)
        {
            ZF() = true;
            res = Rand(); // result is undefined in this case
        }
        // otherwise perform the search
        else
        {
            ZF() = false;
            res = get_sizecode(s & 128 ? isolate_low_bit(src) : isolate_high_bit(src));
        }

        // update dest and flags
        CPURegisters[s & 15][sizecode] = res;
		RFLAGS() ^= Rand() & MASK_UNION_5(CF, OF, SF, AF, PF);

        return true;
    }

    bool Computer::ProcessTZCNT()
    {
        u8 s;
        u64 src, sizecode, res;
        if (!_Process_BSx_common(s, src, sizecode)) return false;

        // if src is zero
        if (src == 0)
        {
            CF() = true;
            res = get_size_bits(sizecode); // result is operand size (in bits) in this case
        }
        // otherwise perform the search
        else
        {
            CF() = false;
            res = get_sizecode(isolate_low_bit(src));
        }

        // update dest and flags
        CPURegisters[s & 15][sizecode] = res;
        ZF() = res == 0;
		RFLAGS() ^= Rand() & MASK_UNION_4(OF, SF, AF, PF);

        return true;
    }

    // identical to ProcessUNKNOWN() - added for clarity for UD instruction
    bool Computer::ProcessUD()
    {
        // ud explicitly triggers an unknown opcode error
        terminate_err(ErrorCode::UnknownOp);
        return false;
    }

    /*
    [2: sizecode][2:][2:][1: in 0 / out 1][1: dx]
        dx = 0:
        dx = 1: [8: port]
    */
    bool Computer::ProcessIO()
    {
        u8 s;
        u16 port;
        if (!GetMemAdv<u8>(s)) return false;

        if (s & 1) port = (u16)DX();
        else
        {
            u8 t;
            if (!GetMemAdv<u8>(t)) return false;
            port = t;
        }

        if (s & 2)
        {
            u64 v;
            if (!input(port, v, s >> 6)) return false;
            CPURegisters[0][s >> 6] = v;
            return true;
        }
        else return output(port, CPURegisters[0][s >> 6], s >> 6);
    }

    // -----------------------------------------------------------

    bool Computer::ProcessFINIT()
    {
        FINIT();
        return true;
    }

    bool Computer::ProcessFCLEX()
    {
        FPU_status &= 0xff00;
        return true;
    }

    fext Computer::PerformRoundTrip(fext val, u32 rc)
    {
        switch (rc)
        {
        case 0: std::fesetround(FE_TONEAREST); return std::nearbyint(val);
        case 1: std::fesetround(FE_DOWNWARD); return std::nearbyint(val);
        case 2: std::fesetround(FE_UPWARD); return std::nearbyint(val);
        case 3: std::fesetround(FE_TOWARDZERO); return std::nearbyint(val);

        default: throw std::runtime_error("RC out of range");
        }
    }

    /*
    [1:][3: i][1:][3: mode]
    mode = 0: st(0) <- f(st(0), st(i))
    mode = 1: st(i) <- f(st(i), st(0))
    mode = 2: || + pop
    mode = 3: st(0) <- f(st(0), fp32M)
    mode = 4: st(0) <- f(st(0), fp64M)
    mode = 5: st(0) <- f(st(0), int16M)
    mode = 6: st(0) <- f(st(0), int32M)
    else UND
    */
    bool Computer::FetchFPUBinaryFormat(u8 &s, fext &a, fext &b)
    {
        u64 m;
        if (!GetMemAdv<u8>(s)) return false;

        // switch through mode
        switch (s & 7)
        {
        case 0:
            if (ST(0).Empty() || ST(s >> 4).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            a = ST(0); b = ST(s >> 4); return true;
        case 1:
        case 2:
            if (ST(0).Empty() || ST(s >> 4).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            b = ST(0); a = ST(s >> 4); return true;

        default:
            if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            a = ST(0); b = 0;
            if (!GetAddressAdv(m)) return false;
            switch (s & 7)
            {
            case 3: if (u32 t; read_mem<u32>(m, t)) b = (fext)transmute<f32>(t); else return false; return true;
            case 4: if (u64 t; read_mem<u64>(m, t)) b = (fext)transmute<f64>(t); else return false; return true;
            case 5: if (u16 t; read_mem<u16>(m, t)) b = (fext)(i64)(i16)t; else return false; return true;
            case 6: if (u32 t; read_mem<u32>(m, t)) b = (fext)(i64)(i32)t; else return false; return true;

            default: terminate_err(ErrorCode::UndefinedBehavior); return false;
            }
        }
    }
    bool Computer::StoreFPUBinaryFormat(u8 s, fext res)
    {
        switch (s & 7)
        {
        case 1: ST(s >> 4) = res; return true;
        case 2: ST(s >> 4) = res; return PopFPU();

        default: ST(0) = res; return true;
        }
    }

    bool Computer::PushFPU(fext val)
	{
        // decrement top (wraps automatically as a 3-bit unsigned value)
        --FPU_TOP();

        // if this fpu reg is in use, it's an error
        if (!ST(0).Empty()) { terminate_err(ErrorCode::FPUStackOverflow); return false; }

        // store the value
        ST(0) = val;

        return true;
    }
    bool Computer::PopFPU(fext &val)
    {
        // if this register is not in use, it's an error
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUStackUnderflow); return false; }

        // extract the value
        val = ST(0);
        ST(0).Free();

        // increment top (wraps automatically as a 3-bit unsigned value)
        ++FPU_TOP();

        return true;
    }
    bool Computer::PopFPU()
    {
        fext _;
        return PopFPU(_);
    }

    /*
    [8: mode]   [address]
    mode = 0: FSTSW AX
    mode = 1: FSTSW
    mode = 2: FSTCW
    mode = 3: FLDCW
    mode = 4: STMXCSR
    mode = 5: LDMXCSR
    mode = 6: FSAVE
    mode = 7: FRSTOR
    mode = 8: FSTENV
    mode = 9: FLDENV
    else UND
    */
    bool Computer::ProcessFSTLD_WORD()
    {
        u8 s;
        u64 m;
        if (!GetMemAdv<u8>(s)) return false;

        // handle FSTSW AX case specially (doesn't have an address)
        if (s == 0)
        {
            AX() = FPU_status;
            return true;
        }
        else if (!GetAddressAdv(m)) return false;

        // switch through mode
        switch (s)
        {
        case 1: return write_mem<u16>(m, FPU_status);
        case 2: return write_mem<u16>(m, FPU_control);
        case 3: return read_mem<u16>(m, FPU_control);

        case 4: return write_mem<u32>(m, _MXCSR);
        case 5:
        {
            u32 t;
            if (!read_mem<u32>(m, t)) return false;
            _MXCSR = (_MXCSR & 0xffff0000) | (u16)t; // make sure user can't modify the upper 16 reserved bits
            return true;
        }

        case 6:
            if (!write_mem<u16>(m + 0, FPU_control)) return false;
            if (!write_mem<u16>(m + 4, FPU_status)) return false;
            if (!write_mem<u16>(m + 8, FPU_tag)) return false;

            if (!write_mem<u32>(m + 12, (u32)EIP())) return false;
            if (!write_mem<u16>(m + 16, (u16)0)) return false;

            if (!write_mem<u32>(m + 20, (u32)m)) return false;
            if (!write_mem<u16>(m + 24, (u16)0)) return false;

            // for cross-platformability/speed, writes native f64 instead of some tword hacks
            if (!write_mem<u64>(m + 28, transmute<u64>((f64)ST(0)))) return false;
            if (!write_mem<u64>(m + 38, transmute<u64>((f64)ST(1)))) return false;
            if (!write_mem<u64>(m + 48, transmute<u64>((f64)ST(2)))) return false;
            if (!write_mem<u64>(m + 58, transmute<u64>((f64)ST(3)))) return false;
            if (!write_mem<u64>(m + 68, transmute<u64>((f64)ST(4)))) return false;
            if (!write_mem<u64>(m + 78, transmute<u64>((f64)ST(5)))) return false;
            if (!write_mem<u64>(m + 88, transmute<u64>((f64)ST(6)))) return false;
            if (!write_mem<u64>(m + 98, transmute<u64>((f64)ST(7)))) return false;

            // after storing fpu state, re-initializes
            FINIT();
            return true;
        case 7:
            if (!read_mem<u16>(m + 0, FPU_control)) return false;
            if (!read_mem<u16>(m + 4, FPU_status)) return false;
            if (!read_mem<u16>(m + 8, FPU_tag)) return false;

            // for cross-platformability/speed, reads native f64 instead of some tword hacks
            {
                u64 t;
                if (!read_mem<u64>(m + 28, t)) { return false; } ST(0) = transmute<f64>(t);
                if (!read_mem<u64>(m + 38, t)) { return false; } ST(1) = transmute<f64>(t);
                if (!read_mem<u64>(m + 48, t)) { return false; } ST(2) = transmute<f64>(t);
                if (!read_mem<u64>(m + 58, t)) { return false; } ST(3) = transmute<f64>(t);
                if (!read_mem<u64>(m + 68, t)) { return false; } ST(4) = transmute<f64>(t);
                if (!read_mem<u64>(m + 78, t)) { return false; } ST(5) = transmute<f64>(t);
                if (!read_mem<u64>(m + 88, t)) { return false; } ST(6) = transmute<f64>(t);
                if (!read_mem<u64>(m + 98, t)) { return false; } ST(7) = transmute<f64>(t);
            }

            return true;

        case 8:
            if (!write_mem<u16>(m + 0, FPU_control)) return false;
            if (!write_mem<u16>(m + 4, FPU_status)) return false;
            if (!write_mem<u16>(m + 8, FPU_tag)) return false;

            if (!write_mem<u32>(m + 12, (u32)EIP())) return false;
            if (!write_mem<u16>(m + 16, (u16)0)) return false;

            if (!write_mem<u32>(m + 20, (u32)m)) return false;
            if (!write_mem<u16>(m + 24, (u16)0)) return false;

            return true;
        case 9:
            if (!read_mem<u16>(m + 0, FPU_control)) return false;
            if (!read_mem<u16>(m + 4, FPU_status)) return false;
            if (!read_mem<u16>(m + 8, FPU_tag)) return false;

            return true;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
    bool Computer::ProcessFLD_const()
    {
        u8 ext;
        if (!GetMemAdv<u8>(ext)) return false;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        switch (ext)
        {
        case 0: return PushFPU(1L); // 1
        case 1: return PushFPU(3.321928094887362347870319429489390175864831393024580612054L); // lb 10
        case 2: return PushFPU(1.442695040888963407359924681001892137426645954152985934135L); // lb e
        case 3: return PushFPU(3.141592653589793238462643383279502884197169399375105820974L); // pi
        case 4: return PushFPU(0.301029995663981195213738894724493026768189881462108541310L); // log 2
        case 5: return PushFPU(0.693147180559945309417232121458176568075500134360255254120L); // ln 2
        case 6: return PushFPU(0L); // 0

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
    /*
    [1:][3: i][1:][3: mode]
    mode = 0: push st(i)
    mode = 1: push m32fp
    mode = 2: push m64fp
    mode = 3: push m16int
    mode = 4: push m32int
    mode = 5: push m64int
    else UND
    */
    bool Computer::ProcessFLD()
    {
        u8 s;
        u64 m;
        if (!GetMemAdv<u8>(s)) return false;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        // switch through mode
        switch (s & 7)
        {
        case 0:
            if (ST(s >> 4).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            return PushFPU(ST(s >> 4));

        default:
            if (!GetAddressAdv(m)) return false;
            switch (s & 7)
            {
            case 1: { u32 t; return read_mem<u32>(m, t) && PushFPU((fext)transmute<f32>(t)); }
            case 2: { u64 t; return read_mem<u64>(m, t) && PushFPU((fext)transmute<f64>(t)); }

            case 3: { u16 t; return read_mem<u16>(m, t) && PushFPU((fext)(i64)(i16)t); }
            case 4: { u32 t; return read_mem<u32>(m, t) && PushFPU((fext)(i64)(i32)t); }
            case 5: { u64 t; return read_mem<u64>(m, t) && PushFPU((fext)(i64)t); }

            default: terminate_err(ErrorCode::UndefinedBehavior); return false;
            }
        }
    }

    /*
    [1:][3: i][4: mode]
    mode = 0: st(i) <- st(0)
    mode = 1: || + pop
    mode = 2: fp32M <- st(0)
    mode = 3: || + pop
    mode = 4: fp64M <- st(0)
    mode = 5: || + pop
    mode = 6: int16M <- st(0)
    mode = 7: || + pop
    mode = 8: int32M <- st(0)
    mode = 9: || + pop
    mode = 10: int64M <- st(0) + pop
    mode = 11: int16M <- st(0) + pop (truncation)
    mode = 12: int32M <- st(0) + pop (truncation)
    mode = 13: int64M <- st(0) + pop (truncation)
    else UND
    */
    bool Computer::ProcessFST()
    {
        u8 s;
        u64 m;
        if (!GetMemAdv<u8>(s)) return false;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        switch (s & 15)
        {
        case 0:
        case 1:
            // make sure we can read the value
            if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            // record the value (is allowed to be not in use)
            ST(s >> 4) = ST(0);
            break;

        default:
            if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            if (!GetAddressAdv(m)) return false;
            switch (s & 15)
            {
            case 2: case 3: if (!write_mem<u32>(m, transmute<u32>((f32)ST(0)))) return false; break;
            case 4: case 5: if (!write_mem<u64>(m, transmute<u64>((f64)ST(0)))) return false; break;
            case 6: case 7: if (!write_mem<u16>(m, (u16)(u64)(i64)PerformRoundTrip(ST(0), FPU_RC()))) return false; break;
            case 8: case 9: if (!write_mem<u32>(m, (u32)(u64)(i64)PerformRoundTrip(ST(0), FPU_RC()))) return false; break;
            case 10: if (!write_mem<u64>(m, (u64)(i64)PerformRoundTrip(ST(0), FPU_RC()))) return false; break;
            case 11: if (!write_mem<u16>(m, (u16)(u64)(i64)ST(0))) return false; break;
            case 12: if (!write_mem<u32>(m, (u32)(u64)(i64)ST(0))) return false; break;
            case 13: if (!write_mem<u64>(m, (u64)(i64)ST(0))) return false; break;

            default: terminate_err(ErrorCode::UndefinedBehavior); return false;
            }
            break;
        }

        switch (s & 15)
        {
        case 0: case 2: case 4: case 6: case 8: return true;
        default: return PopFPU();
        }
    }
    bool Computer::ProcessFXCH()
    {
        u8 i;
        if (!GetMemAdv<u8>(i)) return false;
        i &= 7;

        // make sure they're both in use
        if (ST(0).Empty() || ST(i).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext temp = ST(0);
        ST(0) = ST(i);
        ST(i) = temp;

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C2, FPU_C3);
        FPU_C1() = false;

        return true;
    }
    /*
    [1:][3: i][1:][3: cc]
    cc = 0: E  (=Z)
    cc = 1: NE (=NZ)
    cc = 2: B
    cc = 3: BE
    cc = 4: A
    cc = 5: AE
    cc = 6: U  (=P)
    cc = 7: NU (=NP)
    */
    bool Computer::ProcessFMOVcc()
    {
        u8 s;
        if (!GetMemAdv<u8>(s)) return false;

        // get the flag
        bool flag;
        switch (s & 7)
        {
        case 0: flag = ZF(); break;
        case 1: flag = !ZF(); break;
        case 2: flag = cc_b(); break;
        case 3: flag = cc_be(); break;
        case 4: flag = cc_a(); break;
        case 5: flag = cc_ae(); break;
        case 6: flag = PF(); break;
        case 7: flag = !PF(); break;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        // if flag is set, do the move
        if (flag)
        {
            int i = (s >> 4) & 7;
            if (ST(0).Empty() || ST(i).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            ST(0) = ST(s >> 4);
        }

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }

    bool Computer::ProcessFADD()
    {
        u8 s;
        fext a, b;
        if (!FetchFPUBinaryFormat(s, a, b)) return false;

        fext res = a + b;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return StoreFPUBinaryFormat(s, res);
    }
    bool Computer::ProcessFSUB()
    {
        u8 s;
        fext a, b;
        if (!FetchFPUBinaryFormat(s, a, b)) return false;

        fext res = a - b;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return StoreFPUBinaryFormat(s, res);
    }
    bool Computer::ProcessFSUBR()
    {
        u8 s;
        fext a, b;
        if (!FetchFPUBinaryFormat(s, a, b)) return false;

        fext res = b - a;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return StoreFPUBinaryFormat(s, res);
    }

    bool Computer::ProcessFMUL()
    {
        u8 s;
        fext a, b;
        if (!FetchFPUBinaryFormat(s, a, b)) return false;

        fext res = a * b;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return StoreFPUBinaryFormat(s, res);
    }
    bool Computer::ProcessFDIV()
    {
        u8 s;
        fext a, b;
        if (!FetchFPUBinaryFormat(s, a, b)) return false;

        fext res = a / b;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return StoreFPUBinaryFormat(s, res);
    }
    bool Computer::ProcessFDIVR()
    {
        u8 s;
        fext a, b;
        if (!FetchFPUBinaryFormat(s, a, b)) return false;

        fext res = b / a;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return StoreFPUBinaryFormat(s, res);
    }

    bool Computer::ProcessF2XM1()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        // get the value
        f64 val = ST(0);
        // val must be in range [-1, 1]
        if (val < -1 || val > 1) { terminate_err(ErrorCode::FPUError); return false; }

        ST(0) = std::pow(2, val) - 1;

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }
    bool Computer::ProcessFABS()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        ST(0) = std::fabs(ST(0));

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C2, FPU_C3);
        FPU_C1() = false;

        return true;
    }
    bool Computer::ProcessFCHS()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        ST(0) = -ST(0);

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C2, FPU_C3);
        FPU_C1() = false;

        return true;
    }
    bool Computer::ProcessFPREM()
    {
        if (ST(0).Empty() || ST(1).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext a = ST(0);
        fext b = ST(1);

        // compute remainder with truncation
		fext res = std::fmod(a, b);

        // store value
        ST(0) = res;

        // get the bits
        u64 bits = transmute<u64>((f64)res);

        FPU_C0() = (bits & 4) != 0;
        FPU_C1() = (bits & 1) != 0;
        FPU_C2() = false;
        FPU_C3() = (bits & 2) != 0;

        return true;
    }
    bool Computer::ProcessFPREM1()
    {
        if (ST(0).Empty() || ST(1).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext a = ST(0);
        fext b = ST(1);

        // compute remainder with rounding
		fext res = std::remainder(a, b);

        // store value
        ST(0) = res;

        // get the bits - should be from 80-bit fp, but we have no control over that :(
        u64 bits = transmute<u64>((f64)res);

        FPU_C0() = (bits & 4) != 0;
        FPU_C1() = (bits & 1) != 0;
        FPU_C2() = false;
        FPU_C3() = (bits & 2) != 0;

        return true;
    }
    bool Computer::ProcessFRNDINT()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext val = ST(0);
        fext res = PerformRoundTrip(val, FPU_RC());

        ST(0) = res;

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C2, FPU_C3);
        FPU_C1() = res > val;

        return true;
    }
    bool Computer::ProcessFSQRT()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        ST(0) = std::sqrt(ST(0));

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }
    bool Computer::ProcessFYL2X()
    {
        if (ST(0).Empty() || ST(1).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext a = ST(0);
        fext b = ST(1);

        (void)PopFPU(); // pop stack and place in the new st(0) - discarding result because check above ensures this will succeed
        ST(0) = b * std::log2(a);

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }
    bool Computer::ProcessFYL2XP1()
    {
        if (ST(0).Empty() || ST(1).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext a = ST(0);
        fext b = ST(1);

        (void)PopFPU(); // pop stack and place in the new st(0) - discarding result because check above ensures this will succeed
        ST(0) = b * std::log2(a + 1);

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }
    bool Computer::ProcessFXTRACT()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        // get value and extract exponent/significand
        f64 exp, sig;
        ExtractDouble(ST(0), exp, sig);

        // exponent in st0, then push the significand
        ST(0) = exp;
        return PushFPU(sig);
    }
    bool Computer::ProcessFSCALE()
    {
        if (ST(0).Empty() || ST(1).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        f64 a = ST(0);
        f64 b = ST(1);

        // get exponent and significand of st0
        f64 exp, sig;
        ExtractDouble(a, exp, sig);

        // add (truncated) st1 to exponent of st0
        ST(0) = AssembleDouble(exp + (i64)b, sig);

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }

    bool Computer::ProcessFXAM()
    {
        fext val = ST(0);
        u64 bits = transmute<u64>((f64)val);

        // C1 gets sign bit
        FPU_C1() = (bits & 0x8000000000000000) != 0;

        // empty
        if (ST(0).Empty()) { FPU_C3() = true; FPU_C2() = false; FPU_C0() = true; }
        // NaN
        else if (std::isnan(val)) { FPU_C3() = false; FPU_C2() = false; FPU_C0() = true; }
        // inf
        else if (std::isinf(val)) { FPU_C3() = false; FPU_C2() = true; FPU_C0() = true; }
        // zero
        else if (val == 0) { FPU_C3() = true; FPU_C2() = false; FPU_C0() = false; }
        // denormalized
        else if (is_denormal<fext>(val)) { FPU_C3() = true; FPU_C2() = true; FPU_C0() = false; }
        // normal
        else { FPU_C3() = false; FPU_C2() = true; FPU_C0() = false; }

        return true;
    }
    bool Computer::ProcessFTST()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext a = ST(0);

        // for FTST, nan is an arithmetic error
        if (std::isnan(a)) { terminate_err(ErrorCode::ArithmeticError); return false; }

        // do the comparison
        if (a > 0) { FPU_C3() = false; FPU_C2() = false; FPU_C0() = false; }
        else if (a < 0) { FPU_C3() = false; FPU_C2() = false; FPU_C0() = true; }
        else { FPU_C3() = true; FPU_C2() = false; FPU_C0() = false; }

        // C1 is cleared
        FPU_C1() = false;

        return true;
    }

    /*
    [1: unordered][3: i][4: mode]
    mode = 0: cmp st(0), st(i)
    mode = 1: || + pop
    mode = 2: || + 2x pop
    mode = 3: cmp st(0), fp32
    mode = 4: || + pop
    mode = 5: cmp st(0), fp64
    mode = 6: || + pop
    mode = 7: cmp st(0), int16
    mode = 8: || + pop
    mode = 9: cmp st(0), int32
    mode = 10: || + pop
    mode = 11 cmp st(0), st(i) eflags
    mode = 12 || + pop
    else UND
    */
    bool Computer::ProcessFCOM()
    {
        u8 s;
        u64 m;
        if (!GetMemAdv<u8>(s)) return false;

        fext a, b;

        // swith through mode
        switch (s & 15)
        {
        case 0:
        case 1:
        case 2:
        case 11:
        case 12:
            if (ST(0).Empty() || ST(s >> 4).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            a = ST(0); b = ST(s >> 4);
            break;

        default:
            if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }
            a = ST(0);
            if (!GetAddressAdv(m)) return false;
            switch (s & 15)
            {
            case 3: case 4: if (u32 t; read_mem<u32>(m, t)) b = (fext)transmute<f32>(t); else return false; break;
            case 5: case 6: if (u64 t; read_mem<u64>(m, t)) b = (fext)transmute<f64>(t); else return false; break;

            case 7: case 8: if (u16 t; read_mem<u16>(m, t)) b = (fext)(i64)(i16)t; else return false; break;
            case 9: case 10: if (u32 t; read_mem<u32>(m, t)) b = (fext)(i64)(i32)t; else return false; break;

            default: terminate_err(ErrorCode::UndefinedBehavior); return false;
            }
            break;
        }

        bool x, y, z; // temporaries for cmp flag data

        // do the comparison
        if (a > b) { x = false; y = false; z = false; }
        else if (a < b) { x = false; y = false; z = true; }
        else if (a == b) { x = true; y = false; z = false; }
        // otherwise is unordered
        else
        {
            if ((s & 128) == 0) { terminate_err(ErrorCode::ArithmeticError); return false; }
            x = y = z = true;
        }
		
        // eflags
        if ((s & 15) == 11 || (s & 15) == 12)
        {
            ZF() = x;
            PF() = y;
            CF() = z;
        }
        // fflags
        else
        {
            FPU_C3() = x;
            FPU_C2() = y;
            FPU_C0() = z;
        }
        FPU_C1() = false; // C1 is cleared in either case

        // handle popping cases
        switch (s & 15)
        {
        case 2: return PopFPU() && PopFPU();
        case 1: case 4: case 6: case 8: case 10: case 12: return PopFPU();
        default: return true;
        }
    }

    bool Computer::ProcessFSIN()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        ST(0) = std::sin(ST(0));

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C1, FPU_C3);
        FPU_C2() = false;

        return true;
    }
    bool Computer::ProcessFCOS()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        ST(0) = std::cos(ST(0));

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C1, FPU_C3);
        FPU_C2() = false;

        return true;
    }
    bool Computer::ProcessFSINCOS()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C1, FPU_C3);
        FPU_C2() = false;

        // get the value
        fext val = ST(0);

        // st(0) <- sin, push cos
        ST(0) = std::sin(val);
        return PushFPU(std::cos(val));
    }
    bool Computer::ProcessFPTAN()
    {
        if (ST(0).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        ST(0) = std::tan(ST(0));

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C1, FPU_C3);
        FPU_C2() = false;

        // also push 1 onto fpu stack
        return PushFPU(1);
    }
    bool Computer::ProcessFPATAN()
    {
        if (ST(0).Empty() || ST(1).Empty()) { terminate_err(ErrorCode::FPUAccessViolation); return false; }

        fext a = ST(0);
        fext b = ST(1);

        (void)PopFPU(); // pop stack and place in new st(0) - discarding result because check above ensures this will succeed
        ST(0) = std::atan2(b, a);

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C1, FPU_C3);
        FPU_C2() = false;

        return true;
    }

    /*
    [7:][1: mode]
    mode = 0: inc
    mode = 1: dec
    */
    bool Computer::ProcessFINCDECSTP()
    {
        u8 ext;
        if (!GetMemAdv<u8>(ext)) return false;

        // does not modify tag word
        switch (ext & 1)
        {
        case 0: ++FPU_TOP(); break;
        case 1: --FPU_TOP(); break;

        default: return true; // can't happen but compiler is stupid
        }

        FPU_status ^= Rand() & MASK_UNION_3(FPU_C0, FPU_C2, FPU_C3);
        FPU_C1() = false;

        return true;
    }
    bool Computer::ProcessFFREE()
    {
        u8 i;
        if (!GetMemAdv<u8>(i)) return false;
        i &= 7;

        // mark as not in use
        ST(i).Free();

        FPU_status ^= Rand() & MASK_UNION_4(FPU_C0, FPU_C1, FPU_C2, FPU_C3);

        return true;
    }

    // ---------------------------------------------------------------------------

    /*
    [5: reg][1: aligned][2: reg_size]   [1: has_mask][1: zmask][1: scalar][1:][2: elem_size][2: mode]   ([count: mask])
    mode = 0: [3:][5: src]   reg <- src
    mode = 1: [address]      reg <- M[address]
    mode = 2: [address]      M[address] <- src
    else UND
    */
    bool Computer::ProcessVPUMove()
    {
        // read settings bytes
        u8 s1, s2;
        u64 m, temp;
        if (!GetMemAdv<u8>(s1) || !GetMemAdv<u8>(s2)) return false;
        u64 reg_sizecode = s1 & 3;
        u64 elem_sizecode = (s2 >> 2) & 3;

        // -- get the register to work with -- //

        // this check isn't optional - if it's 3 it can go out of bounds of the ZMMRegister object
        if (reg_sizecode == 3) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

        if constexpr (StrictUND)
        {
            // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
            if (reg_sizecode != 2 && (s1 & 0x80) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        ZMMRegister &reg = ZMMRegisters[s1 >> 3];

        // -- prepare for execution -- //

        // get number of elements to process (accounting for scalar flag)
        const u64 elem_count = s2 & 0x20 ? 1 : get_size(reg_sizecode + 4) >> elem_sizecode;

        // get the mask (default of all 1's)
        u64 mask = ~(u64)0;
        if ((s2 & 0x80) != 0 && !GetMemAdv(bits_to_bytes(elem_count), mask)) return false;
        // get the zmask flag
        bool zmask = (s2 & 0x40) != 0;

        // switch through mode
        switch (s2 & 3)
        {
        case 0:
        {
            u8 _src;
            if (!GetMemAdv<u8>(_src)) return false;

            if constexpr (StrictUND)
            {
                // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
                if (reg_sizecode != 2 && (_src & 0x10) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            const ZMMRegister &src = ZMMRegisters[_src & 0x1f];

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1)
            {
                if (mask & 1) reg.uint((int)elem_sizecode, i) = src.uint((int)elem_sizecode, i);
                else if (zmask) reg.uint(elem_sizecode, i) = 0;
            }

            break;
        } 
        case 1:
            if (!GetAddressAdv(m)) return false;
            // if we're in vector mode and aligned flag is set, make sure address is aligned
            if (elem_count > 1 && (s1 & 4) != 0 && m % get_size(reg_sizecode + 4) != 0) { terminate_err(ErrorCode::AlignmentViolation); return false; }

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1, m += get_size(elem_sizecode))
            {
                if (mask & 1)
                {
                    if (!GetMemRaw(m, get_size(elem_sizecode), temp)) return false;
                    reg.uint((int)elem_sizecode, i) = temp;
                }
                else if (zmask) reg.uint(elem_sizecode, i) = 0;
            }

            break;
        case 2:
            if (!GetAddressAdv(m)) return false;
            // if we're in vector mode and aligned flag is set, make sure address is aligned
            if (elem_count > 1 && (s1 & 4) != 0 && m % get_size(reg_sizecode + 4) != 0) { terminate_err(ErrorCode::AlignmentViolation); return false; }

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1, m += get_size(elem_sizecode))
            {
                if (mask & 1)
                {
                    if (!SetMemRaw(m, get_size(elem_sizecode), reg.uint(elem_sizecode, i))) return false;
                }
                else if (zmask && !SetMemRaw(m, get_size(elem_sizecode), 0)) return false;
            }

            break;

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        return true;
    }
    /*
    [5: dest][1: aligned][2: dest_size]   [1: has_mask][1: zmask][1: scalar][1:][2: elem_size][1:][1: mem]   ([count: mask])   [3:][5: src1]
    mem = 0: [3:][5: src2]   dest <- f(src1, src2)
    mem = 1: [address]       dest <- f(src1, M[address])
    */
    bool Computer::ProcessVPUBinary(u64 elem_size_mask, VPUBinaryDelegate func)
    {
        // read settings bytes
        u8 s1, s2, _src1, _src2;
        u64 res, m;
        if (!GetMemAdv<u8>(s1) || !GetMemAdv<u8>(s2)) return false;
        u64 dest_sizecode = s1 & 3;
        u64 elem_sizecode = (s2 >> 2) & 3;

        // make sure this element size is allowed
        if ((get_size(elem_sizecode) & elem_size_mask) == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

        // -- get the register to work with -- //

        // this check isn't optional - if it's 3 it can go out of bounds of the ZMMRegister object
        if (dest_sizecode == 3) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

        if constexpr (StrictUND)
        {
            // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
            if (dest_sizecode != 2 && s1 & 0x80) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        ZMMRegister &dest = ZMMRegisters[s1 >> 3];

        // -- prepare for execution -- //

        // get number of elements to process (accounting for scalar flag)
        u64 elem_count = s2 & 0x20 ? 1 : get_size(dest_sizecode + 4) >> elem_sizecode;

        // get the mask (default of all 1's)
        u64 mask = ~(u64)0;
        if ((s2 & 0x80) != 0 && !GetMemAdv(bits_to_bytes((u64)elem_count), mask)) return false;
        // get the zmask flag
        bool zmask = (s2 & 0x40) != 0;

        // get src1
        if (!GetMemAdv<u8>(_src1)) return false;

        if constexpr (StrictUND)
        {
            // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
            if (dest_sizecode != 2 && (_src1 & 0x10) != 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        const ZMMRegister &src1 = ZMMRegisters[_src1 & 0x1f];

        // if src2 is a register
        if ((s2 & 1) == 0)
        {
            if (!GetMemAdv<u8>(_src2)) return false;

            if constexpr (StrictUND)
            {
                // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
                if (dest_sizecode != 2 && _src2 & 0x10) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            const ZMMRegister &src2 = ZMMRegisters[_src2 & 0x1f];

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1)
            {
                if (mask & 1)
                {
                    // hand over to the delegate for processing
                    if (!(this->*func)(elem_sizecode, res, src1.uint(elem_sizecode, i), src2.uint(elem_sizecode, i), i)) return false;
                    dest.uint(elem_sizecode, i) = res;
                }
                else if (zmask) dest.uint(elem_sizecode, i) = 0;
            }
        }
        // otherwise src is memory
        else
        {
            if (!GetAddressAdv(m)) return false;
            // if we're in vector mode and aligned flag is set, make sure address is aligned
            if (elem_count > 1 && (s1 & 4) != 0 && m % get_size(dest_sizecode + 4) != 0) { terminate_err(ErrorCode::AlignmentViolation); return false; }

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1, m += get_size(elem_sizecode))
                if (mask & 1)
                {
                    if (!GetMemRaw(m, get_size(elem_sizecode), res)) return false;

                    // hand over to the delegate for processing
                    if (!(this->*func)(elem_sizecode, res, src1.uint(elem_sizecode, i), res, i)) return false;
                    dest.uint(elem_sizecode, i) = res;
                }
                else if (zmask) dest.uint(elem_sizecode, i) = 0;
        }

        return true;
    }
    /*
    [5: dest][1: aligned][2: dest_size]   [1: has_mask][1: zmask][1: scalar][1:][2: elem_size][1:][1: mem]   ([count: mask])
    mem = 0: [3:][5: src]   dest <- f(src)
    mem = 1: [address]      dest <- f(M[address])
    */
    bool Computer::ProcessVPUUnary(u64 elem_size_mask, VPUUnaryDelegate func)
    {
        // read settings bytes
        u8 s1, s2;
        u64 res, m;
        if (!GetMemAdv<u8>(s1) || !GetMemAdv<u8>(s2)) return false;
        u64 dest_sizecode = s1 & 3;
        u64 elem_sizecode = (s2 >> 2) & 3;

        // make sure this element size is allowed
        if ((get_size(elem_sizecode) & elem_size_mask) == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

        // -- get the register to work with -- //

        // this check isn't optional - if it's 3 it can go out of bounds of the ZMMRegister object
        if (dest_sizecode == 3) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

        if constexpr (StrictUND)
        {
            // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
            if (dest_sizecode != 2 && s1 & 0x80) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        ZMMRegister &dest = ZMMRegisters[s1 >> 3];

        // -- prepare for execution -- //

        // get number of elements to process (accounting for scalar flag)
        u64 elem_count = s2 & 0x20 ? 1 : get_size(dest_sizecode + 4) >> elem_sizecode;
        
        // get the mask (default of all 1's)
        u64 mask = ~(u64)0;
        if (s2 & 0x80 && !GetMemAdv(bits_to_bytes((u64)elem_count), mask)) return false;
        // get the zmask flag
        bool zmask = s2 & 0x40;

        // if src is a register
        if ((s2 & 1) == 0)
        {
            u8 _src;
            if (!GetMemAdv<u8>(_src)) return false;

            if constexpr (StrictUND)
            {
                // this check is optional - just prevents XMM / YMM ops from accessing vpu registers 16-31 (standard intel stuff)
                if (dest_sizecode != 2 && _src & 0x10) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
            }

            ZMMRegister &src = ZMMRegisters[_src & 0x1f];

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1)
                if (mask & 1)
                {
                    // hand over to the delegate for processing
                    if (!(this->*func)(elem_sizecode, res, src.uint(elem_sizecode, i), i)) return false;
                    dest.uint(elem_sizecode, i) = res;
                }
                else if (zmask) dest.uint(elem_sizecode, i) = 0;
        }
        // otherwise src is memory
        else
        {
            if (!GetAddressAdv(m)) return false;
            // if we're in vector mode and aligned flag is set, make sure address is aligned
            if (elem_count > 1 && (s1 & 4) != 0 && m % get_size(dest_sizecode + 4) != 0) { terminate_err(ErrorCode::AlignmentViolation); return false; }

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1, m += get_size(elem_sizecode))
                if (mask & 1)
                {
                    if (!GetMemRaw(m, get_size(elem_sizecode), res)) return false;
                    
                    // hand over to the delegate for processing
                    if (!(this->*func)(elem_sizecode, res, res, i)) return false;
                    dest.uint(elem_sizecode, i) = res;
                }
                else if (zmask) dest.uint(elem_sizecode, i) = 0;
        }

        return true;
    }

    /*
    [5: dest][1: mem][1: has_mask][1: zmask]   ([count: mask])
    mem = 0: [3:][5: src]   dest <- f(src)
    mem = 1: [address]      dest <- f(M[address])
    */
    bool Computer::ProcessVPUCVT_packed(u64 elem_count, u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func)
    {
        // read settings byte
        u8 s;
        if (!GetMemAdv<u8>(s)) return false;
        u64 dest = s >> 3;

        // get the mask (default of all 1's)
        u64 mask = 0xffffffffffffffff;
        if (s & 2 && !GetMemAdv(bits_to_bytes(elem_count), mask)) return false;
        // get the zmask flag
        bool zmask = s & 1;

        // because we may be changing sizes, and writing to the source register, we need to do our work in a temporary buffer
        ZMMRegister temp_dest;
        // result should be zero-filled outside of the conversion range, so just clear the whole temporary
        temp_dest.clear();

        // if src is a register
        if ((s & 4) == 0)
        {
            u8 _src;
            u64 res;
            if (!GetMemAdv<u8>(_src)) return false;

            ZMMRegister &src = ZMMRegisters[_src & 0x1f];

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1)
            {
                if (mask & 1)
                {
                    // hand over to the delegate for processing
                    if (!(this->*func)(res, src.uint(from_elem_sizecode, i))) return false;
                    temp_dest.uint(to_elem_sizecode, i) = res;
                }
                else if (zmask) temp_dest.uint(to_elem_sizecode, i) = 0;
            }
        }
        // otherwise src is memory
        else
        {
            u64 m, res;
            if (!GetAddressAdv(m)) return false;
            // make sure source address is aligned
            if (m % (elem_count << from_elem_sizecode) != 0) { terminate_err(ErrorCode::AlignmentViolation); return false; }

            for (u64 i = 0; i < elem_count; ++i, mask >>= 1, m += get_size(from_elem_sizecode))
            {
                if (mask & 1)
                {
                    if (!GetMemRaw(m, get_size(from_elem_sizecode), res)) return false;

                    // hand over to the delegate for processing
                    if (!(this->*func)(res, res)) return false;
                    temp_dest.uint(to_elem_sizecode, i) = res;
                }
                else if (zmask) temp_dest.uint(to_elem_sizecode, i) = 0;
            }
        }

        // store resulting temporary back to the correct register
        ZMMRegisters[dest] = temp_dest;

        return true;
    }

    /*
    [4: dest][4: src]
    */
    bool Computer::ProcessVPUCVT_scalar_xmm_xmm(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func)
    {
        // read the settings byte
        u8 s;
        u64 temp;
        if (!GetMemAdv<u8>(s)) return false;

        // perform the conversion
        if (!(this->*func)(temp, ZMMRegisters[s & 15].uint(from_elem_sizecode, 0))) return false;

        // store the result
        ZMMRegisters[s >> 4].uint(to_elem_sizecode, 0) = temp;

        return true;
    }
    /*
    [4: dest][4: src]
    */
    bool Computer::ProcessVPUCVT_scalar_xmm_reg(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func)
    {
        // read the settings byte
        u8 s;
        u64 temp;
        if (!GetMemAdv<u8>(s)) return false;

        // perform the conversion
        if (!(this->*func)(temp, CPURegisters[s & 15][from_elem_sizecode])) return false;

        // store the result
        ZMMRegisters[s >> 4].uint(to_elem_sizecode, 0) = temp;

        return true;
    }
    /*
    [4: dest][4:]   [address]
    */
    bool Computer::ProcessVPUCVT_scalar_xmm_mem(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func)
    {
        // read the settings byte
        u8 s;
        u64 temp;
        if (!GetMemAdv<u8>(s)) return false;

        // get value to convert in temp
        if (!GetAddressAdv(temp) || !GetMemRaw(temp, get_size(from_elem_sizecode), temp)) return false;

        // perform the conversion
        if (!(this->*func)(temp, temp)) return false;

        // store the result
        ZMMRegisters[s >> 4].uint(to_elem_sizecode, 0) = temp;

        return true;
    }
    /*
    [4: dest][4: src]
    */
    bool Computer::ProcessVPUCVT_scalar_reg_xmm(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func)
    {
        // read the settings byte
        u8 s;
        u64 temp;
        if (!GetMemAdv<u8>(s)) return false;

        // perform the conversion
        if (!(this->*func)(temp, ZMMRegisters[s & 15].uint(from_elem_sizecode, 0))) return false;

        // store the result
        CPURegisters[s >> 4][to_elem_sizecode] = temp;

        return true;
    }
    /*
    [4: dest][4:]   [address]
    */
    bool Computer::ProcessVPUCVT_scalar_reg_mem(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func)
    {
        // read the settings byte
        u8 s;
        u64 temp;
        if (!GetMemAdv<u8>(s)) return false;

        // get value to convert in temp
        if (!GetAddressAdv(temp) || !GetMemRaw(temp, get_size(from_elem_sizecode), temp)) return false;

        // perform the conversion
        if (!(this->*func)(temp, temp)) return false;

        // store the result
        CPURegisters[s >> 4][to_elem_sizecode] = temp;

        return true;
    }

    bool Computer::_TryPerformVEC_FADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // 64-bit fp
        if (elem_sizecode == 3) res = transmute<u64>(transmute<f64>(a) + transmute<f64>(b));
        // 32-bit fp
        else res = transmute<u32>(transmute<f32>((u32)a) + transmute<f32>((u32)b));

        return true;
    }
    bool Computer::_TryPerformVEC_FSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // 64-bit fp
        if (elem_sizecode == 3) res = transmute<u64>(transmute<f64>(a) - transmute<f64>(b));
        // 32-bit fp
        else res = transmute<u32>(transmute<f32>((u32)a) - transmute<f32>((u32)b));

        return true;
    }
    bool Computer::_TryPerformVEC_FMUL(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // 64-bit fp
        if (elem_sizecode == 3) res = transmute<u64>(transmute<f64>(a) * transmute<f64>(b));
        // 32-bit fp
        else res = transmute<u32>(transmute<f32>((u32)a) * transmute<f32>((u32)b));

        return true;
    }
    bool Computer::_TryPerformVEC_FDIV(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // 64-bit fp
        if (elem_sizecode == 3) res = transmute<u64>(transmute<f64>(a) / transmute<f64>(b));
        // 32-bit fp
        else res = transmute<u32>(transmute<f32>((u32)a) / transmute<f32>((u32)b));

        return true;
    }

    bool Computer::TryProcessVEC_FADD() { return ProcessVPUBinary(12, &Computer::_TryPerformVEC_FADD); }
    bool Computer::TryProcessVEC_FSUB() { return ProcessVPUBinary(12, &Computer::_TryPerformVEC_FSUB); }
    bool Computer::TryProcessVEC_FMUL() { return ProcessVPUBinary(12, &Computer::_TryPerformVEC_FMUL); }
    bool Computer::TryProcessVEC_FDIV() { return ProcessVPUBinary(12, &Computer::_TryPerformVEC_FDIV); }

    bool Computer::_TryPerformVEC_AND(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a & b;
        return true;
    }
    bool Computer::_TryPerformVEC_OR(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a | b;
        return true;
    }
    bool Computer::_TryPerformVEC_XOR(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a ^ b;
        return true;
    }
    bool Computer::_TryPerformVEC_ANDN(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = ~a & b;
        return true;
    }

    bool Computer::TryProcessVEC_AND()  { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_AND); }
    bool Computer::TryProcessVEC_OR()   { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_OR); }
    bool Computer::TryProcessVEC_XOR()  { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_XOR); }
    bool Computer::TryProcessVEC_ANDN() { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_ANDN); }

    bool Computer::_TryPerformVEC_ADD(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a + b;
        return true;
    }
    bool Computer::_TryPerformVEC_ADDS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // get sign mask
        u64 smask = sign_mask(elem_sizecode);

        res = a + b;

        // get sign bits
        bool res_sign = (res & smask) != 0;
        bool a_sign = (a & smask) != 0;
        bool b_sign = (b & smask) != 0;

        // if there was an over/underflow, handle saturation cases
        if (a_sign == b_sign && a_sign != res_sign) res = a_sign ? smask : smask - 1;

        return true;
    }
    bool Computer::_TryPerformVEC_ADDUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // get trunc mask
        u64 tmask = trunc_mask(elem_sizecode);

        res = (a + b) & tmask; // truncated for logic below

        // if there was an over/underflow, handle saturation cases
        if (res < a) res = tmask;

        return true;
    }

    bool Computer::TryProcessVEC_ADD()   { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_ADD); }
    bool Computer::TryProcessVEC_ADDS()  { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_ADDS); }
    bool Computer::TryProcessVEC_ADDUS() { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_ADDUS); }

    bool Computer::_TryPerformVEC_SUB(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a - b;
        return true;
    }
    bool Computer::_TryPerformVEC_SUBS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index)
    {
        // since this one's signed, we can just add the negative
        return _TryPerformVEC_ADDS(elem_sizecode, res, a, truncate(~b + 1, elem_sizecode), index);
    }
    bool Computer::_TryPerformVEC_SUBUS(u64, u64 &res, u64 a, u64 b, u64)
    {
        // handle unsigned sub saturation
        res = a > b ? a - b : 0;
        return true;
    }

    bool Computer::TryProcessVEC_SUB()   { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_SUB); }
    bool Computer::TryProcessVEC_SUBS()  { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_SUBS); }
    bool Computer::TryProcessVEC_SUBUS() { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_SUBUS); }

    bool Computer::_TryPerformVEC_MULL(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        res = (i64)sign_extend(a, elem_sizecode) * (i64)sign_extend(b, elem_sizecode);
        return true;
    }

    bool Computer::TryProcessVEC_MULL() { return ProcessVPUBinary(15, &Computer::_TryPerformVEC_MULL); }

    bool Computer::_TryProcessVEC_FMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // this exploits c++ returning false on comparison to NaN. see http://www.felixcloutier.com/x86/MINPD.html for the actual algorithm
        if (elem_sizecode == 3) res = transmute<f64>(a) < transmute<f64>(b) ? a : b;
        else res = transmute<f32>((u32)a) < transmute<f32>((u32)b) ? a : b;

        return true;
    }
    bool Computer::_TryProcessVEC_FMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // this exploits c++ returning false on comparison to NaN. see http://www.felixcloutier.com/x86/MAXPD.html for the actual algorithm
        if (elem_sizecode == 3) res = transmute<f64>(a) > transmute<f64>(b) ? a : b;
        else res = transmute<f32>((u32)a) > transmute<f32>((u32)b) ? a : b;

        return true;
    }

    bool Computer::TryProcessVEC_FMIN() { return ProcessVPUBinary(12, &Computer::_TryProcessVEC_FMIN); }
    bool Computer::TryProcessVEC_FMAX() { return ProcessVPUBinary(12, &Computer::_TryProcessVEC_FMAX); }

    bool Computer::_TryProcessVEC_UMIN(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a < b ? a : b; // a and b are guaranteed to be properly truncated, so this is invariant of size
        return true;
    }
    bool Computer::_TryProcessVEC_SMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // just extend to 64-bit and do a signed compare
        res = (i64)sign_extend(a, elem_sizecode) < (i64)sign_extend(b, elem_sizecode) ? a : b;
        return true;
    }
    bool Computer::_TryProcessVEC_UMAX(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = a > b ? a : b; // a and b are guaranteed to be properly truncated, so this is invariant of size
        return true;
    }
    bool Computer::_TryProcessVEC_SMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64)
    {
        // just extend to 64-bit and do a signed compare
        res = (i64)sign_extend(a, elem_sizecode) > (i64)sign_extend(b, elem_sizecode) ? a : b;
        return true;
    }

    bool Computer::TryProcessVEC_UMIN() { return ProcessVPUBinary(15, &Computer::_TryProcessVEC_UMIN); }
    bool Computer::TryProcessVEC_SMIN() { return ProcessVPUBinary(15, &Computer::_TryProcessVEC_SMIN); }
    bool Computer::TryProcessVEC_UMAX() { return ProcessVPUBinary(15, &Computer::_TryProcessVEC_UMAX); }
    bool Computer::TryProcessVEC_SMAX() { return ProcessVPUBinary(15, &Computer::_TryProcessVEC_SMAX); }

    bool Computer::_TryPerformVEC_FADDSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index)
    {
        // 64-bit fp
        if (elem_sizecode == 3) res = transmute<u64>(index % 2 == 0 ? transmute<f64>(a) - transmute<f64>(b) : transmute<f64>(a) + transmute<f64>(b));
        // 32-bit fp
        else res = transmute<u32>(index % 2 == 0 ? transmute<f32>((u32)a) - transmute<f32>((u32)b) : transmute<f32>((u32)a) + transmute<f32>((u32)b));

        return true;
    }

    bool Computer::TryProcessVEC_FADDSUB() { return ProcessVPUBinary(12, &Computer::_TryPerformVEC_FADDSUB); }

    bool Computer::_TryPerformVEC_AVG(u64, u64 &res, u64 a, u64 b, u64)
    {
        res = (a + b + 1) >> 1; // doesn't work for 64-bit, but Intel doesn't offer a 64-bit variant anyway, so that's fine
        return true;
    }

    bool Computer::TryProcessVEC_AVG() { return ProcessVPUBinary(3, &Computer::_TryPerformVEC_AVG); }

    // constants used to represent the result of a "true" simd floating-point comparison
    inline static constexpr u64 _fp64_simd_cmp_true = 0xffffffffffffffff;
    inline static constexpr u64 _fp32_simd_cmp_true = 0xffffffff;

    bool Computer::_TryProcessVEC_FCMP_helper(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 /*index*/, bool great, bool less, bool equal, bool unord, bool signal) 
    {
        if (elem_sizecode == 3)
        {
            const auto fa = transmute<f64>(a);
            const auto fb = transmute<f64>(b);
            bool cmp;
            if (std::isnan(fa) || std::isnan(fb))
            {
                cmp = unord;
                if (signal) { terminate_err(ErrorCode::ArithmeticError); return false; }
            }
            else if (fa >  fb) cmp = great;
            else if (fa <  fb) cmp = less;
            else if (fa == fb) cmp = equal;
            else cmp = false; /* if something weird happens, catch as false */
            res = cmp ? _fp64_simd_cmp_true : 0;
        }
        else
        {
            const auto fa = transmute<f32>((u32)a);
            const auto fb = transmute<f32>((u32)b);

            bool cmp;
            if (std::isnan(fa) || std::isnan(fb))
            {
                cmp = unord;
                if (signal) { terminate_err(ErrorCode::ArithmeticError); return false; }
            }
            else if (fa >  fb) cmp = great;
            else if (fa <  fb) cmp = less;
            else if (fa == fb) cmp = equal;
            else cmp = false; /* if something weird happens, catch as false */
            res = cmp ? _fp32_simd_cmp_true : 0;
        }
        return true;
    }

    bool Computer::_TryProcessVEC_FCMP_EQ_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, true, false, false); }
    bool Computer::_TryProcessVEC_FCMP_LT_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, false, false, true); }
    bool Computer::_TryProcessVEC_FCMP_LE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, true, false, true); }
    bool Computer::_TryProcessVEC_FCMP_UNORD_Q(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, false, true, false); }
    bool Computer::_TryProcessVEC_FCMP_NEQ_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, false, true, false); }
    bool Computer::_TryProcessVEC_FCMP_NLT_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, true, true, true); }
    bool Computer::_TryProcessVEC_FCMP_NLE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, false, true, true); }
    bool Computer::_TryProcessVEC_FCMP_ORD_Q(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, true, false, false); }
    bool Computer::_TryProcessVEC_FCMP_EQ_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, true, true, false); }
    bool Computer::_TryProcessVEC_FCMP_NGE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, false, true, true); }
    bool Computer::_TryProcessVEC_FCMP_NGT_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, true, true, true); }
    bool Computer::_TryProcessVEC_FCMP_FALSE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, false, false, false); }
    bool Computer::_TryProcessVEC_FCMP_NEQ_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, false, false, false); }
    bool Computer::_TryProcessVEC_FCMP_GE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, true, false, true); }
    bool Computer::_TryProcessVEC_FCMP_GT_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, false, false, true); }
    bool Computer::_TryProcessVEC_FCMP_TRUE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, true, true, false); }
    bool Computer::_TryProcessVEC_FCMP_EQ_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, true, false, true); }
    bool Computer::_TryProcessVEC_FCMP_LT_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, false, false, false); }
    bool Computer::_TryProcessVEC_FCMP_LE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, true, false, false); }
    bool Computer::_TryProcessVEC_FCMP_UNORD_S(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, false, true, true); }
    bool Computer::_TryProcessVEC_FCMP_NEQ_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, false, true, true); }
    bool Computer::_TryProcessVEC_FCMP_NLT_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, true, true, false); }
    bool Computer::_TryProcessVEC_FCMP_NLE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, false, true, false); }
    bool Computer::_TryProcessVEC_FCMP_ORD_S(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, true, false, true); }
    bool Computer::_TryProcessVEC_FCMP_EQ_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, true, true, true); }
    bool Computer::_TryProcessVEC_FCMP_NGE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, false, true, false); }
    bool Computer::_TryProcessVEC_FCMP_NGT_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, true, true, true, false); }
    bool Computer::_TryProcessVEC_FCMP_FALSE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, false, false, false, false, true); }
    bool Computer::_TryProcessVEC_FCMP_NEQ_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, false, false, true); }
    bool Computer::_TryProcessVEC_FCMP_GE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, true, false, false); }
    bool Computer::_TryProcessVEC_FCMP_GT_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, false, false, false, false); }
    bool Computer::_TryProcessVEC_FCMP_TRUE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index) { return _TryProcessVEC_FCMP_helper(elem_sizecode, res, a, b, index, true, true, true, true, true); }

    bool Computer::TryProcessVEC_FCMP()
    {
        // read condition byte
        u8 cond;
        if (!GetMemAdv<u8>(cond)) return false;

        // ensure condition is in range - not optional
        if (cond >= 32) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

        // perform the comparison
        return ProcessVPUBinary(12, _TryProcessVEC_FCMP_lookup[cond]);
    }

    // in order to avoid creating another format just for this, will use VPUBinary.
    // the result will be src1, thus creating the desired no-modification behavior so long as dest == src1.
    // each pair of elements will update the flags in turn, thus the total comparison is on the last pair processed - standard behavior requires it be scalar operation.
    // the assembler will ensure all of this is the case.
    bool Computer::_TryProcessVEC_FCOMI(u64 elem_sizecode, u64 &res, u64 _a, u64 _b, u64)
    {
        // temporaries for cmp results
        bool x, y, z;

        if (elem_sizecode == 3)
        {
            const auto a = transmute<f64>(_a);
            const auto b = transmute<f64>(_b);

            if (a > b) { x = false; y = false; z = false; }
            else if (a < b) { x = false; y = false; z = true; }
            else if (a == b) { x = true; y = false; z = false; }
            // otherwise is unordered
            else { x = true; y = true; z = true; }
        }
        else
        {
            const auto a = transmute<f32>((u32)_a);
            const auto b = transmute<f32>((u32)_b);

            if (a > b) { x = false; y = false; z = false; }
            else if (a < b) { x = false; y = false; z = true; }
            else if (a == b) { x = true; y = false; z = false; }
            // otherwise is unordered
            else { x = true; y = true; z = true; }
        }

        // update comparison flags
        ZF() = x;
        PF() = y;
        CF() = z;
        
        // clear OF, AF, and SF
		RFLAGS() &= ~MASK_UNION_3(OF, AF, SF);

        // result is src1 (see explanation above)
        res = _a;

        return true;
    }

    bool Computer::TryProcessVEC_FCOMI() { return ProcessVPUBinary(12, &Computer::_TryProcessVEC_FCOMI); }

    // these trigger ArithmeticError on negative sqrt - spec doesn't specify explicitly what to do
    bool Computer::_TryProcessVEC_FSQRT(u64 elem_sizecode, u64 &res, u64 a, u64)
    {
        if (elem_sizecode == 3)
        {
            const auto f = transmute<f64>(a);
            if (f < 0) { terminate_err(ErrorCode::ArithmeticError); return false; }
            res = transmute<u64>(std::sqrt(f));
        }
        else
        {
            const auto f = transmute<f32>((u32)a);
            if (f < 0) { terminate_err(ErrorCode::ArithmeticError); return false; }
            res = transmute<u32>(std::sqrt(f));
        }
        return true;
    }
    bool Computer::_TryProcessVEC_FRSQRT(u64 elem_sizecode, u64 &res, u64 a, u64)
    {
        if (elem_sizecode == 3)
        {
            const auto f = transmute<f64>(a);
            if (f < 0) { terminate_err(ErrorCode::ArithmeticError); return false; }
            res = transmute<u64>(1.0 / std::sqrt(f));
        }
        else
        {
            const auto f = transmute<f32>((u32)a);
            if (f < 0) { terminate_err(ErrorCode::ArithmeticError); return false; }
            res = transmute<u32>(1.0f / std::sqrt(f));
        }
        return true;
    }

    bool Computer::TryProcessVEC_FSQRT() { return ProcessVPUUnary(12, &Computer::_TryProcessVEC_FSQRT); }
    bool Computer::TryProcessVEC_FRSQRT() { return ProcessVPUUnary(12, &Computer::_TryProcessVEC_FRSQRT); }

    // VPUCVTDelegates for conversions
    bool Computer::_f64_to_i32(u64 &res, u64 val)
    {
        res = (u32)(i32)PerformRoundTrip(transmute<f64>(val), MXCSR_RC());
        return true;
    }
    bool Computer::_single_to_i32(u64 &res, u64 val)
    {
        res = (u32)(i32)PerformRoundTrip(transmute<f32>((u32)val), MXCSR_RC());
        return true;
    }

    bool Computer::_f64_to_i64(u64 &res, u64 val)
    {
        res = (u64)(i64)PerformRoundTrip(transmute<f64>(val), MXCSR_RC());
        return true;
    }
    bool Computer::_single_to_i64(u64 &res, u64 val)
    {
        res = (u64)(i64)PerformRoundTrip(transmute<f32>((u32)val), MXCSR_RC());
        return true;
    }

    bool Computer::_f64_to_ti32(u64 &res, u64 val)
    {
        res = (u32)(i32)transmute<f64>(val);
        return true;
    }
    bool Computer::_single_to_ti32(u64 &res, u64 val)
    {
        res = (u32)(i32)transmute<f32>((u32)val);
        return true;
    }

    bool Computer::_f64_to_ti64(u64 &res, u64 val)
    {
        res = (u64)(i64)transmute<f64>(val);
        return true;
    }
    bool Computer::_single_to_ti64(u64 &res, u64 val)
    {
        res = (u64)(i64)transmute<f32>((u32)val);
        return true;
    }

    bool Computer::_i32_to_f64(u64 &res, u64 val)
    {
        res = transmute<u64>((f64)(i32)val);
        return true;
    }
    bool Computer::_i32_to_single(u64 &res, u64 val)
    {
        res = transmute<u32>((f32)(i32)val);
        return true;
    }

    bool Computer::_i64_to_f64(u64 &res, u64 val)
    {
        res = transmute<u64>((f64)(i64)val);
        return true;
    }
    bool Computer::_i64_to_single(u64 &res, u64 val)
    {
        res = transmute<u32>((f32)(i64)val);
        return true;
    }

    bool Computer::_f64_to_single(u64 &res, u64 val)
    {
        res = transmute<u32>((f32)transmute<f64>(val));
        return true;
    }
    bool Computer::_single_to_f64(u64 &res, u64 val)
    {
        res = transmute<u64>((f64)transmute<f32>((u32)val));
        return true;
    }

    /*
    [8: mode]

    mode =  0: CVTSD2SI r32, xmm
    mode =  1: CVTSD2SI r32, m64
    mode =  2: CVTSD2SI r64, xmm
    mode =  3: CVTSD2SI r64, m64

    mode =  4: CVTSS2SI r32, xmm
    mode =  5: CVTSS2SI r32, m32
    mode =  6: CVTSS2SI r64, xmm
    mode =  7: CVTSS2SI r64, m32

    mode =  8: CVTTSD2SI r32, xmm
    mode =  9: CVTTSD2SI r32, m64
    mode = 10: CVTTSD2SI r64, xmm
    mode = 11: CVTTSD2SI r64, m64

    mode = 12: CVTTSS2SI r32, xmm
    mode = 13: CVTTSS2SI r32, m32
    mode = 14: CVTTSS2SI r64, xmm
    mode = 15: CVTTSS2SI r64, m32

    mode = 16: CVTSI2SD xmm, r32
    mode = 17: CVTSI2SD xmm, m32
    mode = 18: CVTSI2SD xmm, r64
    mode = 19: CVTSI2SD xmm, m64

    mode = 20: CVTSI2SS xmm, r32
    mode = 21: CVTSI2SS xmm, m32
    mode = 22: CVTSI2SS xmm, r64
    mode = 23: CVTSI2SS xmm, m64

    mode = 24: CVTSD2SS xmm, xmm
    mode = 25: CVTSD2SS xmm, m64

    mode = 26: CVTSS2SD xmm, xmm
    mode = 27: CVTSS2SD xmm, m32

    // ---------------------- //

    mode = 28: CVTPD2DQ 2
    mode = 29: CVTPD2DQ 4
    mode = 30: CVTPD2DQ 8

    mode = 31: CVTPS2DQ 4
    mode = 32: CVTPS2DQ 8
    mode = 33: CVTPS2DQ 16

    mode = 34: CVTTPD2DQ 2
    mode = 35: CVTTPD2DQ 4
    mode = 36: CVTTPD2DQ 8

    mode = 37: CVTTPS2DQ 4
    mode = 38: CVTTPS2DQ 8
    mode = 39: CVTTPS2DQ 16

    mode = 40: CVTDQ2PD 2
    mode = 41: CVTDQ2PD 4
    mode = 42: CVTDQ2PD 8

    mode = 43: CVTDQ2PS 4
    mode = 44: CVTDQ2PS 8
    mode = 45: CVTDQ2PS 16

    mode = 46: CVTPD2PS 2
    mode = 47: CVTPD2PS 4
    mode = 48: CVTPD2PS 8

    mode = 49: CVTPS2PD 2
    mode = 50: CVTPS2PD 4
    mode = 51: CVTPS2PD 8

    else UND
    */
    bool Computer::TryProcessVEC_CVT()
    {
        // read mode byte
        u8 mode;
        if (!GetMemAdv<u8>(mode)) return false;

        // route to handlers
        switch (mode)
        {
        case 0: return ProcessVPUCVT_scalar_reg_xmm(2, 3, &Computer::_f64_to_i32);
        case 1: return ProcessVPUCVT_scalar_reg_mem(2, 3, &Computer::_f64_to_i32);
        case 2: return ProcessVPUCVT_scalar_reg_xmm(3, 3, &Computer::_f64_to_i64);
        case 3: return ProcessVPUCVT_scalar_reg_mem(3, 3, &Computer::_f64_to_i64);

        case 4: return ProcessVPUCVT_scalar_reg_xmm(2, 2, &Computer::_single_to_i32);
        case 5: return ProcessVPUCVT_scalar_reg_mem(2, 2, &Computer::_single_to_i32);
        case 6: return ProcessVPUCVT_scalar_reg_xmm(3, 2, &Computer::_single_to_i64);
        case 7: return ProcessVPUCVT_scalar_reg_mem(3, 2, &Computer::_single_to_i64);

        case 8: return ProcessVPUCVT_scalar_reg_xmm(2, 3, &Computer::_f64_to_ti32);
        case 9: return ProcessVPUCVT_scalar_reg_mem(2, 3, &Computer::_f64_to_ti32);
        case 10: return ProcessVPUCVT_scalar_reg_xmm(3, 3, &Computer::_f64_to_ti64);
        case 11: return ProcessVPUCVT_scalar_reg_mem(3, 3, &Computer::_f64_to_ti64);

        case 12: return ProcessVPUCVT_scalar_reg_xmm(2, 2, &Computer::_single_to_ti32);
        case 13: return ProcessVPUCVT_scalar_reg_mem(2, 2, &Computer::_single_to_ti32);
        case 14: return ProcessVPUCVT_scalar_reg_xmm(3, 2, &Computer::_single_to_ti64);
        case 15: return ProcessVPUCVT_scalar_reg_mem(3, 2, &Computer::_single_to_ti64);

        case 16: return ProcessVPUCVT_scalar_xmm_reg(3, 2, &Computer::_i32_to_f64);
        case 17: return ProcessVPUCVT_scalar_xmm_mem(3, 2, &Computer::_i32_to_f64);
        case 18: return ProcessVPUCVT_scalar_xmm_reg(3, 3, &Computer::_i64_to_f64);
        case 19: return ProcessVPUCVT_scalar_xmm_mem(3, 3, &Computer::_i64_to_f64);

        case 20: return ProcessVPUCVT_scalar_xmm_reg(2, 2, &Computer::_i32_to_single);
        case 21: return ProcessVPUCVT_scalar_xmm_mem(2, 2, &Computer::_i32_to_single);
        case 22: return ProcessVPUCVT_scalar_xmm_reg(2, 3, &Computer::_i64_to_single);
        case 23: return ProcessVPUCVT_scalar_xmm_mem(2, 3, &Computer::_i64_to_single);

        case 24: return ProcessVPUCVT_scalar_xmm_xmm(2, 3, &Computer::_f64_to_single);
        case 25: return ProcessVPUCVT_scalar_xmm_mem(2, 3, &Computer::_f64_to_single);

        case 26: return ProcessVPUCVT_scalar_xmm_xmm(3, 2, &Computer::_single_to_f64);
        case 27: return ProcessVPUCVT_scalar_xmm_mem(3, 2, &Computer::_single_to_f64);

        // ------------------------------------------------------------------------ //

        case 28: return ProcessVPUCVT_packed(2, 2, 3, &Computer::_f64_to_i32);
        case 29: return ProcessVPUCVT_packed(4, 2, 3, &Computer::_f64_to_i32);
        case 30: return ProcessVPUCVT_packed(8, 2, 3, &Computer::_f64_to_i32);

        case 31: return ProcessVPUCVT_packed(4, 2, 2, &Computer::_single_to_i32);
        case 32: return ProcessVPUCVT_packed(8, 2, 2, &Computer::_single_to_i32);
        case 33: return ProcessVPUCVT_packed(16, 2, 2, &Computer::_single_to_i32);

        case 34: return ProcessVPUCVT_packed(2, 2, 3, &Computer::_f64_to_ti32);
        case 35: return ProcessVPUCVT_packed(4, 2, 3, &Computer::_f64_to_ti32);
        case 36: return ProcessVPUCVT_packed(8, 2, 3, &Computer::_f64_to_ti32);

        case 37: return ProcessVPUCVT_packed(4, 2, 2, &Computer::_single_to_ti32);
        case 38: return ProcessVPUCVT_packed(8, 2, 2, &Computer::_single_to_ti32);
        case 39: return ProcessVPUCVT_packed(16, 2, 2, &Computer::_single_to_ti32);

        case 40: return ProcessVPUCVT_packed(2, 3, 2, &Computer::_i32_to_f64);
        case 41: return ProcessVPUCVT_packed(4, 3, 2, &Computer::_i32_to_f64);
        case 42: return ProcessVPUCVT_packed(8, 3, 2, &Computer::_i32_to_f64);

        case 43: return ProcessVPUCVT_packed(4, 2, 2, &Computer::_i32_to_single);
        case 44: return ProcessVPUCVT_packed(8, 2, 2, &Computer::_i32_to_single);
        case 45: return ProcessVPUCVT_packed(16, 2, 2, &Computer::_i32_to_single);

        case 46: return ProcessVPUCVT_packed(2, 2, 3, &Computer::_f64_to_single);
        case 47: return ProcessVPUCVT_packed(4, 2, 3, &Computer::_f64_to_single);
        case 48: return ProcessVPUCVT_packed(8, 2, 3, &Computer::_f64_to_single);

        case 49: return ProcessVPUCVT_packed(2, 3, 2, &Computer::_single_to_f64);
        case 50: return ProcessVPUCVT_packed(4, 3, 2, &Computer::_single_to_f64);
        case 51: return ProcessVPUCVT_packed(8, 3, 2, &Computer::_single_to_f64);

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }
    }
	
    // -------------------------------------------------------------------------------------

	/*
	[8: mode]

	mode = 0: [4: dest][4: src]    dest (r32) <- src (xmm)
	mode = 1: [4: dest][4: src]    dest (xmm) <- src (r32)

	mode = 2: [4: dest][4: src]    dest (r64) <- src (xmm)
	mode = 3: [4: dest][4: src]    dest (xmm) <- src (r64)

	mode = 4: [binary op]          dest <- bswap(src)
	*/
	bool Computer::TryProcessTRANS()
	{
		u8 temp;
		if (!GetMemAdv<u8>(temp)) return false;

		switch (temp)
		{
		case 0:
			if (!GetMemAdv<u8>(temp)) return false;
			CPURegisters[temp >> 4].x32() = ZMMRegisters[temp & 15].get<u32>(0);
			return true;
		case 1:
			if (!GetMemAdv<u8>(temp)) return false;
			ZMMRegisters[temp >> 4].get<u32>(0) = CPURegisters[temp & 15].x32();
			return true;

		// -----------------------------------

		case 2:
			if (!GetMemAdv<u8>(temp)) return false;
			CPURegisters[temp >> 4].x64() = ZMMRegisters[temp & 15].get<u64>(0);
			return true;
		case 3:
			if (!GetMemAdv<u8>(temp)) return false;
			ZMMRegisters[temp >> 4].get<u64>(0) = CPURegisters[temp & 15].x64();
			return true;

		// -----------------------------------

		case 4:
		{
            u8 s1, s2;
			u64 m, a, b;
			if (!FetchBinaryOpFormat(s1, s2, m, a, b, false)) return false;
			u64 sizecode = (s1 >> 2) & 3;

			return StoreBinaryOpFormat(s1, s2, m, bswap(b, sizecode));
		}

		// -----------------------------------

		default:
			terminate_err(ErrorCode::UndefinedBehavior);
			return false;
		}
	}

    bool Computer::ProcessDEBUG()
    {
        u8 op;
        if (!GetMemAdv<u8>(op)) return false;

        switch (op)
        {
        case 0: WriteCPUDebugString(std::cout); break;
        case 1: WriteVPUDebugString(std::cout); break;
        case 2: WriteFullDebugString(std::cout); break;
		case 3:
        {
            u64 m, t;
            if (!GetAddressAdv(m) || !GetMemAdv<u64>(t)) { terminate_err(ErrorCode::UndefinedBehavior); return false; }

            // if starting position is out of bounds, print 0 characters (don't no-op cause then user might think it's not working)
            if (m >= mem.size()) t = 0;
            // otherwise if printing more than exists, print as many as possible instead
            else if (t > mem.size() || m + t > mem.size()) t = mem.size() - m;

            std::cout << '\n';
            Dump(std::cout, mem.data(), m, t);
            break;
        }

        default: terminate_err(ErrorCode::UndefinedBehavior); return false;
        }

        return true;
    }
    bool Computer::ProcessUNKNOWN()
    {
        terminate_err(ErrorCode::UnknownOp);
        return false;
    }
}
