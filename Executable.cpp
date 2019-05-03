#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

#include "CoreTypes.h"
#include "Utility.h"
#include "csx_exceptions.h"

#include "Executable.h"

namespace CSX64
{
	Executable::Executable() { clear(); }

	Executable::Executable(Executable &&other) noexcept : _content(std::move(other._content))
	{
		std::memcpy(_seglens, other._seglens, sizeof(_seglens));

		other.clear(); // make sure other is left in the empty state
	}
	Executable &Executable::operator=(Executable &&other) noexcept
	{	
		swap(*this, other); // just use the swapping idiom - this is safe on self-assignment
	}

	// ------------------------------------------------- //

	void swap(Executable &a, Executable &b) noexcept
	{
		using std::swap;

		swap(a._seglens, b._seglens);
		swap(a._content, b._content);
	}

	// ------------------------------------------------- //

	void Executable::construct(const std::vector<u8> &text, const std::vector<u8> &rodata, const std::vector<u8> &data, u64 bsslen)
	{
		// record segment lengths
		_seglens[0] = text.size();
		_seglens[1] = rodata.size();
		_seglens[2] = data.size();
		_seglens[3] = bsslen;

		// make sure seg lengths don't overflow u64
		{
			u64 sum = 0;
			for (u64 val : _seglens) if ((sum += val) < val)
			{
				clear(); // if we throw an exception, we must leave the exe in the empty state
				throw std::overflow_error("Total executable length exceeds maximum size");
			}
		}

		// resize _content to hold all the segments (except bss) - if this throws, set to empty state and rethrow
		_content.clear();
		try { _content.resize(text.size() + rodata.size() + data.size()); }
		catch (...) { clear(); throw; }

		// copy over all the segments
		if (text.size() != 0) /*  */ std::memcpy(_content.data() + 0, /*                     */ text.data(), text.size());
		if (rodata.size() != 0) /**/ std::memcpy(_content.data() + text.size(), /*           */ rodata.data(), rodata.size());
		if (data.size() != 0) /*  */ std::memcpy(_content.data() + text.size() + rodata.size(), data.data(), data.size());
	}

	// ------------------------------------------------- //

	bool Executable::empty() const noexcept
	{
		for (u64 i : _seglens) if (i != 0) return false;
		return true;
	}
	void Executable::clear() noexcept
	{
		for (u64 &i : _seglens) i = 0;
		_content.clear();
	}

	// ------------------------------------------------- //

	static const u8 header[] = { 'C', 'S', 'X', '6', '4', 'e', 'x', 'e' };

	void Executable::save(const std::string &path) const
	{
		std::ofstream file(path, std::ios::binary);
		if (!file) throw FileOpenError("Failed to open file for saving Executable");

		// write exe header and CSX64 version number
		file.write(reinterpret_cast<const char*>(header), sizeof(header));
		BinWrite(file, Version);

		// write the segment lengths
		file.write(reinterpret_cast<const char*>(_seglens), sizeof(_seglens));

		// write the content of the executable
		file.write(reinterpret_cast<const char*>(_content.data()), _content.size());

		// make sure the writes succeeded
		if (!file) throw IOError("Failed to write Executable to file");
	}
	void Executable::load(const std::string &path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) throw FileOpenError("Failed to open file for loading Executable");

		// get the file size and seek back to the beginning
		const u64 file_size = file.tellg();
		file.seekg(0);

		u8 header_temp[sizeof(header)];
		u64 Version_temp;

		// -- file validation -- //

		// read the header from the file and make sure it matches - match failure is a type error, not a format error.
		if (!file.read(reinterpret_cast<char*>(header_temp), sizeof(header)) || file.gcount() != sizeof(header)) goto err;
		if (std::memcmp(header_temp, header, sizeof(header)))
		{
			clear(); // if we throw an exception we must set to the empty state first
			throw TypeError("File was not a CSX64 executable");
		}

		// read the version number from the file and make sure it matches - match failure is a version error, not a format error.
		if (!BinRead(file, Version_temp)) goto err;
		if (Version_temp != Version)
		{
			clear(); // if we throw an exception we must set to the empty state first
			throw VersionError("Executable was from an incompatible version of CSX64");
		}

		// -- read executable info -- //

		// read the segment lengths - make sure we got everything
		if (!file.read(reinterpret_cast<char*>(_seglens), sizeof(_seglens)) || file.gcount() != sizeof(_seglens)) goto err;

		// make sure seg lengths don't overflow u64
		{
			u64 sum = 0;
			for (u64 val : _seglens) if ((sum += val) < val) goto err;
		}

		// make sure the file is the correct size
		if (file_size != 48 + _seglens[0] + _seglens[1] + _seglens[2]) goto err;

		// -- read executable content -- //

		// allocate space to hold the executable content - if that fails set the exe to empty state and rethrow
		_content.clear();
		try { _content.resize(_seglens[0] + _seglens[1] + _seglens[2]); }
		catch (...) { clear(); throw; }

		// read the content - make sure we got everything
		if (!file.read(reinterpret_cast<char*>(_content.data()), _content.size()) || file.gcount() != _content.size()) goto err;

		return;

	err:
		clear(); // if we throw an exception we must leave the exe in the empty state
		throw FormatError("Executable file was corrupted");
	}
}
