#ifndef CSX64_HOLEDATA_H
#define CSX64_HOLEDATA_H

#include <iostream>

#include "CoreTypes.h"
#include "Expr.h"

namespace CSX64
{
	/// <summary>
	/// Holds information on the location and value of missing pieces of information in an object file
	/// </summary>
	class HoleData
	{
	public:

		// The local address of the hole in the file
		u64 Address;
		// The size of the hole
		u8 Size;

		// The line where this hole was created
		int Line;
		// The expression that represents this hole's value
		Expr expr;

	public: // -- IO -- //

		/// <summary>
		/// Writes a binary representation of a hole to the stream
		/// </summary>
		/// <param name="writer">the binary writer to use</param>
		/// <param name="hole">the hole to write</param>
		static std::ostream &WriteTo(std::ostream &writer, const HoleData &hole);
		/// <summary>
		/// Reads a binary representation of a hole from the stream
		/// </summary>
		/// <param name="reader">the binary reader to use</param>
		/// <param name="hole">the resulting hole</param>
		static std::istream &ReadFrom(std::istream &reader, HoleData &hole);
	};
}

#endif
