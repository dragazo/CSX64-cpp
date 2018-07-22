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
		Exit,

		Read, Write,
		Open, Close,

		Flush,
		Seek, Tell,

		Move, Remove,
		Mkdir, Rmdir,

		Brk,
	};

	// acts as a reference to T, but gets/sets the value from a location of type U
	// meant for use with integral types
	template<typename T, typename U>
	struct ReferenceRouter
	{
		U &dest;

		inline operator T() { return dest; }
		inline ReferenceRouter operator=(T val) { dest = val; return *this; }
		inline ReferenceRouter operator=(ReferenceRouter wrapper) { dest = (T)wrapper.dest; return *this; }

		inline ReferenceRouter operator++() { *this = *this + 1; return *this; }
		inline ReferenceRouter operator--() { *this = *this - 1; return *this; }

		inline T operator++(int) { T val = *this; *this = *this + 1; return val; }
		inline T operator--(int) { T val = *this; *this = *this - 1; return val; }

		inline ReferenceRouter operator+=(T val) { *this = *this + val; return *this; }
		inline ReferenceRouter operator-=(T val) { *this = *this - val; return *this; }
		inline ReferenceRouter operator*=(T val) { *this = *this * val; return *this; }
		inline ReferenceRouter operator/=(T val) { *this = *this / val; return *this; }
		inline ReferenceRouter operator%=(T val) { *this = *this % val; return *this; }

		inline ReferenceRouter operator&=(T val) { *this = *this & val; return *this; }
		inline ReferenceRouter operator|=(T val) { *this = *this | val; return *this; }
		inline ReferenceRouter operator^=(T val) { *this = *this ^ val; return *this; }

		inline ReferenceRouter operator<<=(int val) { *this = *this << val; return *this; }
		inline ReferenceRouter operator>>=(int val) { *this = *this >> val; return *this; }
	};
	// acts as a reference to a bitfield in an integer of type "T"
	template<typename T, int pos, int len>
	struct BitfieldWrapper
	{
		T &data;

		static constexpr T __mask = (T)1 << (len - 1);
		static constexpr T low_mask = __mask | (__mask - 1);
		static constexpr T high_mask = low_mask << pos;

		inline operator T() { return (data >> pos) & low_mask; }
		inline BitfieldWrapper operator=(T val) { data = (data & ~high_mask) | ((val & low_mask) << pos); return *this; }
		inline BitfieldWrapper operator=(BitfieldWrapper wrapper) { data = (data & ~high_mask) | (wrapper.data & high_mask); return *this; }

		inline BitfieldWrapper operator++() { *this = *this + 1; return *this; }
		inline BitfieldWrapper operator--() { *this = *this - 1; return *this; }

		inline T operator++(int) { T val = *this; *this = *this + 1; return val; }
		inline T operator--(int) { T val = *this; *this = *this - 1; return val; }

		inline BitfieldWrapper operator+=(T val) { *this = *this + val; return *this; }
		inline BitfieldWrapper operator-=(T val) { *this = *this - val; return *this; }
		inline BitfieldWrapper operator*=(T val) { *this = *this * val; return *this; }
		inline BitfieldWrapper operator/=(T val) { *this = *this / val; return *this; }
		inline BitfieldWrapper operator%=(T val) { *this = *this % val; return *this; }

		inline BitfieldWrapper operator&=(T val) { *this = *this & val; return *this; }
		inline BitfieldWrapper operator|=(T val) { *this = *this | val; return *this; }
		inline BitfieldWrapper operator^=(T val) { *this = *this ^ val; return *this; }

		inline BitfieldWrapper operator<<=(int val) { *this = *this << val; return *this; }
		inline BitfieldWrapper operator>>=(int val) { *this = *this >> val; return *this; }
	};
	// acts a reference to a 1-bit flag in an integer of type "T"
	template<typename T, int pos>
	struct FlagWrapper
	{
		T &data;

		static constexpr T mask = (T)1 << pos;

		inline operator bool() { return data & mask; }
		inline FlagWrapper operator=(bool val) { data = val ? data | mask : data & ~mask; return *this; }
		inline FlagWrapper operator=(FlagWrapper wrapper) { data = (data & ~mask) | (wrapper.data & mask); return *this; }
	};

	struct CPURegister_sizecode_wrapper;
	// Represents a 64-bit register
	struct CPURegister
	{
	private:
		u64 data;

	public:
		inline u64 &x64() { return data; }
		inline ReferenceRouter<u32, u64> x32() { return {data}; }
		inline u16 &x16() { return *(u16*)&data; }
		inline u8 &x8() { return *(u8*)&data; }

		inline u8 &x8h() { return *((u8*)&data + 1); }

		/// <summary>
		/// Gets/sets the register partition with the specified size code
		/// </summary>
		/// <param name="sizecode">the size code to select</param>
		CPURegister_sizecode_wrapper operator[](int sizecode);
	};
	struct CPURegister_sizecode_wrapper
	{
		CPURegister &reg;
		int sizecode;

		inline operator u64()
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
		inline u64 operator=(u64 value)
		{
			switch (sizecode)
			{
			case 3: return reg.x64() = value;
			case 2: return reg.x32() = value;
			case 1: return reg.x16() = value;
			case 0: return reg.x8() = value;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}

			return *this;
		}
		inline u64 operator=(CPURegister_sizecode_wrapper other) { return *this = (u64)other; }
	};

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

		ZMMRegister_sizecode_wrapper uint(int sizecode, int index);
	};
	struct ZMMRegister_sizecode_wrapper
	{
		ZMMRegister &reg;
		i16 index;
		i16 sizecode;

		inline operator u64()
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
		inline u64 operator=(u64 value)
		{
			switch (sizecode)
			{
			case 3: return reg.uint64(index) = value;
			case 2: return reg.uint32(index) = value;
			case 1: return reg.uint16(index) = value;
			case 0: return reg.uint8(index) = value;

			default: throw std::invalid_argument("sizecode must be on range [0,3]");
			}
		}

		inline u64 operator=(ZMMRegister_sizecode_wrapper other) { return *this = (u64)other; }
	};

	/// <summary>
	/// Represents a file descriptor used by the <see cref="CSX64"/> processor
	/// </summary>
	class FileDescriptor
	{
	public:

		/// <summary>
		/// Marks that this stream is managed by the processor.
		/// Managed files can be opened and closed by the processor, and are closed upon client program termination
		/// </summary>
		bool Managed;
		/// <summary>
		/// Marks that the stream has an associated interactive input from external code (i.e. not client code).
		/// Reading past EOF on an interactive stream sets the SuspendedRead flag of the associated <see cref="CSX64"/>
		/// </summary>
		bool Interactive;

		/// <summary>
		/// The underlying stream associated with this file descriptor.
		/// If you close this, you should also null it, or - preferably - call <see cref="Close"/> instead of closing it yourself.
		/// </summary>
		std::iostream *BaseStream = nullptr;

		/// <summary>
		/// Returns true iff the file descriptor is currently in use
		/// </summary>
		inline bool InUse() { return BaseStream; }

		// ---------------------

		/// <summary>
		/// Assigns the given stream to this file descriptor. Throws <see cref="AccessViolationException"/> if already in use
		/// </summary>
		/// <param name="stream">the stream source</param>
		/// <param name="managed">marks that this stream is considered "managed". see CSX64 manual for more information</param>
		/// <param name="interactive">marks that this stream is considered "interactive" see CSX64 manual for more information</param>
		/// <exception cref="AccessViolationException"></exception>
		void Open(std::iostream &stream, bool managed, bool interactive)
		{
			if (InUse()) throw std::runtime_error("Attempt to assign to a FileDescriptor that was currently in use");

			BaseStream = &stream;
			Managed = managed;
			Interactive = interactive;
		}
		/// <summary>
		/// Unlinks the stream and makes this file descriptor unused. If managed, first closes the stream.
		/// If not currenty in use, does nothing. Returns true if successful (no errors).
		/// </summary>
		bool Close()
		{
			// closing unused file is no-op
			if (InUse())
			{
				// if the file is managed
				if (Managed)
				{
					// close the stream
					try { delete BaseStream; }
					// fail case must still null the stream (user is free to ignore the error and reuse the object)
					catch (...) { BaseStream = nullptr; return false; }
				}

				// unlink the stream
				BaseStream = nullptr;
			}

			return true;
		}

		/// <summary>
		/// Flushes the stream tied to this file descriptor. Throws <see cref="AccessViolationException"/> if already in use.
		/// Returns true on success (no errors).
		/// </summary>
		/// <exception cref="AccessViolationException"></exception>
		bool Flush()
		{
			if (!InUse()) throw std::runtime_error("Attempt to access a FileDescriptor that was not in use");

			// flush the stream
			try { BaseStream->flush(); return true; }
			catch (...) { return false; }
		}

		/// <summary>
		/// Sets the current position in the file. Throws <see cref="AccessViolationException"/> if already in use.
		/// Returns true on success (no errors).
		/// </summary>
		/// <exception cref="AccessViolationException"></exception>
		bool Seek(i64 offset, int origin)
		{
			if (!InUse()) throw std::runtime_error("Attempt to access a FileDescriptor that was not in use");
			
			// seek to new position (seek both to make sure it works for all types of streams)
			try { BaseStream->seekg(offset, origin); BaseStream->seekp(offset, origin); return true; }
			catch (...) { return false; }
		}
		/// <summary>
		/// Gets the current position in the file. Throws <see cref="AccessViolationException"/> if already in use.
		/// Returns true on success (no errors).
		/// </summary>
		/// <param name="pos">the position in the file if successful</param>
		/// <exception cref="AccessViolationException"></exception>
		bool Tell(i64 &pos)
		{
			if (!InUse()) throw std::runtime_error("Attempt to access a FileDescriptor that was not in use");

			// return position (the getter can throw)
			try { pos = BaseStream->tellg(); return true; }
			catch (...) { pos = -1; return false; }
		}
	};
}

#endif
