#ifndef CSX64_TYPES_H
#define CSX64_TYPES_H

#include <cstdint>
#include <iostream>
#include <fstream>
#include <exception>
#include <algorithm>

#include "CoreTypes.h"

namespace CSX64
{
	enum class ErrorCode
	{
		None, OutOfBounds, UnhandledSyscall, UndefinedBehavior, ArithmeticError, Abort,
		IOFailure, FSDisabled, AccessViolation, InsufficientFDs, FDNotInUse, NotImplemented, StackOverflow,
		FPUStackOverflow, FPUStackUnderflow, FPUError, FPUAccessViolation,
		AlignmentViolation, UnknownOp,
	};
	enum class SyscallCode
	{
		sys_exit,

		sys_read, sys_write,
		sys_open, sys_close,
		sys_lseek,

		sys_brk,

		sys_rename, sys_unlink,
		sys_mkdir, sys_rmdir,
	};
	enum class OpenFlags
	{
		// access flags
		read = 1,
		write = 2,
		read_write = 3,

		// creation flags
		create = 4,
		temp = 8,
		trunc = 16,

		// status flags
		append = 32,
	};
	enum class SeekMode
	{
		set, cur, end
	};

	// acts as a reference to T, but gets/sets the value from a location of type U
	// meant for use with integral types
	template<typename T, typename U>
	struct ReferenceRouter
	{
		U &dest;

		inline constexpr operator T() const noexcept { return (T)dest; }
		inline constexpr ReferenceRouter operator=(T val) noexcept { dest = val; return *this; }
		inline constexpr ReferenceRouter operator=(ReferenceRouter wrapper) noexcept { dest = (T)wrapper.dest; return *this; }

		inline constexpr ReferenceRouter operator++() noexcept { *this = *this + 1; return *this; }
		inline constexpr ReferenceRouter operator--() noexcept { *this = *this - 1; return *this; }

		inline constexpr T operator++(int) noexcept { T val = *this; *this = *this + 1; return val; }
		inline constexpr T operator--(int) noexcept { T val = *this; *this = *this - 1; return val; }

		inline constexpr ReferenceRouter operator+=(T val) noexcept { *this = *this + val; return *this; }
		inline constexpr ReferenceRouter operator-=(T val) noexcept { *this = *this - val; return *this; }
		inline constexpr ReferenceRouter operator*=(T val) noexcept { *this = *this * val; return *this; }

		inline constexpr ReferenceRouter operator/=(T val) { *this = *this / val; return *this; }
		inline constexpr ReferenceRouter operator%=(T val) { *this = *this % val; return *this; }

		inline constexpr ReferenceRouter operator&=(T val) noexcept { *this = *this & val; return *this; }
		inline constexpr ReferenceRouter operator|=(T val) noexcept { *this = *this | val; return *this; }
		inline constexpr ReferenceRouter operator^=(T val) noexcept { *this = *this ^ val; return *this; }

		inline constexpr ReferenceRouter operator<<=(int val) noexcept { *this = *this << val; return *this; }
		inline constexpr ReferenceRouter operator>>=(int val) noexcept { *this = *this >> val; return *this; }
	};
	// acts as a reference to a bitfield in an integer of type "T"
	template<typename T, int pos, int len>
	struct BitfieldWrapper
	{
		T &data;

		static constexpr T __mask = (T)1 << (len - 1);
		static constexpr T low_mask = __mask | (__mask - 1);
		static constexpr T mask = low_mask << pos;

		inline constexpr operator T() const noexcept { return (data >> pos) & low_mask; }
		inline constexpr BitfieldWrapper operator=(T val) noexcept { data = (data & ~mask) | ((val & low_mask) << pos); return *this; }
		inline constexpr BitfieldWrapper operator=(BitfieldWrapper wrapper) noexcept { data = (data & ~mask) | (wrapper.data & mask); return *this; }

		inline constexpr BitfieldWrapper operator++() noexcept { *this = *this + 1; return *this; }
		inline constexpr BitfieldWrapper operator--() noexcept { *this = *this - 1; return *this; }

		inline constexpr T operator++(int) noexcept { T val = *this; *this = *this + 1; return val; }
		inline constexpr T operator--(int) noexcept { T val = *this; *this = *this - 1; return val; }

