#include <cassert>

#include "../include/Computer.h"

#define OPCODE_COUNTS 0

#if OPCODE_COUNTS
u64 op_exe_count[256];
#endif

namespace CSX64
{
    bool Computer::realloc(std::size_t size, bool preserve_contents)
    {
		if (!preserve_contents) mem.clear();
		mem.resize(size);
		return true;
    }

    void Computer::initialize(const Executable &exe, const std::vector<std::string> &args, std::size_t stacksize)
	{
		// get size of memory we need to allocate
		std::size_t size = exe.total_size() + stacksize;

		// make sure we catch overflow from adding stacksize
		if (size < stacksize) throw std::overflow_error("memory get_size overflow");
		// make sure it's within max memory usage limits
		if (size > max_mem_size) throw MemoryAllocException("executable get_size exceeded max memory");
		
		// allocate the required space (we can safely discard any previous values)
		if (!this->realloc(size, false)) throw MemoryAllocException("memory allocation failed");

		// mark the minimum memory size (so client code can't truncate off program code/data/stack/etc.)
		min_mem_size = size;

		// copy the executable content into our memory array
		std::memcpy(mem.data(), exe.content(), (std::size_t)exe.content_size());
		// zero the bss segment
		std::memset(mem.data() + exe.content_size(), 0, (std::size_t)exe.bss_seglen());

		// set up memory barriers
		exe_barrier = exe.text_seglen();
		readonly_barrier = exe.text_seglen() + exe.rodata_seglen();
		stack_barrier = exe.text_seglen() + exe.rodata_seglen() + exe.data_seglen() + exe.bss_seglen();

		// set up cpu registers
		for (int i = 0; i < 16; ++i) CPURegisters[i].x64() = Rand();

		// set up fpu registers
		FINIT();

		// set up vpu registers
		for (int i = 0; i < 32; ++i)
			for (int j = 0; j < 8; ++j) ZMMRegisters[i].get<u64>(j) = Rand();
		MXCSR() = 0x1f80;

		// set execution state
		RIP() = 0;
		RFLAGS() = 2; // x86 standard dictates this initial state
		_running = true;
		_error = ErrorCode::None;

		// get the stack pointer
		u64 stack = size;
		RBP() = stack; // RBP points to before we start pushing args

		// an array of pointers to command line args in computer memory.
		// one for each arg, plus a null terminator.
		std::vector<u64> cmdarg_pointers(args.size() + 1);

		// put each arg on the stack and get their addresses
		for (std::size_t i = 0; i < args.size(); ++i)
		{
			// push the arg onto the stack
			stack -= (u64)args[i].size() + 1;
			(void)write_str(stack, args[i]);

			// record pointer to this arg
			cmdarg_pointers[i] = stack;
		}
		// the last pointer is null (C guarantees this, so we will as well)
		cmdarg_pointers[args.size()] = 0;

		// make room for the command line pointer array
		stack -= 8 * (u64)cmdarg_pointers.size();
		// write pointer array to memory
		for (std::size_t i = 0; i < cmdarg_pointers.size(); ++i) (void)write_mem<u64>(stack + i * 8, cmdarg_pointers[i]);

		// load arg count and arg array pointer to RDI, RSI
		RDI() = args.size();
		RSI() = stack;

		// initialize RSP
		RSP() = stack;

		// also push args to stack (RTL)
		(void)push_mem<u64>(RSI());
		(void)push_mem<u64>(RDI());

		#if OPCODE_COUNTS
		for (u64 i = 0; i < 256; ++i) op_exe_count[i] = 0;
		#endif
	}

	u64 Computer::tick(u64 count)
	{
		if (!running()) return 0;

		u8 op;
		u64 ticks;
		for (ticks = 0; ticks < count; ++ticks)
		{
			// make sure we're before the executable barrier before reading the opcode
			if (RIP() >= exe_barrier) { terminate_err(ErrorCode::AccessViolation); break; }
			if (!GetMemAdv<u8>(op)) break;

			#if OPCODE_COUNTS
			++op_exe_count[op];
			#endif

			// perform the instruction - if it returns false it is requesting tick loop to break early for some reason
			if (!(this->*opcode_handlers[op])()) break;
		}
		return ticks;
	}

