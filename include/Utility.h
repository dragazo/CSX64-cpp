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
#include <string_view>
#include <cctype>
#include <sstream>
#include <bit>

#include "CoreTypes.h"

namespace CSX64::detail
{
	// -- versioning info -- //

	const u64 Version = 0x501;

	// -- sfinae helpers -- //
	
	template<typename T> inline constexpr bool is_uint = std::is_same_v<T, u8> || std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>;
	template<typename T> inline constexpr bool is_sint = std::is_same_v<T, i8> || std::is_same_v<T, i16> || std::is_same_v<T, i32> || std::is_same_v<T, i64>;

	template<typename T> inline constexpr bool is_int = is_uint<T> || is_sint<T>;

	template<typename T> inline constexpr bool is_floating = std::is_same_v<T, f32> || std::is_same_v<T, f64> || std::is_same_v<T, fext>;

	namespace detail
	{
		template<typename T, typename U> struct is_restricted_transmutable : std::false_type {};
		template<> struct is_restricted_transmutable<u64, f64> : std::true_type {};
		template<> struct is_restricted_transmutable<u32, f32> : std::true_type {};
	}
	
	template<typename T, typename U> inline constexpr bool is_restricted_transmutable = detail::is_restricted_transmutable<T, U>::value || detail::is_restricted_transmutable<U, T>::value;

	// -- encoding -- //

	inline constexpr u16 bswap(u16 v) { return (v << 8) | (v >> 8); }
	inline constexpr u32 bswap(u32 v)
	{
		u32 res = (v << 16) | ((v & 0xffff0000) >> 16);
		res = ((res & 0x00ff00ff) << 8) | ((res & 0xff00ff00) >> 8);
		return res;
	}
	inline constexpr u64 bswap(u64 v)
	{
		u64 res = (v << 32) | ((v & 0xffffffff00000000) >> 32);
		res = ((res & 0x0000ffff0000ffff) << 16) | ((res & 0xffff0000ffff0000) >> 16);
		res = ((res & 0x00ff00ff00ff00ff) << 8) | ((res & 0xff00ff00ff00ff00) >> 8);
		return res;
	}

	inline constexpr i16 bswap(i16 v) { return (i16)bswap((u16)v); }
	inline constexpr i32 bswap(i32 v) { return (i32)bswap((u32)v); }
	inline constexpr i64 bswap(i64 v) { return (i64)bswap((u64)v); }

	inline constexpr u8 bswap(u8 v) { return v; }
	inline constexpr i8 bswap(i8 v) { return v; }

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

	// -- serialization -- //

	// reads/writes a binary buffer from/to a file.
	// if this fails to read/write exactly count bytes, sets the stream's failbit.
	std::ostream &write_bin(std::ostream &ostr, const void *p, std::size_t count);
	std::istream &read_bin(std::istream &istr, void *p, std::size_t count);

	// reads/writes a binary representation of a string to the stream.
	std::ostream &write_str(std::ostream &ostr, std::string_view str);
	std::istream &read_str(std::istream &istr, std::string &str);

	// writes a little-endian value to the stream
	template<typename T, typename U, std::enable_if_t<is_int<T> && std::is_same_v<T, U>, int> = 0>
	std::ostream &write(std::ostream &ostr, U val)
	{
		if constexpr (std::endian::native == std::endian::big) val = bswap(val);
		return write_bin(ostr, &val, sizeof(val));
	}

	// reads a little-endian value from the stream
	template<typename T, typename U, std::enable_if_t<is_int<T> && std::is_same_v<T, U>, int> = 0>
	std::istream &read(std::istream &istr, U &val)
	{
		if constexpr (std::endian::native == std::endian::little) return read_bin(istr, &val, sizeof(val));
		else
		{
			read_bin(istr, &val, sizeof(val));
			val = bswap(val);
			return istr;
		}
	}

	// -- memory access -- //

	// writes a little-endian value to the given memory location
	template<typename T, typename U, std::enable_if_t<is_int<T> && std::is_same_v<T, U>, int> = 0>
	void write(void *dest, U val)
	{
		if constexpr (std::endian::native == std::endian::big) val = bswap(val);
		std::memcpy(dest, &val, sizeof(val));
	}

	// reads a little-endian value from the given memory location
	template<typename T, std::enable_if_t<is_int<T>, int> = 0>
	T read(const void *src)
	{
		T val;
		std::memcpy(&val, src, sizeof(T));
		if constexpr (std::endian::native == std::endian::big) val = bswap(val);
		return val;
	}
	
