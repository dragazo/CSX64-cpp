#ifndef CSX64_TEST_COMMON_H
#define CSX64_TEST_COMMON_H

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <type_traits>

template<typename T> std::string sstostr(const T &v)
{
	if constexpr (std::is_enum_v<T>) return std::to_string((std::underlying_type_t<T>)v);
	else { std::ostringstream ss; ss << v; return ss.str(); }
}

struct assertion_error : std::runtime_error { using std::runtime_error::runtime_error; };

#define _TOSTR(x) #x
#define TOSTR(x) _TOSTR(x)

#define ASSERT(expr) { if (!(expr)) throw assertion_error("assertion failed: " __FILE__ ":" TOSTR(__LINE__) "\n\t" #expr "\n\tevaluated to false"); }

#define ASSERT_OP(actual, expected, op) { auto v1 = (actual); auto v2 = (expected); if (!(v1 op v2)) throw assertion_error("assertion failed: " __FILE__ ":" TOSTR(__LINE__) "\n\t" #actual " " #op " " #expected "\n\tevaluated to false\n\tleft:  " + sstostr(v2) + "\n\tright: " + sstostr(v1)); }
#define ASSERT_EQ(actual, expected) ASSERT_OP(actual, expected, ==)
#define ASSERT_NEQ(actual, expected) ASSERT_OP(actual, expected, !=)
#define ASSERT_L(actual, expected) ASSERT_OP(actual, expected, <)
#define ASSERT_LE(actual, expected) ASSERT_OP(actual, expected, <=)
#define ASSERT_G(actual, expected) ASSERT_OP(actual, expected, >)
#define ASSERT_GE(actual, expected) ASSERT_OP(actual, expected, >=)

#define ASSERT_THROWS(expr, ex) { int temp_state_name = 0; try { (expr); temp_state_name = 1; } catch (const ex&) {} catch (...) { temp_state_name = 2; } \
	switch (temp_state_name) { \
		case 1: throw assertion_error("assertion failed: " __FILE__ ":" TOSTR(__LINE__) "\n\t" #expr "\n\tdid not throw"); \
		case 2: throw assertion_error("assertion failed: " __FILE__ ":" TOSTR(__LINE__) "\n\t" #expr "\n\tthrew wrong type"); \
	} \
}

#define RUN_TEST(x) run_test(x, #x)

static void run_test(void(*test)(), const char *test_name)
{
	try { test(); std::cout << "passed " << test_name << '\n'; }
	catch (const std::exception &ex) { std::cerr << "FAILED " << test_name << " - " << ex.what() << '\n'; }
	catch (...) { std::cerr << test_name << " FAILED - unhandled exception of unknown type\n"; }
}

#endif
