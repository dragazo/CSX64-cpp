#ifndef CSX64_TYPES_H
#define CSX64_TYPES_H

#include <cstdint>
#include <iostream>
#include <fstream>
#include <exception>
#include <stdexcept>
#include <algorithm>
#include <memory>
#include <cstring>
#include <string>

#include "CoreTypes.h"

namespace CSX64
{
	enum class ErrorCode
	{
		None, OutOfBounds, UnhandledSyscall, UndefinedBehavior, ArithmeticError, Abort,
		IOFailure, FSDisabled, AccessViolation, InsufficientFDs, FDNotInUse, NotImplemented, StackOverflow,
		FPUStackOverflow, FPUStackUnderflow, FPUError, FPUAccessViolation,
		AlignmentViolation, UnknownOp, FilePermissions,
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
		inline constexpr u64 x64() const noexcept { return data; }

		inline constexpr ReferenceRouter<u32, u64> x32() noexcept { return {data}; }
		inline constexpr u32 x32() const noexcept { return *(u32*)&data; }

		inline constexpr u16 &x16() noexcept { return *(u16*)&data; }
		inline constexpr u16 x16() const noexcept { return *(u16*)&data; }

		inline constexpr u8 &x8() noexcept { return *(u8*)&data; }
		inline constexpr u8 x8() const noexcept { return *(u8*)&data; }

		inline constexpr u8 &x8h() noexcept { return *((u8*)&data + 1); }
		inline constexpr u8 x8h() const noexcept { return *((u8*)&data + 1); }

