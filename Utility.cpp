#include <utility>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <exception>
#include <cctype>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include "Utility.h"

// !! WORK NEEDED !! //
// 
// impl 64-bit division un UnsignedDiv() and SignedDiv()
// 

namespace CSX64
{
	// -- serialization -- //

	std::ostream &BinWrite(std::ostream &ostr, const std::string &str)
	{
		// write length prefix
		u16 len = str.size();
		BinWrite(ostr, len);
		// write the string
		ostr.write(&str[0], len);

		return ostr;
	}
	std::istream &BinRead(std::istream &istr, std::string &str)
	{
		// read length prefix
		u16 len;
		BinRead(istr, len);
		// allocate space and read the string
		str.resize(len);
		istr.read(&str[0], len);
		// make sure we got the whole thing
		if (istr.gcount() != len) istr.setstate(std::ios::failbit);

		return istr;
	}

	// -- misc utilities -- //

	void UnsignedMul(u64 a, u64 b, u64 &res_high, u64 &res_low)
	{
		// a * b = (wh + x) * (yh + z) = (wy)h^2 + (wz + xy)h + xz

		// extract the values
		u64 w = a >> 32;
		u64 x = a & 0xffffffff;
		u64 y = b >> 32;
		u64 z = b & 0xffffffff;

		// compute the results
		u64 high = w * y;
		u64 mid = w * z + x * y;
		u64 low = x * z;

		// merge mid into high
		high += mid >> 32;
		// account for overflow from mid calculation
		if (mid < w * z) high += (u64)1 << 32;

		// merge mid into low
		w = low;
		low += mid << 32;
		// account for overflow in low
		if (low < w) ++high;

		// store results
		res_high = high;
		res_low = low;
	}
	void SignedMul(u64 a, u64 b, u64 &res_high, u64 &res_low)
	{
		// we'll do the signed variant in terms of unsigned

		// get net negative flag
		bool neg = false;
		if ((i64)a < 0) { a = ~a + 1; neg = !neg; }
		if ((i64)b < 0) { b = ~b + 1; neg = !neg; }

		// perform unsigned product
		UnsignedMul(a, b, res_high, res_low);

		// account for sign
		if (neg)
		{
			res_high = ~res_high;
			res_low = ~res_low + 1;
			if (res_low < 1) ++res_high;
		}
	}

	void UnsignedDiv(u64 num_high, u64 num_low, u64 denom, u64 &quot_high, u64 &quot_low, u64 &rem)
	{
		std::cerr << "64-BIT DIVISION NOT IMPLEMENTED YET\n";
		abort();
	}
	void SignedDiv(u64 num_high, u64 num_low, u64 denom, u64 &quot_high, u64 &quot_low, u64 &rem)
	{
		std::cerr << "64-BIT DIVISION NOT IMPLEMENTED YET\n";
		abort();
	}

	bool TruncGood_128_64(u64 high, u64 low)
	{
		return (i64)low < 0 ? high == ~(u64)0 : high == (u64)0;
	}

	std::string remove_ch(const std::string &str, char ch)
	{
		std::string res;

		for (std::size_t i = 0; i < str.size(); ++i)
			if (str[i] != ch) res.push_back(str[i]);

		return res;
	}

	std::string ToString(long double val)
	{
		std::ostringstream str;
		str << val;
		return str.str();
	}

	u64 Rand64(std::default_random_engine &engine)
	{
		return ((u64)engine() << 32) | engine();
	}

	/// <summary>
	/// Returns true if this value is a power of two. (zero returns false)
	/// </summary>
	bool IsPowerOf2(u64 val)
	{
		return val != 0 && (val & (val - 1)) == 0;
	}

	/// <summary>
	/// Extracts 2 distinct powers of 2 from the specified value. Returns true if the value is made up of exactly two non-zero powers of 2.
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="a">the first (larger) power of 2</param>
	/// <param name="b">the second (smaller) power of 2</param>
	bool Extract2PowersOf2(u64 val, u64 &a, u64 &b)
	{
		// isolate the lowest power of 2
		b = val & (~val + 1);
		// disable the lowest power of 2
		val = val & (val - 1);

		// isolate the next lowest power of 2
		a = val & (~val + 1);
		// disable the next lowest power of 2
		val = val & (val - 1);
		
		// if val is now zero and a and b are nonzero, we got 2 distinct powers of 2
		return val == 0 && a != 0 && b != 0;
	}

