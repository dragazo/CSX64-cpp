#include "test_common.h"

#include "../../include/Computer.h"

using namespace CSX64;

void test_transmutes()
{
	ASSERT_EQ(0x400a000000000000u, detail::transmute<u64>(3.25));
	ASSERT_EQ(0x40500000u, detail::transmute<u32>(3.25f));

	ASSERT_EQ(3.25, detail::transmute<f64>(0x400a000000000000u));
	ASSERT_EQ(3.25f, detail::transmute<f32>(0x40500000u));
}
void test_cpu_registers()
{
	detail::CPURegister r;
	ASSERT_EQ(sizeof(r), 8);

	r.x64() = 0x0102030405060708;
	ASSERT_EQ(r.x64(), 0x0102030405060708);
	ASSERT_EQ(r.x32(), 0x05060708);
	ASSERT_EQ(r.x16(),  0x0708);
	ASSERT_EQ(r.x8(), 0x08);
	ASSERT_EQ(r.x8h(), 0x07);

	r.x32() = 0xdeadbeef;
	ASSERT_EQ(r.x64(), 0x00000000deadbeef);
	ASSERT_EQ(r.x32(), 0xdeadbeef);
	ASSERT_EQ(r.x16(), 0xbeef);
	ASSERT_EQ(r.x8(), 0xef);
	ASSERT_EQ(r.x8h(), 0xbe);

	r.x64() |= 0x1234fedc00000000;
	ASSERT_EQ(r.x64(), 0x1234fedcdeadbeef);
	ASSERT_EQ(r.x32(), 0xdeadbeef);
	ASSERT_EQ(r.x16(), 0xbeef);
	ASSERT_EQ(r.x8(), 0xef);
	ASSERT_EQ(r.x8h(), 0xbe);

	r.x16() = 0xabcd;
	ASSERT_EQ(r.x64(), 0x1234fedcdeadabcd);
	ASSERT_EQ(r.x32(), 0xdeadabcd);
	ASSERT_EQ(r.x16(), 0xabcd);
	ASSERT_EQ(r.x8(), 0xcd);
	ASSERT_EQ(r.x8h(), 0xab);

	r.x8() = 0xff;
	ASSERT_EQ(r.x64(), 0x1234fedcdeadabff);
	ASSERT_EQ(r.x32(), 0xdeadabff);
	ASSERT_EQ(r.x16() ,0xabff);
	ASSERT_EQ(r.x8(), 0xff);
	ASSERT_EQ(r.x8h(), 0xab);

	r.x8h() = 0x69;
	ASSERT_EQ(r.x64(), 0x1234fedcdead69ff);
	ASSERT_EQ(r.x32(), 0xdead69ff);
	ASSERT_EQ(r.x16(), 0x69ff);
	ASSERT_EQ(r.x8(), 0xff);
	ASSERT_EQ(r.x8h(), 0x69);

	r[0] = 0;
	ASSERT_EQ(r.x64(), 0x1234fedcdead6900);
	ASSERT_EQ(r.x32(), 0xdead6900);
	ASSERT_EQ(r.x16(), 0x6900);
	ASSERT_EQ(r.x8(), 0x0);
	ASSERT_EQ(r.x8h(), 0x69);

	r[1] = 0xafda;
	ASSERT_EQ(r.x64(), 0x1234fedcdeadafda);
	ASSERT_EQ(r.x32(), 0xdeadafda);
	ASSERT_EQ(r.x16(), 0xafda);
	ASSERT_EQ(r.x8(), 0xda);
	ASSERT_EQ(r.x8h(), 0xaf);

	r[3] = 0x1234567890abcdef;
	ASSERT_EQ(r.x64(), 0x1234567890abcdef);
	ASSERT_EQ(r.x32(), 0x90abcdef);
	ASSERT_EQ(r.x16(), 0xcdef);
	ASSERT_EQ(r.x8(), 0xef);
	ASSERT_EQ(r.x8h(), 0xcd);

	r[2] = 0x12;
	ASSERT_EQ(r.x64(), 0x0000000000000012);
	ASSERT_EQ(r.x32(), 0x00000012);
	ASSERT_EQ(r.x16(), 0x0012);
	ASSERT_EQ(r.x8(), 0x12);
	ASSERT_EQ(r.x8h(), 0x00);
}

void type_tests()
{
	RUN_TEST(test_transmutes);
	RUN_TEST(test_cpu_registers);
}
