#include <sstream>
#include <list>
#include <string>
#include <utility>
#include <memory>

#include "asm_test_common.h"

using namespace CSX64;

std::unique_ptr<Computer> asm_lnk(std::initializer_list<const char*> progs)
{
	std::istringstream code_stream;
	std::list<std::pair<std::string, ObjectFile>> objs;

	std::size_t i = 0;
	for (const std::string &prog : progs)
	{
		code_stream.str(prog);

		auto &obj_raw = objs.emplace_back();
		obj_raw.first = "<string:" + sstostr(i++) + ">";
		auto &obj = obj_raw.second;

		if (auto asm_res = assemble(code_stream, obj); asm_res.Error != AssembleError::None)
			throw AssembleException(obj_raw.first + " - assemble error: " + asm_res.ErrorMsg);
	}

	Executable exe;
	if (auto lnk_res = link(exe, objs); lnk_res.Error != LinkError::None) throw LinkException("link error: " + lnk_res.ErrorMsg);

	auto p = std::make_unique<Computer>();
	p->initialize(exe, {});
	return p;
}