	void ExtractDouble(double val, double &exp, double &sig)
	{
		if (std::isnan(val))
		{
			exp = sig = std::numeric_limits<double>::quiet_NaN();
		}
		else if (std::isinf(val))
		{
			if (val > 0)
			{
				exp = std::numeric_limits<double>::infinity();
				sig = 1;
			}
			else
			{
				exp = std::numeric_limits<double>::infinity();
				sig = -1;
			}
		}
		else
		{
			// get the raw bits
			u64 bits = DoubleAsUInt64(val);

			// get the raw exponent
			u64 raw_exp = (bits >> 52) & 0x7ff;

			// get exponent and subtract bias
			exp = (double)raw_exp - 1023;
			// get significand (m.0) and offset to 0.m
			sig = (double)(bits & 0xfffffffffffff) / (1ul << 52);

			// if it's denormalized, add 1 to exponent
			if (raw_exp == 0) exp += 1;
			// otherwise add the invisible 1 to get 1.m
			else sig += 1;

			// handle negative case
			if (val < 0) sig = -sig;
		}
	}
	double AssembleDouble(double exp, double sig)
	{
		return sig * pow(2, exp);
	}

	/// <summary>
	/// Returns true if the floating-point value is denormalized (including +-0)
	/// </summary>
	/// <param name="val">the value to test</param>
	bool IsDenorm(double val)
	{
		// denorm has exponent field of zero
		return (DoubleAsUInt64(val) & 0x7ff0000000000000ul) == 0;
	}

	// -- memory utilities -- //

	/// <summary>
	/// Writes a value to the array
	/// </summary>
	/// <param name="arr">the data to write to</param>
	/// <param name="pos">the index to begin at</param>
	/// <param name="size">the size of the value in bytes</param>
	/// <param name="val">the value to write</param>
	/// <exception cref="ArgumentException"></exception>
	bool Write(std::vector<u8> &arr, u64 pos, u64 size, u64 val)
	{
		// make sure we're not exceeding memory bounds
		if (pos >= arr.size() || pos + size > arr.size()) return false;

		// write the value (little-endian)
		for (int i = 0; i < (int)size; ++i)
		{
			arr[pos + i] = val;
			val >>= 8;
		}

		return true;
	}
	/// <summary>
	/// Reads a value from the array
	/// </summary>
	/// <param name="arr">the data to write to</param>
	/// <param name="pos">the index to begin at</param>
	/// <param name="size">the size of the value in bytes</param>
	/// <param name="res">the read value</param>
	bool Read(const std::vector<u8> &arr, u64 pos, u64 size, u64 &res)
	{
		// make sure we're not exceeding memory bounds
		if (pos >= arr.size() || pos + size > arr.size()) return false;

		// read the value (little-endian)
		res = 0;
		for (int i = (int)size - 1; i >= 0; --i)
			res = (res << 8) | arr[pos + i];

		return true;
	}

	/// <summary>
	/// Appends a value to an array of bytes in a list
	/// </summary>
	/// <param name="data">the byte array</param>
	/// <param name="size">the size in bytes of the value to write</param>
	/// <param name="val">the value to write</param>
	void Append(std::vector<u8> &arr, u64 size, u64 val)
	{
		// write the value (little-endian)
		for (int i = 0; i < (int)size; ++i)
		{
			arr.push_back(val);
			val >>= 8;
		}
	}
	
