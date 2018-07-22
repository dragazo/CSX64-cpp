#ifndef CSX64_COMPUTER_H
#define CSX64_COMPUTER_H

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <random>
#include <string>
#include <iostream>
#include <iomanip>
#include <memory>
#include <cfenv>
#include <cmath>

#include "CoreTypes.h"
#include "ExeTypes.h"
#include "Utility.h"

namespace CSX64
{
	class Computer
	{
	public:
		static const u64 Version = 0x0500;

		static const int FDCount = 16;

	//protected:

		std::vector<u8> Memory;
		FileDescriptor FileDescriptors[FDCount];

		std::default_random_engine Rand;

		// ----------------------------------------

		/// <summary>
		/// The maximum amount of memory the client can request
		/// </summary>
		u64 MaxMemory = 2 * 1024 * 1024 * 1024;
		/// <summary>
		/// Gets the amount of memory (in bytes) the computer currently has access to
		/// </summary>
		u64 MemorySize() const { return Memory.size(); }
		/// <summary>
		/// Gets the amount of memory (in bytes) the computer initially had access to
		/// </summary>
		u64 InitMemorySize;

		/// <summary>
		/// Flag marking if the program is still executing (still true even in halted state)
		/// </summary>
		bool Running;
		/// <summary>
			/// Gets if the processor is awaiting data from an interactive stream
			/// </summary>
		bool SuspendedRead;
		/// <summary>
			/// Gets the current error code
			/// </summary>
		ErrorCode Error;
		/// <summary>
			/// The return value from the program after errorless termination
			/// </summary>
		int ReturnValue;

		/// <summary>
		/// The barrier before which memory is executable
		/// </summary>
		u64 ExeBarrier;
		/// <summary>
			/// The barrier before which memory is read-only
			/// </summary>
		u64 ReadonlyBarrier;
		/// <summary>
			/// Gets the barrier before which the stack can't enter
			/// </summary>
		u64 StackBarrier;

	public:

		/// <summary>
		/// Validates the machine for operation, but does not prepare it for execute (see Initialize)
		/// </summary>
		Computer()
		{
			// define initial state
			Running = false;
			Error = ErrorCode::None;
		}
		~Computer()
		{
			CloseFiles();
		}

		/// <summary>
		/// Initializes the computer for execution
		/// </summary>
		/// <param name="exe">the memory to load before starting execution (memory beyond this range is undefined)</param>
		/// <param name="args">the command line arguments to provide to the computer. pass null or empty array for none</param>
		/// <param name="stacksize">the amount of additional space to allocate for the program's stack</param>
		bool Initialize(std::vector<u8> &exe, std::vector<std::string> args, u64 stacksize = 2 * 1024 * 1024);

		/// <summary>
		/// Causes the machine to end execution with an error code and release various system resources (e.g. file handles).
		/// </summary>
		/// <param name="err">the error code to emit</param>
		void Terminate(ErrorCode err = ErrorCode::None)
		{
			// only do this if we're currently running (so we don't override what error caused the initial termination)
			if (Running)
			{
				// set error and stop execution
				Error = err;
				Running = false;

				CloseFiles(); // close all the file descriptors
			}
		}
		/// <summary>
		/// Causes the machine to end execution with a return value and release various system resources (e.g. file handles).
		/// </summary>
		/// <param name="ret">the program return value to emit</param>
		void Exit(int ret = 0)
		{
			// only do this if we're currently running
			if (Running)
			{
				// set return value and stop execution
				ReturnValue = ret;
				Running = false;

				CloseFiles(); // close all the file descriptors
			}
		}

		/// <summary>
		/// Unsets the suspended read state
		/// </summary>
		void ResumeSuspendedRead()
		{
			if (Running) SuspendedRead = false;
		}

		/// <summary>
		/// Gets the file descriptor at the specified index. (no bounds checking)
		/// </summary>
		/// <param name="index">the index of the file descriptor</param>
		FileDescriptor &GetFD(int index) { return FileDescriptors[index]; }
		/// <summary>
		/// Finds the first available file descriptor, or null if there are none available
		/// </summary>
		/// <param name="index">the index of the result - only modified on success - null is ignored</param>
		FileDescriptor *FindAvailableFD(int *index)
		{
			// get the first available file descriptor
			for (int i = 0; i < FDCount; ++i)
				if (!FileDescriptors[i].InUse())
				{
					if (index) *index = i;
					return &FileDescriptors[i];
				}

			return nullptr;
		}

		/// <summary>
		/// Closes all the managed file descriptors and severs ties to unmanaged file descriptors.
		/// </summary>
		void CloseFiles()
		{
			// close all files
			for (int i = 0; i < FDCount; ++i)
				FileDescriptors[i].Close();
		}

		/// <summary>
		/// Handles syscall instructions from the processor. Returns true iff the syscall was handled successfully.
		/// Should not be called directly: only by interpreted syscall instructions
		/// </summary>
		virtual bool Syscall()
		{
			switch ((SyscallCode)RAX())
			{
			case SyscallCode::Exit: Exit(RBX()); return true;

				/*
			case SyscallCode::Read: return Sys_Read();
			case SyscallCode::Write: return Sys_Write();

			case SyscallCode::Open: return Sys_Open();
			case SyscallCode::Close: return Sys_Close();

			case SyscallCode::Flush: return Sys_Flush();

			case SyscallCode::Seek: return Sys_Seek();
			case SyscallCode::Tell: return Sys_Tell();

			case SyscallCode::Move: return Sys_Move();
			case SyscallCode::Remove: return Sys_Remove();
			case SyscallCode::Mkdir: return Sys_Mkdir();
			case SyscallCode::Rmdir: return Sys_Rmdir();

			case SyscallCode::Brk: return Sys_Brk();
				*/
				// ----------------------------------

				// otherwise syscall not found
			default: Terminate(ErrorCode::UnhandledSyscall); return false;
			}
		}

		/// <summary>
		/// Performs a single operation. Returns the number of successful operations.
		/// Returning a lower number than requested (even zero) does not necessarily indicate termination or error.
		/// To check for termination/error, use <see cref="Running"/>.
		/// </summary>
		/// <param name="count">The maximum number of operations to perform</param>
		u64 Tick(u64 count);

		// FINISH THIS LATER

		// Writes a string containing all non-vpu register/flag states"/>
		std::ostream &WriteCPUDebugString(std::ostream &ostr)
		{
			ostr << std::hex << std::setfill('0');

			ostr << "RAX: " << std::setw(16) << RAX() << "     CF: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "RBX: " << std::setw(16) << RBX() << "     PF: " << PF() << "     RIP:    " << std::setw(16) << RIP() << '\n';
			ostr << "RCX: " << std::setw(16) << RCX() << "     AF: " << AF() << '\n';
			ostr << "RDX: " << std::setw(16) << RDX() << "     ZF: " << ZF() << "     ST0: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "RSI: " << std::setw(16) << RSI() << "     SF: " << SF() << "     ST1: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "RDI: " << std::setw(16) << RDI() << "     OF: " << OF() << "     ST2: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "RBP: " << std::setw(16) << RBP() << "               ST3: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "RSP: " << std::setw(16) << RSP() << "     b:  " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R8:  " << std::setw(16) << R8() << "     be: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R9:  " << std::setw(16) << R9() << "     a:  " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R10: " << std::setw(16) << R10() << "    ae: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R11: " << std::setw(16) << R11() << "         " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R12: " << std::setw(16) << R12() << "     l:  " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R13: " << std::setw(16) << R13() << "     le: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R14: " << std::setw(16) << R14() << "     g:  " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
			ostr << "R15: " << std::setw(16) << R15() << "     ge: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';

			return ostr;
		}
		// Writes a string containing all vpu register states
		std::ostream &WriteVPUDebugString(std::ostream &ostr)
		{
			ostr << std::hex << std::setfill('0');

			for (int i = 0; i < 32; ++i)
			{
				ostr << "ZMM" << i << ": ";
				if (i < 10) ostr << ' ';

				for (int j = 7; j >= 0; --j) ostr << std::setw(16) << ZMMRegisters[i].int64(j);

				ostr << '\n';
			}

			return ostr;
		}
		// Writes a string containing both <see cref="GetCPUDebugString"/> and <see cref="GetVPUDebugString"/>
		std::ostream &WriteFullDebugString(std::ostream &ostr)
		{
			WriteCPUDebugString(ostr);
			ostr << '\n';
			WriteVPUDebugString(ostr);

			return ostr;
		}

	protected:
		CPURegister CPURegisters[16];

	public:
		// -- public memory utilities -- //

		/// <summary>
		/// Reads a C-style string from memory. Returns true if successful, otherwise fails with OutOfBounds and returns false
		/// </summary>
		/// <param name="pos">the address in memory of the first character in the string</param>
		/// <param name="charsize">the size of each character in bytes</param>
		/// <param name="str">the resulting string</param>
		/// <param name="_abide_slow">if the memory access should abide by SMF. only pass false if it makes sense, otherwise slow should be slow</param>
		bool GetCString(u64 pos, std::string &str)
		{
			// refer to utility function
			if (!ReadCString(Memory, pos, str)) { Terminate(ErrorCode::OutOfBounds); return false; }

			return true;
		}
		/// <summary>
		/// Writes a C-style string to memory. Returns true if successful, otherwise fails with OutOfBounds and returns false
		/// </summary>
		/// <param name="pos">the address in memory of the first character to write</param>
		/// <param name="charsize">the size of each character in bytes</param>
		/// <param name="str">the string to write</param>
		/// <param name="_abide_slow">if the memory access should abide by SMF. only pass false if it makes sense, otherwise slow should be slow</param>
		bool SetCString(u64 pos, const std::string &str)
		{
			// make sure we're not in the readonly segment
			if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

			// refer to utility function
			if (!WriteCString(Memory, pos, str)) { Terminate(ErrorCode::OutOfBounds); return false; }

			return true;
		}

		template<typename T>
		bool GetMem(u64 pos, T &val)
		{
			// refer to utility function
			if (!Read(Memory, pos, val)) { Terminate(ErrorCode::OutOfBounds); return false; }
			return true;
		}
		template<typename T>
		bool SetMem(u64 pos, const T &val)
		{
			// refer to utility function
			if (!Write(Memory, pos, val)) { Terminate(ErrorCode::OutOfBounds); return false; }
			return true;
		}

		template<typename T>
		bool Pop(T &val)
		{
			// refer to utility function
			if (!Read(Memory, RSP(), val)) { Terminate(ErrorCode::OutOfBounds); return false; }
			RSP() += sizeof(T);
			return true;
		}
		template<typename T>
		bool Push(const T &val)
		{
			// refer to utility function
			RSP() -= sizeof(T);
			if (!Write(Memory, RSP(), val)) { Terminate(ErrorCode::OutOfBounds); return false; }
			return true;
		}

	private: // -- private memory utilities -- //

		/// <summary>
	/// Pushes a value onto the stack
	/// </summary>
	/// <param name="size">the size of the value (in bytes)</param>
	/// <param name="val">the value to push</param>
		bool PushRaw(u64 size, u64 val)
		{
			RSP() -= size;

			// do the stack barrier test
			if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }

			return SetMemRaw(RSP(), size, val);
		}
		/// <summary>
		/// Pops a value from the stack
		/// </summary>
		/// <param name="size">the size of the value (in bytes)</param>
		/// <param name="val">the resulting value</param>
		bool PopRaw(u64 size, u64 &val)
		{
			// do stack barrier test before increment (makes sure the value we're fetching was in the stack segment)
			if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }

			if (!GetMemRaw(RSP(), size, val)) return false;
			RSP() += size;

