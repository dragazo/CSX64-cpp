#ifndef CSX64_COMPUTER_H
#define CSX64_COMPUTER_H

#include <iostream>
#include <memory>
#include <type_traits>

#include "CoreTypes.h"
#include "ExeTypes.h"
#include "Utility.h"
#include "FastRng.h"
#include "Executable.h"

#include "../ios-frstor/iosfrstor.h"

namespace CSX64
{
	class Computer
	{
	public: // -- info -- //

		// the number of file descriptors available to this computer
		static constexpr int FDCount = 16;

		// the alignment of the internal memory array
		static constexpr u64 MemAlignment = 64;
		
		// CSX64 considers many valid, but non-intel things to be undefined behavior at runtime (e.g. 8-bit addressing).
		// however, these types of things are already blocked by the assembler.
		// if this is set to true, emits ErrorCode::UndefinedBehavior in these cases - otherwise permits them (more efficient).
		static constexpr bool StrictUND = false;

		// if set to true, uses mask unions to perform the UpdateFlagsZSP() function - otherwise uses flag accessors (slower)
		static constexpr bool FlagAccessMasking = true;

	public: // -- special types -- //

		// wraps a physical ST register's info into a more convenient package
		struct ST_Wrapper_common
		{
		protected:

			// can't delete this type directly
			~ST_Wrapper_common() = default;

		private:

			const Computer &c;
			const u32 index; // physical index of ST register

			friend class Computer;
			ST_Wrapper_common(const Computer &_c, u32 _index) noexcept : c(_c), index(_index) {}

		public:

			operator long double() const { return c.FPURegisters[index]; }
			ST_Wrapper_common &operator=(const ST_Wrapper_common&) = delete;

			// ---------------------------------------- //

			static constexpr u16 tag_normal = 0;
			static constexpr u16 tag_zero = 1;
			static constexpr u16 tag_special = 2;
			static constexpr u16 tag_empty = 3;

			static u16 ComputeFPUTag(long double val)
			{
				if (std::isnan(val) || std::isinf(val) || IsDenorm(val)) return tag_special;
				else if (val == 0) return tag_zero;
				else return tag_normal;
			}

			// ---------------------------------------- //

			// gets the tag for this ST register
			u16 Tag() const { return (c.FPU_tag >> (index * 2)) & 3; }

			bool Normal() const { return Tag() == tag_normal; }
			bool Zero() const { return Tag() == tag_zero; }
			bool Special() const { return Tag() == tag_special; }
			bool Empty() const { return Tag() == tag_empty; }

			// ---------------------------------------- //

			friend std::ostream &operator<<(std::ostream &ostr, const ST_Wrapper_common &wrapper)
			{
				// set up a format restore point and clear the restore width (we'll use the provided width)
				iosfrstor _frstor(ostr);
				_frstor.fmt().width(0);

				if (wrapper.Empty()) ostr << "Empty";
				else ostr << std::defaultfloat << std::setprecision(17) << (long double)wrapper;

				return ostr;
			}
		};
		struct ST_Wrapper : ST_Wrapper_common
		{
		private:

			// ST_Wrapper_common will store a const Computer&, but we need non-const.
			// we'll therefore construct from a non-const (converted to const) and use const_cast to appease the compiler.

			friend class Computer;
			ST_Wrapper(Computer &_c, u32 _index) noexcept : ST_Wrapper_common(_c, _index) {}

		public:

			ST_Wrapper &operator=(long double value)
			{
				// store value, accounting for precision control flag
				switch (c.FPU_PC())
				{
				case 0: value = (float)value; break;
				case 2: value = (double)value; break;
				}

				// const_cast does not violate const correctness - see above comment for private ctor
				const_cast<Computer&>(c).FPURegisters[index] = value;
				const_cast<Computer&>(c).FPU_tag = (c.FPU_tag & ~(3 << (index * 2))) | (ComputeFPUTag(value) << (index * 2));

				return *this;
			}
			ST_Wrapper &operator=(ST_Wrapper wrapper) { *this = (long double)wrapper; return *this; }

			ST_Wrapper &operator+=(long double val) { *this = *this + val; return *this; }
			ST_Wrapper &operator-=(long double val) { *this = *this - val; return *this; }
			ST_Wrapper &operator*=(long double val) { *this = *this * val; return *this; }
			ST_Wrapper &operator/=(long double val) { *this = *this / val; return *this; }

			// marks the ST register as empty
			void Free() noexcept { const_cast<Computer&>(c).FPU_tag |= 3 << (index * 2); }
		};
		struct const_ST_Wrapper : ST_Wrapper_common
		{
		private:

			// ST_Wrapper_common is the impl for const_ST_Wrapper - the distinction only exists so that we don't require virtual destructors.

			friend class Computer;
			const_ST_Wrapper(const Computer &_c, u32 _index) noexcept : ST_Wrapper_common(_c, _index) {}

		public:

			// creates a const wrapper from a non-const wrapper
			const_ST_Wrapper(ST_Wrapper wrap) noexcept : ST_Wrapper_common(wrap.c, wrap.index) {}
		};

	private: // -- data -- //

		void *mem;    // pointer to position 0 of memory array (alloc/dealloc with CSX64::aligned_malloc/free)
		u64 mem_size; // current size of memory array
		u64 mem_cap;  // current capacity of memory array (cap >= size) (size is the user-accessible portion)

		u64 min_mem_size; // memory size after initialization (acts as a minimum for sys_brk)
		u64 max_mem_size; // requested limit on memory size (acts as a maximum for sys_brk)

		u64 ExeBarrier;      // The barrier before which memory is executable
		u64 ReadonlyBarrier; // The barrier before which memory is read-only
		u64 StackBarrier;    // Gets the barrier before which the stack can't enter

		bool running;
		bool suspended_read;
		ErrorCode error;
		int return_value;

