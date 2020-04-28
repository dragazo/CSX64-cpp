#include "asm_test_common.h"
#include "test_common.h"

using namespace CSX64;

void mov_tests()
{
	auto p = asm_lnk(R"(
segment .text
mov rax, 0x93f7a810f45e0e3c
mov rbx, 0x12de639fcd11a4cb
mov rcx, 0x046579a453add4b8
mov rdx, 0xfd221d7ea00b6846
mov rsi, 0xf1c89e98daa39a38
mov rdi, 0xbdb00d43f2aaff23
mov rbp, 0x949316d6a85099a0
mov rsp, 0xa228b0bd6d86600e
mov r8, 0x076899314a3e420b
mov r9, 0x05cc3887b130b66e
mov r10, 0x781b5ce0538f3fd0
mov r11, 0x2569467b20f81cb8
mov r12, 0xc0a9ed7647a335c4
mov r13, 0xf1570b21a8d728b5
mov r14, 0x65902d29eac939fb
mov r15, 0xec7aa569a6155ab1
hlt
mov eax, 0x7d22cbb4
mov ebx, 0xbecb162e
mov ecx, 0xae23158e
mov edx, 0x0ddfe51b
mov esi, 0xa773170f
mov edi, 0xa71a36d7
mov ebp, 0xd130b0c0
mov esp, 0x83b280d4
mov r8d, 0xa53b7121
mov r9d, 0x74c9e6d0
mov r10d, 0x58b7c4e7
mov r11d, 0xcabefe91
mov r12d, 0xaa92e8b4
mov r13d, 0x86bbdbc1
mov r14d, 0x79f4e348
mov r15d, 0xc023567e
hlt
mov ax, 0xcb04
mov bx, 0x43f4
mov cx, 0x6493
mov dx, 0xacd9
mov si, 0xf019
mov di, 0x7d26
mov bp, 0x60f1
mov sp, 0x6476
mov r8w, 0x3329
mov r9w, 0x09f4
mov r10w, 0x2cd7
mov r11w, 0x6b08
mov r12w, 0x3644
mov r13w, 0x217f
mov r14w, 0xb5a4
mov r15w, 0x8df6
hlt
mov al, 0x1f
mov bl, 0x5d
mov cl, 0x82
mov dl, 0xfb
mov sil, 0x83
mov dil, 0x78
mov bpl, 0x45
mov spl, 0x08
mov r8b, 0xc6
mov r9b, 0x5a
mov r10b, 0xd2
mov r11b, 0x3e
mov r12b, 0x87
mov r13b, 0x48
mov r14b, 0x94
mov r15b, 0x05
hlt
mov ah, 0x8c
mov bh, 0xae
mov ch, 0xe1
mov dh, 0xaf
hlt
mov eax, 0
mov ebx, 0xfe630756
syscall
times 256 nop
	)");
	ASSERT(p);
	u64 ticks = 0;

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 16);
	ASSERT_EQ(p->rax(), 0x93f7a810f45e0e3c);
	ASSERT_EQ(p->rbx(), 0x12de639fcd11a4cb);
	ASSERT_EQ(p->rcx(), 0x046579a453add4b8);
	ASSERT_EQ(p->rdx(), 0xfd221d7ea00b6846);
	ASSERT_EQ(p->rsi(), 0xf1c89e98daa39a38);
	ASSERT_EQ(p->rdi(), 0xbdb00d43f2aaff23);
	ASSERT_EQ(p->rbp(), 0x949316d6a85099a0);
	ASSERT_EQ(p->rsp(), 0xa228b0bd6d86600e);
	ASSERT_EQ(p->r8(), 0x076899314a3e420b);
	ASSERT_EQ(p->r9(), 0x05cc3887b130b66e);
	ASSERT_EQ(p->r10(), 0x781b5ce0538f3fd0);
	ASSERT_EQ(p->r11(), 0x2569467b20f81cb8);
	ASSERT_EQ(p->r12(), 0xc0a9ed7647a335c4);
	ASSERT_EQ(p->r13(), 0xf1570b21a8d728b5);
	ASSERT_EQ(p->r14(), 0x65902d29eac939fb);
	ASSERT_EQ(p->r15(), 0xec7aa569a6155ab1);

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 16);
	ASSERT_EQ(p->rax(), 0x000000007d22cbb4);
	ASSERT_EQ(p->rbx(), 0x00000000becb162e);
	ASSERT_EQ(p->rcx(), 0x00000000ae23158e);
	ASSERT_EQ(p->rdx(), 0x000000000ddfe51b);
	ASSERT_EQ(p->rsi(), 0x00000000a773170f);
	ASSERT_EQ(p->rdi(), 0x00000000a71a36d7);
	ASSERT_EQ(p->rbp(), 0x00000000d130b0c0);
	ASSERT_EQ(p->rsp(), 0x0000000083b280d4);
	ASSERT_EQ(p->r8(), 0x00000000a53b7121);
	ASSERT_EQ(p->r9(), 0x0000000074c9e6d0);
	ASSERT_EQ(p->r10(), 0x0000000058b7c4e7);
	ASSERT_EQ(p->r11(), 0x00000000cabefe91);
	ASSERT_EQ(p->r12(), 0x00000000aa92e8b4);
	ASSERT_EQ(p->r13(), 0x0000000086bbdbc1);
	ASSERT_EQ(p->r14(), 0x0000000079f4e348);
	ASSERT_EQ(p->r15(), 0x00000000c023567e);

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 16);
	ASSERT_EQ(p->rax(), 0x000000007d22cb04);
	ASSERT_EQ(p->rbx(), 0x00000000becb43f4);
	ASSERT_EQ(p->rcx(), 0x00000000ae236493);
	ASSERT_EQ(p->rdx(), 0x000000000ddfacd9);
	ASSERT_EQ(p->rsi(), 0x00000000a773f019);
	ASSERT_EQ(p->rdi(), 0x00000000a71a7d26);
	ASSERT_EQ(p->rbp(), 0x00000000d13060f1);
	ASSERT_EQ(p->rsp(), 0x0000000083b26476);
	ASSERT_EQ(p->r8(), 0x00000000a53b3329);
	ASSERT_EQ(p->r9(), 0x0000000074c909f4);
	ASSERT_EQ(p->r10(), 0x0000000058b72cd7);
	ASSERT_EQ(p->r11(), 0x00000000cabe6b08);
	ASSERT_EQ(p->r12(), 0x00000000aa923644);
	ASSERT_EQ(p->r13(), 0x0000000086bb217f);
	ASSERT_EQ(p->r14(), 0x0000000079f4b5a4);
	ASSERT_EQ(p->r15(), 0x00000000c0238df6);

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 16);
	ASSERT_EQ(p->rax(), 0x000000007d22cb1f);
	ASSERT_EQ(p->rbx(), 0x00000000becb435d);
	ASSERT_EQ(p->rcx(), 0x00000000ae236482);
	ASSERT_EQ(p->rdx(), 0x000000000ddfacfb);
	ASSERT_EQ(p->rsi(), 0x00000000a773f083);
	ASSERT_EQ(p->rdi(), 0x00000000a71a7d78);
	ASSERT_EQ(p->rbp(), 0x00000000d1306045);
	ASSERT_EQ(p->rsp(), 0x0000000083b26408);
	ASSERT_EQ(p->r8(), 0x00000000a53b33c6);
	ASSERT_EQ(p->r9(), 0x0000000074c9095a);
	ASSERT_EQ(p->r10(), 0x0000000058b72cd2);
	ASSERT_EQ(p->r11(), 0x00000000cabe6b3e);
	ASSERT_EQ(p->r12(), 0x00000000aa923687);
	ASSERT_EQ(p->r13(), 0x0000000086bb2148);
	ASSERT_EQ(p->r14(), 0x0000000079f4b594);
	ASSERT_EQ(p->r15(), 0x00000000c0238d05);

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0x000000007d228c1f);
	ASSERT_EQ(p->rbx(), 0x00000000becbae5d);
	ASSERT_EQ(p->rcx(), 0x00000000ae23e182);
	ASSERT_EQ(p->rdx(), 0x000000000ddfaffb);

	ASSERT(p->running());
	ticks = p->tick(2000000);
	ASSERT_EQ(ticks, 2);
	ASSERT(!p->running());
	ASSERT_EQ(p->error(), ErrorCode::None);
	ASSERT_EQ(p->return_value(), 0xfe630756);
}

void asm_tests()
{
	RUN_TEST(mov_tests);
}
