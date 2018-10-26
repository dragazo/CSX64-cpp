#include "HoleData.h"
#include "Utility.h"

namespace CSX64
{
	std::ostream &HoleData::WriteTo(std::ostream &writer, const HoleData &hole)
	{
		BinWrite(writer, hole.Address);
		BinWrite(writer, hole.Size);

		BinWrite(writer, hole.Line);
		Expr::WriteTo(writer, hole.expr);

		return writer;
	}
	std::istream &HoleData::ReadFrom(std::istream &reader, HoleData &hole)
	{
		BinRead(reader, hole.Address);
		BinRead(reader, hole.Size);

		BinRead(reader, hole.Line);
		Expr::ReadFrom(reader, hole.expr);

		return reader;
	}
}