		inline constexpr BitfieldWrapper operator+=(T val) noexcept { *this = *this + val; return *this; }
		inline constexpr BitfieldWrapper operator-=(T val) noexcept { *this = *this - val; return *this; }
		inline constexpr BitfieldWrapper operator*=(T val) noexcept { *this = *this * val; return *this; }

		inline constexpr BitfieldWrapper operator/=(T val) { *this = *this / val; return *this; }
		inline constexpr BitfieldWrapper operator%=(T val) { *this = *this % val; return *this; }

		inline constexpr BitfieldWrapper operator&=(T val) noexcept { *this = *this & val; return *this; }
		inline constexpr BitfieldWrapper operator|=(T val) noexcept { *this = *this | val; return *this; }
		inline constexpr BitfieldWrapper operator^=(T val) noexcept { *this = *this ^ val; return *this; }

		inline constexpr BitfieldWrapper operator<<=(int val) noexcept { *this = *this << val; return *this; }
		inline constexpr BitfieldWrapper operator>>=(int val) noexcept { *this = *this >> val; return *this; }
	};
	// acts a reference to a 1-bit flag in an integer of type "T"
	template<typename T, int pos>
	struct FlagWrapper
	{
		T &data;

		static constexpr T mask = (T)1 << pos;

		inline constexpr operator bool() const noexcept { return data & mask; }
		inline constexpr FlagWrapper operator=(bool val) noexcept { data = val ? data | mask : data & ~mask; return *this; }
		inline constexpr FlagWrapper operator=(FlagWrapper wrapper) noexcept { data = (data & ~mask) | (wrapper.data & mask); return *this; }
	};

	struct CPURegister_sizecode_wrapper;
	// Represents a 64-bit register
	struct CPURegister
	{
	private:
		u64 data;

	public:
		inline constexpr u64 &x64() noexcept { return data; }
		inline constexpr ReferenceRouter<u32, u64> x32() noexcept { return {data}; }
		inline constexpr u16 &x16() noexcept { return *(u16*)&data; }
		inline constexpr u8 &x8() noexcept { return *(u8*)&data; }

		inline constexpr u8 &x8h() noexcept { return *((u8*)&data + 1); }

		// Gets/sets the register partition with the specified size code
		inline CPURegister_sizecode_wrapper operator[](u64 sizecode);
	};
	struct CPURegister_sizecode_wrapper
	{
		CPURegister &reg;
		u8 sizecode;

