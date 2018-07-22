#ifndef CSX64_OFILE_H
#define CSX64_OFILE_H

#include <vector>
#include <unordered_set>
#include <string>
#include <iostream>

#include "CoreTypes.h"
#include "Expr.h"
#include "HoleData.h"

namespace CSX64
{
	/// <summary>
	/// Represents an assembled object file used to create an executable
	/// </summary>
	class ObjectFile
	{
	//private:
	public:
		bool _Clean = false;

		//friend AssembleResult Assemble(const std::string &code, ObjectFile &file);

	public:

		// The list of exported symbol names
		std::unordered_set<std::string> GlobalSymbols;
		// The list of imported symbol names
		std::unordered_set<std::string> ExternalSymbols;

		// The symbols defined in the file
		std::unordered_map<std::string, Expr> Symbols;

		u32 TextAlign = 1;
		u32 RodataAlign = 1;
		u32 DataAlign = 1;
		u32 BSSAlign = 1;

		std::vector<HoleData> TextHoles;
		std::vector<HoleData> RodataHoles;
		std::vector<HoleData> DataHoles;

		std::vector<u8> Text;
		std::vector<u8> Rodata;
		std::vector<u8> Data;
		u32 BssLen = 0;

		// Marks that this object file is in a valid, usable state
		bool Clean() const { return _Clean; }
		// marks the object file as dirty
		void MakeDirty() { _Clean = false; }

		// clears the contents of the object file (result is dirty)
		void Clear();

		// ---------------------------

		/// <summary>
		/// Writes a binary representation of an object file to the stream. Throws <see cref="std::invalid_argument"/> if file is dirty
		/// </summary>
		/// <param name="writer">the binary writer to use. must be clean</param>
		/// <param name="obj">the object file to write</param>
		static std::ostream &WriteTo(std::ostream &writer, const ObjectFile &obj);
		/// <summary>
		/// Reads a binary representation of an object file from the stream
		/// </summary>
		/// <param name="reader">the binary reader to use</param>
		/// <param name="hole">the resulting object file</param>
		static std::istream &ReadFrom(std::istream &reader, ObjectFile &obj);
	};
}

#endif
