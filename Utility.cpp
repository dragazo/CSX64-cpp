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
#include <stdexcept>
#include <cctype>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "Utility.h"
#include "BiggerInts/BiggerInts.h"
#include "ios-frstor/iosfrstor.h"

namespace CSX64
{
	// -- serialization -- //

	std::ostream &BinWrite(std::ostream &ostr, const std::string &str)
	{
		// write length prefix
		u16 len = (u16)str.size();
		BinWrite(ostr, len);
		// write the string
		ostr.write(str.data(), len);

		return ostr;
	}
	std::istream &BinRead(std::istream &istr, std::string &str)
	{
		// read length prefix
		u16 len;
		BinRead(istr, len);
		// allocate space and read the string
		str.resize(len);
		istr.read(str.data(), len);
		// make sure we got the whole thing
		if (istr.gcount() != len) istr.setstate(std::ios::failbit);

		return istr;
	}

	// -- misc utilities -- //

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

	// -- memory utilities -- //

	void *aligned_malloc(std::size_t size, std::size_t align)
	{
		// calling with 0 yields nullptr
		if (size == 0) return nullptr;

		// allocate enough space for a void*, padding, and the array
		size += sizeof(void*) + align - 1;

		// grab that much space - if that fails, return null
		void *raw = std::malloc(size);
		if (!raw) return nullptr;

		// get the pointer to return (before alignment)
		void *ret = reinterpret_cast<char*>(raw) + sizeof(void*);

		// align the return pointer
		ret = reinterpret_cast<char*>(ret) + (-(std::intptr_t)ret & (align - 1));

		// store the raw pointer before start of ret array
		reinterpret_cast<void**>(ret)[-1] = raw; // aliasing is safe because only we (should) ever modify it

		// return ret pointer
		return ret;
	}
	void aligned_free(void *ptr)
	{
		// free the raw pointer (freeing nullptr does nothing)
		if (ptr) std::free(reinterpret_cast<void**>(ptr)[-1]); // aliasing is safe because only we (should) ever modify it
	}

	bool Write(std::vector<u8> &arr, u64 pos, u64 size, u64 val)
	{
		// make sure we're not exceeding memory bounds
		if (pos >= arr.size() || pos + size > arr.size()) return false;

		// write the value (little-endian)
		for (int i = 0; i < (int)size; ++i)
		{
			arr[pos + i] = (u8)val;
			val >>= 8;
		}

		return true;
	}
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

	void Append(std::vector<u8> &arr, u64 size, u64 val)
	{
		// write the value (little-endian)
		for (u64 i = 0; i < size; ++i)
		{
			arr.push_back((u8)val);
			val >>= 8;
		}
	}
	
	u64 AlignOffset(u64 address, u64 size)
	{
		u64 pos = address % size;
		return pos == 0 ? 0 : size - pos;
	}
	u64 Align(u64 address, u64 size)
	{
		return address + AlignOffset(address, size);
	}
	void Pad(std::vector<u8> &arr, u64 count)
	{
		for (; count > 0; --count) arr.push_back(0);
	}
	void Align(std::vector<u8> &arr, u64 size)
	{
		Pad(arr, AlignOffset(arr.size(), size));
	}

	// -- string stuff -- //
	
	std::string TrimStart(const std::string &str)
	{
		std::size_t i;
		for (i = 0; i < str.size() && std::isspace((unsigned char)str[i]); ++i);

		std::string res = str.substr(i);
		return res;
	}
	std::string TrimEnd(const std::string &str)
	{
		std::size_t sz;
		for (sz = str.size(); sz > 0 && std::isspace((unsigned char)str[sz - 1]); --sz);
		
		std::string res = str.substr(0, sz);
		return res;
	}
	std::string TrimEnd(std::string &&str)
	{
		std::size_t sz;
		for (sz = str.size(); sz > 0 && std::isspace((unsigned char)str[sz - 1]); --sz);

		// if it's an rvalue we can exploit std::string being a dynamic array
		std::string res = std::move(str);
		res.resize(sz);
		return res;
	}
	std::string Trim(const std::string &str)
	{
		std::size_t i, sz;
		for (i = 0; i < str.size() && std::isspace((unsigned char)str[i]); ++i);
		for (sz = str.size(); sz > 0 && std::isspace((unsigned char)str[sz - 1]); --sz);

		std::string res = str.substr(i, sz - i);
		return res;
	}