		inline constexpr operator u64() const
		{
			switch (sizecode)
			{
			case 0: return reg.x8();
			case 1: return reg.x16();
			case 2: return reg.x32();
			case 3: return reg.x64();

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr CPURegister_sizecode_wrapper &operator=(u64 value)
		{
			switch (sizecode)
			{
			case 0: reg.x8() = (u8)value; return *this;
			case 1: reg.x16() = (u16)value; return *this;
			case 2: reg.x32() = (u32)value; return *this;
			case 3: reg.x64() = (u64)value; return *this;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr CPURegister_sizecode_wrapper &operator=(CPURegister_sizecode_wrapper other) { *this = (u64)other; return *this; }
	};
	CPURegister_sizecode_wrapper CPURegister::operator[](u64 sizecode) { return {*this, (u8)sizecode}; }

	struct ZMMRegister_sizecode_wrapper;
	// Represents a 512-bit register used by vpu instructions
	struct alignas(64) ZMMRegister
	{
	private:
		u64 data[8]; // reserves the needed space and ensures 8-byte alignment

	public:

		// -- fill utilities -- //

		// fills the register with zeros
		inline void clear() { data[0] = data[1] = data[2] = data[3] = data[4] = data[5] = data[6] = data[7] = 0; }

		// -- index access utilities -- //

		inline u64 &uint64(u64 index) { return ((u64*)data)[index]; }
		inline u32 &uint32(u64 index) { return ((u32*)data)[index]; }
		inline u16 &uint16(u64 index) { return ((u16*)data)[index]; }
		inline u8 &uint8(u64 index) { return ((u8*)data)[index]; }

		inline u64 &int64(u64 index) { return ((u64*)data)[index]; }
		inline u32 &int32(u64 index) { return ((u32*)data)[index]; }
		inline u16 &int16(u64 index) { return ((u16*)data)[index]; }
		inline u8 &int8(u64 index) { return ((u8*)data)[index]; }

		inline double &fp64(u64 index) { return ((double*)data)[index]; }
		inline float &fp32(u64 index) { return ((float*)data)[index]; }

		// -- sizecode access utilities -- //

		inline ZMMRegister_sizecode_wrapper uint(u64 sizecode, u64 index);
	};
	struct ZMMRegister_sizecode_wrapper
	{
		ZMMRegister &reg;
		u8 index;
		u8 sizecode;

		inline constexpr operator u64() const
		{
			switch (sizecode)
			{
			case 0: return reg.uint8(index);
			case 1: return reg.uint16(index);
			case 2: return reg.uint32(index);
			case 3: return reg.uint64(index);

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr ZMMRegister_sizecode_wrapper &operator=(u64 value)
		{
			switch (sizecode)
			{
			case 0: reg.uint8(index) = (u8)value; return *this;
			case 1: reg.uint16(index) = (u16)value; return *this;
			case 2: reg.uint32(index) = (u32)value; return *this;
			case 3: reg.uint64(index) = (u64)value; return *this;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr ZMMRegister_sizecode_wrapper &operator=(ZMMRegister_sizecode_wrapper other) { *this = (u64)other; return *this; }
	};
	ZMMRegister_sizecode_wrapper ZMMRegister::uint(u64 sizecode, u64 index) { return {*this, (u8)index, (u8)sizecode}; }

	/// <summary>
	/// Represents a file descriptor used by the <see cref="CSX64"/> processor
	/// </summary>
	class FileDescriptor
	{
	private:

		std::iostream *stream;

		bool managed;
		bool interactive;

	public:
		// gets if this file is managed (i.e. stream will be deleted on close)
		inline bool Managed() const noexcept { return managed; }
		// gets if this file is interactive (i.e. Computer object reading past eof sets SuspendedRead state).
		inline bool Interactive() const noexcept { return interactive; }

		// gets a pointer to the currently-opened stream (nullptr if not in use).
		inline std::iostream *Stream() const noexcept { return stream; }
		// gets if this file descriptor is currently bound to a stream.
		inline bool InUse() const noexcept { return stream; }

		// ----------------------------

		constexpr FileDescriptor() : stream(nullptr), managed(false), interactive(false) {}
		~FileDescriptor() { Close(); }

		FileDescriptor(const FileDescriptor&) = delete;
		FileDescriptor(FileDescriptor &&other) : stream(other.stream), managed(other.managed), interactive(other.interactive)
		{
			other.stream = nullptr;
		}

		FileDescriptor &operator=(const FileDescriptor&) = delete;
		FileDescriptor &operator=(FileDescriptor &&other)
		{
			// make sure we close our stream before we steal other's
			Close();

			// steal other's stuff
			stream = other.stream;
			managed = other.managed;
			interactive = other.interactive;

			// empty other
			other.stream = nullptr;

			return *this;
		}

		friend void swap(FileDescriptor &a, FileDescriptor &b)
		{
			using std::swap;

			swap(a.managed, b.managed);
			swap(a.interactive, b.interactive);
			swap(a.stream, b.stream);
		}

		// ----------------------------

		/// <summary>
		/// Assigns the given stream to this file descriptor. Throws <see cref="std::runtime_error"/> if already in use.
		/// On success, is usable. On failure, is not modified.
		/// </summary>
		/// <param name="stream">the stream source</param>
		/// <param name="managed">marks that this stream is considered "managed". see CSX64 manual for more information</param>
		/// <param name="interactive">marks that this stream is considered "interactive" see CSX64 manual for more information</param>
		/// <exception cref="std::runtime_error"></exception>
		void Open(std::iostream *stream, bool managed, bool interactive)
		{
			if (InUse()) throw std::runtime_error("Attempt to assign to a FileDescriptor that was currently in use");

			this->stream = stream;
			this->managed = managed;
			this->interactive = interactive;
		}
		/// <summary>
		/// Unlinks the stream and makes this file descriptor unused. If managed, first closes the stream.
		/// If not currenty in use, does nothing. Returns true if successful (no errors).
		/// </summary>
		bool Close()
		{
			// delete the stream if we were asked to manage it
			if (managed) delete stream;

			// unlink it in either case
			stream = nullptr;

			return true;
		}
	};
}

#endif
