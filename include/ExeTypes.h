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
#include <vector>

#include "CoreTypes.h"
#include "punning.h"
#include "csx_exceptions.h"

namespace CSX64
{
	// error codes resulting from executing client code (these don't trigger exceptions).
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

	// acts as a reference to T, but gets/sets the value from a location of type U.
	// this is performed by casting the T value to U before the store, thus modifying the high order bits.
	// meant for use with integral types.
	template<typename T, typename U>
	struct ReferenceRouter
	{
		U &dest;

		constexpr operator T() const noexcept { return (T)dest; }
		constexpr ReferenceRouter operator=(T val) noexcept { dest = (U)val; return *this; }
		constexpr ReferenceRouter operator=(ReferenceRouter wrapper) noexcept { dest = (U)(T)wrapper; return *this; }

		constexpr ReferenceRouter operator++() noexcept { *this = *this + 1; return *this; }
		constexpr ReferenceRouter operator--() noexcept { *this = *this - 1; return *this; }

		constexpr T operator++(int) noexcept { T val = *this; *this = *this + 1; return val; }
		constexpr T operator--(int) noexcept { T val = *this; *this = *this - 1; return val; }

		constexpr ReferenceRouter operator+=(T val) noexcept { *this = *this + val; return *this; }
		constexpr ReferenceRouter operator-=(T val) noexcept { *this = *this - val; return *this; }
		constexpr ReferenceRouter operator*=(T val) noexcept { *this = *this * val; return *this; }

		constexpr ReferenceRouter operator/=(T val) { *this = *this / val; return *this; }
		constexpr ReferenceRouter operator%=(T val) { *this = *this % val; return *this; }

		constexpr ReferenceRouter operator&=(T val) noexcept { *this = *this & val; return *this; }
		constexpr ReferenceRouter operator|=(T val) noexcept { *this = *this | val; return *this; }
		constexpr ReferenceRouter operator^=(T val) noexcept { *this = *this ^ val; return *this; }

		constexpr ReferenceRouter operator<<=(int val) noexcept { *this = *this << val; return *this; }
		constexpr ReferenceRouter operator>>=(int val) noexcept { *this = *this >> val; return *this; }
	};

	// acts as a reference to a bitfield in an integer of type "T"
	template<typename T, int pos, int len>
	struct BitfieldWrapper
	{
		T &data;

		static constexpr T __mask = (T)1 << (len - 1);
		static constexpr T low_mask = __mask | (__mask - 1);
		static constexpr T mask = low_mask << pos;

		constexpr operator T() const noexcept { return (data >> pos) & low_mask; }
		constexpr BitfieldWrapper operator=(T val) noexcept { data = (data & ~mask) | ((val & low_mask) << pos); return *this; }
		constexpr BitfieldWrapper operator=(BitfieldWrapper wrapper) noexcept { data = (data & ~mask) | (wrapper.data & mask); return *this; }

		constexpr BitfieldWrapper operator++() noexcept { *this = *this + 1; return *this; }
		constexpr BitfieldWrapper operator--() noexcept { *this = *this - 1; return *this; }

		constexpr T operator++(int) noexcept { T val = *this; *this = *this + 1; return val; }
		constexpr T operator--(int) noexcept { T val = *this; *this = *this - 1; return val; }

		constexpr BitfieldWrapper operator+=(T val) noexcept { *this = *this + val; return *this; }
		constexpr BitfieldWrapper operator-=(T val) noexcept { *this = *this - val; return *this; }
		constexpr BitfieldWrapper operator*=(T val) noexcept { *this = *this * val; return *this; }

		constexpr BitfieldWrapper operator/=(T val) { *this = *this / val; return *this; }
		constexpr BitfieldWrapper operator%=(T val) { *this = *this % val; return *this; }

		constexpr BitfieldWrapper operator&=(T val) noexcept { *this = *this & val; return *this; }
		constexpr BitfieldWrapper operator|=(T val) noexcept { *this = *this | val; return *this; }
		constexpr BitfieldWrapper operator^=(T val) noexcept { *this = *this ^ val; return *this; }

		constexpr BitfieldWrapper operator<<=(int val) noexcept { *this = *this << val; return *this; }
		constexpr BitfieldWrapper operator>>=(int val) noexcept { *this = *this >> val; return *this; }
	};
	// acts a reference to a 1-bit flag in an integer of type "T"
	template<typename T, int pos>
	struct FlagWrapper
	{
		T &data;