		CPURegister CPURegisters[16];
		u64 _RFLAGS, _RIP;

		long double FPURegisters[8];
		u16 FPU_control, FPU_status, FPU_tag;

		ZMMRegister ZMMRegisters[32];
		u32 _MXCSR;

		std::unique_ptr<IFileWrapper> FileDescriptors[FDCount];

		FastRNG Rand;

	public: // -- data access -- //

		// Gets the maximum amount of memory the client can request
		u64 MaxMemory() const noexcept { return max_mem_size; }
		// Sets the maximum amount of memory the client can request in the future. Does not impact the current memory array.
		void MaxMemory(u64 max) noexcept { max_mem_size = max; }

		// Gets the amount of memory (in bytes) the computer currently has access to
		u64 MemorySize() const noexcept { return mem_size; }

		// Flag marking if the program is still executing (still true even in halted state)
		bool Running() const noexcept { return running; }
		// Gets if the processor is awaiting data from an interactive stream
		bool SuspendedRead() const noexcept { return suspended_read; }
		// Gets the current error code
		ErrorCode Error() const noexcept { return error; }
		// The return value from the program after errorless termination
		int ReturnValue() const noexcept { return return_value; }

	public: // -- ctor/dtor -- //

		// Validates the machine for operation, but does not prepare it for execute (see Initialize)
		Computer() :
			mem(nullptr), mem_size(0), mem_cap(0), max_mem_size((u64)8 * 1024 * 1024 * 1024),
			running(false), error(ErrorCode::None),
			Rand((unsigned int)std::time(nullptr))
		{}
		virtual ~Computer() { CSX64::aligned_free(mem); }
		
		Computer(const Computer&) = delete;
		Computer(Computer&&) = delete;

		Computer &operator=(const Computer&) = delete;
		Computer &operator=(Computer&&) = delete;

	public: // -- exe interface -- //
		
		// reallocates the current array to be the specified size. returns true on success.
		// if preserve_contents is true, the contents of the result is identical up to the lesser of the current and requested sizes. otherwise the contents are undefined.
		// this will attempt to resize the array in-place; however, if force_realloc is true it will guarantee a full reallocation operation (e.g. for releasing extraneous memory).
		// on success, the memory size is updated to the specified size.
		// on failure, nothing is changed (as if the request was not made) - strong guarantee.
		bool realloc(u64 size, bool preserve_contents, bool force_realloc = false);

		// Initializes the computer for execution
		// exe       - the memory to load before starting execution (memory beyond this range is undefined)</param>
		// args      - the command line arguments to provide to the computer. pass null or empty array for none</param>
		// stacksize - the amount of additional space to allocate for the program's stack</param>
		// throws std::overflow_error if the memory size calculation results in an overflow.
		// throws MemoryAllocException if attempting to exceed max memory settings or if allocation fails.
		void Initialize(const Executable &exe, const std::vector<std::string> &args, u64 stacksize = 2 * 1024 * 1024);

		/// <summary>
		/// Performs a single operation. Returns the number of successful operations.
		/// Returning a lower number than requested (even zero) does not necessarily indicate termination or error.
		/// To check for termination/error, use <see cref="Running"/>.
		/// </summary>
		/// <param name="count">The maximum number of operations to perform</param>
		u64 Tick(u64 count);

		// Causes the machine to end execution with an error code and release various system resources (e.g. file handles).
		void Terminate(ErrorCode err = ErrorCode::None);
		// Causes the machine to end execution with a return value and release various system resources (e.g. file handles).
		void Exit(int ret = 0);

		// Unsets the suspended read state
		void ResumeSuspendedRead();

		// links the provided file to the first available file descriptor.
		// returns the file descriptor that was used. if none were available, does not link the file and returns -1.
		int OpenFileWrapper(std::unique_ptr<IFileWrapper> f);
		// links the provided file descriptor to the file (no bounds checking - see FDCount).
		// if the file descriptor is already in use, it is first closed.
		void OpenFileWrapper(int fd, std::unique_ptr<IFileWrapper> f);

		// closes the file wrapper with specified file descriptor (no bounds checking).
		void CloseFileWrapper(int fd);
		// Closes all the managed file descriptors and severs ties to unmanaged ones.
		void CloseFiles();

		// gets the wrapper object for the specified file descriptor. (no bounds checking - see FDCount)
		// this value is null iff the specified file descriptor is not in use.
		IFileWrapper *GetFileWrapper(int fd);

		// returns the lowest-index available file descriptor. if there are no available file descriptors, returns -1.
		int FindAvailableFD();

		// Writes a string containing all non-vpu register/flag states"/>
		std::ostream &WriteCPUDebugString(std::ostream &ostr);
		// Writes a string containing all vpu register states
		std::ostream &WriteVPUDebugString(std::ostream &ostr);
		// Writes a string containing both cpu and vpu info
		std::ostream &WriteFullDebugString(std::ostream &ostr);

	protected: // -- virtual functions -- //

		// this is what handles syscalls from client code.
		// you can overload this to add your own, but for the sake of users should preserve pre-existing syscall behavior.
		virtual bool ProcessSYSCALL();

		// these handle raw IO port data transfers
		// port is the IO port to use
		// dest/value hold the values to read/write from/to.
		// sizecode specifies transaction size (0=8 bit, 1=16 bit, 2=32 bit, 3=64 bit).
		virtual bool Input(u64 port, u64 &dest, u64 sizecode) { dest = 0; return true; }
		virtual bool Output(u64 port, u64 value, u64 sizecode) { return true; }

	private: // -- syscall functions -- //

		bool Process_sys_read();
		bool Process_sys_write();
		bool Process_sys_open();
		bool Process_sys_close();
		bool Process_sys_lseek();

		bool Process_sys_brk();

		bool Process_sys_rename();
		bool Process_sys_unlink();
		bool Process_sys_mkdir();
		bool Process_sys_rmdir();

