#ifndef CSX64_UTILITY_H
#define CSX64_UTILITY_H

#include <type_traits>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <cctype>
#include <sstream>
#include <bit>

#include "CoreTypes.h"

namespace CSX64
{
	// -- versioning info -- //

	const u64 Version = 0x501;

	// -- helpers -- //

	template<typename T> inline constexpr bool is_uint = std::is_same_v<T, u8> || std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>;

	// -- serialization -- //

	inline constexpr u8 bswap(u8 v) { return v; }
	inline constexpr u16 bswap(u16 v) { return (v << 8) | (v >> 8); }
	inline constexpr u32 bswap(u32 v)
	{
		u32 res = (v << 16) | (v >> 16);
		res = ((v & 0x00ff00ff) << 8) | ((v & 0xff00ff00) >> 8);
		return res;
	}
	inline constexpr u64 bswap(u64 v)
	{
		u64 res = (v << 32) | (v >> 32);
		res = ((v & 0x0000ffff0000ffff) << 16) | ((v & 0xffff0000ffff0000) >> 16);
		res = ((v & 0x00ff00ff00ff00ff) << 8) | ((v & 0xff00ff00ff00ff00) >> 8);
		return res;
	}

	inline constexpr u64 bswap(u64 val, u64 sizecode)
	{
		switch (sizecode)
		{
		case 3: return bswap(val);
		case 2: return bswap((u32)val);
		case 1: return bswap((u16)val);
		case 0: return bswap((u8)val);

		default: return 0; // this should never happen
		}
	}

	// reads/writes a binary buffer from/to a file.
	// if this fails to read/write exactly count bytes, sets the stream's failbit.
	std::ostream &BinWrite(std::ostream &ostr, const void *p, std::size_t count);
	std::istream &BinRead(std::istream &istr, void *p, std::size_t count);

	// writes a little-endian value to the stream
	template<typename T, typename U, std::enable_if_t<is_uint<T> && std::is_same_v<T, U>, int> = 0>
	std::ostream &write(std::ostream &ostr, U val)
	{
		if constexpr (std::endian::native == std::endian::big) val = bswap(val);
		return BinWrite(ostr, &val, sizeof(val));
	}

	// reads a little-endian value from the stream
	template<typename T, typename U, std::enable_if_t<is_uint<T> && std::is_same_v<T, U>, int> = 0>
	std::istream &read(std::istream &istr, U &val)
	{
		if constexpr (std::endian::native == std::endian::little) return BinRead(istr, &val, sizeof(val));
		else
		{
			BinRead(istr, &val, sizeof(val));
			val = bswap(val);
			return istr;
		}
	}

	// reads/writes a binary representation of a string to the stream.
	std::ostream &BinWrite(std::ostream &ostr, const std::string &str);
	std::istream &BinRead(std::istream &istr, std::string &str);

	// -- binary access -- //

	// writes a little-endian value to the given memory location
	template<typename T, typename U, std::enable_if_t<is_uint<T> && std::is_same_v<T, U>, int> = 0>
	void write(void *dest, U val)
	{
		if constexpr (std::endian::native == std::endian::big) val = bswap(val);
		std::memcpy(dest, &val, sizeof(val));
	}

	// reads a little-endian value from the given memory location
	template<typename T, std::enable_if_t<is_uint<T>, int> = 0>
	T read(const void *src)
	{
		T val;
		std::memcpy(&val, src, sizeof(T));
		if constexpr (std::endian::native == std::endian::big) val = bswap(val);
		return val;
	}

	// -- container utilities -- //

	template<typename T> bool Contains(const std::vector<T> &c, const T &val) { return std::find(c.begin(), c.end(), val) != c.end(); }
	template<typename T> bool Contains(const std::deque<T> &c, const T &val) { return std::find(c.begin(), c.end(), val) != c.end(); }
	template<typename T> bool Contains(const std::unordered_set<T> &set, const T &value) { return set.find(value) != set.end(); }

	template<typename T, typename U> bool ContainsKey(const std::unordered_map<T, U> &map, const T &key) { return map.find(key) != map.end(); }
	template<typename T, typename U> bool ContainsValue(const std::unordered_map<T, U> &map, const U &value)
	{
		for (const auto &entry : map) if (entry.second == value) return true;
		return false;
	}

	// returns true if the map contains the key - if so, stores the address of the associated value in ptr
	template<typename T, typename U> bool TryGetValue(const std::unordered_map<T, U> &map, const T &key, const U *&ptr)
	{
		if (auto i = map.find(key); i != map.end()) { ptr = &i->second; return true; } else return false;
	}
	template<typename T, typename U> bool TryGetValue(std::unordered_map<T, U> &map, const T &key, U *&ptr)
	{
		if (auto i = map.find(key); i != map.end()) { ptr = &i->second; return true; } else return false;
	}

	// returns true if the map contains the key - if so, stores the associated value in dest (by copy)
	template<typename T, typename U> bool TryGetValue(const std::unordered_map<T, U> &map, const T &key, U &dest)
	{
		if (auto i = map.find(key); i != map.end()) { dest = i->second; return true; } else return false;
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
		while (val & (val - 1)) val = val & (val - 1);
		return val;
	}
	// isolates the lowest set bit. if val is zero, returns zero.
	inline constexpr u64 IsolateLowBit(u64 val) { return val & (~val + 1); }

	// Returns true if this value is a power of two. (zero returns false)
	inline constexpr bool IsPowerOf2(u64 val) { return val != 0 && (val & (val - 1)) == 0; }

	/// <summary>
	/// Extracts 2 distinct powers of 2 from the specified value. Returns true if the value is made up of exactly two non-zero powers of 2.
	/// </summary>
	/// <param name="val">the value to process</param>
	/// <param name="a">the first (larger) power of 2</param>
	/// <param name="b">the second (smaller) power of 2</param>
	bool Extract2PowersOf2(u64 val, u64 &a, u64 &b);

	static constexpr u64  _SignMasks[] = {0x80, 0x8000, 0x80000000, 0x8000000000000000};
	static constexpr u64 _TruncMasks[] = {0xff, 0xffff, 0xffffffff, 0xffffffffffffffff};
	static constexpr u64 _ExtendMasks[] = {0xffffffffffffff00, 0xffffffffffff0000, 0xffffffff00000000, 0x00};

	static constexpr u64     _Sizes[] = {1, 2, 4, 8};
	static constexpr u64 _SizesBits[] = {8, 16, 32, 64};

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

	// safely puns an object of type U into an object of type T.
	// this function only participates in overload resolution if T and U are both trivial types and equal in size.
	template<typename T, typename U, std::enable_if_t<std::is_trivial<T>::value && std::is_trivial<U>::value && sizeof(T) == sizeof(U), int> = 0>
	constexpr T pun(const U &value)
	{
		T temp;
		std::memcpy(std::addressof(temp), std::addressof(value), sizeof(T));
		return temp;
	}

	// Interprets a double as its raw bits
	inline u64 DoubleAsUInt64(double val)
	{

		return pun<u64>(val);
	}
	// Interprets raw bits as a double
	inline double AsDouble(u64 val)
	{
		return pun<double>(val);
	}

	// Interprets a float as its raw bits (placed in low 32 bits)
	inline u64 FloatAsUInt64(float val)
	{
		return pun<u32>(val);
	}
	// Interprets raw bits as a float (low 32 bits)
	inline float AsFloat(u32 val)
	{
		return pun<float>(val);
	}

	void ExtractDouble(double val, double &exp, double &sig);
	double AssembleDouble(double exp, double sig);

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
