#ifndef CSX64_UTILITY_H
#define CSX64_UTILITY_H

#include <utility>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>
#include <list>
#include <deque>
#include <exception>
#include <stdexcept>
#include <cctype>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <type_traits>

#include "CoreTypes.h"
#include "punning.h"

namespace CSX64
{
	// -- versioning info -- //

	const u64 Version = 0x500;

	// -- arch encoding helpers -- //

	// returns true iff the current system is little-endian
	bool IsLittleEndian();

	// -- serialization -- //

	// writes count bytes from the buffer to the stream.
	std::ostream &BinWrite(std::ostream &ostr, const char *p, std::size_t count);
	// reads count bytes from the stream to the buffer.
	// if this fails to extract exactly count bytes, sets the stream's failbit.
	std::istream &BinRead(std::istream &istr, char *p, std::size_t count);

	// writes a binary representation of val to the stream.
	template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
	std::ostream &BinWrite(std::ostream &ostr, const T &val)
	{ return BinWrite(ostr, reinterpret_cast<const char*>(&val), sizeof(T)); }
	// reads a binary representation of val from the stream.
	template<typename T, std::enable_if_t<std::is_trivial<T>::value, int> = 0>
	std::istream &BinRead(std::istream &istr, T &val)
	{ return BinRead(istr, reinterpret_cast<char*>(&val), sizeof(T)); }

	// writes a binary representation of str to the stream.
	std::ostream &BinWrite(std::ostream &ostr, const std::string &str);
	// reads a binary representation of str from the stream.
	std::istream &BinRead(std::istream &istr, std::string &str);

	// -- container utilities -- //

	// returns true if the container has at least one entry equal to val
	template<typename T, typename U> bool Contains(const std::vector<T> &c, const U &val) { return std::find(c.begin(), c.end(), val) != c.end(); }
	template<typename T, typename U> bool Contains(const std::deque<T> &c, const U &val) { return std::find(c.begin(), c.end(), val) != c.end(); }

	template<typename T, typename U>
	bool Contains(const std::unordered_set<T> &set, const U &value) { return set.find(value) != set.end(); }

	// returns true if the map contains a node with the specified key
	template<typename T, typename U>
	bool ContainsKey(const std::unordered_map<T, U> &map, const T &key) { return map.find(key) != map.end(); }
	// returns true if the map contains a node with the specified value
	template<typename T, typename U>
	bool ContainsValue(const std::unordered_map<T, U> &map, const U &value)
	{
		for (const auto &entry : map) if (entry.second == value) return true;
		return false;
	}

	// returns a pointer to the node with specified key if it exists, otherwise null
	template<typename T, typename V>
	bool TryGetValue(const std::unordered_set<T> &set, const V &value, T *&ptr)
	{
		auto iter = set.find(value);
		return ptr = (iter == set.end() ? nullptr : &*iter);
	}

	// returns a pointer to the node with specified key if it exists, otherwise null
	template<typename T, typename U, typename K>
	bool TryGetValue(const std::unordered_map<T, U> &map, const K &key, const U *&ptr)
	{
		auto iter = map.find(key);
		ptr = (iter == map.end() ? nullptr : &iter->second);
		return ptr;
	}
	template<typename T, typename U, typename K>
	bool TryGetValue(std::unordered_map<T, U> &map, const K &key, U *&ptr)
	{
		auto iter = map.find(key);
		ptr = (iter == map.end() ? nullptr : &iter->second);
		return ptr;
	}

	// returns true if the map contains the key - if so, stores the associated value in dest (by copy)
	template<typename T, typename U, typename K>
	bool TryGetValue(const std::unordered_map<T, U> &map, const K &key, U &dest)
	{
		auto iter = map.find(key);
		if (iter != map.end()) { dest = iter->second; return true; }
		else return false;
	}

	// -- memory utilities -- //

	// allocates space and returns a pointer to at least <size> bytes which is aligned to <align>.
	// it is undefined behavior if align is not a power of 2.
	// allocating zero bytes returns null.
	// on failure, returns null.
	// must be deallocated via aligned_free().
	[[nodiscard]]
	void *aligned_malloc(std::size_t size, std::size_t align);
	// deallocates a block of memory allocated by aligned_malloc().
	// if <ptr> is null, does nothing.
	void aligned_free(void *ptr);