		static constexpr T mask = (T)1 << pos;

		constexpr operator bool() const noexcept { return data & mask; }
		constexpr FlagWrapper operator=(bool val) noexcept { data = val ? data | mask : data & ~mask; return *this; }
		constexpr FlagWrapper operator=(FlagWrapper wrapper) noexcept { data = (data & ~mask) | (wrapper.data & mask); return *this; }
	};

	// represents a value held as a void* that is accessed as T& via bin_cpy<T>().
	// this is used to avoid strict aliasing violations on entire words where flag/bitfield wrappers are insufficient.
	template<typename T>
	struct bin_cpy_wrapper
	{
		void *loc;

		operator T() const noexcept { return bin_read<T>(loc); }
		bin_cpy_wrapper operator=(T val) noexcept { bin_write<T>(loc, val); return *this; }
		bin_cpy_wrapper operator=(bin_cpy_wrapper other) { bin_write<T>(loc, (T)other); return *this; }

		bin_cpy_wrapper operator++() noexcept { *this = *this + 1; return *this; }
		bin_cpy_wrapper operator--() noexcept { *this = *this - 1; return *this; }

		T operator++(int) noexcept { T val = *this; ++*this; return val; }
		T operator--(int) noexcept { T val = *this; --*this; return val; }

		bin_cpy_wrapper operator+=(T val) noexcept { *this = *this + val; return *this; }
		bin_cpy_wrapper operator-=(T val) noexcept { *this = *this - val; return *this; }
		bin_cpy_wrapper operator*=(T val) noexcept { *this = *this * val; return *this; }

		bin_cpy_wrapper operator/=(T val) noexcept { *this = *this / val; return *this; }
		bin_cpy_wrapper operator%=(T val) noexcept { *this = *this % val; return *this; }

		bin_cpy_wrapper operator&=(T val) noexcept { *this = *this & val; return *this; }
		bin_cpy_wrapper operator|=(T val) noexcept { *this = *this | val; return *this; }
		bin_cpy_wrapper operator^=(T val) noexcept { *this = *this ^ val; return *this; }

		bin_cpy_wrapper operator<<=(int val) noexcept { *this = *this << val; return *this; }
		bin_cpy_wrapper operator>>=(int val) noexcept { *this = *this >> val; return *this; }
	};

	struct CPURegister_sizecode_wrapper;
	// Represents a 64-bit register
	struct CPURegister
	{
	private: // -- data -- //

		u64 data; // the raw data block used to represent all partitions of the 64-bit general-purpose register

	public: // -- partition access -- //

		constexpr u64 &x64() noexcept { return data; }
		constexpr ReferenceRouter<u32, u64> x32() noexcept { return {data}; }
		constexpr BitfieldWrapper<u64, 0, 16> x16() noexcept { return {data}; }
		constexpr BitfieldWrapper<u64, 0, 8> x8() noexcept { return {data}; }
		constexpr BitfieldWrapper<u64, 8, 8> x8h() noexcept { return {data}; }

		constexpr u64 x64() const noexcept { return data; }
		constexpr u32 x32() const noexcept { return (u32)data; }
		constexpr u16 x16() const noexcept { return (u16)data; }
		constexpr u8 x8() const noexcept { return (u8)data; }
		constexpr u8 x8h() const noexcept { return (u8)(data >> 8); }

		// Gets/sets the register partition with the specified size code
		CPURegister_sizecode_wrapper operator[](u64 sizecode);
		u64 operator[](u64 sizecode) const
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

		constexpr operator u64() const
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
		constexpr CPURegister_sizecode_wrapper &operator=(u64 value)
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
		constexpr CPURegister_sizecode_wrapper &operator=(CPURegister_sizecode_wrapper other) { *this = (u64)other; return *this; }
	};
	inline CPURegister_sizecode_wrapper CPURegister::operator[](u64 sizecode) { return {*this, (u8)sizecode}; }

	struct ZMMRegister_sizecode_wrapper;
	// Represents a 512-bit register used by vpu instructions
	struct alignas(64) ZMMRegister
	{
	private:
		
