#include <iostream>
#include <fstream>

#include "Assembly.h"
#include "Utility.h"
#include "csx_exceptions.h"

namespace CSX64
{
	void ObjectFile::clear() noexcept
	{
		_Clean = false;

		GlobalSymbols.clear();
		ExternalSymbols.clear();

		Symbols.clear();

		TextAlign = 1;
		RodataAlign = 1;
		DataAlign = 1;
		BSSAlign = 1;

		TextHoles.clear();
		RodataHoles.clear();
		DataHoles.clear();

		Text.clear();
		Rodata.clear();
		Data.clear();
		BssLen = 0;
	}

	// -------------------------------------------------- //

	static const u8 header[] = { 'C', 'S', 'X', '6', '4', 'o', 'b', 'j' };

	void ObjectFile::save(const std::string &path) const
	{
		// ensure the object is clean
		if (!is_clean()) throw DirtyError("Attempt to save dirty object file");

		std::ofstream file(path, std::ios::binary);
		if (!file) throw FileOpenError("Failed to open file for saving object file");

		// -- write header and CSX64 version number -- //

		file.write(reinterpret_cast<const char*>(header), sizeof(header));
		BinWrite(file, Version);

		// -- write globals -- //

		BinWrite<u64>(file, GlobalSymbols.size());
		for (const std::string &symbol : GlobalSymbols)
			BinWrite(file, symbol);
		
		// -- write externals -- //

		BinWrite<u64>(file, ExternalSymbols.size());
		for (const std::string &symbol : ExternalSymbols)
			BinWrite(file, symbol);

		// -- write symbols -- //

		BinWrite<u64>(file, Symbols.size());
		for(const auto &entry : Symbols)
		{
			BinWrite(file, entry.first);
			Expr::WriteTo(file, entry.second);
		}
		
		// -- write alignments -- //

		BinWrite(file, TextAlign);
		BinWrite(file, RodataAlign);
		BinWrite(file, DataAlign);
		BinWrite(file, BSSAlign);

		// -- write segment holes -- //

		BinWrite<u64>(file, TextHoles.size());
		for (const HoleData &hole : TextHoles) HoleData::WriteTo(file, hole);

		BinWrite<u64>(file, RodataHoles.size());
		for (const HoleData &hole : RodataHoles) HoleData::WriteTo(file, hole);

		BinWrite<u64>(file, DataHoles.size());
		for (const HoleData &hole : DataHoles) HoleData::WriteTo(file, hole);
		
		// -- write segments -- //

		BinWrite<u64>(file, Text.size());
		file.write(reinterpret_cast<const char*>(Text.data()), Text.size());

		BinWrite<u64>(file, Rodata.size());
		file.write(reinterpret_cast<const char*>(Rodata.data()), Rodata.size());

		BinWrite<u64>(file, Data.size());
		file.write(reinterpret_cast<const char*>(Data.data()), Data.size());
		
		BinWrite<u64>(file, BssLen);

		// -- validation -- //

		// make sure the writes succeeded
		if (!file) throw IOError("Failed to write object file to file");
	}
	void ObjectFile::load(const std::string &path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file) throw FileOpenError("Failed to open file for loading object file");

		// mark as initially dirty
		_Clean = false;

		u64 val;
		std::string str;
		Expr expr;
		HoleData hole;
		u8 header_temp[sizeof(header)];

		// -- file validation -- //

		// read header and make sure it matches - match failure is type error, not format error.
		if (!file.read(reinterpret_cast<char*>(header_temp), sizeof(header)) || file.gcount() != sizeof(header)) goto err;
		if (std::memcmp(header_temp, header, sizeof(header)))
		{
			throw TypeError("File was not a CSX64 executable");
		}

		// read the version number and make sure it matches - match failure is a version error, not a format error
		if (!BinRead(file, val)) goto err;
		if (val != Version)
		{
			throw VersionError("Executable was from an incompatible version of CSX64");
		}

		// -- read globals -- //

		if (!BinRead(file, val)) goto err;
		GlobalSymbols.clear();
		GlobalSymbols.reserve(val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!BinRead(file, str)) goto err;
			GlobalSymbols.emplace(std::move(str));
		}

		// -- read externals -- //

		if (!BinRead(file, val)) goto err;
		ExternalSymbols.clear();
		ExternalSymbols.reserve(val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!BinRead(file, str)) goto err;
			ExternalSymbols.emplace(std::move(str));
		}

		// -- read symbols -- //

		if (!BinRead(file, val)) goto err;
		Symbols.clear();
		Symbols.reserve(val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!BinRead(file, str)) goto err;
			if (!Expr::ReadFrom(file, expr)) goto err;
			Symbols.emplace(std::move(str), std::move(expr));
		}

		// -- read alignments -- //

		if (!BinRead(file, TextAlign) || !IsPowerOf2(TextAlign)) goto err;
		if (!BinRead(file, RodataAlign) || !IsPowerOf2(RodataAlign)) goto err;
		if (!BinRead(file, DataAlign) || !IsPowerOf2(DataAlign)) goto err;
		if (!BinRead(file, BSSAlign) || !IsPowerOf2(BSSAlign)) goto err;

		// -- read segment holes -- //

		if (!BinRead(file, val)) goto err;
		TextHoles.clear();
		TextHoles.reserve(val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!HoleData::ReadFrom(file, hole)) goto err;
			TextHoles.emplace_back(std::move(hole));
		}

		if (!BinRead(file, val)) goto err;
		RodataHoles.clear();
		RodataHoles.reserve(val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!HoleData::ReadFrom(file, hole)) goto err;
			RodataHoles.emplace_back(std::move(hole));
		}

		if (!BinRead(file, val)) goto err;
		DataHoles.clear();
		DataHoles.reserve(val);
		for (u64 i = 0; i < val; ++i)
		{
			if (!HoleData::ReadFrom(file, hole)) goto err;
			DataHoles.emplace_back(std::move(hole));
		}

		// -- read segments -- //

		if (!BinRead(file, val)) goto err;
		Text.clear();
		Text.resize(val);
		if (!file.read(reinterpret_cast<char*>(Text.data()), val) || file.gcount() != val) goto err;

		if (!BinRead(file, val)) goto err;
		Rodata.clear();
		Rodata.resize(val);
		if (!file.read(reinterpret_cast<char*>(Rodata.data()), val) || file.gcount() != val) goto err;

		if (!BinRead(file, val)) goto err;
		Data.clear();
		Data.resize(val);
		if (!file.read(reinterpret_cast<char*>(Data.data()), val) || file.gcount() != val) goto err;

		if (!BinRead(file, BssLen)) goto err;

		// -- done -- //

		// validate the object
		_Clean = true;

		return;

	err:
		throw FormatError("Object file was corrupted");
	}
}
