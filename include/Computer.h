#ifndef CSX64_COMPUTER_H
#define CSX64_COMPUTER_H

#include <iostream>
#include <memory>
#include <type_traits>
#include <iomanip>
#include <vector>

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
		static inline constexpr int FDCount = 16;
		
		// default value for max memory used by any given Computer
		static inline constexpr std::size_t DefaultMaxMemSize = (std::size_t)(sizeof(std::size_t) > sizeof(u32) ? (u64)8 * 1024 * 1024 * 1024 : (u64)2 * 1024 * 1024 * 1024);

		// CSX64 considers many valid, but non-intel things to be undefined behavior at runtime (e.g. 8-bit addressing).
		// however, these types of things are already blocked by the assembler.
		// if this is set to true, emits ErrorCode::UndefinedBehavior in these cases - otherwise permits them (more efficient).
		static inline constexpr bool StrictUND = false;

		// if set to true, uses mask unions to perform the UpdateFlagsZSP() function - otherwise uses flag accessors (slower)
		static inline constexpr bool FlagAccessMasking = true;

		// the bitmask representing flags from RFLAGS that are allowed to be modified manually by instructions such as POPF
		static inline constexpr u64 ModifiableFlags = 0x003f0fd5ul;

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

			operator fext() const { return c.FPURegisters[index]; }
			ST_Wrapper_common &operator=(const ST_Wrapper_common&) = delete;

			// ---------------------------------------- //

			static constexpr u16 tag_normal = 0;
			static constexpr u16 tag_zero = 1;
			static constexpr u16 tag_special = 2;
			static constexpr u16 tag_empty = 3;

			static u16 ComputeFPUTag(fext val)
			{
				if (std::isnan(val) || std::isinf(val) || detail::IsDenorm(val)) return tag_special;
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
				else ostr << std::defaultfloat << std::setprecision(17) << (fext)wrapper;

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

			ST_Wrapper &operator=(fext value)
			{
				// store value, accounting for precision control flag
				switch (c.FPU_PC())
				{
				case 0: value = (f64)value; break;
				case 2: value = (f64)value; break;
				}

				// const_cast does not violate const correctness - see above comment for private ctor
				const_cast<Computer&>(c).FPURegisters[index] = value;
				const_cast<Computer&>(c).FPU_tag = (c.FPU_tag & ~(3 << (index * 2))) | (ComputeFPUTag(value) << (index * 2));

				return *this;
			}
			ST_Wrapper &operator=(ST_Wrapper wrapper) { *this = (fext)wrapper; return *this; }

			ST_Wrapper &operator+=(fext val) { *this = *this + val; return *this; }
			ST_Wrapper &operator-=(fext val) { *this = *this - val; return *this; }
			ST_Wrapper &operator*=(fext val) { *this = *this * val; return *this; }
			ST_Wrapper &operator/=(fext val) { *this = *this / val; return *this; }

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

		std::vector<u8> mem; // internal execution memory

		std::size_t min_mem_size = 0;                 // memory size after initialization (acts as a minimum for sys_brk)
		std::size_t max_mem_size = DefaultMaxMemSize; // requested limit on memory size (acts as a maximum for sys_brk)

		std::size_t exe_barrier = 0;      // The barrier before which memory is executable
		std::size_t readonly_barrier = 0; // The barrier before which memory is read-only
		std::size_t stack_barrier = 0;    // Gets the barrier before which the stack can't enter
		
		bool _running = false;
		ErrorCode _error = ErrorCode::None;
		int _return_value = 0;

		detail::CPURegister CPURegisters[16];
		u64 _RFLAGS, _RIP;

		fext FPURegisters[8];
		u16 FPU_control, FPU_status, FPU_tag;

		detail::ZMMRegister ZMMRegisters[32];
		u32 _MXCSR;

		std::unique_ptr<IFileWrapper> FileDescriptors[FDCount];

		detail::FastRNG Rand;

	public: // -- data access -- //

		// gets the maximum amount of memory the client can request (bytes).
		[[nodiscard]] std::size_t max_memory() const noexcept { return max_mem_size; }
		// sets the maximum amount of memory the client can request in the future (bytes).
		// this does NOT impact the current memory array - intended to be set prior to initialize().
		void max_memory(std::size_t max) noexcept { max_mem_size = max; }

		// gets the amount of memory the computer is currently using (bytes).
		[[nodiscard]] std::size_t memory_size() const noexcept { return mem.size(); }

		// checks if the client program has not yet terminated due to exit or error.
		[[nodiscard]] bool running() const noexcept { return _running; }
		// checks the current error status: for running clients or clients that terminated errorlessly this is ErrorCode::None.
		[[nodiscard]] ErrorCode error() const noexcept { return _error; }
		// if the client has terminated errorlessly, this gets the return value - otherwise meaningless.
		[[nodiscard]] int return_value() const noexcept { return _return_value; }

	public: // -- ctor/dtor -- //

		// Validates the machine for operation, but does not prepare it for execute (see initialize())
		Computer() : Rand((unsigned int)std::time(nullptr)) {}
		virtual ~Computer() = default;
		
		Computer(const Computer&) = delete;
		Computer(Computer&&) = delete;

		Computer &operator=(const Computer&) = delete;
		Computer &operator=(Computer&&) = delete;

	public: // -- exe interface -- //
		
		// reallocates the current array to be the specified size. returns true on success.
		// if preserve_contents is true, the contents of the result is identical up to the lesser of the current and requested sizes. otherwise the contents are undefined.
		// on success, the memory size is updated to the specified size.
		// on failure, nothing is changed (as if the request was not made) - strong guarantee.
		bool realloc(std::size_t size, bool preserve_contents);

		// initializes the computer for execution
		// exe       - the memory to load before starting execution (memory beyond this range is undefined)</param>
		// args      - the command line arguments to provide to the computer. pass null or empty array for none</param>
		// stacksize - the amount of additional space to allocate for the program's stack</param>
		// throws std::overflow_error if the memory size calculation results in an overflow.
		// throws MemoryAllocException if attempting to exceed max memory settings or if allocation fails.
		void initialize(const Executable &exe, const std::vector<std::string> &args, std::size_t stacksize = 2 * 1024 * 1024);

		// performs up to the specified number of operations. returns the number of successful operations.
		// returning a lower number than requested (even zero) does not necessarily indicate termination or error.
		// to check for termination/error, use running() and error().
		u64 tick(u64 count);

		// Causes the machine to end execution with an error code and release various system resources (e.g. file handles).
		void terminate_err(ErrorCode err);
		// Causes the machine to end execution with a return value and release various system resources (e.g. file handles).
		void terminate_ok(int ret);

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
		[[nodiscard]] virtual bool syscall();

		// these handle raw IO port data transfers
		// port is the IO port to use
		// dest/value hold the values to read/write from/to.
		// sizecode specifies transaction size (0=8 bit, 1=16 bit, 2=32 bit, 3=64 bit).
		[[nodiscard]] virtual bool input(u16 port, u64 &dest, u64 sizecode) { dest = 0; return true; }
		[[nodiscard]] virtual bool output(u16 port, u64 value, u64 sizecode) { return true; }

	private: // -- builtin syscall functions -- //

		[[nodiscard]] bool Process_sys_read();
		[[nodiscard]] bool Process_sys_write();
		[[nodiscard]] bool Process_sys_open();
		[[nodiscard]] bool Process_sys_close();
		[[nodiscard]] bool Process_sys_lseek();

		[[nodiscard]] bool Process_sys_brk();

		[[nodiscard]] bool Process_sys_rename();
		[[nodiscard]] bool Process_sys_unlink();
		[[nodiscard]] bool Process_sys_mkdir();
		[[nodiscard]] bool Process_sys_rmdir();

	public: // -- public memory access -- //

		// reads a value from memory. returns true on success. on failure, val is not modified.
		template<typename T, typename U, std::enable_if_t<detail::is_int<T> && std::is_same_v<T, U>, int> = 0>
		[[nodiscard]] bool read_mem(u64 pos, U &val)
		{
			if (pos >= mem.size() || pos + sizeof(T) > mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }
			val = detail::read<T>(mem.data() + pos);
			return true;
		}
		// writes a value to memory. returns true on success. on failure, memory is not modified.
		template<typename T, typename U, std::enable_if_t<detail::is_int<T> && std::is_same_v<T, U>, int> = 0>
		[[nodiscard]] bool write_mem(u64 pos, U val)
		{
			if (pos >= mem.size() || pos + sizeof(T) > mem.size()) { terminate_err(ErrorCode::OutOfBounds); return false; }
			if (pos < readonly_barrier) { terminate_err(ErrorCode::AccessViolation);  return false; }
			detail::write<T>(mem.data() + pos, val);
			return true;
		}

		// pushes a value onto the stack. returns true on success. on failure, rsp and memory are not modified.
		template<typename T, typename U, std::enable_if_t<detail::is_int<T> && std::is_same_v<T, U>, int> = 0>
		[[nodiscard]] bool push_mem(U val)
		{
			if (RSP() < stack_barrier) { terminate_err(ErrorCode::StackOverflow); return false; }
			if (!write_mem<T>(RSP() - sizeof(T), val)) return false;
			RSP() -= sizeof(T);
			return true;
		}
		// pops a value off the stack. returns true on success. on failure rsp and val are not modified.
		template<typename T, typename U, std::enable_if_t<detail::is_int<T> && std::is_same_v<T, U>, int> = 0>
		[[nodiscard]] bool pop_mem(U &val)
		{
			if (RSP() < stack_barrier) { terminate_err(ErrorCode::StackOverflow); return false; }
			if (!read_mem<T>(RSP(), val)) return false;
			RSP() += sizeof(T);
			return true;
		}

		// reads a C-style string from memory. returns true if successful, otherwise fails with OutOfBounds and returns false.
		[[nodiscard]] bool read_str(u64 pos, std::string &str);
		// writes a C-style string to memory. returns true if successful, otherwise fails with OutOfBounds and returns false.
		[[nodiscard]] bool write_str(u64 pos, const std::string &str);

	private: // -- exe memory utilities -- //

		[[nodiscard]] bool PushRaw(u64 size, u64 val);
		[[nodiscard]] bool PopRaw(u64 size, u64 &val);

		[[nodiscard]] bool GetMemRaw(u64 pos, u64 size, u64 &res);
		[[nodiscard]] bool SetMemRaw(u64 pos, u64 size, u64 val);

		[[nodiscard]] bool GetMemRaw_szc(u64 pos, u64 sizecode, u64 &res);
		[[nodiscard]] bool SetMemRaw_szc(u64 pos, u64 sizecode, u64 val);

		// gets a value and advances the execution pointer. returns true on success
		[[nodiscard]] bool GetMemAdv(u64 size, u64 &res);

		template<typename T, typename U, std::enable_if_t<detail::is_int<T> && std::is_same_v<T, U>, int> = 0>
		[[nodiscard]] bool GetMemAdv(U &res)
		{
			if (!read_mem<T>(RIP(), res)) return false;
			RIP() += sizeof(T);
			return true;
		}

		// as GetMemAdv() but takes a sizecode instead of a size
		[[nodiscard]] bool GetMemAdv_szc(u64 sizecode, u64 &res);

		// gets an address and advances the execution pointer. returns true on success
		[[nodiscard]] bool GetAddressAdv(u64 &res);

	public: // -- register access -- //

		u64                                &RFLAGS() { return _RFLAGS; }
		detail::BitfieldWrapper<u64, 0, 32> EFLAGS() { return {_RFLAGS}; }
		detail::BitfieldWrapper<u64, 0, 16> FLAGS() { return {_RFLAGS}; }
		
		u64 RFLAGS() const { return _RFLAGS; }
		u32 EFLAGS() const { return (u32)_RFLAGS; }
		u16 FLAGS() const { return (u16)_RFLAGS; }

		u64                              &RIP() { return _RIP; }
		detail::ReferenceRouter<u32, u64> EIP() { return {_RIP}; }
		detail::ReferenceRouter<u16, u64> IP() { return {_RIP}; }

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

		detail::FlagWrapper<u64, 0>         CF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 2>         PF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 4>         AF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 6>         ZF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 7>         SF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 8>         TF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 9>         IF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 10>        DF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 11>        OF() { return {_RFLAGS}; }
		detail::BitfieldWrapper<u64, 12, 2> IOPL() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 14>        NT() { return {_RFLAGS}; }

		detail::FlagWrapper<u64, 16> RF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 17> VM() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 18> AC() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 19> VIF() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 20> VIP() { return {_RFLAGS}; }
		detail::FlagWrapper<u64, 21> ID() { return {_RFLAGS}; }

		bool CF() const { return _RFLAGS & detail::FlagWrapper<u64, 0>::mask; }
		bool PF() const { return _RFLAGS & detail::FlagWrapper<u64, 2>::mask; }
		bool AF() const { return _RFLAGS & detail::FlagWrapper<u64, 4>::mask; }
		bool ZF() const { return _RFLAGS & detail::FlagWrapper<u64, 6>::mask; }
		bool SF() const { return _RFLAGS & detail::FlagWrapper<u64, 7>::mask; }
		bool TF() const { return _RFLAGS & detail::FlagWrapper<u64, 8>::mask; }
		bool IF() const { return _RFLAGS & detail::FlagWrapper<u64, 9>::mask; }
		bool DF() const { return _RFLAGS & detail::FlagWrapper<u64, 10>::mask; }
		bool OF() const { return _RFLAGS & detail::FlagWrapper<u64, 11>::mask; }
		u64 IOPL() const { return (_RFLAGS & detail::BitfieldWrapper<u64, 12, 2>::mask) >> 12; }
		bool NT() const { return _RFLAGS & detail::FlagWrapper<u64, 14>::mask; }

		bool RF() const { return _RFLAGS & detail::FlagWrapper<u64, 16>::mask; }
		bool VM() const { return _RFLAGS & detail::FlagWrapper<u64, 17>::mask; }
		bool AC() const { return _RFLAGS & detail::FlagWrapper<u64, 18>::mask; }
		bool VIF() const { return _RFLAGS & detail::FlagWrapper<u64, 19>::mask; }
		bool VIP() const { return _RFLAGS & detail::FlagWrapper<u64, 20>::mask; }
		bool ID() const { return _RFLAGS & detail::FlagWrapper<u64, 21>::mask; }

		// File System Flag - denotes if the client is allowed to perform potentially-dangerous file system syscalls (open, delete, mkdir, etc.)
		detail::FlagWrapper<u64, 32> FSF() { return {_RFLAGS}; }
		bool                         FSF() const { return _RFLAGS & detail::FlagWrapper<u64, 32>::mask; }

		// One-Tick-REP Flag - denotes if REP instructions are performed in a single tick (more efficient, but could result in expensive ticks)
		detail::FlagWrapper<u64, 33> OTRF() { return {_RFLAGS}; }
		bool                         OTRF() const { return _RFLAGS & detail::FlagWrapper<u64, 33>::mask; }

		bool cc_b() const { return CF(); }
		bool cc_be() const { return CF() || ZF(); }
		bool cc_a() const { return !CF() && !ZF(); }
		bool cc_ae() const { return !CF(); }

		bool cc_l() const { return SF() != OF(); }
		bool cc_le() const { return ZF() || SF() != OF(); }
		bool cc_g() const { return !ZF() && SF() == OF(); }
		bool cc_ge() const { return SF() == OF(); }

		// source : http://www.website.masmforum.com/tutorials/fptute/fpuchap1.htm

		detail::FlagWrapper<u16, 0> FPU_IM() { return {FPU_control}; }
		detail::FlagWrapper<u16, 1> FPU_DM() { return {FPU_control}; }
		detail::FlagWrapper<u16, 2> FPU_ZM() { return {FPU_control}; }
		detail::FlagWrapper<u16, 3> FPU_OM() { return {FPU_control}; }
		detail::FlagWrapper<u16, 4> FPU_UM() { return {FPU_control}; }
		detail::FlagWrapper<u16, 5> FPU_PM() { return {FPU_control}; }
		detail::FlagWrapper<u16, 7> FPU_IEM() { return {FPU_control}; }
		detail::BitfieldWrapper<u16, 8, 2> FPU_PC() { return {FPU_control}; }
		detail::BitfieldWrapper<u16, 10, 2> FPU_RC() { return {FPU_control}; }
		detail::FlagWrapper<u16, 12> FPU_IC() { return {FPU_control}; }

		detail::FlagWrapper<u16, 0> FPU_I() { return {FPU_status}; }
		detail::FlagWrapper<u16, 1> FPU_D() { return {FPU_status}; }
		detail::FlagWrapper<u16, 2> FPU_Z() { return {FPU_status}; }
		detail::FlagWrapper<u16, 3> FPU_O() { return {FPU_status}; }
		detail::FlagWrapper<u16, 4> FPU_U() { return {FPU_status}; }
		detail::FlagWrapper<u16, 5> FPU_P() { return {FPU_status}; }
		detail::FlagWrapper<u16, 6> FPU_SF() { return {FPU_status}; }
		detail::FlagWrapper<u16, 7> FPU_IR() { return {FPU_status}; }
		detail::FlagWrapper<u16, 8> FPU_C0() { return {FPU_status}; }
		detail::FlagWrapper<u16, 9> FPU_C1() { return {FPU_status}; }
		detail::FlagWrapper<u16, 10> FPU_C2() { return {FPU_status}; }
		detail::BitfieldWrapper<u16, 11, 3> FPU_TOP() { return {FPU_control}; }
		detail::FlagWrapper<u16, 14> FPU_C3() { return {FPU_status}; }
		detail::FlagWrapper<u16, 15> FPU_B() { return {FPU_status}; }

		bool FPU_IM() const { return FPU_control & detail::FlagWrapper<u16, 0>::mask; }
		bool FPU_DM() const { return FPU_control & detail::FlagWrapper<u16, 1>::mask; }
		bool FPU_ZM() const { return FPU_control & detail::FlagWrapper<u16, 2>::mask; }
		bool FPU_OM() const { return FPU_control & detail::FlagWrapper<u16, 3>::mask; }
		bool FPU_UM() const { return FPU_control & detail::FlagWrapper<u16, 4>::mask; }
		bool FPU_PM() const { return FPU_control & detail::FlagWrapper<u16, 5>::mask; }
		bool FPU_IEM() const { return FPU_control & detail::FlagWrapper<u16, 7>::mask; }
		u16 FPU_PC() const { return (FPU_control & detail::BitfieldWrapper<u16, 8, 2>::mask) >> 8; }
		u16 FPU_RC() const { return (FPU_control & detail::BitfieldWrapper<u16, 10, 2>::mask) >> 10; }
		bool FPU_IC() const { return FPU_control & detail::FlagWrapper<u16, 12>::mask; }

		bool FPU_I() const { return FPU_status & detail::FlagWrapper<u16, 0>::mask; }
		bool FPU_D() const { return FPU_status & detail::FlagWrapper<u16, 1>::mask; }
		bool FPU_Z() const { return FPU_status & detail::FlagWrapper<u16, 2>::mask; }
		bool FPU_O() const { return FPU_status & detail::FlagWrapper<u16, 3>::mask; }
		bool FPU_U() const { return FPU_status & detail::FlagWrapper<u16, 4>::mask; }
		bool FPU_P() const { return FPU_status & detail::FlagWrapper<u16, 5>::mask; }
		bool FPU_SF() const { return FPU_status & detail::FlagWrapper<u16, 6>::mask; }
		bool FPU_IR() const { return FPU_status & detail::FlagWrapper<u16, 7>::mask; }
		bool FPU_C0() const { return FPU_status & detail::FlagWrapper<u16, 8>::mask; }
		bool FPU_C1() const { return FPU_status & detail::FlagWrapper<u16, 9>::mask; }
		bool FPU_C2() const { return FPU_status & detail::FlagWrapper<u16, 10>::mask; }
		u16 FPU_TOP() const { return (FPU_control & detail::BitfieldWrapper<u16, 11, 3>::mask) >> 11; }
		bool FPU_C3() const { return FPU_status & detail::FlagWrapper<u16, 14>::mask; }
		bool FPU_B() const { return FPU_status & detail::FlagWrapper<u16, 15>::mask; }

		// initializes the FPU as if by the FINIT instruction.
		void FINIT() { FPU_control = 0x3bf; FPU_status = 0; FPU_tag = 0xffff; }

		ST_Wrapper       ST(u64 num) { return {*this, (u32)((FPU_TOP() + num) & 7)}; }
		const_ST_Wrapper ST(u64 num) const { return {*this, (u32)((FPU_TOP() + num) & 7)}; }
		
		detail::ZMMRegister       &ZMM(u64 num) { return ZMMRegisters[num]; }
		const detail::ZMMRegister &ZMM(u64 num) const { return ZMMRegisters[num]; }

		u32 &MXCSR() { return _MXCSR; }
		u32  MXCSR() const { return _MXCSR; }

		detail::FlagWrapper<u32, 0> MXCSR_IE() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 1> MXCSR_DE() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 2> MXCSR_ZE() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 3> MXCSR_OE() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 4> MXCSR_UE() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 5> MXCSR_PE() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 6> MXCSR_DAZ() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 7> MXCSR_IM() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 8> MXCSR_DM() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 9> MXCSR_ZM() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 10> MXCSR_OM() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 11> MXCSR_UM() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 12> MXCSR_PM() { return {_MXCSR}; }
		detail::BitfieldWrapper<u32, 13, 2> MXCSR_RC() { return {_MXCSR}; }
		detail::FlagWrapper<u32, 15> MXCSR_FTZ() { return {_MXCSR}; }

		bool MXCSR_IE() const { return _MXCSR & detail::FlagWrapper<u32, 0>::mask; }
		bool MXCSR_DE() const { return _MXCSR & detail::FlagWrapper<u32, 1>::mask; }
		bool MXCSR_ZE() const { return _MXCSR & detail::FlagWrapper<u32, 2>::mask; }
		bool MXCSR_OE() const { return _MXCSR & detail::FlagWrapper<u32, 3>::mask; }
		bool MXCSR_UE() const { return _MXCSR & detail::FlagWrapper<u32, 4>::mask; }
		bool MXCSR_PE() const { return _MXCSR & detail::FlagWrapper<u32, 5>::mask; }
		bool MXCSR_DAZ() const { return _MXCSR & detail::FlagWrapper<u32, 6>::mask; }
		bool MXCSR_IM() const { return _MXCSR & detail::FlagWrapper<u32, 7>::mask; }
		bool MXCSR_DM() const { return _MXCSR & detail::FlagWrapper<u32, 8>::mask; }
		bool MXCSR_ZM() const { return _MXCSR & detail::FlagWrapper<u32, 9>::mask; }
		bool MXCSR_OM() const { return _MXCSR & detail::FlagWrapper<u32, 10>::mask; }
		bool MXCSR_UM() const { return _MXCSR & detail::FlagWrapper<u32, 11>::mask; }
		bool MXCSR_PM() const { return _MXCSR & detail::FlagWrapper<u32, 12>::mask; }
		u32 MXCSR_RC() const { return (_MXCSR & detail::BitfieldWrapper<u32, 13, 2>::mask) >> 13; }
		bool MXCSR_FTZ() const { return _MXCSR & detail::FlagWrapper<u32, 15>::mask; }

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
		static const VPUBinaryDelegate _TryProcessVEC_FCMP_lookup[];

	private: // -- operators -- //

		/*
		[4: dest][2: size][1:dh][1: mem]   [size: imm]
		mem = 0: [1: sh][3:][4: src]    dest <- f(reg, imm)
		mem = 1: [address]              dest <- f(M[address], imm)
		(dh and sh mark AH, BH, CH, or DH for dest or src)
		*/
		[[nodiscard]] bool FetchTernaryOpFormat(u8 &s, u64 &a, u64 &b);
		[[nodiscard]] bool StoreTernaryOPFormat(u8 s, u64 res);

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
		[[nodiscard]] bool FetchBinaryOpFormat(u8 &s1, u8 &s2, u64 &m, u64 &a, u64 &b, bool get_a = true, int _a_sizecode = -1, int _b_sizecode = -1, bool allow_b_mem = true);
		[[nodiscard]] bool StoreBinaryOpFormat(u8 s1, u8 s2, u64 m, u64 res);

		/*
		[4: dest][2: size][1: dh][1: mem]
		mem = 0:             dest <- f(dest)
		mem = 1: [address]   M[address] <- f(M[address])
		(dh marks AH, BH, CH, or DH for dest)
		*/
		[[nodiscard]] bool FetchUnaryOpFormat(u8 &s, u64 &m, u64 &a, bool get_a = true, int _a_sizecode = -1);
		[[nodiscard]] bool StoreUnaryOpFormat(u8 s, u64 m, u64 res);

		/*
		[4: dest][2: size][1: dh][1: mem]   [1: CL][1:][6: count]   ([address])
		*/
		[[nodiscard]] bool FetchShiftOpFormat(u8 &s, u64 &m, u64 &val, u8 &count);
		[[nodiscard]] bool StoreShiftOpFormat(u8 s, u64 m, u64 res);

		/*
		[4: reg][2: size][2: mode]
		mode = 0:               reg
		mode = 1:               h reg (AH, BH, CH, or DH)
		mode = 2: [size: imm]   imm
		mode = 3: [address]     M[address]
		*/
		[[nodiscard]] bool FetchIMMRMFormat(u8 &s, u64 &a, int _a_sizecode = -1);

		/*
		[4: dest][2: size][1: dh][1: mem]   [1: src_1_h][3:][4: src_1]
		mem = 0: [1: src_2_h][3:][4: src_2]
		mem = 1: [address_src_2]
		*/
		[[nodiscard]] bool FetchRR_RMFormat(u8 &s1, u8 &s2, u64 &dest, u64 &a, u64 &b);
		[[nodiscard]] bool StoreRR_RMFormat(u8 s1, u64 res);

		// updates the flags for integral ops (identical for most integral ops)
		void UpdateFlagsZSP(u64 value, u64 sizecode);

		// -- impl -- //

		[[nodiscard]] bool ProcessSYSCALL() { return syscall(); }

		[[nodiscard]] bool ProcessNOP() { return true; }
		[[nodiscard]] bool ProcessHLT() { return false; }

		[[nodiscard]] bool ProcessSTLDF();

		[[nodiscard]] bool ProcessFlagManip();

		[[nodiscard]] bool ProcessSETcc();

		[[nodiscard]] bool ProcessMOV();
		[[nodiscard]] bool ProcessMOVcc();

		[[nodiscard]] bool ProcessXCHG();

		[[nodiscard]] bool ProcessJMP_raw(u64 &aft);
		[[nodiscard]] bool ProcessJMP();
		[[nodiscard]] bool ProcessJcc();
		[[nodiscard]] bool ProcessLOOPcc();

		[[nodiscard]] bool ProcessCALL();
		[[nodiscard]] bool ProcessRET();

		[[nodiscard]] bool ProcessPUSH();
		[[nodiscard]] bool ProcessPOP();

		[[nodiscard]] bool ProcessLEA();

		[[nodiscard]] bool ProcessADD();
		[[nodiscard]] bool ProcessSUB();

		[[nodiscard]] bool ProcessSUB_raw(bool apply);

		[[nodiscard]] bool ProcessMUL_x();
		[[nodiscard]] bool ProcessMUL();
		[[nodiscard]] bool ProcessMULX();
		[[nodiscard]] bool ProcessIMUL();
		[[nodiscard]] bool ProcessUnary_IMUL();
		[[nodiscard]] bool ProcessBinary_IMUL();
		[[nodiscard]] bool ProcessTernary_IMUL();

		[[nodiscard]] bool ProcessDIV();
		[[nodiscard]] bool ProcessIDIV();

		[[nodiscard]] bool ProcessSHL();
		[[nodiscard]] bool ProcessSHR();

		[[nodiscard]] bool ProcessSAL();
		[[nodiscard]] bool ProcessSAR();

		[[nodiscard]] bool ProcessROL();
		[[nodiscard]] bool ProcessROR();

		[[nodiscard]] bool ProcessRCL();
		[[nodiscard]] bool ProcessRCR();

		[[nodiscard]] bool ProcessAND_raw(bool apply);

		[[nodiscard]] bool ProcessAND();
		[[nodiscard]] bool ProcessOR();
		[[nodiscard]] bool ProcessXOR();

		[[nodiscard]] bool ProcessINC();
		[[nodiscard]] bool ProcessDEC();

		[[nodiscard]] bool ProcessNEG();
		[[nodiscard]] bool ProcessNOT();

		[[nodiscard]] bool ProcessCMP();
		[[nodiscard]] bool ProcessTEST();

		[[nodiscard]] bool ProcessCMPZ();

		[[nodiscard]] bool ProcessBSWAP();
		[[nodiscard]] bool ProcessBEXTR();
		[[nodiscard]] bool ProcessBLSI();
		[[nodiscard]] bool ProcessBLSMSK();
		[[nodiscard]] bool ProcessBLSR();
		[[nodiscard]] bool ProcessANDN();

		[[nodiscard]] bool ProcessBTx();

		[[nodiscard]] bool ProcessCxy();
		[[nodiscard]] bool ProcessMOVxX();

		[[nodiscard]] bool ProcessADXX();
		[[nodiscard]] bool ProcessAAX();

		[[nodiscard]] bool _ProcessSTRING_MOVS(u64 sizecode);
		[[nodiscard]] bool _ProcessSTRING_CMPS(u64 sizecode);
		[[nodiscard]] bool _ProcessSTRING_LODS(u64 sizecode);
		[[nodiscard]] bool _ProcessSTRING_STOS(u64 sizecode);
		[[nodiscard]] bool _ProcessSTRING_SCAS(u64 sizecode);

		[[nodiscard]] bool ProcessSTRING();

		[[nodiscard]] bool _Process_BSx_common(u8 &s, u64 &src, u64 &sizecode);
		[[nodiscard]] bool ProcessBSx();

		[[nodiscard]] bool ProcessTZCNT();

		[[nodiscard]] bool ProcessUD();

		[[nodiscard]] bool ProcessIO();

		// -- floating point stuff -- //

		[[nodiscard]] bool ProcessFINIT();

		[[nodiscard]] bool ProcessFCLEX();

		// Performs a round trip on the value based on the specified rounding mode (as per Intel x87)
		// val - the value to round
		// rc  - the rounding control field
		[[nodiscard]] static fext PerformRoundTrip(fext val, u32 rc);

		[[nodiscard]] bool FetchFPUBinaryFormat(u8 &s, fext &a, fext &b);
		[[nodiscard]] bool StoreFPUBinaryFormat(u8 s, fext res);

		[[nodiscard]] bool PushFPU(fext val);
		[[nodiscard]] bool PopFPU(fext &val);
		[[nodiscard]] bool PopFPU();

		[[nodiscard]] bool ProcessFSTLD_WORD();

		[[nodiscard]] bool ProcessFLD_const();
		[[nodiscard]] bool ProcessFLD();

		[[nodiscard]] bool ProcessFST();
		[[nodiscard]] bool ProcessFXCH();
		[[nodiscard]] bool ProcessFMOVcc();

		[[nodiscard]] bool ProcessFADD();
		[[nodiscard]] bool ProcessFSUB();
		[[nodiscard]] bool ProcessFSUBR();

		[[nodiscard]] bool ProcessFMUL();
		[[nodiscard]] bool ProcessFDIV();
		[[nodiscard]] bool ProcessFDIVR();

		[[nodiscard]] bool ProcessF2XM1();
		[[nodiscard]] bool ProcessFABS();
		[[nodiscard]] bool ProcessFCHS();
		[[nodiscard]] bool ProcessFPREM();
		[[nodiscard]] bool ProcessFPREM1();
		[[nodiscard]] bool ProcessFRNDINT();
		[[nodiscard]] bool ProcessFSQRT();
		[[nodiscard]] bool ProcessFYL2X();
		[[nodiscard]] bool ProcessFYL2XP1();
		[[nodiscard]] bool ProcessFXTRACT();
		[[nodiscard]] bool ProcessFSCALE();

		[[nodiscard]] bool ProcessFXAM();
		[[nodiscard]] bool ProcessFTST();

		[[nodiscard]] bool ProcessFCOM();

		[[nodiscard]] bool ProcessFSIN();
		[[nodiscard]] bool ProcessFCOS();
		[[nodiscard]] bool ProcessFSINCOS();
		[[nodiscard]] bool ProcessFPTAN();
		[[nodiscard]] bool ProcessFPATAN();

		[[nodiscard]] bool ProcessFINCDECSTP();
		[[nodiscard]] bool ProcessFFREE();

		// -- vpu stuff -- //

		[[nodiscard]] bool ProcessVPUMove();
		[[nodiscard]] bool ProcessVPUBinary(u64 elem_size_mask, VPUBinaryDelegate func);
		[[nodiscard]] bool ProcessVPUUnary(u64 elem_size_mask, VPUUnaryDelegate func);

		[[nodiscard]] bool ProcessVPUCVT_packed(u64 elem_count, u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);

		[[nodiscard]] bool ProcessVPUCVT_scalar_xmm_xmm(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		[[nodiscard]] bool ProcessVPUCVT_scalar_xmm_reg(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		[[nodiscard]] bool ProcessVPUCVT_scalar_xmm_mem(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		[[nodiscard]] bool ProcessVPUCVT_scalar_reg_xmm(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);
		[[nodiscard]] bool ProcessVPUCVT_scalar_reg_mem(u64 to_elem_sizecode, u64 from_elem_sizecode, VPUCVTDelegate func);

		[[nodiscard]] bool _TryPerformVEC_FADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_FSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_FMUL(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_FDIV(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_FADD();
		[[nodiscard]] bool TryProcessVEC_FSUB();
		[[nodiscard]] bool TryProcessVEC_FMUL();
		[[nodiscard]] bool TryProcessVEC_FDIV();

		[[nodiscard]] bool _TryPerformVEC_AND(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_OR(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_XOR(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_ANDN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_AND();
		[[nodiscard]] bool TryProcessVEC_OR();
		[[nodiscard]] bool TryProcessVEC_XOR();
		[[nodiscard]] bool TryProcessVEC_ANDN();

		[[nodiscard]] bool _TryPerformVEC_ADD(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_ADDS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_ADDUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_ADD();
		[[nodiscard]] bool TryProcessVEC_ADDS();
		[[nodiscard]] bool TryProcessVEC_ADDUS();

		[[nodiscard]] bool _TryPerformVEC_SUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_SUBS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryPerformVEC_SUBUS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_SUB();
		[[nodiscard]] bool TryProcessVEC_SUBS();
		[[nodiscard]] bool TryProcessVEC_SUBUS();

		[[nodiscard]] bool _TryPerformVEC_MULL(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_MULL();

		[[nodiscard]] bool _TryProcessVEC_FMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_FMIN();
		[[nodiscard]] bool TryProcessVEC_FMAX();

		[[nodiscard]] bool _TryProcessVEC_UMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_SMIN(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_UMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_SMAX(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_UMIN();
		[[nodiscard]] bool TryProcessVEC_SMIN();
		[[nodiscard]] bool TryProcessVEC_UMAX();
		[[nodiscard]] bool TryProcessVEC_SMAX();

		[[nodiscard]] bool _TryPerformVEC_FADDSUB(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_FADDSUB();

		[[nodiscard]] bool _TryPerformVEC_AVG(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_AVG();

		[[nodiscard]] bool _TryProcessVEC_FCMP_helper(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index, bool great, bool less, bool equal, bool unord, bool signal);
		
		[[nodiscard]] bool _TryProcessVEC_FCMP_EQ_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_LT_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_LE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_UNORD_Q(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NEQ_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NLT_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NLE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_ORD_Q(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_EQ_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NGE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NGT_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_FALSE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NEQ_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_GE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_GT_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_TRUE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_EQ_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_LT_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_LE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_UNORD_S(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NEQ_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NLT_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NLE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_ORD_S(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_EQ_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NGE_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NGT_UQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_FALSE_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_NEQ_OS(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_GE_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_GT_OQ(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FCMP_TRUE_US(u64 elem_sizecode, u64 &res, u64 a, u64 b, u64 index);

		[[nodiscard]] bool TryProcessVEC_FCMP();

		[[nodiscard]] bool _TryProcessVEC_FCOMI(u64 elem_sizecode, u64 &res, u64 _a, u64 _b, u64 index);

		[[nodiscard]] bool TryProcessVEC_FCOMI();

		[[nodiscard]] bool _TryProcessVEC_FSQRT(u64 elem_sizecode, u64 &res, u64 a, u64 index);
		[[nodiscard]] bool _TryProcessVEC_FRSQRT(u64 elem_sizecode, u64 &res, u64 a, u64 index);

		[[nodiscard]] bool TryProcessVEC_FSQRT();
		[[nodiscard]] bool TryProcessVEC_FRSQRT();

		[[nodiscard]] bool _f64_to_i32(u64 &res, u64 val);
		[[nodiscard]] bool _single_to_i32(u64 &res, u64 val);

		[[nodiscard]] bool _f64_to_i64(u64 &res, u64 val);
		[[nodiscard]] bool _single_to_i64(u64 &res, u64 val);

		[[nodiscard]] bool _f64_to_ti32(u64 &res, u64 val);
		[[nodiscard]] bool _single_to_ti32(u64 &res, u64 val);

		[[nodiscard]] bool _f64_to_ti64(u64 &res, u64 val);
		[[nodiscard]] bool _single_to_ti64(u64 &res, u64 val);

		[[nodiscard]] bool _i32_to_f64(u64 &res, u64 val);
		[[nodiscard]] bool _i32_to_single(u64 &res, u64 val);

		[[nodiscard]] bool _i64_to_f64(u64 &res, u64 val);
		[[nodiscard]] bool _i64_to_single(u64 &res, u64 val);

		[[nodiscard]] bool _f64_to_single(u64 &res, u64 val);
		[[nodiscard]] bool _single_to_f64(u64 &res, u64 val);

		[[nodiscard]] bool TryProcessVEC_CVT();

		// -- misc instructions -- //

		[[nodiscard]] bool TryProcessTRANS();

		[[nodiscard]] bool ProcessDEBUG();
		[[nodiscard]] bool ProcessUNKNOWN();
	};
}

#endif