	// -- memory utilities -- //

	// writes a little-endian value of the specified size (bytes) to the array. must be entirely in bounds.
	bool Write(std::vector<u8> &arr, u64 pos, u64 size, u64 val);

	// appents a little-endian value of the specified size (bytes) to the array.
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

	// -- container utilities -- //

	template<typename T> bool contains(const std::vector<T> &c, const T &val) { return std::find(c.begin(), c.end(), val) != c.end(); }
	template<typename T> bool contains(const std::deque<T> &c, const T &val) { return std::find(c.begin(), c.end(), val) != c.end(); }
	template<typename T> bool contains(const std::unordered_set<T> &set, const T &value) { return set.find(value) != set.end(); }

	template<typename T, typename U> bool contains_key(const std::unordered_map<T, U> &map, const T &key) { return map.find(key) != map.end(); }
	template<typename T, typename U> bool contains_value(const std::unordered_map<T, U> &map, const U &value)
	{
		for (const auto &entry : map) if (entry.second == value) return true;
		return false;
	}

	// returns true if the map contains the key - if so, stores the address of the associated value in ptr
	template<typename T, typename U> bool try_get_value(const std::unordered_map<T, U> &map, const T &key, const U *&ptr)
	{
		if (auto i = map.find(key); i != map.end()) { ptr = &i->second; return true; }
		else return false;
	}
	template<typename T, typename U> bool try_get_value(std::unordered_map<T, U> &map, const T &key, U *&ptr)
	{
		if (auto i = map.find(key); i != map.end()) { ptr = &i->second; return true; }
		else return false;
	}

	// returns true if the map contains the key - if so, stores the associated value in dest (by copy)
	template<typename T, typename U> bool try_get_value(const std::unordered_map<T, U> &map, const T &key, U &dest)
	{
		if (auto i = map.find(key); i != map.end()) { dest = i->second; return true; }
		else return false;
	}

	// -- string utilities -- //

	template<typename T, std::enable_if_t<is_uint<T> && (sizeof(T) >= sizeof(unsigned)), int> = 0>
	std::string tostr(T val) { return std::to_string(val); }
	template<typename T, std::enable_if_t<is_uint<T> && (sizeof(T) >= sizeof(unsigned)), int> = 0>
	std::string tohex(T val)
	{
		std::ostringstream ostr;
		ostr << std::hex << val;
		return ostr.str();
	}

	std::string to_upper(std::string str);
	std::string to_lower(std::string str);

	std::string trim_start(std::string_view str);
	std::string trim_end(std::string str);
	std::string trim(std::string str);

	std::string remove_char(std::string_view str, char ch);

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
	bool TryParseDouble(const std::string &str, f64 &val);

	bool starts_with(std::string_view str, std::string_view val);
	/// <summary>
	/// Returns true if the the string is equal to the specified value or begins with it and is followed by white space.
	/// </summary>
	/// <param name="str">the string to search in</param>
	/// <param name="val">the header value to test for</param>
	bool starts_with_token(std::string_view str, std::string_view val);

	// return true if str ends with val
	bool ends_with(std::string_view str, std::string_view val);

	/// <summary>
	/// returns a binary dump representation of the data
	/// </summary>
	/// <param name="data">the data to dump</param>
	/// <param name="start">the index at which to begin dumping</param>
	/// <param name="count">the number of bytes to write</param>
	std::ostream &Dump(std::ostream &dump, void *data, u64 start, u64 count);

	// -- CSX64 encoding utilities -- //

	// isolates the highest set bit. if val is zero, returns zero.
	inline constexpr u64 isolate_high_bit(u64 val)
	{
		while (val & (val - 1)) val = val & (val - 1);
		return val;
	}
	// isolates the lowest set bit. if val is zero, returns zero.
	inline constexpr u64 isolate_low_bit(u64 val) { return val & (~val + 1); }

	// checks if val is a power of two (zero returns false)
	inline constexpr bool is_pow2(u64 val) { return val != 0 && (val & (val - 1)) == 0; }

	static constexpr u64  _SignMasks[] = {0x80, 0x8000, 0x80000000, 0x8000000000000000};
	static constexpr u64 _TruncMasks[] = {0xff, 0xffff, 0xffffffff, 0xffffffffffffffff};
	static constexpr u64 _ExtendMasks[] = {0xffffffffffffff00, 0xffffffffffff0000, 0xffffffff00000000, 0x00};