    void Computer::terminate_err(ErrorCode err)
    {
        // only do this if we're currently running (so we don't override what error caused the initial termination)
        if (_running)
        {
            // set error and stop execution
            _error = err;
            _running = false;

            CloseFiles(); // close all the file descriptors
        }
    }
    void Computer::terminate_ok(int ret)
    {
        // only do this if we're currently running (so we don't override the "real" return value)
        if (_running)
        {
			#if OPCODE_COUNTS
			std::cout << "\n\nOPCode Counts:\n" << std::dec << std::setfill(' ');
			for (u64 i = 0; i < 256; ++i)
			{
				std::cout << std::setw(3) << i << ": " << std::setw(16) << op_exe_count[i] << "   ";
				if (i > 0 && i % 4 == 3) std::cout << '\n';
			}
			#endif

            // set return value and stop execution
            _return_value = ret;
            _running = false;

            CloseFiles(); // close all the file descriptors
        }
    }

    int Computer::OpenFileWrapper(std::unique_ptr<IFileWrapper> f)
    {
        int fd = FindAvailableFD();
        if (fd >= 0) FileDescriptors[fd] = std::move(f);
        return fd;
    }
    void Computer::OpenFileWrapper(int fd, std::unique_ptr<IFileWrapper> f)
    {
        FileDescriptors[fd] = std::move(f); 
    }

    void Computer::CloseFileWrapper(int fd)
    {
        FileDescriptors[fd] = nullptr; 
    }
    void Computer::CloseFiles()
    {
        for (auto &fd : FileDescriptors) fd = nullptr;
    }

    IFileWrapper *Computer::GetFileWrapper(int fd)
    {
        return FileDescriptors[fd].get();
    }

    int Computer::FindAvailableFD()
    {
        for (int i = 0; i < FDCount; ++i)
            if (FileDescriptors[i] == nullptr) return i;

        return -1;
    }

	std::ostream &Computer::WriteCPUDebugString(std::ostream &ostr)
	{
		// clear the width field and set up a format restore point
		ostr.width(0);
		iosfrstor _frstor(ostr);

		ostr << std::hex << std::setfill('0') << std::noboolalpha;

		ostr << '\n';
		ostr << "RAX: " << std::setw(16) << RAX() << "     CF: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
		ostr << "RBX: " << std::setw(16) << RBX() << "     PF: " << PF() << "     RIP:    " << std::setw(16) << RIP() << '\n';
		ostr << "RCX: " << std::setw(16) << RCX() << "     AF: " << AF() << '\n';
		ostr << "RDX: " << std::setw(16) << RDX() << "     ZF: " << ZF() << "     ST0: " << ST(0) << '\n';
		ostr << "RSI: " << std::setw(16) << RSI() << "     SF: " << SF() << "     ST1: " << ST(1) << '\n';
		ostr << "RDI: " << std::setw(16) << RDI() << "     OF: " << OF() << "     ST2: " << ST(2) << '\n';
		ostr << "RBP: " << std::setw(16) << RBP() << "               ST3: " << ST(3) << '\n';
		ostr << "RSP: " << std::setw(16) << RSP() << "     b:  " << cc_b() << "     ST4: " << ST(4) << '\n';
		ostr << "R8:  " << std::setw(16) << R8() << "     be: " << cc_be() << "     ST5: " << ST(5) << '\n';
		ostr << "R9:  " << std::setw(16) << R9() << "     a:  " << cc_a() << "     ST6: " << ST(6) << '\n';
		ostr << "R10: " << std::setw(16) << R10() << "     ae: " << cc_ae() << "     ST7: " << ST(7) << '\n';
		ostr << "R11: " << std::setw(16) << R11() << '\n';
		ostr << "R12: " << std::setw(16) << R12() << "     l:  " << cc_l() << "     C0: " << FPU_C0() << '\n';
		ostr << "R13: " << std::setw(16) << R13() << "     le: " << cc_le() << "     C1: " << FPU_C1() << '\n';
		ostr << "R14: " << std::setw(16) << R14() << "     g:  " << cc_g() << "     C2: " << FPU_C2() << '\n';
		ostr << "R15: " << std::setw(16) << R15() << "     ge: " << cc_ge() << "     C3: " << FPU_C3() << '\n';

		return ostr;
	}
	std::ostream &Computer::WriteVPUDebugString(std::ostream &ostr)
	{
		// clear the width field and set up a format restore point
		ostr.width(0);
		iosfrstor _frstor(ostr);

		ostr << std::setfill('0');

		ostr << '\n';
		for (int i = 0; i < 32; ++i)
		{
			ostr << "ZMM" << std::dec << i << ": ";
			if (i < 10) ostr << ' ';

			ostr << std::hex;
			for (int j = 7; j >= 0; --j) ostr << std::setw(16) << ZMMRegisters[i].get<u64>(j) << ' ';
			ostr << '\n';
		}

		return ostr;
	}
	std::ostream &Computer::WriteFullDebugString(std::ostream &ostr)
	{
		WriteCPUDebugString(ostr);
		WriteVPUDebugString(ostr);

		return ostr;
	}
}