	/// <summary>
	/// Gets the numeric value of a hexadecimal digit. returns true if the character was in the hex range.
	/// </summary>
	/// <param name="ch">the character to test</param>
	/// <param name="val">the character's value [0-15] - undefined on faiure</param>
	bool GetHexValue(char ch, int &val)
	{
		if (ch >= '0' && ch <= '9') val = ch - '0';
		else
		{
			ch |= 32;
			if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;
			else { return false; }
		}

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
		if (radix < 2 || radix > 36) throw std::runtime_error("radix must be in range 2-36");

		val = 0;          // initialize to zero
		unsigned int add; // amount to add

		// fail on null or empty
		if (str.empty()) return false;

		// for each character
		for (char ch : str)
		{
			val *= radix; // shift val

			// if it's a digit, add directly
			if (ch >= '0' && ch <= '9') add = ch - '0';
			else
			{
				// if it's a letter, add it + 10
				ch |= 32;
				if (ch >= 'a' && ch <= 'z') add = ch - 'a' + 10;
				// otherwise we don't recognize it
				else return false;
			}

			// if add value was out of range, fail
			if (add >= radix) return false;

			val += add; // add to val
		}

		return true;
	}
	bool TryParseDouble(const std::string &str, double &val)
	{
		// extract the double value
		std::istringstream istr(str);
		istr >> val;

		// that should've succeeded and consumed the entire string
		return istr && istr.peek() == EOF;
	}

	/// <summary>
	/// Returns true if the string contains at least one occurrence of the specified character
	/// </summary>
	/// <param name="str">the string to test</param>
	/// <param name="ch">the character to look for</param>
	bool Contains(const std::string &str, char ch)
	{
		for (std::size_t i = 0; i < str.size(); ++i) if (str[i] == ch) return true;

		return false;
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
		return StartsWith(str, val) && (str.size() == val.size() || std::isspace((unsigned char)str[val.size()]));
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
		// clear the width field and set up a format restore point
		dump.width(0);
		iosfrstor _rstor(dump);

		// make a header
		dump << "           ";
		for (int i = 0; i < 16; ++i) dump << ' ' << std::hex << i << ' ';

		// if it's not starting on a new row
		if (start % 16 != 0)
		{
			// we need to write a line header
			dump << ' ' << std::hex << (start - start % 16) << " - ";

			// and tack on some white space
			for (int i = 0; i < (int)start % 16; ++i) dump << "   ";
		}

		// write the data
		for (int i = 0; i < (int)count; ++i)
		{
			// start of new row gets a line header
			if ((start + i) % 16 == 0) dump << '\n' << std::hex << std::setw(8) << (start + i) << " - ";
			
			dump << std::hex << std::setw(2) << (int)reinterpret_cast<const unsigned char*>(data)[start + i] << ' '; // aliasing is safe because u8 is unsigned char type
		}

		// end with a new line
		return dump << '\n';
	}

	// -- CSX64 encoding utilities -- //

	bool Extract2PowersOf2(u64 val, u64 &a, u64 &b)
	{
		b = val & (~val + 1);  // isolate the lowest power of 2
		val = val & (val - 1); // disable the lowest power of 2

		a = val & (~val + 1);  // isolate the next lowest power of 2
		val = val & (val - 1); // disable the next lowest power of 2

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
			sig = (double)(bits & 0xfffffffffffff) / ((u64)1 << 52);

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
}
