#include "../include/Computer.h"

namespace CSX64
{
    bool Computer::GetCString(u64 pos, std::string &str)
    {
        str.clear();

        // read the string
        const char *ptr = reinterpret_cast<const char*>(mem) + pos; // aliasing is ok because casting to char type
        for (; ; ++pos, ++ptr)
        {
            // ensure we're in bounds
            if (pos >= mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

            // if it's not a terminator, append it
            if (*ptr != 0) str.push_back(*ptr);
            // otherwise we're done reading
            else break;
        }

        return true;
    }
    bool Computer::SetCString(u64 pos, const std::string &str)
    {
        // make sure we're in bounds
        if (pos >= mem_size || pos + (str.size() + 1) > mem_size) return false;

        // make sure we're not in the readonly segment
        if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

        // write the string
        std::memcpy(reinterpret_cast<char*>(mem) + pos, str.data(), str.size()); // aliasing ok because casting to char type
        // write a null terminator
        (reinterpret_cast<char*>(mem) + pos)[str.size()] = 0; // aliasing ok because casting to char type

        return true;
    }

    bool Computer::PushRaw(u64 size, u64 val)
    {
        RSP() -= size;
        if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }
        return SetMemRaw(RSP(), size, val);
    }
    bool Computer::PopRaw(u64 size, u64 &val)
    {
        if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }
        if (!GetMemRaw(RSP(), size, val)) return false;
        RSP() += size;
        return true;
    }

    bool Computer::GetMemRaw(u64 pos, u64 size, u64 &res)
    {
        if (pos >= mem_size || pos + size > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

        switch (size)
        {
        case 1: res = bin_read<u8>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type
        case 2: res = bin_read<u16>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type
        case 4: res = bin_read<u32>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type
        case 8: res = bin_read<u64>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type

        default: throw std::runtime_error("GetMemRaw size was non-standard");
        }
    }
    bool Computer::GetMemRaw_szc(u64 pos, u64 sizecode, u64 &res)
    {
        if (pos >= mem_size || pos + Size(sizecode) > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

        switch (sizecode)
        {
        case 0: res = bin_read<u8>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type
        case 1: res = bin_read<u16>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type
        case 2: res = bin_read<u32>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type
        case 3: res = bin_read<u64>(reinterpret_cast<const char*>(mem) + pos); return true; // aliasing ok because casting to char type

        default: throw std::runtime_error("GetMemRaw size was non-standard");
        }
    }

    bool Computer::SetMemRaw(u64 pos, u64 size, u64 val)
    {
        if (pos >= mem_size || pos + size > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
        if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

        switch (size)
        {
        case 1: bin_write<u8>(reinterpret_cast<char*>(mem) + pos, (u8)val); return true; // aliasing ok because casting to char type
        case 2: bin_write<u16>(reinterpret_cast<char*>(mem) + pos, (u16)val); return true; // aliasing ok because casting to char type
        case 4: bin_write<u32>(reinterpret_cast<char*>(mem) + pos, (u32)val); return true; // aliasing ok because casting to char type
        case 8: bin_write<u64>(reinterpret_cast<char*>(mem) + pos, (u64)val); return true; // aliasing ok because casting to char type

        default: throw std::runtime_error("GetMemRaw size was non-standard");
        }
    }
    bool Computer::SetMemRaw_szc(u64 pos, u64 sizecode, u64 val)
    {
        if (pos >= mem_size || pos + Size(sizecode) > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
        if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); std::cerr << "\ntried to access: " << std::hex << pos << std::dec << '\n'; return false; }

        switch (sizecode)
        {
        case 0: bin_write<u8>(reinterpret_cast<char*>(mem) + pos, (u8)val); return true; // aliasing ok because casting to char type
        case 1: bin_write<u16>(reinterpret_cast<char*>(mem) + pos, (u16)val); return true; // aliasing ok because casting to char type
        case 2: bin_write<u32>(reinterpret_cast<char*>(mem) + pos, (u32)val); return true; // aliasing ok because casting to char type
        case 3: bin_write<u64>(reinterpret_cast<char*>(mem) + pos, (u64)val); return true; // aliasing ok because casting to char type

        default: throw std::runtime_error("GetMemRaw size was non-standard");
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
        RIP() += Size(sizecode);
        return true;
    }

    bool Computer::GetByteAdv(u8 &res)
    {
        if (RIP() >= mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

        res = bin_read<u8>(reinterpret_cast<const char*>(mem) + RIP()++); // aliasing ok because casting to char type

        return true;
    }

    bool Computer::GetCompactImmAdv(u64 &res)
    {
        // [1: fill][5:][2: size]   [size: imm]

        u8 prefix;

        // read the prefix byte
        if (!GetByteAdv(prefix)) return false;

        // read the (raw) imm and handle sign extension
        switch (prefix & 3)
        {
        case 0: if (!GetMemAdv<u8>(res)) return false;  if (prefix & 0x80) res |= (u64)0xffffffffffffff00; break;
        case 1: if (!GetMemAdv<u16>(res)) return false; if (prefix & 0x80) res |= (u64)0xffffffffffff0000; break;
        case 2: if (!GetMemAdv<u32>(res)) return false; if (prefix & 0x80) res |= (u64)0xffffffff00000000; break;
        case 3: if (!GetMemAdv<u64>(res)) return false; break;
        }

        return true;
    }

    bool Computer::GetAddressAdv(u64 &res)
    {
        // [1: imm][1:][2: mult_1][2: size][1: r1][1: r2]   ([4: r1][4: r2])   ([size: imm])

        u8 settings, sizecode, regs = 0; // regs = 0 is only to appease warnings
        res = 0; // initialize res - functions as imm parsing location, so it has to start at 0

        // get the settings byte and regs byte if applicable
        if (!GetByteAdv(settings) || ((settings & 3) != 0 && !GetByteAdv(regs))) return false;

        // get the sizecode
        sizecode = (settings >> 2) & 3;

        if constexpr (StrictUND)
        {
            // 8-bit addressing is not allowed
            if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
        }

        // get the imm if applicable - store into res
        if ((settings & 0x80) != 0 && !GetMemAdv(Size(sizecode), res)) return false;

        // if r1 was used, add that pre-multiplied by the multiplier
        if ((settings & 2) != 0) res += CPURegisters[regs >> 4][sizecode] << ((settings >> 4) & 3);
        // if r2 was used, add that
        if ((settings & 1) != 0) res += CPURegisters[regs & 15][sizecode];

        // got an address
        return true;
    }
}