	static constexpr u64     _Sizes[] = {1, 2, 4, 8};
	static constexpr u64 _SizesBits[] = {8, 16, 32, 64};

	// gets the bitmask for the sign bit of an integer with the specified sizecode. sizecode must be [0,3] otherwise und.
	inline constexpr u64 sign_mask(u64 sizecode) { return _SignMasks[sizecode]; }
	// gets the bitmask that includes the entire valid domain for the specified sizecode. sizecode must be [0,3] otherwise und.
	inline constexpr u64 trunc_mask(u64 sizecode) { return _TruncMasks[sizecode]; }

	// checks if the value with specified sizecode is positive (0 is considered positive). sizecode must be [0,3] otherwise und.
	inline constexpr bool positive(u64 val, u64 sizecode) { return (val & sign_mask(sizecode)) == 0; }
	// checks if the value with specified sizecode is negative. sizecode must be [0,3] otherwise und.
	inline constexpr bool negative(u64 val, u64 sizecode) { return (val & sign_mask(sizecode)) != 0; }

	// sign extends a value to 64-bits. sizecode represents the current size. sizecode must be [0,3] otherwise und.
	inline constexpr u64 sign_extend(u64 val, u64 sizecode) { return positive(val, sizecode) ? val : val | _ExtendMasks[sizecode]; }
	// truncates the value to the specified size. sizecode specifies the target size. sizecode must be [0,3] otherwise und.
	inline constexpr u64 truncate(u64 val, u64 sizecode) { return val & trunc_mask(sizecode); }

	// converts a sizecode to a size in bytes 0:1  1:2  2:4  3:8
	inline constexpr u64 get_size(u64 sizecode) { return (u64)1 << sizecode; }
	// converts a sizecode to a size in bits 0:8  1:16  2:32  3:64
	inline constexpr u64 get_size_bits(u64 sizecode) { return (u64)8 << sizecode; }
	
	// gets the sizecode for the specified size. throws std::invalid_argument if size is not a power of 2.
	inline constexpr u64 get_sizecode(u64 size)
	{
		if (!is_pow2(size)) throw std::invalid_argument("argument was not a power of 2");

		// compute sizecode by repeated shifting
		for (int i = 0; ; ++i)
		{
			size >>= 1;
			if (size == 0) return i;
		}
	}

	// returns an elementary word size in bytes sufficient to hold the specified number of bits
	inline constexpr u64 bits_to_bytes(u64 bits)
	{
		if (bits <= 8) return 1;
		else if (bits <= 16) return 2;
		else if (bits <= 32) return 4;
		else if (bits <= 64) return 8;
		else throw std::invalid_argument("bit get_size must be in range [0,64]");
	}

	// safely reinterprets a value of type U as a value of type T.
	// for semantical reasons, this function only participates in overload resolution for a specific subset of safe conversions.
	template<typename T, typename U, std::enable_if_t<is_restricted_transmutable<T, U>, int> = 0>
	constexpr T transmute(U value)
	{
		T temp;
		std::memcpy(std::addressof(temp), std::addressof(value), sizeof(T));
		return temp;
	}

	void ExtractDouble(f64 val, f64 &exp, f64 &sig);
	f64 AssembleDouble(f64 exp, f64 sig);

	// checks if val is denormal.
	template<typename T, typename U, std::enable_if_t<std::is_same_v<T, U> && is_floating<T>, int> = 0>
	inline bool is_denormal(U val)
	{
		if constexpr (std::is_same_v<T, f32>) return (transmute<u32>(val) & 0x7f800000u) == 0;
		else if constexpr (std::is_same_v<T, f64>) return (transmute<u64>(val) & 0x7ff0000000000000u) == 0;
		else return is_denormal<f64>((f64)val); // TODO: maybe something better can be done for fext, but pretty sure on most systems fext == f64 but are still considered distinct types
	}

	/// <summary>
	/// Returns true if the value is zero (int or floating)
	/// </summary>
	/// <param name="val">the value to test</param>
	/// <param name="floating">marks that val should be treated as a floating-point value (specifically, f64)</param>
	inline bool IsZero(u64 val, bool floating) { return floating ? transmute<f64>(val) == 0 : val == 0; }
}

#endif
