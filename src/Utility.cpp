#include <climits>
#include <iomanip>

#include "../include/Utility.h"
#include "../ios-frstor/iosfrstor.h"

namespace CSX64::detail
{
	// -- assertions -- //

	static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big, "mixed endian systems are not currently supported");
	static_assert(CHAR_BIT == 8, "only 8-bit byte systems are currently supported");
	static_assert(sizeof(u8) == 1, "u8 is not get_size 1");

	// -- serialization -- //

	std::ostream &write_bin(std::ostream &ostr, const void *p, std::size_t count)
	{
		ostr.write(reinterpret_cast<const char*>(p), count);
		return ostr;
	}
	std::istream &read_bin(std::istream &istr, void *p, std::size_t count)
	{
		istr.read(reinterpret_cast<char*>(p), count);
		if ((std::size_t)istr.gcount() != count) istr.setstate(std::ios::failbit);
		return istr;
	}

	std::ostream &write_str(std::ostream &ostr, std::string_view str)
	{
		u16 len = (u16)str.size();
		if (len != str.size()) throw std::invalid_argument("write_str(std::string): string was too long");

		write<u16>(ostr, len);
		return write_bin(ostr, str.data(), len);
	}
	std::istream &read_str(std::istream &istr, std::string &str)
	{
		u16 len;
		read<u16>(istr, len);
		str.resize(len);
		return read_bin(istr, str.data(), len);
	}

	// -- memory utilities -- //

	bool Write(std::vector<u8> &arr, u64 pos, u64 size, u64 val)
	{
		// make sure we're not exceeding memory bounds
		if (pos >= arr.size() || pos + size > arr.size()) return false;

		// write the value (little-endian)
		for (std::size_t i = 0; i < size; ++i)
		{
			arr[(std::size_t)pos + i] = (u8)val;
			val >>= 8;
		}

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
	
	std::string to_upper(std::string str)
	{
		for (char &i : str) i = std::toupper((unsigned char)i);
		return str;
	}
	std::string to_lower(std::string str)
	{
		for (char &i : str) i = std::tolower((unsigned char)i);
		return str;
	}

	std::string trim_start(std::string_view str)
	{
		std::size_t i;
		for (i = 0; i < str.size() && std::isspace((unsigned char)str[i]); ++i);
		return { str.begin() + i, str.end() };
	}
	std::string trim_end(std::string str)
	{
		std::size_t i;
		for (i = str.size(); i > 0 && std::isspace((unsigned char)str[i - 1]); --i);
		str.resize(i);
		return str;
	}
	std::string trim(std::string str)
	{
		return trim_start(trim_end(std::move(str)));
	}

	std::string remove_char(std::string_view str, char ch)
	{
		std::string res;
		res.reserve(str.size());
		for (char i : str) if (i != ch) res.push_back(i);
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
	bool TryParseDouble(const std::string &str, f64 &val)
	{
		// extract the f64 value
		std::istringstream istr(str);
		istr >> val;

		// that should've succeeded and consumed the entire string
		return istr && istr.peek() == EOF;
	}

	bool starts_with(std::string_view str, std::string_view val)
	{
		if (str.size() < val.size()) return false; // must be at least long enough to hold it
		if (val.size() == 0) return true;          // everything starts with empty string
		
		return std::memcmp(str.data(), val.data(), val.size()) == 0;
	}
	bool starts_with_token(std::string_view str, std::string_view val)
	{
		return starts_with(str, val) && (str.size() == val.size() || std::isspace((unsigned char)str[val.size()]));
	}
	
	bool ends_with(std::string_view str, std::string_view val)
	{
		if (str.size() < val.size()) return false; // must be at least long enough to hold it
		if (val.size() == 0) return true;          // everything ends with empty string

		return std::memcmp(str.data() + (str.size() - val.size()), val.data(), val.size()) == 0;
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

		char str[18];   // create a 16-character dump string
		str[16] = '\n'; // each string dump is at the end of a line
		str[17] = 0;    // null terminate it

		u64 len = 0; // current length of str

		dump << std::hex << std::setfill('0');                                        // switch to hex mode and zero fill
		dump << "                  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n"; // write the top header
		dump << std::setw(16) << (start & ~(u64)15) << ' ';                           // write the first hex dump address

		// tack on white space to skip any characters we're missing in the first row
		for (u64 i = start & 15; i > 0; --i)
		{
			dump << "   ";
			str[len++] = ' '; // for any skipped byte, set its string char to space
		}

		// write the data
		for (u64 i = 0; i < count; ++i)
		{
			// if we just finished a row, write the string dump and the next hex dump address
			if (len == 16)
			{
				dump << str << std::setw(16) << (start + i) << ' ';
				len = 0;
			}

			unsigned char ch = reinterpret_cast<const unsigned char*>(data)[start + i]; // get the character to print
			dump << std::setw(2) << (int)ch << ' ';   // print it to the hex dump
			str[len++] = std::isprint(ch) ? ch : '.'; // if the character is printable, use it for the string dump, otherwise use '.'
		}

		// finish off the last row
		for (u64 i = 16 - len; i > 0; --i) dump << "   ";
		str[len] = 0;
		dump << str;

		// end with a new line
		return dump << '\n';
	}

	// -- CSX64 encoding utilities -- //

	void ExtractDouble(f64 val, f64 &exp, f64 &sig)
	{
		if (std::isnan(val))
		{
			exp = sig = std::numeric_limits<f64>::quiet_NaN();
		}
		else if (std::isinf(val))
		{
			if (val > 0)
			{
				exp = std::numeric_limits<f64>::infinity();
				sig = 1;
			}
			else
			{
				exp = std::numeric_limits<f64>::infinity();
				sig = -1;
			}
		}
		else
		{
			// get the raw bits
			u64 bits = transmute<u64>(val);

			// get the raw exponent
			u64 raw_exp = (bits >> 52) & 0x7ff;

			// get exponent and subtract bias
			exp = (f64)raw_exp - 1023;
			// get significand (m.0) and offset to 0.m
			sig = (f64)(bits & 0xfffffffffffff) / ((u64)1 << 52);

			// if it's denormalized, add 1 to exponent
			if (raw_exp == 0) exp += 1;
			// otherwise add the invisible 1 to get 1.m
			else sig += 1;

			// handle negative case
			if (val < 0) sig = -sig;
		}
	}
	f64 AssembleDouble(f64 exp, f64 sig)
	{
		return sig * pow(2, exp);
	}
}
