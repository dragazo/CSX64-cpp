#include <iostream>
#include <iomanip>

#include "Computer.h"
#include "ios-frstor/iosfrstor.h"

namespace CSX64
{
	// Writes a string containing all non-vpu register/flag states"/>
	std::ostream &Computer::WriteCPUDebugString(std::ostream &ostr)
	{
		iosfrstor _frstor(ostr);

		ostr << std::hex << std::setfill('0') << std::setw(0) << std::noboolalpha;

		ostr << '\n';
		ostr << "RAX: " << std::setw(16) << RAX() << "     CF: " << CF() << "     RFLAGS: " << std::setw(16) << RFLAGS() << '\n';
		ostr << "RBX: " << std::setw(16) << RBX() << "     PF: " << PF() << "     RIP:    " << std::setw(16) << RIP() << '\n';
		ostr << "RCX: " << std::setw(16) << RCX() << "     AF: " << AF() << '\n';
		ostr << "RDX: " << std::setw(16) << RDX() << "     ZF: " << ZF() << "     ST0: " << ST(0) << '\n';
		ostr << "RSI: " << std::setw(16) << RSI() << "     SF: " << SF() << "     ST1: " << ST(1) << '\n';
		ostr << "RDI: " << std::setw(16) << RDI() << "     OF: " << OF() << "     ST2: " << ST(2) << '\n';
		ostr << "RBP: " << std::setw(16) << RBP() << "               ST3: " << ST(3) << '\n';
		ostr << "RSP: " << std::setw(16) << RSP() << "     b:  " << cc_b() << "     ST4: " << ST(4) << '\n';
		ostr << "R8:  " << std::setw(16) << R8() << "     be: " << cc_be() << "     ST5: " << ST(5) << '\n';
		ostr << "R9:  " << std::setw(16) << R9() << "     a:  " << cc_a() << "     ST6: " << ST(6) << '\n';
		ostr << "R10: " << std::setw(16) << R10() << "     ae: " << cc_ae() << "     ST7: " << ST(7) << '\n';
		ostr << "R11: " << std::setw(16) << R11() << '\n';
		ostr << "R12: " << std::setw(16) << R12() << "     l:  " << cc_l() << "     C0: " << FPU_C0() << '\n';
		ostr << "R13: " << std::setw(16) << R13() << "     le: " << cc_le() << "     C1: " << FPU_C1() << '\n';
		ostr << "R14: " << std::setw(16) << R14() << "     g:  " << cc_g() << "     C2: " << FPU_C2() << '\n';
		ostr << "R15: " << std::setw(16) << R15() << "     ge: " << cc_ge() << "     C3: " << FPU_C3() << '\n';

		return ostr;
	}
	// Writes a string containing all vpu register states
	std::ostream &Computer::WriteVPUDebugString(std::ostream &ostr)
	{
		iosfrstor _frstor(ostr);

		ostr << std::setfill('0') << std::setw(0);

		ostr << '\n';
		for (int i = 0; i < 32; ++i)
		{
			ostr << "ZMM" << std::dec << i << ": ";
			if (i < 10) ostr << ' ';

			ostr << std::hex;
			for (int j = 7; j >= 0; --j) ostr << std::setw(16) << ZMMRegisters[i].int64(j) << ' ';
			ostr << '\n';
		}

		return ostr;
	}
	// Writes a string containing both <see cref="GetCPUDebugString"/> and <see cref="GetVPUDebugString"/>
	std::ostream &Computer::WriteFullDebugString(std::ostream &ostr)
	{
		WriteCPUDebugString(ostr);
		WriteVPUDebugString(ostr);

		return ostr;
	}
}
