#include "ExeTypes.h"

namespace CSX64
{
	CPURegister_sizecode_wrapper CPURegister::operator[](int sizecode) { return {*this, sizecode}; }
	ZMMRegister_sizecode_wrapper ZMMRegister::uint(int sizecode, int index) { return {*this, (i16)index, (i16)sizecode}; }
}
