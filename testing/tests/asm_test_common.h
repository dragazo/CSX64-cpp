#ifndef CSX64_ASM_TEST_H
#define CSX64_ASM_TEST_H

#include <memory>
#include <string>
#include <stdexcept>
#include <initializer_list>

#include "../../include/Computer.h"
#include "../../include/Assembly.h"

#include "test_common.h"

struct AssembleException : std::runtime_error { using std::runtime_error::runtime_error; };
struct LinkException : std::runtime_error { using std::runtime_error::runtime_error; };

#define ASM_LNK(...) asm_lnk(TOSTR(__LINE__), { __VA_ARGS__ })

// assembles and links the program(s) into an executable and loads it into the computer for execution.
// if this fails, it throws an exception of some kind (AssembleException or LinkException for asm/lnk failure).
std::unique_ptr<CSX64::Computer> asm_lnk(const char *loc, std::initializer_list<const char*> progs);

#endif