	public: // -- public memory access -- //

		// Reads a C-style string from memory. Returns true if successful, otherwise fails with OutOfBounds and returns false
		bool GetCString(u64 pos, std::string &str);
		// Writes a C-style string to memory. Returns true if successful, otherwise fails with OutOfBounds and returns false
		bool SetCString(u64 pos, const std::string &str);

		// reads a POD type T from memory. returns true on success
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool GetMem(u64 pos, T &val)
		{
			if (pos >= mem_size || pos + sizeof(T) > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }

			val = bin_read<T>(reinterpret_cast<const char*>(mem) + pos); // aliasing ok because casting to char type

			return true;
		}
		// writes a POD type T to memory. returns true on success
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool SetMem(u64 pos, const T &val)
		{
			if (pos >= mem_size || pos + sizeof(T) > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
			if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation);  return false; }

			bin_write<T>(reinterpret_cast<char*>(mem) + pos, val); // aliasing ok because casting to char type

			return true;
		}

		// pushes a POD type T onto the stack. returns true on success
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool Push(const T &val)
		{
			RSP() -= sizeof(T);
			if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }
			if (!SetMem(RSP(), val)) return false;
			return true;
		}
		// pops a POD type T off the stack. returns true on success
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool Pop(T &val)
		{
			if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }
			if (!GetMem(RSP(), val)) return false;
			RSP() += sizeof(T);
			return true;
		}

	private: // -- exe memory utilities -- //

		// Pushes a raw value onto the stack
		bool PushRaw(u64 size, u64 val);
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool PushRaw(u64 val)
		{
			RSP() -= sizeof(T);
			if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }
			return SetMemRaw<T>(RSP(), val);
		}

		// Pops a value from the stack
		bool PopRaw(u64 size, u64 &val);
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool PopRaw(u64 &val)
		{
			if (RSP() < StackBarrier) { Terminate(ErrorCode::StackOverflow); return false; }
			if (!GetMemRaw<T>(RSP(), val)) return false;
			RSP() += sizeof(T);
			return true;
		}

		// reads a raw value from memory. returns true on success
		bool GetMemRaw(u64 pos, u64 size, u64 &res);
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool GetMemRaw(u64 pos, u64 &res)
		{
			if (pos >= mem_size || pos + sizeof(T) > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
			
			res = bin_read<T>(reinterpret_cast<const char*>(mem) + pos);

			return true;
		}

		// as GetMemRaw() but takes a sizecode instead of a size
		bool GetMemRaw_szc(u64 pos, u64 sizecode, u64 &res);

		// writes a raw value to memory. returns true on success
		bool SetMemRaw(u64 pos, u64 size, u64 val);
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool SetMemRaw(u64 pos, u64 val)
		{
			if (pos >= mem_size || pos + sizeof(T) > mem_size) { Terminate(ErrorCode::OutOfBounds); return false; }
			if (pos < ReadonlyBarrier) { Terminate(ErrorCode::AccessViolation); return false; }

			bin_write<T>(reinterpret_cast<char*>(mem) + pos, (T)val); // aliasing ok because casting to char type

			return true;
		}

		// as SetMemRaw() but takes a sizecode instead of a size
		bool SetMemRaw_szc(u64 pos, u64 sizecode, u64 val);

		// gets a value and advances the execution pointer. returns true on success
		bool GetMemAdv(u64 size, u64 &res);
		template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
		bool GetMemAdv(u64 &res)
		{
			if (!GetMemRaw<T>(RIP(), res)) return false;
			RIP() += sizeof(T);
			return true;
		}

		// as GetMemAdv() but takes a sizecode instead of a size
		bool GetMemAdv_szc(u64 sizecode, u64 &res);

		// reads a byte at RIP and advances the execution pointer
		bool GetByteAdv(u8 &res);

		// get a compact imm and advances the execution pointer. returns true on success.
		bool GetCompactImmAdv(u64 &res);

		// gets an address and advances the execution pointer. returns true on success
		bool GetAddressAdv(u64 &res);

	public: // -- register access -- //

		u64                        &RFLAGS() { return _RFLAGS; }
		BitfieldWrapper<u64, 0, 32> EFLAGS() { return {_RFLAGS}; }
		BitfieldWrapper<u64, 0, 16> FLAGS() { return {_RFLAGS}; }
		
		u64 RFLAGS() const { return _RFLAGS; }
		u32 EFLAGS() const { return (u32)_RFLAGS; }
		u16 FLAGS() const { return (u16)_RFLAGS; }

		u64                      &RIP() { return _RIP; }
		ReferenceRouter<u32, u64> EIP() { return {_RIP}; }
		ReferenceRouter<u16, u64> IP() { return {_RIP}; }

		u64 RIP() const { return _RIP; }
		u32 EIP() const { return (u32)_RIP; }
		u16 IP() const { return (u16)_RIP; }

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

		decltype(auto) EAX() { return CPURegisters[0].x32(); }
		decltype(auto) EBX() { return CPURegisters[1].x32(); }
		decltype(auto) ECX() { return CPURegisters[2].x32(); }
		decltype(auto) EDX() { return CPURegisters[3].x32(); }
		decltype(auto) ESI() { return CPURegisters[4].x32(); }
		decltype(auto) EDI() { return CPURegisters[5].x32(); }
		decltype(auto) EBP() { return CPURegisters[6].x32(); }
		decltype(auto) ESP() { return CPURegisters[7].x32(); }
		decltype(auto) R8D() { return CPURegisters[8].x32(); }
		decltype(auto) R9D() { return CPURegisters[9].x32(); }
		decltype(auto) R10D() { return CPURegisters[10].x32(); }
		decltype(auto) R11D() { return CPURegisters[11].x32(); }
		decltype(auto) R12D() { return CPURegisters[12].x32(); }
		decltype(auto) R13D() { return CPURegisters[13].x32(); }
		decltype(auto) R14D() { return CPURegisters[14].x32(); }
		decltype(auto) R15D() { return CPURegisters[15].x32(); }

		decltype(auto) AX() { return CPURegisters[0].x16(); }
		decltype(auto) BX() { return CPURegisters[1].x16(); }
		decltype(auto) CX() { return CPURegisters[2].x16(); }
		decltype(auto) DX() { return CPURegisters[3].x16(); }
		decltype(auto) SI() { return CPURegisters[4].x16(); }
		decltype(auto) DI() { return CPURegisters[5].x16(); }
		decltype(auto) BP() { return CPURegisters[6].x16(); }
		decltype(auto) SP() { return CPURegisters[7].x16(); }
		decltype(auto) R8W() { return CPURegisters[8].x16(); }
		decltype(auto) R9W() { return CPURegisters[9].x16(); }
		decltype(auto) R10W() { return CPURegisters[10].x16(); }
		decltype(auto) R11W() { return CPURegisters[11].x16(); }
		decltype(auto) R12W() { return CPURegisters[12].x16(); }
		decltype(auto) R13W() { return CPURegisters[13].x16(); }
		decltype(auto) R14W() { return CPURegisters[14].x16(); }
		decltype(auto) R15W() { return CPURegisters[15].x16(); }

		decltype(auto) AL() { return CPURegisters[0].x8(); }
		decltype(auto) BL() { return CPURegisters[1].x8(); }
		decltype(auto) CL() { return CPURegisters[2].x8(); }
		decltype(auto) DL() { return CPURegisters[3].x8(); }
		decltype(auto) SIL() { return CPURegisters[4].x8(); }
		decltype(auto) DIL() { return CPURegisters[5].x8(); }
		decltype(auto) BPL() { return CPURegisters[6].x8(); }
		decltype(auto) SPL() { return CPURegisters[7].x8(); }
		decltype(auto) R8B() { return CPURegisters[8].x8(); }
		decltype(auto) R9B() { return CPURegisters[9].x8(); }
		decltype(auto) R10B() { return CPURegisters[10].x8(); }
		decltype(auto) R11B() { return CPURegisters[11].x8(); }
		decltype(auto) R12B() { return CPURegisters[12].x8(); }
		decltype(auto) R13B() { return CPURegisters[13].x8(); }
		decltype(auto) R14B() { return CPURegisters[14].x8(); }
		decltype(auto) R15B() { return CPURegisters[15].x8(); }

		decltype(auto) AH() { return CPURegisters[0].x8h(); }
		decltype(auto) BH() { return CPURegisters[1].x8h(); }
		decltype(auto) CH() { return CPURegisters[2].x8h(); }
		decltype(auto) DH() { return CPURegisters[3].x8h(); }

		u64 RAX() const { return CPURegisters[0].x64(); }
		u64 RBX() const { return CPURegisters[1].x64(); }
		u64 RCX() const { return CPURegisters[2].x64(); }
		u64 RDX() const { return CPURegisters[3].x64(); }
		u64 RSI() const { return CPURegisters[4].x64(); }
		u64 RDI() const { return CPURegisters[5].x64(); }
		u64 RBP() const { return CPURegisters[6].x64(); }
		u64 RSP() const { return CPURegisters[7].x64(); }
		u64 R8() const { return CPURegisters[8].x64(); }
		u64 R9() const { return CPURegisters[9].x64(); }
		u64 R10() const { return CPURegisters[10].x64(); }
		u64 R11() const { return CPURegisters[11].x64(); }
		u64 R12() const { return CPURegisters[12].x64(); }
		u64 R13() const { return CPURegisters[13].x64(); }
		u64 R14() const { return CPURegisters[14].x64(); }
		u64 R15() const { return CPURegisters[15].x64(); }

		u32 EAX() const { return CPURegisters[0].x32(); }
		u32 EBX() const { return CPURegisters[1].x32(); }
		u32 ECX() const { return CPURegisters[2].x32(); }
		u32 EDX() const { return CPURegisters[3].x32(); }
		u32 ESI() const { return CPURegisters[4].x32(); }
		u32 EDI() const { return CPURegisters[5].x32(); }
		u32 EBP() const { return CPURegisters[6].x32(); }
		u32 ESP() const { return CPURegisters[7].x32(); }
		u32 R8D() const { return CPURegisters[8].x32(); }
		u32 R9D() const { return CPURegisters[9].x32(); }
		u32 R10D() const { return CPURegisters[10].x32(); }
		u32 R11D() const { return CPURegisters[11].x32(); }
		u32 R12D() const { return CPURegisters[12].x32(); }
		u32 R13D() const { return CPURegisters[13].x32(); }
		u32 R14D() const { return CPURegisters[14].x32(); }
		u32 R15D() const { return CPURegisters[15].x32(); }

		u16 AX() const { return CPURegisters[0].x16(); }
		u16 BX() const { return CPURegisters[1].x16(); }
		u16 CX() const { return CPURegisters[2].x16(); }
		u16 DX() const { return CPURegisters[3].x16(); }
		u16 SI() const { return CPURegisters[4].x16(); }
		u16 DI() const { return CPURegisters[5].x16(); }
		u16 BP() const { return CPURegisters[6].x16(); }
		u16 SP() const { return CPURegisters[7].x16(); }
		u16 R8W() const { return CPURegisters[8].x16(); }
		u16 R9W() const { return CPURegisters[9].x16(); }
		u16 R10W() const { return CPURegisters[10].x16(); }
		u16 R11W() const { return CPURegisters[11].x16(); }
		u16 R12W() const { return CPURegisters[12].x16(); }
		u16 R13W() const { return CPURegisters[13].x16(); }
		u16 R14W() const { return CPURegisters[14].x16(); }
		u16 R15W() const { return CPURegisters[15].x16(); }

		u8 AL() const { return CPURegisters[0].x8(); }
		u8 BL() const { return CPURegisters[1].x8(); }
		u8 CL() const { return CPURegisters[2].x8(); }
		u8 DL() const { return CPURegisters[3].x8(); }
		u8 SIL() const { return CPURegisters[4].x8(); }
		u8 DIL() const { return CPURegisters[5].x8(); }
		u8 BPL() const { return CPURegisters[6].x8(); }
		u8 SPL() const { return CPURegisters[7].x8(); }
		u8 R8B() const { return CPURegisters[8].x8(); }
		u8 R9B() const { return CPURegisters[9].x8(); }
		u8 R10B() const { return CPURegisters[10].x8(); }
		u8 R11B() const { return CPURegisters[11].x8(); }
		u8 R12B() const { return CPURegisters[12].x8(); }
		u8 R13B() const { return CPURegisters[13].x8(); }
		u8 R14B() const { return CPURegisters[14].x8(); }
		u8 R15B() const { return CPURegisters[15].x8(); }

		u8 AH() const { return CPURegisters[0].x8h(); }
		u8 BH() const { return CPURegisters[1].x8h(); }
		u8 CH() const { return CPURegisters[2].x8h(); }
		u8 DH() const { return CPURegisters[3].x8h(); }

		// source: https://en.wikipedia.org/wiki/FLAGS_register
		// source: http://www.eecg.toronto.edu/~amza/www.mindsec.com/files/x86regs.html

		FlagWrapper<u64, 0>         CF() { return {_RFLAGS}; }
		FlagWrapper<u64, 2>         PF() { return {_RFLAGS}; }
		FlagWrapper<u64, 4>         AF() { return {_RFLAGS}; }
		FlagWrapper<u64, 6>         ZF() { return {_RFLAGS}; }
		FlagWrapper<u64, 7>         SF() { return {_RFLAGS}; }
		FlagWrapper<u64, 8>         TF() { return {_RFLAGS}; }
		FlagWrapper<u64, 9>         IF() { return {_RFLAGS}; }
		FlagWrapper<u64, 10>        DF() { return {_RFLAGS}; }
		FlagWrapper<u64, 11>        OF() { return {_RFLAGS}; }
		BitfieldWrapper<u64, 12, 2> IOPL() { return {_RFLAGS}; }
		FlagWrapper<u64, 14>        NT() { return {_RFLAGS}; }

		FlagWrapper<u64, 16> RF() { return {_RFLAGS}; }
		FlagWrapper<u64, 17> VM() { return {_RFLAGS}; }
		FlagWrapper<u64, 18> AC() { return {_RFLAGS}; }
		FlagWrapper<u64, 19> VIF() { return {_RFLAGS}; }
		FlagWrapper<u64, 20> VIP() { return {_RFLAGS}; }
		FlagWrapper<u64, 21> ID() { return {_RFLAGS}; }

		bool CF() const { return _RFLAGS & FlagWrapper<u64, 0>::mask; }
		bool PF() const { return _RFLAGS & FlagWrapper<u64, 2>::mask; }
		bool AF() const { return _RFLAGS & FlagWrapper<u64, 4>::mask; }
		bool ZF() const { return _RFLAGS & FlagWrapper<u64, 6>::mask; }
		bool SF() const { return _RFLAGS & FlagWrapper<u64, 7>::mask; }
		bool TF() const { return _RFLAGS & FlagWrapper<u64, 8>::mask; }
		bool IF() const { return _RFLAGS & FlagWrapper<u64, 9>::mask; }
		bool DF() const { return _RFLAGS & FlagWrapper<u64, 10>::mask; }
		bool OF() const { return _RFLAGS & FlagWrapper<u64, 11>::mask; }
		u64 IOPL() const { return (_RFLAGS & BitfieldWrapper<u64, 12, 2>::mask) >> 12; }
		bool NT() const { return _RFLAGS & FlagWrapper<u64, 14>::mask; }

		bool RF() const { return _RFLAGS & FlagWrapper<u64, 16>::mask; }
		bool VM() const { return _RFLAGS & FlagWrapper<u64, 17>::mask; }
		bool AC() const { return _RFLAGS & FlagWrapper<u64, 18>::mask; }
		bool VIF() const { return _RFLAGS & FlagWrapper<u64, 19>::mask; }
		bool VIP() const { return _RFLAGS & FlagWrapper<u64, 20>::mask; }
		bool ID() const { return _RFLAGS & FlagWrapper<u64, 21>::mask; }

		// File System Flag - denotes if the client is allowed to perform potentially-dangerous file system syscalls (open, delete, mkdir, etc.)
		FlagWrapper<u64, 32> FSF() { return {_RFLAGS}; }
		bool                 FSF() const { return _RFLAGS & FlagWrapper<u64, 32>::mask; }

		// One-Tick-REP Flag - denotes if REP instructions are performed in a single tick (more efficient, but could result in expensive ticks)
		FlagWrapper<u64, 33> OTRF() { return {_RFLAGS}; }
		bool                 OTRF() const { return _RFLAGS & FlagWrapper<u64, 33>::mask; }

		bool cc_b() const { return CF(); }
		bool cc_be() const { return CF() || ZF(); }
		bool cc_a() const { return !CF() && !ZF(); }
		bool cc_ae() const { return !CF(); }

		bool cc_l() const { return SF() != OF(); }
		bool cc_le() const { return ZF() || SF() != OF(); }
		bool cc_g() const { return !ZF() && SF() == OF(); }
		bool cc_ge() const { return SF() == OF(); }

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

		bool FPU_IM() const { return FPU_control & FlagWrapper<u16, 0>::mask; }
		bool FPU_DM() const { return FPU_control & FlagWrapper<u16, 1>::mask; }
		bool FPU_ZM() const { return FPU_control & FlagWrapper<u16, 2>::mask; }
		bool FPU_OM() const { return FPU_control & FlagWrapper<u16, 3>::mask; }
		bool FPU_UM() const { return FPU_control & FlagWrapper<u16, 4>::mask; }
		bool FPU_PM() const { return FPU_control & FlagWrapper<u16, 5>::mask; }
		bool FPU_IEM() const { return FPU_control & FlagWrapper<u16, 7>::mask; }
		u16 FPU_PC() const { return (FPU_control & BitfieldWrapper<u16, 8, 2>::mask) >> 8; }
		u16 FPU_RC() const { return (FPU_control & BitfieldWrapper<u16, 10, 2>::mask) >> 10; }
		bool FPU_IC() const { return FPU_control & FlagWrapper<u16, 12>::mask; }

		bool FPU_I() const { return FPU_status & FlagWrapper<u16, 0>::mask; }
		bool FPU_D() const { return FPU_status & FlagWrapper<u16, 1>::mask; }
		bool FPU_Z() const { return FPU_status & FlagWrapper<u16, 2>::mask; }
		bool FPU_O() const { return FPU_status & FlagWrapper<u16, 3>::mask; }
		bool FPU_U() const { return FPU_status & FlagWrapper<u16, 4>::mask; }
		bool FPU_P() const { return FPU_status & FlagWrapper<u16, 5>::mask; }
		bool FPU_SF() const { return FPU_status & FlagWrapper<u16, 6>::mask; }
		bool FPU_IR() const { return FPU_status & FlagWrapper<u16, 7>::mask; }
		bool FPU_C0() const { return FPU_status & FlagWrapper<u16, 8>::mask; }
		bool FPU_C1() const { return FPU_status & FlagWrapper<u16, 9>::mask; }
		bool FPU_C2() const { return FPU_status & FlagWrapper<u16, 10>::mask; }
		u16 FPU_TOP() const { return (FPU_control & BitfieldWrapper<u16, 11, 3>::mask) >> 11; }
		bool FPU_C3() const { return FPU_status & FlagWrapper<u16, 14>::mask; }
		bool FPU_B() const { return FPU_status & FlagWrapper<u16, 15>::mask; }

		ST_Wrapper       ST(u64 num) { return {*this, (u32)((FPU_TOP() + num) & 7)}; }
		const_ST_Wrapper ST(u64 num) const { return {*this, (u32)((FPU_TOP() + num) & 7)}; }
		
		ZMMRegister       &ZMM(u64 num) { return ZMMRegisters[num]; }
		const ZMMRegister &ZMM(u64 num) const { return ZMMRegisters[num]; }

		u32 &MXCSR() { return _MXCSR; }
		u32  MXCSR() const { return _MXCSR; }

		FlagWrapper<u32, 0> MXCSR_IE() { return {_MXCSR}; }
		FlagWrapper<u32, 1> MXCSR_DE() { return {_MXCSR}; }
		FlagWrapper<u32, 2> MXCSR_ZE() { return {_MXCSR}; }
		FlagWrapper<u32, 3> MXCSR_OE() { return {_MXCSR}; }
		FlagWrapper<u32, 4> MXCSR_UE() { return {_MXCSR}; }
		FlagWrapper<u32, 5> MXCSR_PE() { return {_MXCSR}; }
		FlagWrapper<u32, 6> MXCSR_DAZ() { return {_MXCSR}; }
		FlagWrapper<u32, 7> MXCSR_IM() { return {_MXCSR}; }
		FlagWrapper<u32, 8> MXCSR_DM() { return {_MXCSR}; }
		FlagWrapper<u32, 9> MXCSR_ZM() { return {_MXCSR}; }
		FlagWrapper<u32, 10> MXCSR_OM() { return {_MXCSR}; }
		FlagWrapper<u32, 11> MXCSR_UM() { return {_MXCSR}; }
		FlagWrapper<u32, 12> MXCSR_PM() { return {_MXCSR}; }
		BitfieldWrapper<u32, 13, 2> MXCSR_RC() { return {_MXCSR}; }
		FlagWrapper<u32, 15> MXCSR_FTZ() { return {_MXCSR}; }

		bool MXCSR_IE() const { return _MXCSR & FlagWrapper<u32, 0>::mask; }
		bool MXCSR_DE() const { return _MXCSR & FlagWrapper<u32, 1>::mask; }
		bool MXCSR_ZE() const { return _MXCSR & FlagWrapper<u32, 2>::mask; }
		bool MXCSR_OE() const { return _MXCSR & FlagWrapper<u32, 3>::mask; }
		bool MXCSR_UE() const { return _MXCSR & FlagWrapper<u32, 4>::mask; }
		bool MXCSR_PE() const { return _MXCSR & FlagWrapper<u32, 5>::mask; }
		bool MXCSR_DAZ() const { return _MXCSR & FlagWrapper<u32, 6>::mask; }
		bool MXCSR_IM() const { return _MXCSR & FlagWrapper<u32, 7>::mask; }
		bool MXCSR_DM() const { return _MXCSR & FlagWrapper<u32, 8>::mask; }
		bool MXCSR_ZM() const { return _MXCSR & FlagWrapper<u32, 9>::mask; }
		bool MXCSR_OM() const { return _MXCSR & FlagWrapper<u32, 10>::mask; }
		bool MXCSR_UM() const { return _MXCSR & FlagWrapper<u32, 11>::mask; }
		bool MXCSR_PM() const { return _MXCSR & FlagWrapper<u32, 12>::mask; }
		u32 MXCSR_RC() const { return (_MXCSR & BitfieldWrapper<u32, 13, 2>::mask) >> 13; }
		bool MXCSR_FTZ() const { return _MXCSR & FlagWrapper<u32, 15>::mask; }

	private: // -- exe tables -- //

		// (even) parity table - used for updating PF flag
		static const bool parity_table[];

		// holds handlers for all 8-bit opcodes
		static bool(Computer::* const opcode_handlers[])();

		// types used for simd computation handlers
		typedef bool (Computer::* VPUBinaryDelegate)(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		typedef bool (Computer::* VPUUnaryDelegate)(u64 elem_sizecode, u64 &res, u64 a, u64 index);
		typedef bool (Computer::* VPUCVTDelegate)(u64 &res, u64 a);

		// holds cpu delegates for simd fp comparisons
		static const VPUBinaryDelegate __TryProcessVEC_FCMP_lookup[];

	private: // -- operators -- //

		/*
		[4: dest][2: size][1:dh][1: mem]   [size: imm]
		mem = 0: [1: sh][3:][4: src]    dest <- f(reg, imm)
		mem = 1: [address]              dest <- f(M[address], imm)
		(dh and sh mark AH, BH, CH, or DH for dest or src)
		*/
		bool FetchTernaryOpFormat(u64 &s, u64 &a, u64 &b);
		bool StoreTernaryOPFormat(u64 s, u64 res);

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
			bool get_a = true, int _a_sizecode = -1, int _b_sizecode = -1, bool allow_b_mem = true);
		bool StoreBinaryOpFormat(u64 s1, u64 s2, u64 m, u64 res);

		/*
		[4: dest][2: size][1: dh][1: mem]
		mem = 0:             dest <- f(dest)
		mem = 1: [address]   M[address] <- f(M[address])
		(dh marks AH, BH, CH, or DH for dest)
		*/
		bool FetchUnaryOpFormat(u64 &s, u64 &m, u64 &a, bool get_a = true, int _a_sizecode = -1);
		bool StoreUnaryOpFormat(u64 s, u64 m, u64 res);

		/*
		[4: dest][2: size][1: dh][1: mem]   [1: CL][1:][6: count]   ([address])
		*/
		bool FetchShiftOpFormat(u64 &s, u64 &m, u64 &val, u64 &count);
		bool StoreShiftOpFormat(u64 s, u64 m, u64 res);

		/*
		[4: reg][2: size][2: mode]
		mode = 0:               reg
		mode = 1:               h reg (AH, BH, CH, or DH)
		mode = 2: [size: imm]   imm
		mode = 3: [address]     M[address]
		*/
		bool FetchIMMRMFormat(u64 &s, u64 &a, int _a_sizecode = -1);

		/*
		[4: dest][2: size][1: dh][1: mem]   [1: src_1_h][3:][4: src_1]
		mem = 0: [1: src_2_h][3:][4: src_2]
		mem = 1: [address_src_2]
		*/
		bool FetchRR_RMFormat(u64 &s1, u64 &s2, u64 &dest, u64 &a, u64 &b);
		bool StoreRR_RMFormat(u64 s1, u64 res);

		// updates the flags for integral ops (identical for most integral ops)
		void UpdateFlagsZSP(u64 value, u64 sizecode);

		// -- impl -- //

		bool ProcessNOP() { return true; }
		bool ProcessHLT() { Terminate(ErrorCode::Abort); return true; }

		static inline constexpr u64 ModifiableFlags = 0x003f0fd5ul;

		bool ProcessSTLDF();

		bool ProcessFlagManip();

		bool ProcessSETcc();

		bool ProcessMOV();
		bool ProcessMOVcc();

		bool ProcessXCHG();

		bool ProcessJMP_raw(u64 &aft);
		bool ProcessJMP();
		bool ProcessJcc();
		bool ProcessLOOPcc();

		bool ProcessCALL();
		bool ProcessRET();

		bool ProcessPUSH();
		bool ProcessPOP();

		bool ProcessLEA();

		bool ProcessADD();
		bool ProcessSUB();

		bool ProcessSUB_raw(bool apply);

		bool ProcessMUL_x();
		bool ProcessMUL();
		bool ProcessMULX();
		bool ProcessIMUL();
		bool ProcessUnary_IMUL();
		bool ProcessBinary_IMUL();
		bool ProcessTernary_IMUL();

		bool ProcessDIV();
		bool ProcessIDIV();

		bool ProcessSHL();
		bool ProcessSHR();

		bool ProcessSAL();
		bool ProcessSAR();

		bool ProcessROL();
		bool ProcessROR();

		bool ProcessRCL();
		bool ProcessRCR();

		bool ProcessAND_raw(bool apply);

		bool ProcessAND();
		bool ProcessOR();
		bool ProcessXOR();

		bool ProcessINC();
		bool ProcessDEC();

		bool ProcessNEG();
		bool ProcessNOT();

		bool ProcessCMP();
		bool ProcessTEST();

		bool ProcessCMPZ();

		bool ProcessBSWAP();
		bool ProcessBEXTR();
		bool ProcessBLSI();
		bool ProcessBLSMSK();
		bool ProcessBLSR();
		bool ProcessANDN();

		bool ProcessBTx();

		bool ProcessCxy();
		bool ProcessMOVxX();

		bool ProcessADXX();
		bool ProcessAAX();

		bool __ProcessSTRING_MOVS(u64 sizecode);
		bool __ProcessSTRING_CMPS(u64 sizecode);
		bool __ProcessSTRING_LODS(u64 sizecode);
		bool __ProcessSTRING_STOS(u64 sizecode);
		bool __ProcessSTRING_SCAS(u64 sizecode);

		bool ProcessSTRING();

		bool __Process_BSx_common(u64 &s, u64 &src, u64 &sizecode);
		bool ProcessBSx();

		bool ProcessTZCNT();

		bool ProcessUD();

		bool ProcessIO();

		// -- floating point stuff -- //

		// Initializes the FPU as if by FINIT. always returns true
		bool FINIT();

		bool ProcessFCLEX();

		// Performs a round trip on the value based on the specified rounding mode (as per Intel x87)
		// val - the value to round
		// rc  - the rounding control field
		static long double PerformRoundTrip(long double val, u32 rc);

		bool FetchFPUBinaryFormat(u64 &s, long double &a, long double &b);
		bool StoreFPUBinaryFormat(u64 s, long double res);

		bool PushFPU(long double val);
		bool PopFPU(long double &val);
		bool PopFPU();

		bool ProcessFSTLD_WORD();

		bool ProcessFLD_const();
		bool ProcessFLD();

		bool ProcessFST();
		bool ProcessFXCH();
		bool ProcessFMOVcc();

		bool ProcessFADD();
		bool ProcessFSUB();
		bool ProcessFSUBR();

		bool ProcessFMUL();
		bool ProcessFDIV();
		bool ProcessFDIVR();

		bool ProcessF2XM1();
		bool ProcessFABS();
		bool ProcessFCHS();
		bool ProcessFPREM();
		bool ProcessFPREM1();
		bool ProcessFRNDINT();
		bool ProcessFSQRT();
		bool ProcessFYL2X();
		bool ProcessFYL2XP1();
		bool ProcessFXTRACT();
		bool ProcessFSCALE();

		bool ProcessFXAM();
		bool ProcessFTST();

		bool ProcessFCOM();

		bool ProcessFSIN();
		bool ProcessFCOS();
		bool ProcessFSINCOS();
		bool ProcessFPTAN();
		bool ProcessFPATAN();

		bool ProcessFINCDECSTP();
		bool ProcessFFREE();

		// -- vpu stuff -- //

		bool ProcessVPUMove();
		bool ProcessVPUBinary(u64 elem_size_mask, VPUBinaryDelegate func);
		bool ProcessVPUUnary(u64 elem_size_mask, VPUUnaryDelegate func);

		bool ProcessVPUCVT_packed(u64 elem_count, u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);

		bool ProcessVPUCVT_scalar_xmm_xmm(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		bool ProcessVPUCVT_scalar_xmm_reg(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		bool ProcessVPUCVT_scalar_xmm_mem(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		bool ProcessVPUCVT_scalar_reg_xmm(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		bool ProcessVPUCVT_scalar_reg_mem(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);

		bool __TryPerformVEC_FADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_FSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_FMUL(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_FDIV(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_FADD();
		bool TryProcessVEC_FSUB();
		bool TryProcessVEC_FMUL();
		bool TryProcessVEC_FDIV();

		bool __TryPerformVEC_AND(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_OR(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_XOR(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_ANDN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_AND();
		bool TryProcessVEC_OR();
		bool TryProcessVEC_XOR();
		bool TryProcessVEC_ANDN();

		bool __TryPerformVEC_ADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_ADDS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_ADDUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_ADD();
		bool TryProcessVEC_ADDS();
		bool TryProcessVEC_ADDUS();

		bool __TryPerformVEC_SUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_SUBS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryPerformVEC_SUBUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_SUB();
		bool TryProcessVEC_SUBS();
		bool TryProcessVEC_SUBUS();

		bool __TryPerformVEC_MULL(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_MULL();

		bool __TryProcessVEC_FMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_FMIN();
		bool TryProcessVEC_FMAX();

		bool __TryProcessVEC_UMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_SMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_UMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_SMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_UMIN();
		bool TryProcessVEC_SMIN();
		bool TryProcessVEC_UMAX();
		bool TryProcessVEC_SMAX();

		bool __TryPerformVEC_FADDSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_FADDSUB();

		bool __TryPerformVEC_AVG(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_AVG();

		bool __TryProcessVEC_FCMP_helper(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index,
			bool great, bool less, bool equal, bool unord, bool signal);
		
		bool __TryProcessVEC_FCMP_EQ_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_LT_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_LE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_UNORD_Q(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NEQ_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NLT_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NLE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_ORD_Q(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_EQ_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NGE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NGT_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_FALSE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NEQ_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_GE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_GT_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_TRUE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_EQ_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_LT_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_LE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_UNORD_S(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NEQ_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NLT_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NLE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_ORD_S(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_EQ_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NGE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NGT_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_FALSE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_NEQ_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_GE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_GT_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		bool __TryProcessVEC_FCMP_TRUE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		bool TryProcessVEC_FCMP();

		bool __TryProcessVEC_FCOMI(u64 elem_sizecode, u64 &res, u64 _a, u64 _b, u64 index);

		bool TryProcessVEC_FCOMI();

		bool __TryProcessVEC_FSQRT(u64 elem_sizecode, u64 &res, u64 a, u64 index);
		bool __TryProcessVEC_FRSQRT(u64 elem_sizecode, u64 &res, u64 a, u64 index);

		bool TryProcessVEC_FSQRT();
		bool TryProcessVEC_FRSQRT();

		bool __double_to_i32(u64 &res, u64 val);
		bool __single_to_i32(u64 &res, u64 val);

		bool __double_to_i64(u64 &res, u64 val);
		bool __single_to_i64(u64 &res, u64 val);

		bool __double_to_ti32(u64 &res, u64 val);
		bool __single_to_ti32(u64 &res, u64 val);

		bool __double_to_ti64(u64 &res, u64 val);
		bool __single_to_ti64(u64 &res, u64 val);

		bool __i32_to_double(u64 &res, u64 val);
		bool __i32_to_single(u64 &res, u64 val);

		bool __i64_to_double(u64 &res, u64 val);
		bool __i64_to_single(u64 &res, u64 val);

		bool __double_to_single(u64 &res, u64 val);
		bool __single_to_double(u64 &res, u64 val);

		bool TryProcessVEC_CVT();

		// -- misc instructions -- //

		bool TryProcessTRANS();

		bool ProcessDEBUG();
		bool ProcessUNKNOWN();
	};
}

#endif
