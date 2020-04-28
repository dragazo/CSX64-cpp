#ifndef CSX64_ASM_TEST_H
#define CSX64_ASM_TEST_H

#include "../../include/Computer.h"
#include "../../include/Assembly.h"

#include <memory>
#include <string>
#include <stdexcept>

struct AssembleException : std::runtime_error { using std::runtime_error::runtime_error; };
struct LinkException : std::runtime_error { using std::runtime_error::runtime_error; };

// assembles and links the code into an executable and loads it into the computer for execution.
// if this fails, it throws an exception of some kind (AssembleException or LinkException for asm/lnk failure).
std::unique_ptr<CSX64::Computer> asm_lnk(const std::string &code);

#endif
