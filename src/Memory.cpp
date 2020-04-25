#include "../include/Computer.h"

using namespace CSX64::detail;

namespace CSX64
{
    bool Computer::read_str(const u64 pos, std::string &str)
    {
        // only need to bounds check the first character to read
        if (pos >= mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }

        // clear the string, then scan through until a null terminator (success) or the end of the array (failure)
        str.clear();
        for (const u8 *ptr = mem.data() + pos, *const end = mem.data() + mem.size(); ptr != end; ++ptr)
        {
            u8 v = *ptr;
            if (v == 0) return true;
            str.push_back((char)v);
        }
        terminate_err(ErrorCode::OutOfBounds);
        return false;
    }
    bool Computer::write_str(u64 pos, const std::string &str)
    {
        // bounds check the entire string in one go
        if (pos >= mem.size() || pos + str.size() + 1 > mem.size()) return false;

        // make sure we're not in the readonly segment
        if (pos < readonly_barrier) { terminate_err(ErrorCode::AccessViolation); return false; }

        // write the string contents and a null terminator
        std::memcpy(mem.data() + pos, str.data(), str.size());
        (mem.data() + pos)[str.size()] = 0;

        return true;
    }

    bool Computer::PushRaw(u64 size, u64 val)
    {
        RSP() -= size;
        if (RSP() < stack_barrier) { terminate_err(ErrorCode::StackOverflow); return false; }
        return SetMemRaw(RSP(), size, val);
    }
    bool Computer::PopRaw(u64 size, u64 &val)
    {
        if (RSP() < stack_barrier) { terminate_err(ErrorCode::StackOverflow); return false; }
        if (!GetMemRaw(RSP(), size, val)) return false;
        RSP() += size;
        return true;
    }

    bool Computer::GetMemRaw(u64 pos, u64 size, u64 &res)
    {
        if (pos >= mem.size() || pos + size > mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }

        switch (size)
        {
        case 1: res = read<u8>(mem.data() + pos); return true;
        case 2: res = read<u16>(mem.data() + pos); return true;
        case 4: res = read<u32>(mem.data() + pos); return true;
        case 8: res = read<u64>(mem.data() + pos); return true;

        default: throw std::runtime_error("GetMemRaw get_size was non-standard");
        }
    }
    bool Computer::GetMemRaw_szc(u64 pos, u64 sizecode, u64 &res)
    {
        if (pos >= mem.size() || pos + get_size(sizecode) > mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }

        switch (sizecode)
        {
        case 0: res = read<u8>(mem.data() + pos); return true;
        case 1: res = read<u16>(mem.data() + pos); return true;
        case 2: res = read<u32>(mem.data() + pos); return true;
        case 3: res = read<u64>(mem.data() + pos); return true;

        default: throw std::runtime_error("GetMemRaw get_size was non-standard");
        }
    }

    bool Computer::SetMemRaw(u64 pos, u64 size, u64 val)
    {
        if (pos >= mem.size() || pos + size > mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }
        if (pos < readonly_barrier) { terminate_err(ErrorCode::AccessViolation); return false; }

        switch (size)
        {
        case 1: write<u8>(mem.data() + pos, (u8)val); return true;
        case 2: write<u16>(mem.data() + pos, (u16)val); return true;
        case 4: write<u32>(mem.data() + pos, (u32)val); return true;
        case 8: write<u64>(mem.data() + pos, (u64)val); return true;

        default: throw std::runtime_error("GetMemRaw get_size was non-standard");
        }
    }
    bool Computer::SetMemRaw_szc(u64 pos, u64 sizecode, u64 val)
    {
        if (pos >= mem.size() || pos + get_size(sizecode) > mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }
        if (pos < readonly_barrier) { terminate_err(ErrorCode::AccessViolation); std::cerr << "\ntried to access: " << std::hex << pos << std::dec << '\n'; return false; }

        switch (sizecode)
        {
        case 0: write<u8>(mem.data() + pos, (u8)val); return true;
        case 1: write<u16>(mem.data() + pos, (u16)val); return true;
        case 2: write<u32>(mem.data() + pos, (u32)val); return true;
        case 3: write<u64>(mem.data() + pos, (u64)val); return true;

        default: throw std::runtime_error("GetMemRaw get_size was non-standard");
        }
    }

    bool Computer::GetMemAdv(u64 size, u64 &res)
    {
        if (!GetMemRaw(RIP(), size, res)) return false;
        RIP() += size;
        return true;
    }
    bool Computer::GetMemAdv_szc(u64 sizecode, u64 &res)
    {
        if (!GetMemRaw_szc(RIP(), sizecode, res)) return false;
        RIP() += get_size(sizecode);
        return true;
    }

    bool Computer::GetAddressAdv(u64 &res)
    {
        // [1: imm][1:][2: mult_1][2: size][1: r1][1: r2]   ([4: r1][4: r2])   ([size: imm])

        u8 settings, sizecode, regs = 0; // regs = 0 is only to appease warnings
        res = 0; // initialize res - functions as imm parsing location, so it has to start at 0

        // get the settings byte and regs byte if applicable
        if (!GetMemAdv<u8>(settings) || ((settings & 3) != 0 && !GetMemAdv<u8>(regs))) return false;

        // get the sizecode
        sizecode = (settings >> 2) & 3;

        if constexpr (StrictUND)
        {
            // 8-bit addressing is not allowed
            if (sizecode == 0) { terminate_err(ErrorCode::UndefinedBehavior); return false; }
        }

        // get the imm if applicable - store into res
        if ((settings & 0x80) != 0 && !GetMemAdv(get_size(sizecode), res)) return false;

        // if r1 was used, add that pre-multiplied by the multiplier
        if ((settings & 2) != 0) res += CPURegisters[regs >> 4][sizecode] << ((settings >> 4) & 3);
        // if r2 was used, add that
        if ((settings & 1) != 0) res += CPURegisters[regs & 15][sizecode];

        // got an address
        return true;
    }
}