	/// <summary>
	/// Writes a value to the array
	/// </summary>
	/// <param name="arr">the data to write to</param>
	/// <param name="pos">the index to begin at</param>
	/// <param name="size">the size of the value in bytes</param>
	/// <param name="val">the value to write</param>
	/// <exception cref="ArgumentException"></exception>
	bool Write(std::vector<u8> &arr, u64 pos, u64 size, u64 val);
	/// <summary>
	/// Reads a value from the array
	/// </summary>
	/// <param name="arr">the data to write to</param>
	/// <param name="pos">the index to begin at</param>
	/// <param name="size">the size of the value in bytes</param>
	/// <param name="res">the read value</param>
	bool Read(const std::vector<u8> &arr, u64 pos, u64 size, u64 &res);

	/// <summary>
	/// Appends a value to an array of bytes in a list
	/// </summary>
	/// <param name="data">the byte array</param>
	/// <param name="size">the size in bytes of the value to write</param>
	/// <param name="val">the value to write</param>
	void Append(std::vector<u8> &arr, u64 size, u64 val);

	/// <summary>
	/// Gets the amount to offset address by to make it a multiple of size. if address is already a multiple of size, returns 0.
	/// </summary>
	/// <param name="address">the address to examine</param>
	/// <param name="size">the size to align to</param>
	u64 AlignOffset(u64 address, u64 size);
	/// <summary>
	/// Where address is the starting point, returns the next address aligned to the specified size
	/// </summary>
	/// <param name="address">the starting address</param>
	/// <param name="size">the size to align to</param>
	/// <returns></returns>
	u64 Align(u64 address, u64 size);
	/// <summary>
	/// Adds the specified amount of zeroed padding (in bytes) to the array
	/// </summary>
	/// <param name="arr">the data array to pad</param>
	/// <param name="count">the amount of padding in bytes</param>
	void Pad(std::vector<u8> &arr, u64 count);
	/// <summary>
	/// Pads the array with 0's until the length is a multiple of the specified size
	/// </summary>
	/// <param name="arr">the array to align</param>
	/// <param name="size">the size to align to</param>
	void Align(std::vector<u8> &arr, u64 size);

	// -- string stuff -- //

	// converts the argument into a string via operator<<
	template<typename T>
	std::string tostr(const T &val)
	{
		std::ostringstream ostr;
		ostr << val;
		return ostr.str();
	}
	// converts the argument into a hexx string via operator<<
	template<typename T>
	std::string tohex(const T &val)
	{
		std::ostringstream ostr;
		ostr << std::hex << val;
		return ostr.str();
	}

	// converts a string to uppercase (via std::toupper for each char)
	template<typename T>
	std::string ToUpper(T &&str)
	{
		std::string res(std::forward<T>(str));
		for (char &i : res) i = std::toupper((unsigned char)i);
		return res;
	}
	// converts a string to uppercase (via std::tolower for each char)
	template<typename T>
	std::string ToLower(T &&str)
	{
		std::string res(std::forward<T>(str));
		for (char &i : res) i = std::tolower((unsigned char)i);
		return res;
	}

	// removes leading white space (std::isspace)
	std::string TrimStart(const std::string &str);
	// removes trailing white space (std::isspace)
	std::string TrimEnd(const std::string &str);
	std::string TrimEnd(std::string &&str);
	// removes leading and trailing white space (std::isspace)
	std::string Trim(const std::string &str);

	// gets a new string where all instances of the specified character have been removed
	std::string remove_ch(const std::string &str, char ch);

	// extracts the characters that a quoted assembly string token would yield
	bool TryExtractStringChars(const std::string &token, std::string &chars, std::string &err);

	/// <summary>
	/// Gets the numeric value of a hexadecimal digit. returns true if the character was in the hex range.
	/// </summary>
	/// <param name="ch">the character to test</param>
	/// <param name="val">the character's value [0-15]</param>
	bool GetHexValue(char ch, int &val);
	/// <summary>
	/// Attempts to parse the string into an unsigned integer. Returns true on success.
	/// </summary>
	/// <param name="str">the string to parse</param>
	/// <param name="val">the resulting value</param>
	/// <param name="radix">the radix to use (must be 2-36)</param>
	bool TryParseUInt64(const std::string &str, u64 &val, unsigned int radix = 10);
	bool TryParseDouble(const std::string &str, double &val);