		alignas(64) unsigned char data[64]; // the raw data block used to represent the entire 64-byte vector register

	public: // -- fill utilities -- //

		// fills the register with zeros
		void clear() { std::memset(data, 0, sizeof(data)); }

	public: // -- partition access -- //

		// gets the value of type T at the specified index (index offsets based on T)
		template<typename T> bin_cpy_wrapper<T> get(u64 index) noexcept { return {data + index * sizeof(T)}; }
		template<typename T> T get(u64 index) const noexcept { return bin_read<T>(data + index * sizeof(T)); }

	public: // -- sizecode access utilities -- //

		// gets/sets the value with specified sizecode and index
		ZMMRegister_sizecode_wrapper uint(u64 sizecode, u64 index);
		u64 uint(u64 sizecode, u64 index) const
		{
			switch (sizecode)
			{
			case 0: return get<u8>(index);
			case 1: return get<u16>(index);
			case 2: return get<u32>(index);
			case 3: return get<u64>(index);

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
	};
	struct ZMMRegister_sizecode_wrapper
	{
		ZMMRegister &reg;
		u8 index;
		u8 sizecode;

		constexpr operator u64() const
		{
			switch (sizecode)
			{
			case 0: return reg.get<u8>(index);
			case 1: return reg.get<u16>(index);
			case 2: return reg.get<u32>(index);
			case 3: return reg.get<u64>(index);

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		constexpr ZMMRegister_sizecode_wrapper &operator=(u64 value)
		{
			switch (sizecode)
			{
			case 0: reg.get<u8>(index) = (u8)value; return *this;
			case 1: reg.get<u16>(index) = (u16)value; return *this;
			case 2: reg.get<u32>(index) = (u32)value; return *this;
			case 3: reg.get<u64>(index) = (u64)value; return *this;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}
		constexpr ZMMRegister_sizecode_wrapper &operator=(ZMMRegister_sizecode_wrapper other) { *this = (u64)other; return *this; }
	};
	inline ZMMRegister_sizecode_wrapper ZMMRegister::uint(u64 sizecode, u64 index) { return {*this, (u8)index, (u8)sizecode}; }

	// -------- //

	// -- io -- //

	// -------- //

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
		virtual i64 Write(const void *buf, i64 len) = 0;

		// sets the current position in the file based on an offset <off> and an origin <dir>.
		// returns the resulting position (offset from beginning).
		// throws FileWrapperPermissionsException if the file cannot seek.
		virtual i64 Seek(i64 off, std::ios::seekdir dir) = 0;
	};
    inline IFileWrapper::~IFileWrapper() {}

	// a file wrapper that holds a std::fstream object, optionally managing its lifetime internally.
	// all read/write/seek requests are made diretly to the file.
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
			return (i64)f->read(reinterpret_cast<char*>(buf), (std::streamsize)cap).gcount(); // aliasing ok because casting to char type
		}
		virtual i64 Write(const void *buf, i64 len) override
		{
			if (!CanWrite()) throw FileWrapperPermissionsException("FileWrapper not flagged for writing");
			f->write(reinterpret_cast<const char*>(buf), (std::streamsize)len); // aliasing ok because casting to char type
			return len;
		}

		virtual i64 Seek(i64 off, std::ios::seekdir dir) override
		{
			if (!CanSeek()) throw FileWrapperPermissionsException("FileWrapper not flagged for seeking");
			f->seekg((std::streamoff)off, dir); // fstream uses a single pointer for get/put
			return (i64)f->tellg();
		}
	};

	// a file wrapper that holds any istream object, optionally managing its lifetime internally.
	// this wrapper can read, but cannot write or seek. the istream is used to simulate terminal input.
	// when a read request is made, an internal buffer is examined.
	// if empty, it performs a (potentially-blocking) read for a line of input to refill said buffer (terminated by \n).
	// afterwards, a non-blocking read is performed on the current contents of the buffer.
	class TerminalInputFileWrapper : public IFileWrapper
	{
	private: // -- data -- //

		std::istream *f;
		bool _managed;
		bool _interactive;

		std::vector<unsigned char> b;         // the input buffer
		std::size_t                b_pos = 0; // position on the read head in b (to avoid lots of expensive moves)

	public: // -- ctor / dtor / asgn -- //

		// constructs a new TerminalInputFileWrapper from the given stream (which cannot be null).
		// if <managed> is true, the stream is closed and deleted when this object is destroyed.
		// throws std::invalid_argument if <file> is null.
		TerminalInputFileWrapper(std::istream *file, bool managed, bool interactive)
			: f(file), _managed(managed), _interactive(interactive)
		{
			// the file must not be null
			if (file == nullptr) throw std::invalid_argument("file cannot be null");
		}

		virtual ~TerminalInputFileWrapper()
		{
			// if we're managing the file, delete it
			if (_managed) delete f;
		}

		TerminalInputFileWrapper(const TerminalInputFileWrapper&) = delete;
		TerminalInputFileWrapper(TerminalInputFileWrapper&&) = delete;

		TerminalInputFileWrapper &operator=(const TerminalInputFileWrapper&) = delete;
		TerminalInputFileWrapper &operator=(TerminalInputFileWrapper&&) = delete;

	public: // -- interface -- //

		virtual bool IsInteractive() const override { return _interactive; }

		virtual bool CanRead() const override { return true; }
		virtual bool CanWrite() const override { return false; }

		virtual bool CanSeek() const override { return false; }

		virtual i64 Read(void *buf, i64 cap) override
		{
			// if the buffer is empty, refill it
			if (b_pos >= b.size())
			{
				// reset the buffer
				b.clear();
				b_pos = 0;

				// while we can read a character
				for (int ch; (ch = f->get()) != EOF; )
				{
					// add it to the buffer
					b.push_back((unsigned char)ch);

					// if it was a new line, stop
					if (ch == '\n') break;
				}
			}

			// get amount to read
			std::size_t len = std::min(b.size() - b_pos, (std::size_t)cap);

			// perform the read
			std::memcpy(buf, b.data() + b_pos, len);

			// update read position
			b_pos += len;

			// return number of bytes read
			return (i64)len;
		}
		virtual i64 Write(const void*, i64) override { throw FileWrapperPermissionsException("FileWrapper not flagged for writing"); }

		virtual i64 Seek(i64, std::ios::seekdir) override { throw FileWrapperPermissionsException("FileWrapper not flagged for seeking"); }
	};

	// a file wrapper that holds any ostream object, optionally managing its lifetime internally.
	// this wrapper can write, but cannot read or seek.
	// write requests are handed directly to the ostream.
	class TerminalOutputFileWrapper : public IFileWrapper
	{
	private: // -- data -- //

		std::ostream *f;
		bool _managed;
		bool _interactive;

	public: // -- ctor / dtor / asgn -- //

		// constructs a new TerminalOutputFileWrapper from the given stream (which cannot be null).
		// if <managed> is true, the stream is closed and deleted when this object is destroyed.
		// throws std::invalid_argument if <file> is null.
		TerminalOutputFileWrapper(std::ostream *file, bool managed, bool interactive)
			: f(file), _managed(managed), _interactive(interactive)
		{
			// the file must not be null
			if (file == nullptr) throw std::invalid_argument("file cannot be null");
		}

		virtual ~TerminalOutputFileWrapper()
		{
			// if we're managing the file, delete it
			if (_managed) delete f;
		}

		TerminalOutputFileWrapper(const TerminalOutputFileWrapper&) = delete;
		TerminalOutputFileWrapper(TerminalOutputFileWrapper&&) = delete;

		TerminalOutputFileWrapper &operator=(const TerminalOutputFileWrapper&) = delete;
		TerminalOutputFileWrapper &operator=(TerminalOutputFileWrapper&&) = delete;

	public: // -- interface -- //

		virtual bool IsInteractive() const override { return _interactive; }

		virtual bool CanRead() const override { return false; }
		virtual bool CanWrite() const override { return true; }

		virtual bool CanSeek() const override { return false; }

		virtual i64 Read(void*, i64) override { throw FileWrapperPermissionsException("FileWrapper not flagged for reading"); }
		virtual i64 Write(const void *buf, i64 len) override
		{
			f->write(reinterpret_cast<const char*>(buf), (std::streamsize)len); // alias ok because casting to char type
			return len;
		}

		virtual i64 Seek(i64, std::ios::seekdir) override { throw FileWrapperPermissionsException("FileWrapper not flagged for seeking"); }
	};
}

#endif