		// Gets/sets the register partition with the specified size code
		inline CPURegister_sizecode_wrapper operator[](u64 sizecode);
		inline u64 operator[](u64 sizecode) const
		{
			switch (sizecode)
			{
			case 0: return x8();
			case 1: return x16();
			case 2: return x32();
			case 3: return x64();

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
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

		inline u64 &uint64(u64 index) /**/ { return ((u64*)data)[index]; }
		inline u64 uint64(u64 index) const { return ((u64*)data)[index]; }

		inline u32 &uint32(u64 index) /**/ { return ((u32*)data)[index]; }
		inline u32 uint32(u64 index) const { return ((u32*)data)[index]; }

		inline u16 &uint16(u64 index) /**/ { return ((u16*)data)[index]; }
		inline u16 uint16(u64 index) const { return ((u16*)data)[index]; }

		inline u8 &uint8(u64 index) /**/ { return ((u8*)data)[index]; }
		inline u8 uint8(u64 index) const { return ((u8*)data)[index]; }

		// ---------------------------- //

		inline i64 &int64(u64 index) /**/ { return ((i64*)data)[index]; }
		inline i64 int64(u64 index) const { return ((i64*)data)[index]; }

		inline i32 &int32(u64 index) /**/ { return ((i32*)data)[index]; }
		inline i32 int32(u64 index) const { return ((i32*)data)[index]; }

		inline i16 &int16(u64 index) /**/ { return ((i16*)data)[index]; }
		inline i16 int16(u64 index) const { return ((i16*)data)[index]; }

		inline i8 &int8(u64 index) /**/ { return ((i8*)data)[index]; }
		inline i8 int8(u64 index) const { return ((i8*)data)[index]; }

		// ---------------------------- /

		inline double &fp64(u64 index) /**/ { return ((double*)data)[index]; }
		inline double fp64(u64 index) const { return ((double*)data)[index]; }

		inline float &fp32(u64 index) /**/ { return ((float*)data)[index]; }
		inline float fp32(u64 index) const { return ((float*)data)[index]; }

		// -- sizecode access utilities -- //

		// gets/sets the value with specified sizecode and index
		inline ZMMRegister_sizecode_wrapper uint(u64 sizecode, u64 index);
		inline u64 uint(u64 sizecode, u64 index) const
		{
			switch (sizecode)
			{
			case 0: return uint8(index);
			case 1: return uint16(index);
			case 2: return uint32(index);
			case 3: return uint64(index);

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
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

	// -------- //

	// -- io -- //

	// -------- //

	// exception type thrown when IFileWrapper permissions are violated
	struct FileWrapperPermissionsException : std::runtime_error
	{
		using std::runtime_error::runtime_error;
		using std::runtime_error::what;
	};

	// the interface used by CSX64 file descriptors to reference files.
	struct IFileWrapper
	{
		virtual ~IFileWrapper() = 0;

		// returns true iff this stream is interactive (see CSX64 documentation)
		virtual bool IsInteractive() const = 0;

		// returns true iff this stream can read
		virtual bool CanRead() const = 0;
		// returns true iff this stream can write
		virtual bool CanWrite() const = 0;

		// returns true iff this stream can seek
		virtual bool CanSeek() const = 0;

		// reads at most <cap> bytes into buf. no null terminator is appended.
		// returns the number of bytes read.
		// throws FileWrapperPermissionsException if the file cannot read.
		virtual i64 Read(void *buf, i64 cap) = 0;
		// writes <len> bytes from buf.
		// returns the number of bytes written (may be less than <len> due to an io error).
		// throws FileWrapperPermissionsException if the file cannot write.
		virtual i64 Write(void *buf, i64 len) = 0;

		// sets the current position in the file based on an offset <off> and an origin <dir>.
		// returns the resulting position (offset from beginning).
		// throws FileWrapperPermissionsException if the file cannot seek.
		virtual i64 Seek(i64 off, std::ios::seekdir dir) = 0;
	};
    inline IFileWrapper::~IFileWrapper() {}

	class BasicFileWrapper : public IFileWrapper
	{
	private: // -- data -- //

		std::fstream *f;
		bool _managed;
		bool _interactive;

		bool _CanRead;
		bool _CanWrite;

		bool _CanSeek;

	public: // -- ctor / dtor / asgn -- //

		// constructs a new BasicFileWrapper from the given file (which cannot be null).
		// if <managed> is true, the stream is closed and deleted when this object is destroyed.
		// throws std::invalid_argument if <file> is null.
		BasicFileWrapper(std::fstream *file, bool managed, bool interactive, bool canRead, bool canWrite, bool canSeek)
			: f(file), _managed(managed), _interactive(interactive), _CanRead(canRead), _CanWrite(canWrite), _CanSeek(canSeek)
		{
			// the file must not be null
			if (file == nullptr) throw std::invalid_argument("file cannot be null");
		}

		virtual ~BasicFileWrapper()
		{
			// if we're managing the file, delete it
			if (_managed) delete f;
		}

		BasicFileWrapper(const BasicFileWrapper&) = delete;
		BasicFileWrapper &operator=(const BasicFileWrapper&) = delete;

		BasicFileWrapper(BasicFileWrapper &&other) = delete;
		BasicFileWrapper &operator=(BasicFileWrapper &&other) = delete;

	public: // -- interface -- //

		virtual bool IsInteractive() const override { return _interactive; }

		virtual bool CanRead() const override { return _CanRead; }
		virtual bool CanWrite() const override { return _CanWrite; }

		virtual bool CanSeek() const override { return _CanSeek; }

		virtual i64 Read(void *buf, i64 cap) override
		{
			if (!CanRead()) throw FileWrapperPermissionsException("FileWrapper not flagged for reading");
			return (i64)f->read((char*)buf, (std::streamsize)cap).gcount();
		}
		virtual i64 Write(void *buf, i64 len) override
		{
			if (!CanWrite()) throw FileWrapperPermissionsException("FileWrapper not flagged for writing");
			f->write((char*)buf, (std::streamsize)len);
			return len;
		}

		virtual i64 Seek(i64 off, std::ios::seekdir dir) override
		{
			if (!CanSeek()) throw FileWrapperPermissionsException("FileWrapper not flagged for seeking");
			f->seekg((std::streamoff)off, dir); // fstream uses a single pointer for get/put
			return (i64)f->tellg();
		}
	};
}

#endif