	/// <summary>
	/// Returns true if the string contains at least one occurrence of the specified character
	/// </summary>
	/// <param name="str">the string to test</param>
	/// <param name="ch">the character to look for</param>
	bool Contains(const std::string &str, char ch);

	/// <summary>
	/// Returns true if the string starts with the specified character
	/// </summary>
	/// <param name="str">the string to test</param>
	/// <param name="ch">the character it must start with</param>
	bool StartsWith(const std::string &str, char ch);
	bool StartsWith(const std::string &str, const std::string &val);
	/// <summary>
	/// Returns true if the the string is equal to the specified value or begins with it and is followed by white space.
	/// </summary>
	/// <param name="str">the string to search in</param>
	/// <param name="val">the header value to test for</param>
	bool StartsWithToken(const std::string &str, const std::string &val);

	// return true if str ends with val
	bool EndsWith(const std::string &str, const std::string &val);

	/// <summary>
	/// returns a binary dump representation of the data
	/// </summary>
	/// <param name="data">the data to dump</param>
	/// <param name="start">the index at which to begin dumping</param>
	/// <param name="count">the number of bytes to write</param>
	std::ostream &Dump(std::ostream &dump, void *data, u64 start, u64 count);

	// -- CSX64 encoding utilities -- //

	// isolates the highest set bit. if val is zero, returns zero.
	inline constexpr u64 IsolateHighBit(u64 val)
	{
		// while there are multiple set bits, clear the low one
		while (val & (val - 1)) val = val & (val - 1);
		return val;
	}
	// isolates the lowest set bit. if val is zero, returns zero.
	inline constexpr u64 IsolateLowBit(u64 val) { return val & (~val + 1); }

	/// <summary>
	/// Returns true if this value is a power of two. (zero returns false)
	/// </summary>
	inline constexpr bool IsPowerOf2(u64 val) { return val != 0 && (val & (val - 1)) == 0; }

	/// <summary>
	/// Extracts 2 distinct powers of 2 from the specified value. Returns true if the value is made up of exactly two non-zero powers of 2.
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="a">the first (larger) power of 2</param>
	/// <param name="b">the second (smaller) power of 2</param>
	bool Extract2PowersOf2(u64 val, u64 &a, u64 &b);

	constexpr u64  _SignMasks[] = {0x80, 0x8000, 0x80000000, 0x8000000000000000};
	constexpr u64 _TruncMasks[] = {0xff, 0xffff, 0xffffffff, 0xffffffffffffffff};
	constexpr u64 _ExtendMasks[] = {0xffffffffffffff00, 0xffffffffffff0000, 0xffffffff00000000, 0x00};

	constexpr u64     _Sizes[] = {1, 2, 4, 8};
	constexpr u64 _SizesBits[] = {8, 16, 32, 64};

	/// <summary>
	/// Gets the bitmask for the sign bit of an integer with the specified sizecode. sizecode must be [0,3]
	/// </summary>
	/// <param name="sizecode">the sizecode specifying the width of integer to examine</param>
	inline constexpr u64 SignMask(u64 sizecode) { return _SignMasks[sizecode]; }
	/// <summary>
	/// Gets the bitmask that includes the entire valid domain of an integer with the specified width
	/// </summary>
	/// <param name="sizecode">the sizecode specifying the width of integer to examine</param>
	inline constexpr u64 TruncMask(u64 sizecode) { return _TruncMasks[sizecode]; }

	/// <summary>
	/// Returns if the value with specified size code is positive
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="sizecode">the current size code of the value</param>
	inline constexpr bool Positive(u64 val, u64 sizecode) { return (val & SignMask(sizecode)) == 0; }
	/// <summary>
	/// Returns if the value with specified size code is negative
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="sizecode">the current size code of the value</param>
	inline constexpr bool Negative(u64 val, u64 sizecode) { return (val & SignMask(sizecode)) != 0; }