	/// <summary>
	/// Gets the amount to offset address by to make it a multiple of size. if address is already a multiple of size, returns 0.
	/// </summary>
	/// <param name="address">the address to examine</param>
	/// <param name="size">the size to align to</param>
	u64 AlignOffset(u64 address, u64 size)
	{
		u64 pos = address % size;
		return pos == 0 ? 0 : size - pos;
	}
	/// <summary>
	/// Where address is the starting point, returns the next address aligned to the specified size
	/// </summary>
	/// <param name="address">the starting address</param>
	/// <param name="size">the size to align to</param>
	/// <returns></returns>
	u64 Align(u64 address, u64 size)
	{
		return address + AlignOffset(address, size);
	}
	/// <summary>
	/// Adds the specified amount of zeroed padding (in bytes) to the array
	/// </summary>
	/// <param name="arr">the data array to pad</param>
	/// <param name="count">the amount of padding in bytes</param>
	void Pad(std::vector<u8> &arr, u64 count)
	{
		for (; count > 0; --count) arr.push_back(0);
	}
	/// <summary>
	/// Pads the array with 0's until the length is a multiple of the specified size
	/// </summary>
	/// <param name="arr">the array to align</param>
	/// <param name="size">the size to align to</param>
	void Align(std::vector<u8> &arr, u64 size)
	{
		Pad(arr, AlignOffset(arr.size(), size));
	}

	/// <summary>
	/// Writes an ASCII C-style string to memory. Returns true on success
	/// </summary>
	/// <param name="arr">the data array to write to</param>
	/// <param name="pos">the position in the array to begin writing</param>
	/// <param name="str">the string to write</param>
	bool WriteCString(std::vector<u8> &arr, u64 pos, const std::string &str)
	{
		// make sure we're not exceeding memory bounds
		if (pos >= arr.size() || pos + (str.size() + 1) > arr.size()) return false;

		// write each character
		for (std::size_t i = 0; i < str.size(); ++i) arr[pos + i] = str[i];
		// write a null terminator
		arr[pos + str.size()] = 0;

		return true;
	}
	/// <summary>
	/// Reads an ASCII C-style string from memory. Returns true on success
	/// </summary>
	/// <param name="arr">the data array to read from</param>
	/// <param name="pos">the position in the array to begin reading</param>
	/// <param name="str">the string read</param>
	bool ReadCString(const std::vector<u8> &arr, u64 pos, std::string &str)
	{
		str.clear();

		// read the string
		for (; ; ++pos)
		{
			// ensure we're in bounds
			if (pos >= arr.size()) return false;

			// if it's not a terminator, append it
			if (arr[pos] != 0) str.push_back(arr[pos]);
			// otherwise we're done reading
			else break;
		}

		return true;
	}

	// -- string stuff -- //
	
	std::string TrimStart(const std::string &str)
	{
		std::size_t i;
		for (i = 0; i < str.size() && std::isspace(str[i]); ++i);

		std::string res = str.substr(i);
		return res;
	}
	std::string TrimEnd(const std::string &str)
	{
		std::size_t sz;
		for (sz = str.size(); sz > 0 && std::isspace(str[sz - 1]); --sz);
		
		std::string res = str.substr(0, sz);
		return res;
	}
	std::string TrimEnd(std::string &&str)
	{
		std::size_t sz;
		for (sz = str.size(); sz > 0 && std::isspace(str[sz - 1]); --sz);

		// if it's an rvalue we can exploit std::string being a dynamic array
		std::string res = std::move(str);
		res.resize(sz);
		return res;
	}
	std::string Trim(const std::string &str)
	{
		std::size_t i, sz;
		for (i = 0; i < str.size() && std::isspace(str[i]); ++i);
		for (sz = str.size(); sz > 0 && std::isspace(str[sz - 1]); --sz);

		std::string res = str.substr(i, sz - i);
		return res;
	}

