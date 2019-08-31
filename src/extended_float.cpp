#include <cstring>

#include "../include/CoreTypes.h"
#include "../include/Utility.h"

namespace CSX64
{
	f80::f80(f64 val) noexcept
	{
		// get bits of val
		u64 bits = DoubleAsUInt64(val);

		// extract the fields
		u64 frac = bits & (u64)0xfffffffffffff;
		bits >>= 52;
		u16 exp = bits & (u16)0x7ff;
		bool sign = bits & 0x800;

		// extend the fields
		frac <<= 12; // just put zeroes in low order fraction bits
		if (exp == (u16)0x7ff) exp = (u16)0x7fff; // all 1's stays all 1's (inf and nan)
		else if (exp == 0) // denormal must be corrected
		{
			if (frac != 0) // but only if non-zero
			{
				while (true)
				{
					bool high = frac & (u64)0x8000000000000000;
					frac <<= 1;
					++exp;

					if (high) break;
				}
			}
		}
		else exp = (u16)((int)exp - 1023 + 16383); // otherwise rebias to 80 bit

		// merge sign field into the exp field (for convenience)
		if (sign) exp |= (u16)0x8000;

		// store into the data array (little endian)
		std::memcpy(data, &frac, 8);
		std::memcpy(data + 8, &exp, 2);
	}

	f80::operator f64() const noexcept
	{
		// extract the frac and exp:sign fields (see f80(f64) ctor)
		u64 frac;
		u16 exp_sign;
		std::memcpy(&frac, data, 8);
		std::memcpy(&exp_sign, data + 8, 2);

		u16 exp = exp_sign & (u16)0x7fff;
		bool sign = exp_sign & (u16)0x8000;

		// truncate the fields
		frac >>= 12; // chop off the low order fraction bits
		if (exp == (u16)0x7fff) exp = (u16)0x7ff; // all 1's stays all 1's (inf and nan)
		else if (exp == 0) frac = 0; // denormal extended will truncate to (signed) 0 f64
		else
		{
			int _exp = (int)exp - 16383 + 1023; // otherwise rebias to 64 bit

			// if rebiased exponent is negative, we need to denormalize it
			if (_exp < 0)
			{
				exp = 0;
				frac = -_exp < 64 ? frac >> -_exp : 0;
			}
			// otherwise just use the rebiased exponent field
			else exp = (u16)_exp;
		}

		// recombine fields into f64 format
		return AsDouble((sign ? (u64)0x8000000000000000 : 0) | ((u64)exp << 52) | frac);
	}

	f80 &f80::operator+=(const f80 &other) noexcept
	{
		*this = (f64)*this + (f64)other;
		return *this;
	}
	f80 &f80::operator-=(const f80 &other) noexcept
	{
		*this = (f64)*this - (f64)other;
		return *this;
	}

	f80 &f80::operator*=(const f80 &other) noexcept
	{
		*this = (f64)*this * (f64)other;
		return *this;
	}
	f80 &f80::operator/=(const f80 &other) noexcept
	{
		*this = (f64)*this / (f64)other;
		return *this;
	}
}