#include <iostream>

#include "Assembly.h"
#include "Utility.h"

namespace CSX64
{
	void ObjectFile::Clear()
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

	std::ostream &ObjectFile::WriteTo(std::ostream &writer, const ObjectFile &obj)
	{
		// ensure the object is clean
		if (!obj.Clean()) throw std::invalid_argument("Attempt to use dirty object file");

		// -- write globals -- //

		BinWrite<u32>(writer, obj.GlobalSymbols.size());
		for (const std::string &symbol : obj.GlobalSymbols)
			BinWrite(writer, symbol);
		
		// -- write externals -- //

		BinWrite<u32>(writer, obj.ExternalSymbols.size());
		for (const std::string &symbol : obj.ExternalSymbols)
			BinWrite(writer, symbol);

		// -- write symbols -- //

		BinWrite<u32>(writer, obj.Symbols.size());
		for(const auto &entry : obj.Symbols)
		{
			BinWrite(writer, entry.first);
			Expr::WriteTo(writer, entry.second);
		}
		
		// -- write alignments -- //

		BinWrite(writer, obj.TextAlign);
		BinWrite(writer, obj.RodataAlign);
		BinWrite(writer, obj.DataAlign);
		BinWrite(writer, obj.BSSAlign);

		// -- write segment holes -- //

		BinWrite<u32>(writer, obj.TextHoles.size());
		for (const HoleData &hole : obj.TextHoles) HoleData::WriteTo(writer, hole);

		BinWrite<u32>(writer, obj.RodataHoles.size());
		for (const HoleData &hole : obj.RodataHoles) HoleData::WriteTo(writer, hole);

		BinWrite<u32>(writer, obj.DataHoles.size());
		for (const HoleData &hole : obj.DataHoles) HoleData::WriteTo(writer, hole);
		
		// -- write segments -- //

		BinWrite<u64>(writer, obj.Text.size());
		writer.write((char*)obj.Text.data(), obj.Text.size());

		BinWrite<u64>(writer, obj.Rodata.size());
		writer.write((char*)obj.Rodata.data(), obj.Rodata.size());

		BinWrite<u64>(writer, obj.Data.size());
		writer.write((char*)obj.Data.data(), obj.Data.size());
		
		BinWrite<u64>(writer, obj.BssLen);
		
		// -- done -- //

		return writer;
	}
	std::istream &ObjectFile::ReadFrom(std::istream &reader, ObjectFile &obj)
	{
		// mark as initially dirty
		obj._Clean = false;

		u64 i;

		u32 temp32;
		u64 temp64;

		std::string str;
		Expr expr;
		HoleData hole;

		// -- read globals -- //

		if (!BinRead(reader, temp32)) goto err;
		obj.GlobalSymbols.clear();
		obj.GlobalSymbols.reserve(temp32);
		for (i = 0; i < temp32; ++i)
		{
			if (!BinRead(reader, str)) goto err;
			obj.GlobalSymbols.emplace(std::move(str));
		}

		// -- read externals -- //

		if (!BinRead(reader, temp32)) goto err;
		obj.ExternalSymbols.clear();
		obj.ExternalSymbols.reserve(temp32);
		for (i = 0; i < temp32; ++i)
		{
			if (!BinRead(reader, str)) goto err;
			obj.ExternalSymbols.emplace(std::move(str));
		}

		// -- read symbols -- //

		if (!BinRead(reader, temp32)) goto err;
		obj.Symbols.clear();
		obj.Symbols.reserve(temp32);
		for (i = 0; i < temp32; ++i)
		{
			if (!BinRead(reader, str)) goto err;
			if (!Expr::ReadFrom(reader, expr)) goto err;
			obj.Symbols.emplace(std::move(str), std::move(expr));
		}

		// -- read alignments -- //

		if (!BinRead(reader, obj.TextAlign)) goto err;
		if (!BinRead(reader, obj.RodataAlign)) goto err;
		if (!BinRead(reader, obj.DataAlign)) goto err;
		if (!BinRead(reader, obj.BSSAlign)) goto err;

		// -- read segment holes -- //

		if (!BinRead(reader, temp32)) goto err;
		obj.TextHoles.clear();
		obj.TextHoles.reserve(temp32);
		for (i = 0; i < temp32; ++i)
		{
			if (!HoleData::ReadFrom(reader, hole)) goto err;
			obj.TextHoles.emplace_back(std::move(hole));
		}

		if (!BinRead(reader, temp32)) goto err;
		obj.RodataHoles.clear();
		obj.RodataHoles.reserve(temp32);
		for (i = 0; i < temp32; ++i)
		{
			if (!HoleData::ReadFrom(reader, hole)) goto err;
			obj.RodataHoles.emplace_back(std::move(hole));
		}

		if (!BinRead(reader, temp32)) goto err;
		obj.DataHoles.clear();
		obj.DataHoles.reserve(temp32);
		for (i = 0; i < temp32; ++i)
		{
			if (!HoleData::ReadFrom(reader, hole)) goto err;
			obj.DataHoles.emplace_back(std::move(hole));
		}

		// -- read segments -- //

		if (!BinRead(reader, temp64)) goto err;
		obj.Text.clear();
		obj.Text.resize(temp64);
		reader.read((char*)obj.Text.data(), temp64);
		if (reader.gcount() != temp64) goto err;

		if (!BinRead(reader, temp64)) goto err;
		obj.Rodata.clear();
		obj.Rodata.resize(temp64);
		reader.read((char*)obj.Rodata.data(), temp64);
		if (reader.gcount() != temp64) goto err;

		if (!BinRead(reader, temp64)) goto err;
		obj.Data.clear();
		obj.Data.resize(temp64);
		reader.read((char*)obj.Data.data(), temp64);
		if (reader.gcount() != temp64) goto err;

		if (!BinRead(reader, temp64)) goto err;
		obj.BssLen = temp64;

		// -- done -- //

		// validate the object
		obj._Clean = true;
		return reader;

		// error label we can jump to to avoid repetitive throws
		err: throw std::runtime_error("Object file was corrupted");
	}
}