	/// <summary>
	/// Gets the numeric value of a hexadecimal digit. returns true if the character was in the hex range.
	/// </summary>
	/// <param name="ch">the character to test</param>
	/// <param name="val">the character's value [0-15]</param>
	bool GetHexValue(char ch, int &val)
	{
		if (ch >= '0' && ch <= '9') val = ch - '0';
		else if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F') val = ch - 'A' + 10;
		else { val = 0; return false; }

		return true;
	}
	/// <summary>
	/// Attempts to parse the string into an unsigned integer. Returns true on success.
	/// </summary>
	/// <param name="str">the string to parse</param>
	/// <param name="val">the resulting value</param>
	/// <param name="radix">the radix to use (must be 2-36)</param>
	bool TryParseUInt64(const std::string &str, u64 &val, unsigned int radix)
	{
		// ensure radix is in range
		if (radix < 2 || radix > 36) throw std::runtime_error("radix must be in range 0-36");

		val = 0;          // initialize to zero
		unsigned int add; // amount to add

		// fail on null or empty
		if (str.empty()) return false;

		// for each character
		for (std::size_t i = 0; i < str.size(); ++i)
		{
			val *= radix; // shift val

			// if it's a digit, add directly
			if (str[i] >= '0' && str[i] <= '9') add = str[i] - '0';
			else if (str[i] >= 'a' && str[i] <= 'z') add = str[i] - 'a' + 10;
			else if (str[i] >= 'A' && str[i] <= 'Z') add = str[i] - 'A' + 10;
			// if it wasn't a known character, fail
			else return false;

			// if add value was out of range, fail
			if (add >= radix) return false;

			val += add; // add to val
		}

		return true;
	}
	bool TryParseDouble(const std::string &str, double &val)
	{
		std::istringstream istr(str);
		istr >> val;
		return (bool)istr;
	}

	/// <summary>
	/// Returns true if the string contains at least one occurrence of the specified character
	/// </summary>
	/// <param name="str">the string to test</param>
	/// <param name="ch">the character to look for</param>
	bool Contains(const std::string &str, char ch)
	{
		for (int i = 0; i < str.size(); ++i)
			if (str[i] == ch) return true;

		return false;
	}
	/// <summary>
	/// Removes ALL white space from a string
	/// </summary>
	std::string RemoveWhiteSpace(const std::string &str)
	{
		std::string res;

		for (int i = 0; i < str.size(); ++i)
			if (!std::isspace(str[i])) res.push_back(str[i]);

		return res;
	}

	bool StartsWith(const std::string &str, char ch)
	{
		return !str.empty() && str[0] == ch;
	}
	bool StartsWith(const std::string &str, const std::string &val)
	{
		// must be at least long enough to hold it
		if (str.size() < val.size()) return false;

		// make sure all the characters match
		for (std::size_t i = 0; i < val.size(); ++i)
			if (str[i] != val[i]) return false;

		return true;
	}
	bool StartsWithToken(const std::string &str, const std::string &val)
	{
		return StartsWith(str, val) && (str.size() == val.size() || std::isspace(str[val.size()]));
	}
	
	bool EndsWith(const std::string &str, const std::string &val)
	{
		// must be at least long enough to hold it
		if (str.size() < val.size()) return false;

		// make sure all the characters match
		for (std::size_t i = 0, base = str.size() - val.size(); i < val.size(); ++i)
			if (str[base + i] != val[i]) return false;

		return true;
	}

	/// <summary>
	/// returns a binary dump representation of the data
	/// </summary>
	/// <param name="data">the data to dump</param>
	/// <param name="start">the index at which to begin dumping</param>
	/// <param name="count">the number of bytes to write</param>
	std::ostream &Dump(std::ostream &dump, void *data, u64 start, u64 count)
	{
		// make a header
		dump << "           ";
		for (int i = 0; i < 16; ++i) dump << ' ' << std::hex << i << ' ';

		// if it's not starting on a new row
		if (start % 16 != 0)
		{
			// we need to write a line header
			dump << ' ' << std::hex << (start - start % 16) << " - ";

			// and tack on some white space
			for (int i = 0; i < start % 16; ++i) dump << "   ";
		}

		// write the data
		for (int i = 0; i < count; ++i)
		{
			// start of new row gets a line header
			if ((start + i) % 16 == 0) dump << '\n' << std::hex << std::setw(8) << (start + i) << " - ";
			
			dump << std::hex << std::setw(2) << (int)((u8*)data)[start + i] << ' ';
		}

		// end with a new line
		return dump << '\n';
	}

	// -- CSX64 encoding utilities -- //

