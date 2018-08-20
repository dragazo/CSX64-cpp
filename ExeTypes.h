#ifndef CSX64_TYPES_H
#define CSX64_TYPES_H

#include <cstdint>
#include <iostream>
#include <fstream>
#include <exception>

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
		binary = 64, // this one's non-standard but we need it
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
		static constexpr T high_mask = low_mask << pos;

		inline constexpr operator T() const noexcept { return (data >> pos) & low_mask; }
		inline constexpr BitfieldWrapper operator=(T val) noexcept { data = (data & ~high_mask) | ((val & low_mask) << pos); return *this; }
		inline constexpr BitfieldWrapper operator=(BitfieldWrapper wrapper) noexcept { data = (data & ~high_mask) | (wrapper.data & high_mask); return *this; }

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
		u32 sizecode;

		inline constexpr operator u64() const
		{
			switch (sizecode)
			{
			case 3: return reg.x64();
			case 2: return reg.x32();
			case 1: return reg.x16();
			case 0: return reg.x8();

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr CPURegister_sizecode_wrapper operator=(u64 value)
		{
			switch (sizecode)
			{
			case 3: reg.x64() = (u64)value; return *this;
			case 2: reg.x32() = (u32)value; return *this;
			case 1: reg.x16() = (u16)value; return *this;
			case 0: reg.x8() = (u8)value; return *this;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr CPURegister_sizecode_wrapper operator=(CPURegister_sizecode_wrapper other) { *this = (u64)other; return *this; }
	};
	CPURegister_sizecode_wrapper CPURegister::operator[](u64 sizecode) { return {*this, (u32)sizecode}; }

	struct ZMMRegister_sizecode_wrapper;
	// Represents a 512-bit register used by vpu instructions
	struct ZMMRegister
	{
	private:
		u64 data[8]; // reserves the needed space and ensures 8-byte alignment

	public:

		// -- index access utilities -- //

		inline u64 &uint64(int index) { return ((u64*)data)[index]; }
		inline u32 &uint32(int index) { return ((u32*)data)[index]; }
		inline u16 &uint16(int index) { return ((u16*)data)[index]; }
		inline u8 &uint8(int index) { return ((u8*)data)[index]; }

		inline std::int64_t &int64(int index) { return ((std::int64_t*)data)[index]; }
		inline std::int32_t &int32(int index) { return ((std::int32_t*)data)[index]; }
		inline std::int16_t &int16(int index) { return ((std::int16_t*)data)[index]; }
		inline std::int8_t &int8(int index) { return ((std::int8_t*)data)[index]; }

		inline double &fp64(int index) { return ((double*)data)[index]; }
		inline float &fp32(int index) { return ((float*)data)[index]; }

		// -- sizecode access utilities -- //

		inline ZMMRegister_sizecode_wrapper uint(int sizecode, int index);
	};
	struct ZMMRegister_sizecode_wrapper
	{
		ZMMRegister &reg;
		i16 index;
		i16 sizecode;

		inline constexpr operator u64() const
		{
			switch (sizecode)
			{
			case 3: return reg.uint64(index);
			case 2: return reg.uint32(index);
			case 1: return reg.uint16(index);
			case 0: return reg.uint8(index);

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr ZMMRegister_sizecode_wrapper operator=(u64 value)
		{
			switch (sizecode)
			{
			case 3: reg.uint64(index) = (u64)value; return *this;
			case 2: reg.uint32(index) = (u32)value; return *this;
			case 1: reg.uint16(index) = (u16)value; return *this;
			case 0: reg.uint8(index) = (u8)value; return *this;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		inline constexpr ZMMRegister_sizecode_wrapper operator=(ZMMRegister_sizecode_wrapper other) { *this = (u64)other; return *this; }
	};
	ZMMRegister_sizecode_wrapper ZMMRegister::uint(int sizecode, int index) { return {*this, (i16)index, (i16)sizecode}; }

	/// <summary>
	/// Represents a file descriptor used by the <see cref="CSX64"/> processor
	/// </summary>
	class FileDescriptor
	{
	private:
		bool managed;
		bool interactive;

		std::iostream *stream;

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
		FileDescriptor(FileDescriptor &&other) : managed(other.managed), interactive(other.interactive), stream(other.stream)
		{
			other.stream = nullptr;
		}

		FileDescriptor &operator=(const FileDescriptor&) = delete;
		FileDescriptor &operator=(FileDescriptor &&other)
		{
			std::swap(managed, other.managed);
			std::swap(interactive, other.interactive);
			std::swap(stream, other.stream);

			return *this;
		}

		friend void swap(FileDescriptor &a, FileDescriptor &b) { a = std::move(b); }

		// ----------------------------

		/// <summary>
		/// Assigns the given stream to this file descriptor. Throws <see cref="std::runtime_error"/> if already in use
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
			// if the file is managed, we need to close (and deallocate) it
			if (managed) delete stream;
			// unlink the stream
			stream = nullptr;

			return true;
		}
	};
}

#endif