	/// <summary>
	/// Sign extends a value to 64-bits
	/// </summary>
	/// <param name="val">the value to sign extend</param>
	/// <param name="sizecode">the current size code</param>
	inline constexpr u64 SignExtend(u64 val, u64 sizecode) { return Positive(val, sizecode) ? val : val | _ExtendMasks[sizecode]; }
	/// <summary>
	/// Truncates the value to the specified size code (can also be used to zero extend a value)
	/// </summary>
	/// <param name="val">the value to truncate</param>
	/// <param name="sizecode">the size code to truncate to</param>
	inline constexpr u64 Truncate(u64 val, u64 sizecode) { return val & TruncMask(sizecode); }

	/// <summary>
	/// Parses a 2-bit size code into an actual size (in bytes) 0:1  1:2  2:4  3:8
	/// </summary>
	/// <param name="sizecode">the code to parse</param>
	inline constexpr u64 Size(u64 sizecode) { return (u64)1 << sizecode; }
	/// <summary>
	/// Parses a 2-bit size code into an actual size (in bits) 0:8  1:16  2:32  3:64
	/// </summary>
	/// <param name="sizecode">the code to parse</param>
	inline constexpr u64 SizeBits(u64 sizecode) { return (u64)8 << sizecode; }

	/// <summary>
	/// Gets the sizecode of the specified size. Throws <see cref="ArgumentException"/> if the size is not a power of 2
	/// </summary>
	/// <param name="size">the size</param>
	/// <exception cref="ArgumentException"></exception>
	inline constexpr u64 Sizecode(u64 size)
	{
		if (!IsPowerOf2(size)) throw std::invalid_argument("argument was not a power of 2");

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
	inline constexpr u64 BitsToBytes(u64 bits)
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
	inline u64 DoubleAsUInt64(double val)
	{
		return pun<u64>(val);
	}
	/// <summary>
	/// Interprets raw bits as a double
	/// </summary>
	/// <param name="val">value to interpret</param>
	inline double AsDouble(u64 val)
	{
		return pun<double>(val);
	}

	/// <summary>
	/// Interprets a float as its raw bits (placed in low 32 bits)
	/// </summary>
	/// <param name="val">the float to interpret</param>
	inline u64 FloatAsUInt64(float val)
	{
		return pun<u32>(val);
	}
	/// <summary>
	/// Interprets raw bits as a float (low 32 bits)
	/// </summary>
	/// <param name="val">the bits to interpret</param>
	inline float AsFloat(u32 val)
	{
		return pun<float>(val);
	}

	void ExtractDouble(double val, double &exp, double &sig);
	double AssembleDouble(double exp, double sig);

	// returns the result of swaping the byte positions of the specified value
	inline constexpr u64 ByteSwap(u64 val, u64 sizecode)
	{
		u64 res = 0;
		switch (sizecode)
		{
		case 3:
			res = (val << 32) | (val >> 32);
			res = ((val & 0x0000ffff0000ffff) << 16) | ((val & 0xffff0000ffff0000) >> 16);
			res = ((val & 0x00ff00ff00ff00ff) << 8) | ((val & 0xff00ff00ff00ff00) >> 8);
			break;
		case 2:
			res = (val << 16) | (val >> 16);
			res = ((val & 0x00ff00ff) << 8) | ((val & 0xff00ff00) >> 8);
			break;
		case 1: res = (val << 8) | (val >> 8); break;
		case 0: res = val; break;
		}
		return res;
	}

	/// <summary>
	/// Returns true if the floating-point value is denormalized (including +-0)
	/// </summary>
	/// <param name="val">the value to test</param>
	inline bool IsDenorm(double val) { return (DoubleAsUInt64(val) & 0x7ff0000000000000ul) == 0; }

	/// <summary>
	/// Returns true if the value is zero (int or floating)
	/// </summary>
	/// <param name="val">the value to test</param>
	/// <param name="floating">marks that val should be treated as a floating-point value (specifically, double)</param>
	inline bool IsZero(u64 val, bool floating) { return floating ? AsDouble(val) == 0 : val == 0; }
}

#endif