	/// <summary>
	/// Gets the bitmask for the sign bit of an integer with the specified sizecode
	/// </summary>
	/// <param name="sizecode">the sizecode specifying the width of integer to examine</param>
	u64 SignMask(u64 sizecode)
	{
		return 1ul << ((8 << sizecode) - 1);
	}
	/// <summary>
	/// Gets the bitmask that includes the entire valid domain of an integer with the specified width
	/// </summary>
	/// <param name="sizecode">the sizecode specifying the width of integer to examine</param>
	u64 TruncMask(u64 sizecode)
	{
		u64 res = SignMask(sizecode);
		return res | (res - 1);
	}

	/// <summary>
	/// Returns if the value with specified size code is positive
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="sizecode">the current size code of the value</param>
	bool Positive(u64 val, u64 sizecode)
	{
		return (val & SignMask(sizecode)) == 0;
	}
	/// <summary>
	/// Returns if the value with specified size code is negative
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="sizecode">the current size code of the value</param>
	bool Negative(u64 val, u64 sizecode)
	{
		return (val & SignMask(sizecode)) != 0;
	}

	/// <summary>
	/// Sign extends a value to 64-bits
	/// </summary>
	/// <param name="val">the value to sign extend</param>
	/// <param name="sizecode">the current size code</param>
	u64 SignExtend(u64 val, u64 sizecode)
	{
		return Positive(val, sizecode) ? val : val | ~TruncMask(sizecode);
	}
	/// <summary>
	/// Truncates the value to the specified size code (can also be used to zero extend a value)
	/// </summary>
	/// <param name="val">the value to truncate</param>
	/// <param name="sizecode">the size code to truncate to</param>
	u64 Truncate(u64 val, u64 sizecode)
	{
		return val & TruncMask(sizecode);
	}

	/// <summary>
	/// Parses a 2-bit size code into an actual size (in bytes) 0:1  1:2  2:4  3:8
	/// </summary>
	/// <param name="sizecode">the code to parse</param>
	u64 Size(u64 sizecode)
	{
		return 1ul << sizecode;
	}
	/// <summary>
	/// Parses a 2-bit size code into an actual size (in bits) 0:8  1:16  2:32  3:64
	/// </summary>
	/// <param name="sizecode">the code to parse</param>
	u64 SizeBits(u64 sizecode)
	{
		return 8ul << sizecode;
	}

	/// <summary>
	/// Gets the sizecode of the specified size. Throws <see cref="ArgumentException"/> if the size is not a power of 2
	/// </summary>
	/// <param name="size">the size</param>
	/// <exception cref="ArgumentException"></exception>
	u64 Sizecode(u64 size)
	{
		//if (!IsPowerOf2(size)) throw new ArgumentException("argument to Sizecode() was not a power of 2");

		// compute sizecode by repeated shifting
		for (int i = 0; ; ++i)
		{
			size >>= 1;
			if (size == 0) return i;
		}
	}

	/// <summary>
	/// returns an elementary word size in bytes sufficient to hold the specified number of bits
	/// </summary>
	/// <param name="bits">the number of bits in the representation</param>
	u64 BitsToBytes(u64 bits)
	{
		if (bits <= 8) return 1;
		else if (bits <= 16) return 2;
		else if (bits <= 32) return 4;
		else if (bits <= 64) return 8;
		else throw std::invalid_argument("bit size must be in range [0,64]");
	}

	/// <summary>
	/// Interprets a double as its raw bits
	/// </summary>
	/// <param name="val">value to interpret</param>
	u64 DoubleAsUInt64(double val)
	{
		return *(u64*)&val;
	}
	/// <summary>
	/// Interprets raw bits as a double
	/// </summary>
	/// <param name="val">value to interpret</param>
	double AsDouble(u64 val)
	{
		return *(double*)&val;
	}

	/// <summary>
	/// Interprets a float as its raw bits (placed in low 32 bits)
	/// </summary>
	/// <param name="val">the float to interpret</param>
	u64 FloatAsUInt64(float val)
	{
		return *(std::uint32_t*)&val;
	}
	/// <summary>
	/// Interprets raw bits as a float (low 32 bits)
	/// </summary>
	/// <param name="val">the bits to interpret</param>
	float AsFloat(std::uint32_t val)
	{
		return *(float*)&val;
	}
}