			return true;
		}

		/// <summary>
		/// Reads a value from memory (fails with OutOfBounds if invalid). if SMF is set, delays 
		/// </summary>
		/// <param name="pos">Address to read</param>
		/// <param name="size">Number of bytes to read</param>
		/// <param name="res">The result</param>
		bool GetMemRaw(u64 pos, u64 size, u64 &res)
		{
			// refer to utility function
			if (!Read(Memory, pos, size, res)) { Terminate(ErrorCode::OutOfBounds); return false; }
			return true;
		}
		/// <summary>
		/// Writes a value to memory (fails with OutOfBounds if invalid)
		/// </summary>
		/// <param name="pos">Address to write</param>
		/// <param name="size">Number of bytes to write</param>
		/// <param name="val">The value to write</param>
		bool SetMemRaw(u64 pos, u64 size, u64 val)
		{
			// make sure we're not in the readonly segment
			if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

			// refer to utility function
			if (!Write(Memory, pos, size, val)) { Terminate(ErrorCode::OutOfBounds); return false; }
			return true;
		}

		/// <summary>
		/// Gets a value at and advances the execution pointer (fails with OutOfBounds if invalid)
		/// </summary>
		/// <param name="size">Number of bytes to read</param>
		/// <param name="res">The result</param>
		bool GetMemAdv(u64 size, u64 &res)
		{
			// make sure we can get the memory
			if (!GetMemRaw(RIP(), size, res)) return false;
			RIP() += size;
			return true;
		}
		/// <summary>
		/// Gets an address and advances the execution pointer
		/// </summary>
		/// <param name="res">resulting address</param>
		bool GetAddressAdv(u64 &res)
		{
			// [1: imm][1:][2: mult_1][2: size][1: r1][1: r2]   ([4: r1][4: r2])   ([size: imm])

			u64 settings, sizecode, regs;
			res = 0; // initialize res - functions as imm parsing location, so it has to start at 0

			// get the settings byte and regs byte if applicable
			if (!GetMemAdv(1, settings) || (settings & 3) != 0 && !GetMemAdv(1, regs)) return false;

			// get the sizecode
			sizecode = (settings >> 2) & 3;
			// 8-bit addressing is not allowed
			if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

			// get the imm if applicable - store into res
			if ((settings & 0x80) != 0 && !GetMemAdv(Size(sizecode), res)) return false;

			// if r1 was used, add that pre-multiplied by the multiplier
			if ((settings & 2) != 0) res += CPURegisters[regs >> 4][sizecode] << ((settings >> 4) & 3);
			// if r2 was used, add that
			if ((settings & 1) != 0) res += CPURegisters[regs & 15][sizecode];

			// got an address
			return true;
		}

	protected:

		long double FPURegisters[8];
		u16 FPU_control, FPU_status, FPU_tag;

		ZMMRegister ZMMRegisters[32];

		u64 _RFLAGS, _RIP;

	public:

		u64 &RFLAGS() { return _RFLAGS; }
		std::uint32_t &EFLAGS() { return *(std::uint32_t*)&_RFLAGS; }
		u16 &FLAGS() { return *(u16*)&_RFLAGS; }

		u64 &RIP() { return _RIP; }
		ReferenceRouter<std::uint32_t, u64> EIP() { return {_RIP}; }
		ReferenceRouter<u16, u64> IP() { return {_RIP}; }

		u64 &RAX() { return CPURegisters[0].x64(); }
		u64 &RBX() { return CPURegisters[1].x64(); }
		u64 &RCX() { return CPURegisters[2].x64(); }
		u64 &RDX() { return CPURegisters[3].x64(); }
		u64 &RSI() { return CPURegisters[4].x64(); }
		u64 &RDI() { return CPURegisters[5].x64(); }
		u64 &RBP() { return CPURegisters[6].x64(); }
		u64 &RSP() { return CPURegisters[7].x64(); }
		u64 &R8() { return CPURegisters[8].x64(); }
		u64 &R9() { return CPURegisters[9].x64(); }
		u64 &R10() { return CPURegisters[10].x64(); }
		u64 &R11() { return CPURegisters[11].x64(); }
		u64 &R12() { return CPURegisters[12].x64(); }
		u64 &R13() { return CPURegisters[13].x64(); }
		u64 &R14() { return CPURegisters[14].x64(); }
		u64 &R15() { return CPURegisters[15].x64(); }

		auto EAX() { return CPURegisters[0].x32(); }
		auto EBX() { return CPURegisters[1].x32(); }
		auto ECX() { return CPURegisters[2].x32(); }
		auto EDX() { return CPURegisters[3].x32(); }
		auto ESI() { return CPURegisters[4].x32(); }
		auto EDI() { return CPURegisters[5].x32(); }
		auto EBP() { return CPURegisters[6].x32(); }
		auto ESP() { return CPURegisters[7].x32(); }
		auto R8D() { return CPURegisters[8].x32(); }
		auto R9D() { return CPURegisters[9].x32(); }
		auto R10D() { return CPURegisters[10].x32(); }
		auto R11D() { return CPURegisters[11].x32(); }
		auto R12D() { return CPURegisters[12].x32(); }
		auto R13D() { return CPURegisters[13].x32(); }
		auto R14D() { return CPURegisters[14].x32(); }
		auto R15D() { return CPURegisters[15].x32(); }

		u16 &AX() { return CPURegisters[0].x16(); }
		u16 &BX() { return CPURegisters[1].x16(); }
		u16 &CX() { return CPURegisters[2].x16(); }
		u16 &DX() { return CPURegisters[3].x16(); }
		u16 &SI() { return CPURegisters[4].x16(); }
		u16 &DI() { return CPURegisters[5].x16(); }
		u16 &BP() { return CPURegisters[6].x16(); }
		u16 &SP() { return CPURegisters[7].x16(); }
		u16 &R8W() { return CPURegisters[8].x16(); }
		u16 &R9W() { return CPURegisters[9].x16(); }
		u16 &R10W() { return CPURegisters[10].x16(); }
		u16 &R11W() { return CPURegisters[11].x16(); }
		u16 &R12W() { return CPURegisters[12].x16(); }
		u16 &R13W() { return CPURegisters[13].x16(); }
		u16 &R14W() { return CPURegisters[14].x16(); }
		u16 &R15W() { return CPURegisters[15].x16(); }

		u8 &AL() { return CPURegisters[0].x8(); }
		u8 &BL() { return CPURegisters[1].x8(); }
		u8 &CL() { return CPURegisters[2].x8(); }
		u8 &DL() { return CPURegisters[3].x8(); }
		u8 &SIL() { return CPURegisters[4].x8(); }
		u8 &DIL() { return CPURegisters[5].x8(); }
		u8 &BPL() { return CPURegisters[6].x8(); }
		u8 &SPL() { return CPURegisters[7].x8(); }
		u8 &R8B() { return CPURegisters[8].x8(); }
		u8 &R9B() { return CPURegisters[9].x8(); }
		u8 &R10B() { return CPURegisters[10].x8(); }
		u8 &R11B() { return CPURegisters[11].x8(); }
		u8 &R12B() { return CPURegisters[12].x8(); }
		u8 &R13B() { return CPURegisters[13].x8(); }
		u8 &R14B() { return CPURegisters[14].x8(); }
		u8 &R15B() { return CPURegisters[15].x8(); }

		u8 &AH() { return CPURegisters[0].x8h(); }
		u8 &BH() { return CPURegisters[1].x8h(); }
		u8 &CH() { return CPURegisters[2].x8h(); }
		u8 &DH() { return CPURegisters[3].x8h(); }

		// source: https://en.wikipedia.org/wiki/FLAGS_register
		// source: http://www.eecg.toronto.edu/~amza/www.mindsec.com/files/x86regs.html

		FlagWrapper<u64, 0> CF() { return {_RFLAGS}; }
		FlagWrapper<u64, 2> PF() { return {_RFLAGS}; }
		FlagWrapper<u64, 4> AF() { return {_RFLAGS}; }
		FlagWrapper<u64, 6> ZF() { return {_RFLAGS}; }
		FlagWrapper<u64, 7> SF() { return {_RFLAGS}; }
		FlagWrapper<u64, 8> TF() { return {_RFLAGS}; }
		FlagWrapper<u64, 9> IF() { return {_RFLAGS}; }
		FlagWrapper<u64, 10> DF() { return {_RFLAGS}; }
		FlagWrapper<u64, 11> OF() { return {_RFLAGS}; }
		BitfieldWrapper<u64, 12, 2> IOPL() { return {_RFLAGS}; }
		FlagWrapper<u64, 14> NT() { return {_RFLAGS}; }

		FlagWrapper<u64, 16> RF() { return {_RFLAGS}; }
		FlagWrapper<u64, 17> VM() { return {_RFLAGS}; }
		FlagWrapper<u64, 18> AC() { return {_RFLAGS}; }
		FlagWrapper<u64, 19> VIF() { return {_RFLAGS}; }
		FlagWrapper<u64, 20> VIP() { return {_RFLAGS}; }
		FlagWrapper<u64, 21> ID() { return {_RFLAGS}; }

		FlagWrapper<u64, 32> FSF() { return {_RFLAGS}; }

		bool cc_b() { return CF(); }
		bool cc_be() { return CF() || ZF(); }
		bool cc_a() { return !CF() && !ZF(); }
		bool cc_ae() { return !CF(); }

		bool cc_l() { return SF() != OF(); }
		bool cc_le() { return ZF() || SF() != OF(); }
		bool cc_g() { return !ZF() && SF() == OF(); }
		bool cc_ge() { return SF() == OF(); }

		// source : http://www.website.masmforum.com/tutorials/fptute/fpuchap1.htm

		FlagWrapper<u16, 0> FPU_IM() { return {FPU_control}; }
		FlagWrapper<u16, 1> FPU_DM() { return {FPU_control}; }
		FlagWrapper<u16, 2> FPU_ZM() { return {FPU_control}; }
		FlagWrapper<u16, 3> FPU_OM() { return {FPU_control}; }
		FlagWrapper<u16, 4> FPU_UM() { return {FPU_control}; }
		FlagWrapper<u16, 5> FPU_PM() { return {FPU_control}; }
		FlagWrapper<u16, 7> FPU_IEM() { return {FPU_control}; }
		BitfieldWrapper<u16, 8, 2> FPU_PC() { return {FPU_control}; }
		BitfieldWrapper<u16, 10, 2> FPU_RC() { return {FPU_control}; }
		FlagWrapper<u16, 12> FPU_IC() { return {FPU_control}; }

		FlagWrapper<u16, 0> FPU_I() { return {FPU_status}; }
		FlagWrapper<u16, 1> FPU_D() { return {FPU_status}; }
		FlagWrapper<u16, 2> FPU_Z() { return {FPU_status}; }
		FlagWrapper<u16, 3> FPU_O() { return {FPU_status}; }
		FlagWrapper<u16, 4> FPU_U() { return {FPU_status}; }
		FlagWrapper<u16, 5> FPU_P() { return {FPU_status}; }
		FlagWrapper<u16, 6> FPU_SF() { return {FPU_status}; }
		FlagWrapper<u16, 7> FPU_IR() { return {FPU_status}; }
		FlagWrapper<u16, 8> FPU_C0() { return {FPU_status}; }
		FlagWrapper<u16, 9> FPU_C1() { return {FPU_status}; }
		FlagWrapper<u16, 10> FPU_C2() { return {FPU_status}; }
		BitfieldWrapper<u16, 11, 3> FPU_TOP() { return {FPU_control}; }
		FlagWrapper<u16, 14> FPU_C3() { return {FPU_status}; }
		FlagWrapper<u16, 15> FPU_B() { return {FPU_status}; }

		static const int FPU_Tag_normal = 0;
		static const int FPU_Tag_zero = 1;
		static const int FPU_Tag_special = 2;
		static const int FPU_Tag_empty = 3;

		/// <summary>
		/// Gets the ST register's value
		/// </summary>
		/// <param name="num">the ordinal number of the ST register (0 for ST0)</param>
		long double ST(int num)
		{
			num = (FPU_TOP() + num) & 7;
			return FPURegisters[num];
		}
		/// <summary>
		/// Sets the ST register's value
		/// </summary>
		/// <param name="num">the ordinal number of the ST register (0 for ST0)</param>
		/// <param name="value">the value to set</param>
		void ST(int num, long double value)
		{
			num = (FPU_TOP() + num) & 7;
			// store value, accounting for precision control flag
			switch (FPU_PC())
			{
			case 0: FPURegisters[num] = (float)value;
			case 2: FPURegisters[num] = (double)value;
			default: FPURegisters[num] = value;
			}
			FPU_tag = (FPU_tag & ~(3 << (num * 2))) | (ComputeFPUTag(value) << (num * 2));
		}
		/// <summary>
		/// Gets the ST register's tag
		/// </summary>
		/// <param name="num">the ordinal number of the ST register (0 for ST0)</param>
		int ST_Tag(int num)
		{
			num = (FPU_TOP() + num) & 7;
			return (FPU_tag >> (num * 2)) & 3;
		}
		/// <summary>
		/// Sets the ST register's tag to <see cref="FPU_Tag_empty"/>
		/// </summary>
		/// <param name="num">the ordinal number of the ST register (0 for ST0)</param>
		void ST_Free(int num)
		{
			num = (FPU_TOP() + num) & 7;
			FPU_tag = FPU_tag | (3 << (num * 2));
		}

		private: // -- operators -- //


				 /*
				 [4: dest][2: size][1:dh][1: mem]   [size: imm]
				 mem = 0: [1: sh][3:][4: src]
				 dest <- f(reg, imm)
				 mem = 1: [address]
				 dest <- f(M[address], imm)
				 (dh and sh mark AH, BH, CH, or DH for dest or src)
				 */
			bool FetchTernaryOpFormat(u64 &s, u64 &a, u64 &b)
			{
				if (!GetMemAdv(1, s)) { a = b = 0; return false; }
				u64 sizecode = (s >> 2) & 3;

				// make sure dest will be valid for storing (high flag)
				if ((s & 2) != 0 && ((s & 0xc0) != 0 || sizecode != 0)) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				// get b (imm)
				if (!GetMemAdv(Size(sizecode), b)) { a = 0; return false; }

				// get a (reg or mem)
				if ((s & 1) == 0)
				{
					if (!GetMemAdv(1, a)) return false;
					if ((a & 128) != 0)
					{
						if ((a & 0x0c) != 0 || sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						a = CPURegisters[a & 15].x8h();
					}
					else a = CPURegisters[a & 15][sizecode];
					return true;
				}
				else return GetAddressAdv(a) && GetMemRaw(a, Size(sizecode), a);
			}
			bool StoreTernaryOPFormat(u64 s, u64 res)
			{
				if ((s & 2) != 0) CPURegisters[s >> 4].x8h() = res;
				else CPURegisters[s >> 4][(s >> 2) & 3] = res;
				return true;
			}

			/*
			[4: dest][2: size][1:dh][1: sh]   [4: mode][4: src]
			Mode = 0:                           dest <- f(dest, src)
			Mode = 1: [size: imm]               dest <- f(dest, imm)
			Mode = 2: [address]                 dest <- f(dest, M[address])
			Mode = 3: [address]                 M[address] <- f(M[address], src)
			Mode = 4: [address]   [size: imm]   M[address] <- f(M[address], imm)
			Else UND
			(dh and sh mark AH, BH, CH, or DH for dest or src)
			*/
			bool FetchBinaryOpFormat(u64 &s1, u64 &s2, u64 &m, u64 &a, u64 &b,
				bool get_a = true, int _a_sizecode = -1, int _b_sizecode = -1, bool allow_b_mem = true)
			{
				// read settings
				if (!GetMemAdv(1, s1) || !GetMemAdv(1, s2)) { s2 = 0; return false; }

				// if they requested an explicit size for a, change it in the settings byte
				if (_a_sizecode != -1) s1 = (s1 & 0xf3) | ((u64)_a_sizecode << 2);

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
						// make sure we're in registers 0-3 and 8-bit mode
						if ((s1 & 0xc0) != 0 || a_sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						if (get_a) a = CPURegisters[s1 >> 4].x8h();
					}
					else if (get_a) a = CPURegisters[s1 >> 4][a_sizecode];
					// if sh is flagged
					if ((s1 & 1) != 0)
					{
						// make sure we're in registers 0-3 and 8-bit mode
						if ((s2 & 0x0c) != 0 || b_sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						b = CPURegisters[s2 & 15].x8h();
					}
					else b = CPURegisters[s2 & 15][b_sizecode];
					return true;

				case 1:
					// if dh is flagged
					if ((s1 & 2) != 0)
					{
						// make sure we're in registers 0-3 and 8-bit mode
						if ((s1 & 0xc0) != 0 || a_sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						if (get_a) a = CPURegisters[s1 >> 4].x8h();
					}
					else if (get_a) a = CPURegisters[s1 >> 4][a_sizecode];
					// get imm
					return GetMemAdv(Size(b_sizecode), b);

				case 2:
					// handle allow_b_mem case
					if (!allow_b_mem) { Terminate(ErrorCode::UndefinedBehavior); return false; }

					// if dh is flagged
					if ((s1 & 2) != 0)
					{
						// make sure we're in registers 0-3 and 8-bit mode
						if ((s1 & 0xc0) != 0 || a_sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						if (get_a) a = CPURegisters[s1 >> 4].x8h();
					}
					else if (get_a) a = CPURegisters[s1 >> 4][a_sizecode];
					// get mem
					return GetAddressAdv(m) && GetMemRaw(m, Size(b_sizecode), b);

				case 3:
					// get mem
					if (!GetAddressAdv(m) || get_a && !GetMemRaw(m, Size(a_sizecode), a)) return false;
					// if sh is flagged
					if ((s1 & 1) != 0)
					{
						// make sure we're in registers 0-3 and 8-bit mode
						if ((s2 & 0x0c) != 0 || b_sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						b = CPURegisters[s2 & 15].x8h();
					}
					else b = CPURegisters[s2 & 15][b_sizecode];
					return true;

				case 4:
					// get mem
					if (!GetAddressAdv(m) || get_a && !GetMemRaw(m, Size(a_sizecode), a)) return false;
					// get imm
					return GetMemAdv(Size(b_sizecode), b);

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
			}
			bool StoreBinaryOpFormat(u64 s1, u64 s2, u64 m, u64 res)
			{
				u64 sizecode = (s1 >> 2) & 3;

				// switch through mode
				switch (s2 >> 4)
				{
				case 0:
				case 1:
				case 2:
					if ((s1 & 2) != 0) CPURegisters[s1 >> 4].x8h() = res;
					else CPURegisters[s1 >> 4][sizecode] = res;
					return true;

				case 3:
				case 4:
					return SetMemRaw(m, Size(sizecode), res);

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
			}

			/*
			[4: dest][2: size][1: dh][1: mem]
			mem = 0:             dest <- f(dest)
			mem = 1: [address]   M[address] <- f(M[address])
			(dh marks AH, BH, CH, or DH for dest)
			*/
			bool FetchUnaryOpFormat(u64 &s, u64 &m, u64 &a, bool get_a = true, int _a_sizecode = -1)
			{
				// read settings
				if (!GetMemAdv(1, s)) return false;

				// if they requested an explicit size for a, change it in the settings byte
				if (_a_sizecode != -1) s = (s & 0xf3) | ((u64)_a_sizecode << 2);

				u64 a_sizecode = (s >> 2) & 3;

				// switch through mode
				switch (s & 1)
				{
				case 0:
					// if h is flagged
					if ((s & 2) != 0)
					{
						// make sure we're in registers 0-3 and 8-bit mode
						if ((s & 0xc0) != 0 || a_sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						if (get_a) a = CPURegisters[s >> 4].x8h();
					}
					else if (get_a) a = CPURegisters[s >> 4][a_sizecode];
					return true;

				case 1:
					return GetAddressAdv(m) && (!get_a || GetMemRaw(m, Size(a_sizecode), a));

				default: return true; // this should never happen but compiler is complainy
				}
			}
			bool StoreUnaryOpFormat(u64 s, u64 m, u64 res)
			{
				u64 sizecode = (s >> 2) & 3;

				// switch through mode
				switch (s & 1)
				{
				case 0:
					if ((s & 2) != 0) CPURegisters[s >> 4].x8h() = res;
					else CPURegisters[s >> 4][sizecode] = res;
					return true;

				case 1:
					return SetMemRaw(m, Size(sizecode), res);

				default: return true; // this can't happen but compiler is stupid
				}
			}

			/*
			[4: dest][2: size][1: dh][1: mem]   [1: CL][1:][6: count]   ([address])
			*/
			bool FetchShiftOpFormat(u64 &s, u64 &m, u64 &val, u64 &count)
			{
				// read settings byte
				if (!GetMemAdv(1, s) || !GetMemAdv(1, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				// if count set CL flag, replace it with that
				if ((count & 0x80) != 0) count = CL();
				// mask count
				count = count & (sizecode == 3 ? 0x3ful : 0x1ful);

				// if dest is a register
				if ((s & 1) == 0)
				{
					// if high flag set
					if ((s & 2) != 0)
					{
						// need to be in (ABCD)H
						if ((s & 0xc0) != 0 || sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						val = CPURegisters[s >> 4].x8h();
					}
					else val = CPURegisters[s >> 4][sizecode];

					return true;
				}
				// otherwise is memory value
				else return GetAddressAdv(m) && GetMemRaw(m, Size(sizecode), val);
			}
			bool StoreShiftOpFormat(u64 s, u64 m, u64 res)
			{
				u64 sizecode = (s >> 2) & 3;

				// if dest is a register
				if ((s & 1) == 0)
				{
					// if high flag set
					if ((s & 2) != 0) CPURegisters[s >> 4].x8h() = res;
					else CPURegisters[s >> 4][sizecode] = res;

					return true;
				}
				// otherwise dest is memory
				else return SetMemRaw(m, Size(sizecode), res);
			}

			/*
			[4: reg][2: size][2: mode]
			mode = 0:               reg
			mode = 1:               h reg (AH, BH, CH, or DH)
			mode = 2: [size: imm]   imm
			mode = 3: [address]     M[address]
			*/
			bool FetchIMMRMFormat(u64 &s, u64 &a, int _a_sizecode = -1)
			{
				if (!GetMemAdv(1, s)) return false;

				u64 a_sizecode = _a_sizecode == -1 ? (s >> 2) & 3 : (u64)_a_sizecode;

				// get the value into b
				switch (s & 3)
				{
				case 0:
					a = CPURegisters[s >> 4][a_sizecode];
					return true;

				case 1:
					if ((s & 0xc0) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
					a = CPURegisters[s >> 4].x8h();
					return true;

				case 2: return GetMemAdv(Size(a_sizecode), a);

				case 3: return GetAddressAdv(a) && GetMemRaw(a, Size(a_sizecode), a);
				}

				return true;
			}

			/*
			[4: dest][2: size][1: dh][1: mem]   [1: src_1_h][3:][4: src_1]
			mem = 0: [1: src_2_h][3:][4: src_2]
			mem = 1: [address_src_2]
			*/
			bool FetchRR_RMFormat(u64 &s1, u64 &s2, u64 &dest, u64 &a, u64 &b)
			{
				if (!GetMemAdv(1, s1) || !GetMemAdv(1, s2)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				// if dest is high
				if ((s1 & 2) != 0)
				{
					if (sizecode != 0 || (s1 & 0xc0) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
					dest = CPURegisters[s1 >> 4].x8h();
				}
				else dest = CPURegisters[s1 >> 4][sizecode];

				// if a is high
				if ((s2 & 128) != 0)
				{
					if (sizecode != 0 || (s2 & 0x0c) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
					a = CPURegisters[s2 & 15].x8h();
				}
				else a = CPURegisters[s2 & 15][sizecode];

				// if b is register
				if ((s1 & 1) == 0)
				{
					if (!GetMemAdv(1, b)) return false;

					// if b is high
					if ((b & 128) != 0)
					{
						if (sizecode != 0 || (b & 0x0c) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						b = CPURegisters[b & 15].x8h();
					}
					else b = CPURegisters[b & 15][sizecode];
				}
				// otherwise b is memory
				else
				{
					if (!GetAddressAdv(b) || !GetMemRaw(b, Size(sizecode), b)) return false;
				}

				return true;
			}
			bool StoreRR_RMFormat(u64 s1, u64 res)
			{
				// if dest is high
				if ((s1 & 2) != 0) CPURegisters[s1 >> 4].x8h() = res;
				else CPURegisters[s1 >> 4][(s1 >> 2) & 3] = res;

				return true;
			}

			// updates the flags for integral ops (identical for most integral ops)
			void UpdateFlagsZSP(u64 value, u64 sizecode)
			{
				ZF() = value == 0;
				SF() = Negative(value, sizecode);

				// compute parity flag (only of low 8 bits)
				bool parity = true;
				for (u64 i = 128; i != 0; i >>= 1) if ((value & i) != 0) parity = !parity;
				PF() = parity;
			}

			// -- impl -- //

			const u64 ModifiableFlags = 0x003f0fd5ul;

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
			bool ProcessSTLDF()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;

				switch (ext)
				{
					// pushf
				case 0:
				case 1: // VM and RF flags are cleared in the stored image
				case 2:
					return PushRaw(Size(ext + 1), RFLAGS() & ~0x30000ul);

					// popf
				case 3:
				case 4: // can't modify reserved flags
				case 5:
					if (!PopRaw(Size(ext - 2), ext)) return false;
					RFLAGS() = (RFLAGS() & ~ModifiableFlags) | (ext & ModifiableFlags);
					return true;

					// sahf
				case 6: RFLAGS() = (RFLAGS() & ~ModifiableFlags) | (AH() & ModifiableFlags); return true;
					// lahf
				case 7: AH() = RFLAGS(); return true;

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
			bool ProcessFlagManip()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;

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

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
			bool ProcessSETcc()
			{
				u64 ext, s, m, _dest;
				if (!GetMemAdv(1, ext)) return false;
				if (!FetchUnaryOpFormat(s, m, _dest, false, 0)) return false;

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

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}

				return StoreUnaryOpFormat(s, m, flag);
			}

			bool ProcessMOV()
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b, false)) return false;

				return StoreBinaryOpFormat(s1, s2, m, b);
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
			bool ProcessMOVcc()
			{
				u64 ext, s1, s2, m, _dest, src;
				if (!GetMemAdv(1, ext)) return false;
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

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
			bool ProcessXCHG()
			{
				u64 a, b, temp_1, temp_2;

				if (!GetMemAdv(1, a)) return false;
				u64 sizecode = (a >> 2) & 3;

				// if a is high
				if ((a & 2) != 0)
				{
					if ((a & 0xc0) != 0 || sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
					temp_1 = CPURegisters[a >> 4].x8h();
				}
				else temp_1 = CPURegisters[a >> 4][sizecode];

				// if b is reg
				if ((a & 1) == 0)
				{
					if (!GetMemAdv(1, b)) return false;

					// if b is high
					if ((b & 128) != 0)
					{
						if ((b & 0x0c) != 0 || sizecode != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
						temp_2 = CPURegisters[b & 15].x8h();
						CPURegisters[b & 15].x8h() = temp_1;
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
					// get mem value into temp_2 (address in b)
					if (!GetAddressAdv(b) || !GetMemRaw(b, Size(sizecode), temp_2)) return false;
					// store b result
					if (!SetMemRaw(b, Size(sizecode), temp_1)) return false;
				}

				// store a's result (b's was handled internally above)
				if ((a & 2) != 0) CPURegisters[a >> 4].x8h() = temp_2;
				else CPURegisters[a >> 4][sizecode] = temp_2;

				return true;
			}

			bool ProcessJMP(u64 &aft)
			{
				u64 s, val;
				if (!FetchIMMRMFormat(s, val)) return false;
				u64 sizecode = (s >> 2) & 3;

				// 8-bit addressing not allowed
				if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				aft = RIP(); // record point immediately after reading (for CALL return address)
				RIP() = val; // jump

				return true;
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
			bool ProcessJcc()
			{
				u64 ext, s, val;
				if (!GetMemAdv(1, ext)) return false;
				if (!FetchIMMRMFormat(s, val)) return false;
				u64 sizecode = (s >> 2) & 3;

				// 8-bit addressing not allowed
				if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

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

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}

				if (flag) RIP() = val; // jump

				return true;
			}
			bool ProcessLOOPcc()
			{
				u64 ext, s, val;

				// get the cc continue flag
				bool continue_flag;
				if (!GetMemAdv(1, ext)) return false;
				switch (ext)
				{
				case 0: continue_flag = true; break; // LOOP
				case 1: continue_flag = ZF(); break;   // LOOPe
				case 2: continue_flag = !ZF(); break;  // LOOPne

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}

				if (!FetchIMMRMFormat(s, val)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 count;
				switch (sizecode)
				{
				case 3: count = --RCX(); break;
				case 2: count = --ECX(); break;
				case 1: count = --CX(); break;
				case 0: Terminate(ErrorCode::UndefinedBehavior); return false; // 8-bit not allowed

				default: return true; // this can't happen but compiler is stupid
				}

				if (count != 0 && continue_flag) RIP() = val; // jump if nonzero count and continue flag set

				return true;
			}

			bool ProcessPUSH()
			{
				u64 s, a;
				if (!FetchIMMRMFormat(s, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				// 8-bit push not allowed
				if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				return PushRaw(Size(sizecode), a);
			}
			/*
			[4: dest][2: size][1:][1: mem]
			mem = 0:             reg
			mem = 1: [address]   M[address]
			*/
			bool ProcessPOP()
			{
				u64 s, val;
				if (!GetMemAdv(1, s)) return false;
				u64 sizecode = (s >> 2) & 3;

				// 8-bit pop not allowed
				if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				// get the value
				if (!PopRaw(Size(sizecode), val)) return false;

				// if register
				if ((s & 1) == 0)
				{
					CPURegisters[s >> 4][sizecode] = val;
					return true;
				}
				// otherwise is memory
				else return GetAddressAdv(s) && SetMemRaw(s, Size(sizecode), val);
			}

			/*
			[4: dest][2: size][2:]   [address]
			dest <- address
			*/
			bool ProcessLEA()
			{
				u64 s, address;
				if (!GetMemAdv(1, s) || !GetAddressAdv(address)) return false;
				u64 sizecode = (s >> 2) & 3;

				// LEA doesn't allow 8-bit addressing
				if (sizecode == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				CPURegisters[s >> 4][sizecode] = address;
				return true;
			}

			bool ProcessADD()
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 res = Truncate(a + b, sizecode);

				UpdateFlagsZSP(res, sizecode);
				CF() = res < a;
				AF() = (res & 0xf) < (a & 0xf); // AF is just like CF but only the low nibble
				OF() = Positive(a, sizecode) == Positive(b, sizecode) && Positive(a, sizecode) != Positive(res, sizecode);

				return StoreBinaryOpFormat(s1, s2, m, res);
			}
			bool ProcessSUB(bool apply = true)
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 res = Truncate(a - b, sizecode);

				UpdateFlagsZSP(res, sizecode);
				CF() = a < b; // if a < b, a borrow was taken from the highest bit
				AF() = (a & 0xf) < (b & 0xf); // AF is just like CF but only the low nibble
				OF() = Positive(a, sizecode) != Positive(b, sizecode) && Positive(a, sizecode) != Positive(res, sizecode);

				return !apply || StoreBinaryOpFormat(s1, s2, m, res);
			}

			bool ProcessMUL_x()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;

				switch (ext)
				{
				case 0: return ProcessMUL();
				case 1: return ProcessMULX();

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
			}
			bool ProcessMUL()
			{
				u64 s, a, res;
				if (!FetchIMMRMFormat(s, a)) return false;

				// switch through register sizes
				switch ((s >> 2) & 3)
				{
				case 0:
					res = AL() * a;
					AX() = res;
					CF() = OF() = AH() != 0;
					break;
				case 1:
					res = AX() * a;
					DX() = (res >> 16); AX() = res;
					CF() = OF() = DX() != 0;
					break;
				case 2:
					res = EAX() * a;
					EDX() = (res >> 32); EAX() = res;
					CF() = OF() = EDX() != 0;
					break;
				case 3:
					UnsignedMul(RAX(), a, RDX(), RAX());
					CF() = OF() = RDX() != 0;
					break;
				}

				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return true;
			}
			bool ProcessMULX()
			{
				u64 s1, s2, dest, a, b, res;
				if (!FetchRR_RMFormat(s1, s2, dest, a, b)) return false;

				// switch through register sizes
				switch ((s1 >> 2) & 3)
				{
				case 2:
					res = a * b;
					CPURegisters[s1 >> 4].x32() = (res >> 32); CPURegisters[s2 & 15].x32() = res;
					break;
				case 3:
					UnsignedMul(a, b, CPURegisters[s1 >> 4].x64(), CPURegisters[s2 & 15].x64());
					break;

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}

				return true;
			}
			bool ProcessIMUL()
			{
				u64 mode;
				if (!GetMemAdv(1, mode)) return false;

				switch (mode)
				{
				case 0: return ProcessUnary_IMUL();
				case 1: return ProcessBinary_IMUL();
				case 2: return ProcessTernary_IMUL();

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
			}
			bool ProcessUnary_IMUL()
			{
				u64 s, _a;
				if (!FetchIMMRMFormat(s, _a)) return false;
				u64 sizecode = (s >> 2) & 3;

				// get val as sign extended
				i64 a = SignExtend(_a, sizecode);
				i64 res;

				// switch through register sizes
				switch (sizecode)
				{
				case 0:
					res = (i8)AL() * a;
					AX() = res;
					CF() = OF() = res != (i8)res;
					break;
				case 1:
					res = (i16)AX() * a;
					DX() = res >> 16; AX() = res;
					CF() = OF() = res != (i16)res;
					break;
				case 2:
					res = (i32)EAX() * a;
					EDX() = res >> 32; EAX() = res;
					CF() = OF() = res != (i32)res;
					break;
				case 3:
					SignedMul(RAX(), a, RDX(), RAX());
					CF() = OF() = !TruncGood_128_64(RDX(), RAX());
					break;
				}

				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return true;
			}
			bool ProcessBinary_IMUL()
			{
				u64 s1, s2, m, _a, _b;
				if (!FetchBinaryOpFormat(s1, s2, m, _a, _b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				// get vals as sign extended
				i64 a = SignExtend(_a, sizecode);
				i64 b = SignExtend(_b, sizecode);

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
					SignedMul(a, b, (u64&)a, (u64&)res);
					CF() = OF() = !TruncGood_128_64(a, res);
					break;
				}

				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return StoreBinaryOpFormat(s1, s2, m, (u64)res);
			}
			bool ProcessTernary_IMUL()
			{
				u64 s, _a, _b;
				if (!FetchTernaryOpFormat(s, _a, _b)) return false;
				u64 sizecode = (s >> 2) & 3;

				// get vals as sign extended
				i64 a = SignExtend(_a, sizecode);
				i64 b = SignExtend(_b, sizecode);

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
					SignedMul(a, b, (u64&)a, (u64&)res);
					CF() = OF() = !TruncGood_128_64(a, res);
					break;
				}

				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return StoreTernaryOPFormat(s, (u64)res);
			}

			bool ProcessDIV()
			{
				u64 s, a;
				if (!FetchIMMRMFormat(s, a)) return false;

				if (a == 0) { Terminate(ErrorCode::ArithmeticError); return false; }

				u64 full, quo, rem;

				// switch through register sizes
				switch ((s >> 2) & 3)
				{
				case 0:
					full = AX();
					quo = full / a; rem = full % a;
					if (quo > 0xff) { Terminate(ErrorCode::ArithmeticError); return false; }
					AL() = quo; AH() = rem;
					break;
				case 1:
					full = ((u64)DX() << 16) | AX();
					quo = full / a; rem = full % a;
					if (quo > 0xffff) { Terminate(ErrorCode::ArithmeticError); return false; }
					AX() = quo; DX() = rem;
					break;
				case 2:
					full = ((u64)EDX() << 32) | EAX();
					quo = full / a; rem = full % a;
					if (quo > 0xffffffff) { Terminate(ErrorCode::ArithmeticError); return false; }
					EAX() = quo; EDX() = rem;
					break;
				case 3:
					UnsignedDiv(RDX(), RAX(), a, full, quo, rem);
					if (full != 0) { Terminate(ErrorCode::ArithmeticError); return false; }
					RAX() = quo; RDX() = rem;
					break;
				}

				CF() = Rand() & 1;
				OF() = Rand() & 1;
				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return true;
			}
			bool ProcessIDIV()
			{
				u64 s, _a;
				if (!FetchIMMRMFormat(s, _a)) return false;
				u64 sizecode = (s >> 2) & 3;

				if (_a == 0) { Terminate(ErrorCode::ArithmeticError); return false; }

				// get val as signed
				i64 a = (i64)SignExtend(_a, sizecode);

				i64 full, quo, rem;

				// switch through register sizes
				switch ((s >> 2) & 3)
				{
				case 0:
					full = (i16)AX();
					quo = full / a; rem = full % a;
					if (quo != (i8)quo) { Terminate(ErrorCode::ArithmeticError); return false; }
					AL() = quo; AH() = rem;
					break;
				case 1:
					full = ((i32)DX() << 16) | AX();
					quo = full / a; rem = full % a;
					if (quo != (i16)quo) { Terminate(ErrorCode::ArithmeticError); return false; }
					AX() = quo; DX() = rem;
					break;
				case 2:
					full = ((i64)EDX() << 32) | EAX();
					quo = full / a; rem = full % a;
					if (quo != (i32)quo) { Terminate(ErrorCode::ArithmeticError); return false; }
					EAX() = quo; EDX() = rem;
					break;
				case 3:
					SignedDiv(RDX(), RAX(), a, (u64&)full, (u64&)quo, (u64&)rem);
					if (!TruncGood_128_64((u64&)full, (u64&)quo)) { Terminate(ErrorCode::ArithmeticError); return false; }
					RAX() = quo; RDX() = rem;
					break;
				}

				CF() = Rand() & 1;
				OF() = Rand() & 1;
				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return true;
			}

			bool ProcessSHL()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = Truncate(val << count, sizecode);

					UpdateFlagsZSP(res, sizecode);
					CF() = count < SizeBits(sizecode) ? ((val >> (SizeBits(sizecode) - count)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
					OF() = count == 1 ? Negative(res, sizecode) != CF() : Rand() & 1; // OF is 1 if top 2 bits of original value were different (UND for sh != 1)
					AF() = Rand() & 1; // AF is undefined

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}
			bool ProcessSHR()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = val >> count;

					UpdateFlagsZSP(res, sizecode);
					CF() = count < SizeBits(sizecode) ? ((val >> (count - 1)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
					OF() = count == 1 ? Negative(val, sizecode) : Rand() & 1; // OF is high bit of original value (UND for sh != 1)
					AF() = Rand() & 1; // AF is undefined

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}

			bool ProcessSAL()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = Truncate((u64)((i64)SignExtend(val, sizecode) << count), sizecode);

					UpdateFlagsZSP(res, sizecode);
					CF() = count < SizeBits(sizecode) ? ((val >> (SizeBits(sizecode) - count)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
					OF() = count == 1 ? Negative(res, sizecode) != CF() : Rand() & 1; // OF is 1 if top 2 bits of original value were different (UND for sh != 1)
					AF() = Rand() & 1; // AF is undefined

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}
			bool ProcessSAR()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = Truncate((u64)((i64)SignExtend(val, sizecode) >> count), sizecode);

					UpdateFlagsZSP(res, sizecode);
					CF() = count < SizeBits(sizecode) ? ((val >> (count - 1)) & 1) == 1 : Rand() & 1; // CF holds last bit shifted out (UND for sh >= #bits)
					OF() = count == 1 ? false : Rand() & 1; // OF is cleared (UND for sh != 1)
					AF() = Rand() & 1; // AF is undefined

					return StoreShiftOpFormat(s, m, res);
				}
				else return false;
			}

			bool ProcessROL()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				count %= SizeBits(sizecode); // rotate performed modulo-n

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = Truncate((val << count) | (val >> (SizeBits(sizecode) - count)), sizecode);

					CF() = ((val >> (SizeBits(sizecode) - count)) & 1) == 1; // CF holds last bit shifted around
					OF() = count == 1 ? CF() ^ Negative(res, sizecode) : Rand() & 1; // OF is xor of CF (after rotate) and high bit of result (UND if sh != 1)

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}
			bool ProcessROR()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				count %= SizeBits(sizecode); // rotate performed modulo-n

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = Truncate((val >> count) | (val << (SizeBits(sizecode) - count)), sizecode);

					CF() = ((val >> (count - 1)) & 1) == 1; // CF holds last bit shifted around
					OF() = count == 1 ? Negative(res, sizecode) ^ (((res >> (SizeBits(sizecode) - 2)) & 1) != 0) : Rand() & 1; // OF is xor of 2 highest bits of result

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}

			bool ProcessRCL()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				count %= SizeBits(sizecode) + 1; // rotate performed modulo-n+1

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = val, temp;
					u64 high_mask = 1ul << (SizeBits(sizecode) - 1); // mask for highest bit
					for (u16 i = 0; i < count; ++i)
					{
						temp = res << 1; // shift res left by 1, store in temp
						temp |= CF() ? 1 : 0ul; // or in CF to the lowest bit
						CF() = (res & high_mask) != 0; // get the previous highest bit into CF
						res = temp; // store back to res
					}

					OF() = count == 1 ? CF() ^ Negative(res, sizecode) : Rand() & 1; // OF is xor of CF (after rotate) and high bit of result (UND if sh != 1)

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}
			bool ProcessRCR()
			{
				u64 s, m, val, count;
				if (!FetchShiftOpFormat(s, m, val, count)) return false;
				u64 sizecode = (s >> 2) & 3;

				count %= SizeBits(sizecode) + 1; // rotate performed modulo-n+1

				// shift of zero is no-op
				if (count != 0)
				{
					u64 res = val, temp;
					u64 high_mask = 1ul << (SizeBits(sizecode) - 1); // mask for highest bit
					for (u16 i = 0; i < count; ++i)
					{
						temp = res >> 1; // shift res right by 1, store in temp
						temp |= CF() ? high_mask : 0ul; // or in CF to the highest bit
						CF() = (res & 1) != 0; // get the previous low bit into CF
						res = temp; // store back to res
					}

					OF() = count == 1 ? Negative(res, sizecode) ^ (((res >> (SizeBits(sizecode) - 2)) & 1) != 0) : Rand() & 1; // OF is xor of 2 highest bits of result

					return StoreShiftOpFormat(s, m, res);
				}
				else return true;
			}

			bool ProcessAND(bool apply = true)
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 res = a & b;

				UpdateFlagsZSP(res, sizecode);
				OF() = CF() = false;
				AF() = Rand() & 1;

				return !apply || StoreBinaryOpFormat(s1, s2, m, res);
			}
			bool ProcessOR()
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 res = a | b;

				UpdateFlagsZSP(res, sizecode);
				OF() = CF() = false;
				AF() = Rand() & 1;

				return StoreBinaryOpFormat(s1, s2, m, res);
			}
			bool ProcessXOR()
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 res = a ^ b;

				UpdateFlagsZSP(res, sizecode);
				OF() = CF() = false;
				AF() = Rand() & 1;

				return StoreBinaryOpFormat(s1, s2, m, res);
			}

			bool ProcessINC()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = Truncate(a + 1, sizecode);

				UpdateFlagsZSP(res, sizecode);
				AF() = (res & 0xf) == 0; // low nibble of 0 was a nibble overflow (TM)
				OF() = Positive(a, sizecode) && Negative(res, sizecode); // + -> - is overflow

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessDEC()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = Truncate(a - 1, sizecode);

				UpdateFlagsZSP(res, sizecode);
				AF() = (a & 0xf) == 0; // nibble a = 0 results in borrow from the low nibble
				OF() = Negative(a, sizecode) && Positive(res, sizecode); // - -> + is overflow

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessNEG()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = Truncate(0 - a, sizecode);

				UpdateFlagsZSP(res, sizecode);
				CF() = 0 < a; // if 0 < a, a borrow was taken from the highest bit (see SUB code where a=0, b=a)
				AF() = 0 < (a & 0xf); // AF is just like CF but only the low nibble
				OF() = Negative(a, sizecode) && Negative(res, sizecode);

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessNOT()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = Truncate(~a, sizecode);

				return StoreUnaryOpFormat(s, m, res);
			}

			bool ProcessCMPZ()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				UpdateFlagsZSP(a, sizecode);
				CF() = OF() = AF() = false;

				return true;
			}

			bool ProcessBSWAP()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = 0;
				switch (sizecode)
				{
				case 3:
					res = (a << 32) | (a >> 32);
					res = ((a & 0x0000ffff0000ffff) << 16) | ((a & 0xffff0000ffff0000) >> 16);
					res = ((a & 0x00ff00ff00ff00ff) << 8) | ((a & 0xff00ff00ff00ff00) >> 8);
					break;
				case 2:
					res = (a << 16) | (a >> 16);
					res = ((a & 0x00ff00ff) << 8) | ((a & 0xff00ff00) >> 8);
					break;
				case 1: res = (a << 8) | (a >> 8); break;
				case 0: res = a; break;
				}

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessBEXTR()
			{
				u64 s1, s2, m, a, b;
				if (!FetchBinaryOpFormat(s1, s2, m, a, b, true, -1, 1)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				int pos = (b >> 8) % SizeBits(sizecode);
				int len = (b & 0xff) % SizeBits(sizecode);

				u64 res = (a >> pos) & ((1ul << len) - 1);

				EFLAGS() = 2; // clear all the (public) flags (flag 1 must always be set)
				ZF() = res == 0; // ZF is set on zero
				AF() = Rand() & 1; // AF, SF, and PF are undefined
				SF() = Rand() & 1;
				PF() = Rand() & 1;

				return StoreBinaryOpFormat(s1, s2, m, res);
			}
			bool ProcessBLSI()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = a & (~a + 1);

				ZF() = res == 0;
				SF() = Negative(res, sizecode);
				CF() = a != 0;
				OF() = false;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessBLSMSK()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = Truncate(a ^ (a - 1), sizecode);

				SF() = Negative(res, sizecode);
				CF() = a == 0;
				ZF() = OF() = false;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessBLSR()
			{
				u64 s, m, a;
				if (!FetchUnaryOpFormat(s, m, a)) return false;
				u64 sizecode = (s >> 2) & 3;

				u64 res = a & (a - 1);

				ZF() = res == 0;
				SF() = Negative(res, sizecode);
				CF() = a == 0;
				OF() = false;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				return StoreUnaryOpFormat(s, m, res);
			}
			bool ProcessANDN()
			{
				u64 s1, s2, dest, a, b;
				if (!FetchRR_RMFormat(s1, s2, dest, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				// only supports 32 and 64-bit operands
				if (sizecode != 2 && sizecode != 3) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				u64 res = ~a & b;

				ZF() = res == 0;
				SF() = Negative(res, sizecode);
				OF() = false;
				CF() = false;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

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
			bool ProcessBTx()
			{
				u64 ext, s1, s2, m, a, b;
				if (!GetMemAdv(1, ext)) return false;

				if (!FetchBinaryOpFormat(s1, s2, m, a, b, true, -1, 0, false)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 mask = 1ul << (b % SizeBits(sizecode)); // performed modulo-n

				CF() = (a & mask) != 0;
				OF() = Rand() & 1;
				SF() = Rand() & 1;
				AF() = Rand() & 1;
				PF() = Rand() & 1;

				switch (ext)
				{
				case 0: return true;
				case 1: return StoreBinaryOpFormat(s1, s2, m, a | mask);
				case 2: return StoreBinaryOpFormat(s1, s2, m, a & ~mask);
				case 3: return StoreBinaryOpFormat(s1, s2, m, a ^ mask);

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
			bool ProcessCxy()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;

				switch (ext)
				{
				case 0: DX() = (i16)AX() >= 0 ? 0 : 0xffff; return true;
				case 1: EDX() = (i32)EAX() >= 0 ? 0 : 0xffffffff; return true;
				case 2: RDX() = (i64)RAX() >= 0 ? 0 : 0xffffffffffffffff; return true;

				case 3: AX() = SignExtend(AL(), 0); return true;
				case 4: EAX() = SignExtend(AX(), 1); return true;
				case 5: RAX() = SignExtend(EAX(), 2); return true;

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
			}
			/*
			[4: dest][4: mode]   [1: mem][1: sh][2:][4: src]
			mode = 0: 16 <- 8  Zero
			mode = 1: 16 <- 8  Sign
			mode = 2: 32 <- 8  Zero
			mode = 3: 32 <- 16 Zero
			mode = 4: 32 <- 8  Sign
			mode = 5: 32 <- 16 Sign
			mode = 6: 64 <- 8  Zero
			mode = 7: 64 <- 16 Zero
			mode = 8: 64 <- 8  Sign
			mode = 9: 64 <- 16 Sign
			else UND
			(sh marks that source is (ABCD)H)
			*/
			bool ProcessMOVxX()
			{
				u64 s1, s2, src;
				if (!GetMemAdv(1, s1) || !GetMemAdv(1, s2)) return false;

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
							// make sure we're in registers A-D
							if ((s2 & 0x0c) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
							src = CPURegisters[s2 & 15].x8h();
						}
						else src = CPURegisters[s2 & 15].x8();
						break;
					case 3: case 5: case 7: case 9: src = CPURegisters[s2 & 15].x16(); break;

					default: Terminate(ErrorCode::UndefinedBehavior); return false;
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

					default: Terminate(ErrorCode::UndefinedBehavior); return false;
					}
				}

				// store the value
				switch (s1 & 15)
				{
				case 0: CPURegisters[s1 >> 4].x16() = src; break;
				case 1: CPURegisters[s1 >> 4].x16() = SignExtend(src, 0); break;

				case 2: case 3: CPURegisters[s1 >> 4].x32() = src; break;
				case 4: CPURegisters[s1 >> 4].x32() = SignExtend(src, 0); break;
				case 5: CPURegisters[s1 >> 4].x32() = SignExtend(src, 1); break;

				case 6: case 7: CPURegisters[s1 >> 4].x64() = src; break;
				case 8: CPURegisters[s1 >> 4].x64() = SignExtend(src, 0); break;
				case 9: CPURegisters[s1 >> 4].x64() = SignExtend(src, 1); break;
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
			bool ProcessADXX()
			{
				u64 ext, s1, s2, m, a, b;
				if (!GetMemAdv(1, ext)) return false;

				if (!FetchBinaryOpFormat(s1, s2, m, a, b)) return false;
				u64 sizecode = (s1 >> 2) & 3;

				u64 res = a + b;

				switch (ext)
				{
				case 0: case 1: if (CF()) ++res; break;
				case 2: if (OF()) ++res; break;

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}

				res = Truncate(res, sizecode);

				switch (ext)
				{
				case 0:
					CF() = res < a;
					UpdateFlagsZSP(res, sizecode);
					AF() = (res & 0xf) < (a & 0xf); // AF is just like CF but only the low nibble
					OF() = Positive(a, sizecode) == Positive(b, sizecode) && Positive(a, sizecode) != Positive(res, sizecode);
					break;
				case 1:
					CF() = res < a;
					break;
				case 2:
					OF() = Positive(a, sizecode) == Positive(b, sizecode) && Positive(a, sizecode) != Positive(res, sizecode);
					break;
				}

				return StoreBinaryOpFormat(s1, s2, m, res);
			}
			/*
			[8: ext]
			ext = 0: AAA
			ext = 1: AAS
			else UND
			*/
			bool ProcessAAX()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;
				if (ext > 1) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				if ((AL() & 0xf) > 9 || AF())
				{
					// handle ext cases here
					if (ext == 0) AX() += 0x106;
					else
					{
						AX() -= 6;
						--AH();
					}
					AF() = true;
					CF() = true;
				}
				else
				{
					AF() = false;
					CF() = false;
				}
				AL() &= 0xf;

				OF() = Rand() & 1;
				SF() = Rand() & 1;
				ZF() = Rand() & 1;
				PF() = Rand() & 1;

				return true;
			}

			// -- floating point stuff -- //

			/// <summary>
			/// Initializes the FPU as if by FINIT
			/// </summary>
			void FINIT()
			{
				FPU_control = 0x3bf;
				FPU_status = 0;
				FPU_tag = 0xffff;
			}

			/// <summary>
			/// Computes the FPU tag for the specified value
			/// </summary>
			/// <param name="val">the value to test</param>
			static int ComputeFPUTag(long double val)
			{
				if (std::isnan(val) || std::isinf(val) || IsDenorm(val)) return FPU_Tag_special;
				else if (val == 0) return FPU_Tag_zero;
				else return FPU_Tag_normal;
			}

			/// <summary>
			/// Performs a round trip on the value based on the current state of the <see cref="FPU_RC"/> flag
			/// </summary>
			/// <param name="val">the value to round</param>
			long double PerformRoundTrip(long double val)
			{
				switch (FPU_RC())
				{
				case 0: std::fesetround(FE_TONEAREST); return std::nearbyintl(val);
				case 1: std::fesetround(FE_DOWNWARD); return std::nearbyintl(val);
				case 2: std::fesetround(FE_UPWARD); return std::nearbyintl(val);
				case 3: std::fesetround(FE_TOWARDZERO); return std::nearbyintl(val);

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
			bool FetchFPUBinaryFormat(u64 &s, long double &a, long double &b)
			{
				u64 m;
				if (!GetMemAdv(1, s)) return false;

				// switch through mode
				switch (s & 7)
				{
				case 0:
					if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(s >> 4) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					a = ST(0); b = ST(s >> 4); return true;
				case 1:
				case 2:
					if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(s >> 4) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					b = ST(0); a = ST(s >> 4); return true;

				default:
					if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					a = ST(0); b = 0;
					if (!GetAddressAdv(m)) return false;
					switch (s & 7)
					{
					case 3: if (!GetMemRaw(m, 4, m)) return false; b = AsFloat(m); return true;
					case 4: if (!GetMemRaw(m, 8, m)) return false; b = AsDouble(m); return true;
					case 5: if (!GetMemRaw(m, 2, m)) return false; b = (i64)SignExtend(m, 1); return true;
					case 6: if (!GetMemRaw(m, 4, m)) return false; b = (i64)SignExtend(m, 2); return true;

					default: Terminate(ErrorCode::UndefinedBehavior); return false;
					}
				}
			}
			bool StoreFPUBinaryFormat(u64 s, long double res)
			{
				switch (s & 7)
				{
				case 1: ST(s >> 4, res); return true;
				case 2: ST(s >> 4, res); return PopFPU();

				default: ST(0, res); return true;
				}
			}

			bool PushFPU(long double val)
			{
				// decrement top (wraps automatically as a 3-bit unsigned value)
				--FPU_TOP();

				// if this fpu reg is in use, it's an error
				if (ST_Tag(0) != FPU_Tag_empty) { Terminate(ErrorCode::FPUStackOverflow); return false; }

				// store the value
				ST(0, val);

				return true;
			}
			bool PopFPU(long double &val)
			{
				// if this register is not in use, it's an error
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUStackUnderflow); return false; }

				// extract the value
				val = ST(0);
				ST_Free(0);

				// increment top (wraps automatically as a 3-bit unsigned value)
				++FPU_TOP();

				return true;
			}
			bool PopFPU() { long double _; return  PopFPU(_); }

			/*
			[8: mode]   [address]
			mode = 0: FSTSW AX
			mode = 1: FSTSW
			mode = 2: FSTCW
			mode = 3: FLDCW
			else UND
			*/
			bool ProcessFSTLD_WORD()
			{
				u64 m, s;
				if (!GetMemAdv(1, s)) return false;

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
				case 1: return SetMemRaw(m, 2, FPU_status);
				case 2: return SetMemRaw(m, 2, FPU_control);
				case 3:
					if (!GetMemRaw(m, 2, m)) return false;
					FPU_control = m;
					return true;

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
			}

			bool ProcessFLD_const()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;
				
				switch (ext)
				{
				case 0: return PushFPU(1L); // 1
				case 1: return PushFPU(3.321928094887362347870319429489390175864831393024580612054L); // lb 10
				case 2: return PushFPU(1.442695040888963407359924681001892137426645954152985934135L); // lb e
				case 3: return PushFPU(3.141592653589793238462643383279502884197169399375105820974L); // pi
				case 4: return PushFPU(0.301029995663981195213738894724493026768189881462108541310L); // log 2
				case 5: return PushFPU(0.693147180559945309417232121458176568075500134360255254120L); // ln 2
				case 6: return PushFPU(0L); // 0

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
			bool ProcessFLD()
			{
				u64 s, m;
				if (!GetMemAdv(1, s)) return false;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				// switch through mode
				switch (s & 7)
				{
				case 0:
					if (ST_Tag(s >> 4) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					return PushFPU(ST(s >> 4));

				default:
					if (!GetAddressAdv(m)) return false;
					switch (s & 7)
					{
					case 1: if (!GetMemRaw(m, 4, m)) return false; return PushFPU(AsFloat(m));
					case 2: if (!GetMemRaw(m, 8, m)) return false; return PushFPU(AsDouble(m));

					case 3: if (!GetMemRaw(m, 2, m)) return false; return PushFPU((i64)SignExtend(m, 1));
					case 4: if (!GetMemRaw(m, 4, m)) return false; return PushFPU((i64)SignExtend(m, 2));
					case 5: if (!GetMemRaw(m, 8, m)) return false; return PushFPU((i64)m);

					default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
			bool ProcessFST()
			{
				u64 s, m;
				if (!GetMemAdv(1, s)) return false;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				switch (s & 15)
				{
				case 0:
				case 1:
					// make sure we can read the value
					if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					// record the value (is allowed to be not in use)
					ST(s >> 4, ST(0));
					break;

				default:
					if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					if (!GetAddressAdv(m)) return false;
					switch (s & 15)
					{
					case 2: case 3: if (!SetMemRaw(m, 4, FloatAsUInt64(ST(0)))) return false; break;
					case 4: case 5: if (!SetMemRaw(m, 8, DoubleAsUInt64(ST(0)))) return false; break;
					case 6: case 7: if (!SetMemRaw(m, 2, (u64)(i64)PerformRoundTrip(ST(0)))) return false; break;
					case 8: case 9: if (!SetMemRaw(m, 4, (u64)(i64)PerformRoundTrip(ST(0)))) return false; break;
					case 10: if (!SetMemRaw(m, 8, (u64)(i64)PerformRoundTrip(ST(0)))) return false; break;
					case 11: if (!SetMemRaw(m, 2, (u64)(i64)ST(0))) return false; break;
					case 12: if (!SetMemRaw(m, 4, (u64)(i64)ST(0))) return false; break; // !! WORK !! make sure these truncate regardless of std::fesetround()
					case 13: if (!SetMemRaw(m, 8, (u64)(i64)ST(0))) return false; break;
						
					default: Terminate(ErrorCode::UndefinedBehavior); return false;
					}
					break;
				}

				switch (s & 15)
				{
				case 0: case 2: case 4: case 6: case 8: return true;
				default: return PopFPU();
				}
			}
			bool ProcessFXCH()
			{
				u64 i;
				if (!GetMemAdv(1, i)) return false;

				// make sure they're both in use
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(i) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double temp = ST(0);
				ST(0, ST(i));
				ST(i, temp);

				FPU_C0() = Rand() & 1;
				FPU_C1() = false;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

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
			bool ProcessFMOVcc()
			{
				u64 s;
				if (!GetMemAdv(1, s)) return false;

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

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}

				// if flag is set, do the move
				if (flag)
				{
					// !! FIX CS s>>4 is 4-bit - needs &7 !! 
					int i = (s >> 4) & 15;
					if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(i) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					ST(0, ST(s >> 4));
				}

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}

			bool ProcessFADD()
			{
				u64 s;
				long double a, b;
				if (!FetchFPUBinaryFormat(s, a, b)) return false;

				long double res = a + b;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return StoreFPUBinaryFormat(s, res);
			}
			bool ProcessFSUB()
			{
				u64 s;
				long double a, b;
				if (!FetchFPUBinaryFormat(s, a, b)) return false;

				long double res = a - b;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return StoreFPUBinaryFormat(s, res);
			}
			bool ProcessFSUBR()
			{
				u64 s;
				long double a, b;
				if (!FetchFPUBinaryFormat(s, a, b)) return false;

				long double res = b - a;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return StoreFPUBinaryFormat(s, res);
			}

			bool ProcessFMUL()
			{
				u64 s;
				long double a, b;
				if (!FetchFPUBinaryFormat(s, a, b)) return false;

				long double res = a * b;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return StoreFPUBinaryFormat(s, res);
			}
			bool ProcessFDIV()
			{
				u64 s;
				long double a, b;
				if (!FetchFPUBinaryFormat(s, a, b)) return false;

				long double res = a / b;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return StoreFPUBinaryFormat(s, res);
			}
			bool ProcessFDIVR()
			{
				u64 s;
				long double a, b;
				if (!FetchFPUBinaryFormat(s, a, b)) return false;

				long double res = b / a;

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return StoreFPUBinaryFormat(s, res);
			}

			bool ProcessF2XM1()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				// get the value
				double val = ST(0);
				// val must be in range [-1, 1]
				if (val < -1 || val > 1) { Terminate(ErrorCode::FPUError); return false; }

				ST(0, std::powl(2, val) - 1);

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFABS()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				ST(0, std::fabsl(ST(0)));

				FPU_C0() = Rand() & 1;
				FPU_C1() = false;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFCHS()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				ST(0, -ST(0));

				FPU_C0() = Rand() & 1;
				FPU_C1() = false;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFPREM()
			{
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(1) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double a = ST(0);
				long double b = ST(1);

				// compute remainder with truncated quotient
				long double res = a - (i64)(a / b) * b;

				// store value
				ST(0, res);

				// get the bits
				u64 bits = DoubleAsUInt64(res);

				FPU_C0() = (bits & 4) != 0;
				FPU_C1() = (bits & 1) != 0;
				FPU_C2() = false;
				FPU_C3() = (bits & 2) != 0;

				return true;
			}
			bool ProcessFPREM1()
			{
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(1) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double a = ST(0);
				long double b = ST(1);

				// compute remainder with rounded quotient (IEEE)
				long double q = a / b;
				std::fesetround(FE_TONEAREST);
				long double res = a - std::nearbyintl(q) * b;

				// store value
				ST(0, res);

				// get the bits - should be from 80-bit fp, but we have no control over that :(
				u64 bits = DoubleAsUInt64(res);

				FPU_C0() = (bits & 4) != 0;
				FPU_C1() = (bits & 1) != 0;
				FPU_C2() = false;
				FPU_C3() = (bits & 2) != 0;

				return true;
			}
			bool ProcessFRNDINT()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double val = ST(0);
				long double res = PerformRoundTrip(val);

				ST(0, res);

				FPU_C0() = Rand() & 1;
				FPU_C1() = res > val;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFSQRT()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				ST(0, std::sqrtl(ST(0)));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFYL2X()
			{
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(1) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double a = ST(0);
				long double b = ST(1);

				PopFPU(); // pop stack and place in the new st(0)
				ST(0, ST(1) * std::log2l(ST(0)));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFYL2XP1()
			{
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(1) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double a = ST(0);
				long double b = ST(1);

				PopFPU(); // pop stack and place in the new st(0)
				ST(0, b * std::log2(a + 1));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFXTRACT()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				// get value and extract exponent/significand
				double exp, sig;
				ExtractDouble(ST(0), exp, sig);

				// exponent in st0, then push the significand
				ST(0, exp);
				return PushFPU(sig);
			}
			bool ProcessFSCALE()
			{
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(1) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				double a = ST(0);
				double b = ST(1);

				// get exponent and significand of st0
				double exp, sig;
				ExtractDouble(a, exp, sig);

				// add (truncated) st1 to exponent of st0
				ST(0, AssembleDouble(exp + (i64)b, sig));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}

			bool ProcessFXAM()
			{
				long double val = ST(0);
				u64 bits = DoubleAsUInt64(val);

				// C1 gets sign bit
				FPU_C1() = (bits & 0x8000000000000000) != 0;

				// empty
				if (ST_Tag(0) == FPU_Tag_empty) { FPU_C3() = true; FPU_C2() = false; FPU_C0() = true; }
				// NaN
				else if (std::isnan(val)) { FPU_C3() = false; FPU_C2() = false; FPU_C0() = true; }
				// inf
				else if (std::isinf(val)) { FPU_C3() = false; FPU_C2() = true; FPU_C0() = true; }
				// zero
				else if (val == 0) { FPU_C3() = true; FPU_C2() = false; FPU_C0() = false; }
				// denormalized
				else if (IsDenorm(val)) { FPU_C3() = true; FPU_C2() = true; FPU_C0() = false; }
				// normal
				else { FPU_C3() = false; FPU_C2() = true; FPU_C0() = false; }

				return true;
			}
			bool ProcessFTST()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double a = ST(0);

				// for FTST, nan is an arithmetic error
				if (std::isnan(a)) { Terminate(ErrorCode::ArithmeticError); return false; }

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
			bool ProcessFCOM()
			{
				u64 s, m;
				if (!GetMemAdv(1, s)) return false;

				long double a, b;

				// swith through mode
				switch (s & 15)
				{
				case 0:
				case 1:
				case 2:
				case 11:
				case 12:
					if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(s >> 4) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					a = ST(0); b = ST(s >> 4);
					break;

				default:
					if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }
					a = ST(0);
					if (!GetAddressAdv(m)) return false;
					switch (s & 15)
					{
					case 3: case 4: if (!GetMemRaw(m, 4, m)) return false; b = AsFloat(m); break;
					case 5: case 6: if (!GetMemRaw(m, 8, m)) return false; b = AsDouble(m); break;

					case 7: case 8: if (!GetMemRaw(m, 2, m)) return false; b = (i64)SignExtend(m, 1); break;
					case 9: case 10: if (!GetMemRaw(m, 4, m)) return false; b = (i64)SignExtend(m, 2); break;

					default: Terminate(ErrorCode::UndefinedBehavior); return false;
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
					if ((s & 128) == 0) { Terminate(ErrorCode::ArithmeticError); return false; }
					x = y = z = true;
				}

				// eflags
				if (s == 11 || s == 12)
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
				switch (s & 7)
				{
				case 2: return PopFPU() && PopFPU();
				case 1: case 4: case 6: case 8: case 10: case 12: return PopFPU();
				default: return true;
				}
			}

			bool ProcessFSIN()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				ST(0, std::sinl(ST(0)));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = false;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFCOS()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				ST(0, std::cosl(ST(0)));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = false;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFSINCOS()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = false;
				FPU_C3() = Rand() & 1;

				// get the value
				long double val = ST(0);

				// st(0) <- sin, push cos
				ST(0, std::sinl(val));
				return PushFPU(std::cosl(val));
			}
			bool ProcessFPTAN()
			{
				if (ST_Tag(0) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				ST(0, std::tanl(ST(0)));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = false;
				FPU_C3() = Rand() & 1;

				// also push 1 onto fpu stack
				return PushFPU(1);
			}
			bool ProcessFPATAN()
			{
				if (ST_Tag(0) == FPU_Tag_empty || ST_Tag(1) == FPU_Tag_empty) { Terminate(ErrorCode::FPUAccessViolation); return false; }

				long double a = ST(0);
				long double b = ST(1);

				PopFPU(); // pop stack and place in new st(0)
				ST(0, std::atan2l(b, a));

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = false;
				FPU_C3() = Rand() & 1;

				return true;
			}

			/*
			[7:][1: mode]
			mode = 0: inc
			mode = 1: dec
			*/
			bool ProcessFINCDECSTP()
			{
				u64 ext;
				if (!GetMemAdv(1, ext)) return false;

				// does not modify tag word
				switch (ext & 1)
				{
				case 0: ++FPU_TOP(); break;
				case 1: --FPU_TOP(); break;

				default: return true; // can't happen but compiler is stupid
				}

				FPU_C0() = Rand() & 1;
				FPU_C1() = false;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}
			bool ProcessFFREE()
			{
				u64 i;
				if (!GetMemAdv(1, i)) return false;

				// mark as not in use
				ST_Free(i);

				FPU_C0() = Rand() & 1;
				FPU_C1() = Rand() & 1;
				FPU_C2() = Rand() & 1;
				FPU_C3() = Rand() & 1;

				return true;
			}

			// -- vpu stuff -- //

			typedef bool (Computer::* VPUBinaryDelegate)(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index);

			/*
			[5: reg][1: aligned][2: reg_size]   [1: has_mask][1: zmask][1: scalar][1:][2: elem_size][2: mode]   ([count: mask])
			mode = 0: [3:][5: src]   reg <- src
			mode = 1: [address]      reg <- M[address]
			mode = 2: [address]      M[address] <- src
			else UND
			*/
			bool ProcessVPUMove()
			{
				// read settings bytes
				u64 s1, s2, _src, m, temp;
				if (!GetMemAdv(1, s1) || !GetMemAdv(1, s2)) return false;
				u64 reg_sizecode = s1 & 3;
				u64 elem_sizecode = (s2 >> 2) & 3;

				// get the register to work with
				if (reg_sizecode == 3) { Terminate(ErrorCode::UndefinedBehavior); return false; }
				if (reg_sizecode != 2 && (s1 & 0x80) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
				int reg = (int)(s1 >> 3);

				// get number of elements to process (accounting for scalar flag)
				int elem_count = (s2 & 0x20) != 0 ? 1 : (int)(Size(reg_sizecode + 4) >> elem_sizecode);
				int src;

				// get the mask (default of all 1's)
				u64 mask = ~(u64)0;
				if ((s2 & 0x80) != 0 && !GetMemAdv(BitsToBytes((u64)elem_count), mask)) return false;
				// get the zmask flag
				bool zmask = (s2 & 0x40) != 0;
				
				// switch through mode
				switch (s2 & 3)
				{
				case 0:
					if (!GetMemAdv(1, _src)) return false;
					if (reg_sizecode != 2 && (_src & 0x10) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
					src = (int)_src & 0x1f;

					for (int i = 0; i < elem_count; ++i, mask >>= 1)
						if ((mask & 1) != 0) ZMMRegisters[reg].uint(elem_sizecode, i) = ZMMRegisters[src].uint(elem_sizecode, i);
						else if (zmask) ZMMRegisters[reg].uint(elem_sizecode, i) = 0;

						break;
				case 1:
					if (!GetAddressAdv(m)) return false;
					// if we're in vector mode and aligned flag is set, make sure address is aligned
					if (elem_count > 1 && (s1 & 4) != 0 && m % Size(reg_sizecode + 4) != 0) { Terminate(ErrorCode::AlignmentViolation); return false; }

					for (int i = 0; i < elem_count; ++i, mask >>= 1, m += Size(elem_sizecode))
						if ((mask & 1) != 0)
						{
							if (!GetMemRaw(m, Size(elem_sizecode), temp)) return false;
							ZMMRegisters[reg].uint(elem_sizecode, i) = temp;
						}
						else if (zmask) ZMMRegisters[reg].uint(elem_sizecode, i) = 0;

						break;
				case 2:
					if (!GetAddressAdv(m)) return false;
					// if we're in vector mode and aligned flag is set, make sure address is aligned
					if (elem_count > 1 && (s1 & 4) != 0 && m % Size(reg_sizecode + 4) != 0) { Terminate(ErrorCode::AlignmentViolation); return false; }

					for (int i = 0; i < elem_count; ++i, mask >>= 1, m += Size(elem_sizecode))
						if ((mask & 1) != 0)
						{
							if (!SetMemRaw(m, Size(elem_sizecode), ZMMRegisters[reg].uint(elem_sizecode, i))) return false;
						}
						else if (zmask && !SetMemRaw(m, Size(elem_sizecode), 0)) return false;

						break;

				default: Terminate(ErrorCode::UndefinedBehavior); return false;
				}
				
				return true;
			}
			/*
			[5: dest][1: aligned][2: dest_size]   [1: has_mask][1: zmask][1: scalar][1:][2: elem_size][1:][1: mem]   ([count: mask])   [3:][5: src1]
			mem = 0: [3:][5: src2]   dest <- f(src1, src2)
			mem = 1: [address]       dest <- f(src1, M[address])
			*/
			bool ProcessVPUBinary(u64 elem_size_mask, VPUBinaryDelegate func)
			{
				// read settings bytes
				u64 s1, s2, _src1, _src2, res, m;
				if (!GetMemAdv(1, s1) || !GetMemAdv(1, s2)) return false;
				u64 dest_sizecode = s1 & 3;
				u64 elem_sizecode = (s2 >> 2) & 3;

				// make sure this element size is allowed
				if ((Size(elem_sizecode) & elem_size_mask) == 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }

				// get the register to work with
				if (dest_sizecode == 3) { Terminate(ErrorCode::UndefinedBehavior); return false; }
				if (dest_sizecode != 2 && (s1 & 0x80) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
				int dest = (int)(s1 >> 3);

				// get number of elements to process (accounting for scalar flag)
				int elem_count = (s2 & 0x20) != 0 ? 1 : (int)(Size(dest_sizecode + 4) >> elem_sizecode);

				// get the mask (default of all 1's)
				u64 mask = ~(u64)0;
				if ((s2 & 0x80) != 0 && !GetMemAdv(BitsToBytes((u64)elem_count), mask)) return false;
				// get the zmask flag
				bool zmask = (s2 & 0x40) != 0;

				// get src1
				if (!GetMemAdv(1, _src1)) return false;
				if (dest_sizecode != 2 && (_src1 & 0x10) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
				int src1 = (int)(_src1 & 0x1f);

				// if src2 is a register
				if ((s2 & 1) == 0)
				{
					if (!GetMemAdv(1, _src2)) return false;
					if (dest_sizecode != 2 && (_src2 & 0x10) != 0) { Terminate(ErrorCode::UndefinedBehavior); return false; }
					int src2 = (int)(_src2 & 0x1f);

					for (int i = 0; i < elem_count; ++i, mask >>= 1)
						if ((mask & 1) != 0)
						{
							// hand over to the delegate for processing
							if (!(this->*func)(elem_sizecode, res, ZMMRegisters[src1].uint(elem_sizecode, i), ZMMRegisters[src2].uint(elem_sizecode, i), i)) return false;
							ZMMRegisters[dest].uint(elem_sizecode, i) = res;
						}
						else if (zmask) ZMMRegisters[dest].uint(elem_sizecode, i) = 0;
				}
				// otherwise src is memory
				else
				{
					if (!GetAddressAdv(m)) return false;
					// if we're in vector mode and aligned flag is set, make sure address is aligned
					if (elem_count > 1 && (s1 & 4) != 0 && m % Size(dest_sizecode + 4) != 0) { Terminate(ErrorCode::AlignmentViolation); return false; }

					for (int i = 0; i < elem_count; ++i, mask >>= 1, m += Size(elem_sizecode))
						if ((mask & 1) != 0)
						{
							if (!GetMemRaw(m, Size(elem_sizecode), res)) return false;

							// hand over to the delegate for processing
							if (!(this->*func)(elem_sizecode, res, ZMMRegisters[src1].uint(elem_sizecode, i), res, i)) return false;
							ZMMRegisters[dest].uint(elem_sizecode, i) = res;
						}
						else if (zmask) ZMMRegisters[dest].uint(elem_sizecode, i) = 0;
				}

				return true;
			}

			bool __TryPerformVEC_FADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// 64-bit fp
				if (elem_sizecode == 3) res = DoubleAsUInt64(AsDouble(a) + AsDouble(b));
				// 32-bit fp
				else res = FloatAsUInt64(AsFloat(a) + AsFloat(b));

				return true;
			}
			bool __TryPerformVEC_FSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// 64-bit fp
				if (elem_sizecode == 3) res = DoubleAsUInt64(AsDouble(a) - AsDouble(b));
				// 32-bit fp
				else res = FloatAsUInt64(AsFloat(a) - AsFloat(b));

				return true;
			}
			bool __TryPerformVEC_FMUL(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// 64-bit fp
				if (elem_sizecode == 3) res = DoubleAsUInt64(AsDouble(a) * AsDouble(b));
				// 32-bit fp
				else res = FloatAsUInt64(AsFloat(a) * AsFloat(b));

				return true;
			}
			bool __TryPerformVEC_FDIV(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// 64-bit fp
				if (elem_sizecode == 3) res = DoubleAsUInt64(AsDouble(a) / AsDouble(b));
				// 32-bit fp
				else res = FloatAsUInt64(AsFloat(a) / AsFloat(b));

				return true;
			}

			bool TryProcessVEC_FADD() { return  ProcessVPUBinary(12, &Computer::__TryPerformVEC_FADD); }
			bool TryProcessVEC_FSUB() { return  ProcessVPUBinary(12, &Computer::__TryPerformVEC_FSUB); }
			bool TryProcessVEC_FMUL() { return  ProcessVPUBinary(12, &Computer::__TryPerformVEC_FMUL); }
			bool TryProcessVEC_FDIV() { return  ProcessVPUBinary(12, &Computer::__TryPerformVEC_FDIV); }

			bool __TryPerformVEC_AND(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a & b;
				return true;
			}
			bool __TryPerformVEC_OR(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a | b;
				return true;
			}
			bool __TryPerformVEC_XOR(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a ^ b;
				return true;
			}
			bool __TryPerformVEC_ANDN(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = ~a & b;
				return true;
			}

			bool TryProcessVEC_AND() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_AND); }
			bool TryProcessVEC_OR() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_OR); }
			bool TryProcessVEC_XOR() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_XOR); }
			bool TryProcessVEC_ANDN() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_ANDN); }

			bool __TryPerformVEC_ADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a + b;
				return true;
			}
			bool __TryPerformVEC_ADDS(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// get sign mask
				u64 smask = SignMask(elem_sizecode);

				res = a + b;

				// get sign bits
				bool res_sign = (res & smask) != 0;
				bool a_sign = (a & smask) != 0;
				bool b_sign = (b & smask) != 0;

				// if there was an over/underflow, handle saturation cases
				if (a_sign == b_sign && a_sign != res_sign) res = a_sign ? smask : smask - 1;

				return true;
			}
			bool __TryPerformVEC_ADDUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// get trunc mask
				u64 tmask = TruncMask(elem_sizecode);

				res = (a + b) & tmask; // truncated for logic below

				// if there was an over/underflow, handle saturation cases
				if (res < a) res = tmask;

				return true;
			}

			bool TryProcessVEC_ADD() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_ADD); }
			bool TryProcessVEC_ADDS() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_ADDS); }
			bool TryProcessVEC_ADDUS() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_ADDUS); }

			bool __TryPerformVEC_SUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a - b;
				return true;
			}
			bool __TryPerformVEC_SUBS(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// since this one's signed, we can just add the negative
				return __TryPerformVEC_ADDS(elem_sizecode, res, a, Truncate(~b + 1, elem_sizecode), index);
			}
			bool __TryPerformVEC_SUBUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// handle unsigned sub saturation
				res = a > b ? a - b : 0;
				return true;
			}

			bool TryProcessVEC_SUB() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_SUB); }
			bool TryProcessVEC_SUBS() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_SUBS); }
			bool TryProcessVEC_SUBUS() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_SUBUS); }

			bool __TryPerformVEC_MULL(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = (i64)SignExtend(a, elem_sizecode) * (i64)SignExtend(b, elem_sizecode);
				return true;
			}

			bool TryProcessVEC_MULL() { return  ProcessVPUBinary(15, &Computer::__TryPerformVEC_MULL); }

			bool __TryProcessVEC_FMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// this exploits c++ returning false on comparison to NaN. see http://www.felixcloutier.com/x86/MINPD.html for the actual algorithm
				if (elem_sizecode == 3) res = AsDouble(a) < AsDouble(b) ? a : b;
				else res = AsFloat(a) < AsFloat(b) ? a : b;

				return true;
			}
			bool __TryProcessVEC_FMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// this exploits c++ returning false on comparison to NaN. see http://www.felixcloutier.com/x86/MAXPD.html for the actual algorithm
				if (elem_sizecode == 3) res = AsDouble(a) > AsDouble(b) ? a : b;
				else res = AsFloat(a) > AsFloat(b) ? a : b;

				return true;
			}

			bool TryProcessVEC_FMIN() { return  ProcessVPUBinary(12, &Computer::__TryProcessVEC_FMIN); }
			bool TryProcessVEC_FMAX() { return  ProcessVPUBinary(12, &Computer::__TryProcessVEC_FMAX); }

			bool __TryProcessVEC_UMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a < b ? a : b; // a and b are guaranteed to be properly truncated, so this is invariant of size
				return true;
			}
			bool __TryProcessVEC_SMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// just extend to 64-bit and do a signed compare
				res = (i64)SignExtend(a, elem_sizecode) < (i64)SignExtend(b, elem_sizecode) ? a : b;
				return true;
			}
			bool __TryProcessVEC_UMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = a > b ? a : b; // a and b are guaranteed to be properly truncated, so this is invariant of size
				return true;
			}
			bool __TryProcessVEC_SMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// just extend to 64-bit and do a signed compare
				res = (i64)SignExtend(a, elem_sizecode) > (i64)SignExtend(b, elem_sizecode) ? a : b;
				return true;
			}

			bool TryProcessVEC_UMIN() { return  ProcessVPUBinary(15, &Computer::__TryProcessVEC_UMIN); }
			bool TryProcessVEC_SMIN() { return  ProcessVPUBinary(15, &Computer::__TryProcessVEC_SMIN); }
			bool TryProcessVEC_UMAX() { return  ProcessVPUBinary(15, &Computer::__TryProcessVEC_UMAX); }
			bool TryProcessVEC_SMAX() { return  ProcessVPUBinary(15, &Computer::__TryProcessVEC_SMAX); }

			bool __TryPerformVEC_FADDSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				// 64-bit fp
				if (elem_sizecode == 3) res = DoubleAsUInt64(index % 2 == 0 ? AsDouble(a) - AsDouble(b) : AsDouble(a) + AsDouble(b));
				// 32-bit fp
				else res = FloatAsUInt64(index % 2 == 0 ? AsFloat(a) - AsFloat(b) : AsFloat(a) + AsFloat(b));

				return true;
			}

			bool TryProcessVEC_FADDSUB() { return  ProcessVPUBinary(12, &Computer::__TryPerformVEC_FADDSUB); }

			bool __TryPerformVEC_AVG(u64 elem_sizecode, u64 &res, u64 a, u64 b, int index)
			{
				res = (a + b + 1) >> 1; // doesn't work for 64-bit, but Intel doesn't offer a 64-bit variant anyway, so that's fine
				return true;
			}

			bool TryProcessVEC_AVG() { return ProcessVPUBinary(3, &Computer::__TryPerformVEC_AVG); }
	};
}

#endif