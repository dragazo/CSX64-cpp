#include "asm_test_common.h"
#include "test_common.h"

using namespace CSX64;

void nop_tests()
{
	auto p = ASM_LNK(R"(
segment .text
nop
nop
nop
nop
hlt
nop
hlt
hlt
hlt
nop
nop
nop
nop
nop
mov eax, 0
mov ebx, 413
syscall
nop
nop
nop
)");
	ASSERT(p);
	u64 ticks;

	ASSERT(p->running());
	ticks = p->tick(0);
	ASSERT_EQ(ticks, 0);

	ASSERT(p->running());
	ticks = p->tick(1);
	ASSERT_EQ(ticks, 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 3);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 7);

	ASSERT(!p->running());
	ASSERT_EQ(p->error(), ErrorCode::None);
	ASSERT_EQ(p->return_value(), 413);
	ticks = p->tick(2000);
	ASSERT_EQ(ticks, 0);
}
void load_imm_tests()
{
	auto p = ASM_LNK(R"(
segment .text
mov rax, -7784568640113865156
mov rbx, 0x12de639fcd11a4cb
mov rcx, 0x046579a453add4b8
mov rdx, 0o1764420727724002664106
mov rsi, 0xf1c89e98daa39a38
mov rdi, 0xbdb00d43f2aaff23
mov rbp, -7740818_22331708_3_744
mov rsp, 0xa228b0bd6d86600e
mov r8, 0x076899314a3e420b
mov r9, 417771020883113582
mov r10, 0x781b5ce0538f3fd0
mov r11, 0x2569467b20f81cb8
mov r12, 0xc0a9ed7647a335c4
mov r13, 0o17052_7_0_262065_065_624_265
mov r14, 0x65902d29eac939fb
mov r15, 0xec7aa569a6155ab1
hlt
mov eax, 0x7d22cbb4
mov ebx, 0xbecb162e
mov ecx, 0xae23158e
mov edx, 0x0ddfe51b
mov esi, 0o24_734_613_417
mov edi, 0xa71a36d7
mov ebp, 0xd130b0c0
mov esp, 2209513684
mov r8d, 0xa53b7121
mov r9d, 0x74c9e6d0
mov r10d, 0x58b7c4e7
mov r11d, 0b11001010101111101111111010010001
mov r12d, 0xaa92e8b4
mov r13d, 0x86bbdbc1
mov r14d, 0b_0111_1001_1111_0100_1110_0011_0100_1000
mov r15d, 0xc023567e
hlt
mov ax, 0xcb04
mov bx, 0x43f4
mov cx, 0x6493
mov dx, 0xacd9
mov si, 0xf019
mov di, 32_038
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
void expr_tests()
{
	auto p = ASM_LNK(R"(#!shebang test
segment .text
; test symbol definition and linkage
t1: equ 721
mov rax, t1
mov rbx, t1 + 12
mov rcx, t1 +5
mov rdx, t1+  7
mov rsi, t1+54
mov rdi,  t1 + 12.0
mov r8d,  t1 + 12.0
mov r9,   t1 +5.0
mov r10d, t1 +5.0
mov r11,  t1+  7.0
mov r12d, t1+  7.0
mov r13,  t1+54.0
mov r14d, t1+54.0
hlt
mov rax, 4 * 7
mov rbx, 3 * -5
mov rcx, -2 * 3
mov rdx, -4 * -71
mov rsi, 5 * 9.5
mov rdi, -5 * 9.5
mov r8, 65.125 * 11
mov r9, 65.125 * -11
mov r10, 12.5 * 6.25
hlt
mov rax, 80 / 4
mov rbx, -96 / 7
mov rcx, 86 / -5
mov rdx, -46 / -22
mov rsi, 21 / 22
mov rdi, -21 / 22
mov r8, 540 / 7.0
mov r9, -50 / 5.4
mov r10, 524.1 / 11
mov r11, 532.2 / -3
mov r12, 120.5 / 15.75
hlt
mov rax, 80 % 4
mov rbx, -96 % 7
mov rcx, 86 % -5
mov rdx, -46 % -22
mov rsi, 21 % 22
mov rdi, -21 % 22
mov r8, 15 % 7
hlt
mov rax, 80 +/ 4
mov rbx, -96 +/ 7
mov rcx, 86 +/ -5
mov rdx, -46 +/ -22
mov rsi, 21 +/ 22
mov rdi, -21 +/ 22
mov r8, 15 +/ 7
hlt
mov rax, 80 +% 4
mov rbx, -96 +% 7
mov rcx, 86 +% -5
mov rdx, -46 +% -22
mov rsi, 21 +% 22
mov rdi, -21 +% 22
mov r8, 15 +% 7
hlt
mov rax, 3 + 8
mov rbx, -3 + 8
mov rcx, 3 + -8
mov rdx, -3 + -8
mov rsi, 9.0625 + 3
mov rdi, 9.0625 + -3
mov r8, 5 + 10.5
mov r9, -5 + 10.5
mov r10, 12.125 + 0.5
hlt
mov rax, 7 - 55
mov rbx, -7 - 5
mov rcx, 76 - -5
mov rdx, -7 - -1
mov rsi, 12._5__ - 1.5
mov rdi, 1_2_.25_ - -1.5
mov r8, 5 - 10.5
mov r9, -5 - 10.5
mov r10, 12._1_25 - 0.5
hlt
mov rax, 56 << 0
mov rbx, 57 << 1
mov rcx, 58 << 2
mov rdx, 3 << 3
mov rsi, -332 << 32
mov rdi, -101 << 63
mov r8, -103 << 64 ; overshifting saturates for size agnosticism
mov r9, -105 << 65
mov r10, 17 << -1 ; shift count interpreted as unsigned
mov r11, 0 << 7
hlt
mov rax, 56 >> 0
mov rbx, 57 >> 1
mov rcx, 58 >> 2
mov rdx, 3 >> 3
mov rsi, -332 >> 32
mov rdi, 101 >> 63
mov r8, -103 >> 64 ; overshifting saturates for size agnosticism
mov r9, 105 >> 65
mov r10, 17 >> -1 ; shift count interpreted as unsigned
mov r11, -14 >> -1 ; shift count interpreted as unsigned
mov r12, 0 >> 7
hlt
mov rax, 56 +>> 0
mov rbx, 57 +>> 1
mov rcx, 58 +>> 2
mov rdx, 3 +>> 3
mov rsi, -332 +>> 32
mov rdi, 101 +>> 63
mov r8, -103 +>> 64 ; overshifting saturates for size agnosticism
mov r9, 105 +>> 65
mov r10, 17 +>> -1 ; shift count interpreted as unsigned
mov r11, -14 +>> -1 ; shift count interpreted as unsigned
mov r12, 0 +>> 7
hlt
mov rax, 3 < 2
mov rbx, 3 < 3
mov rcx, 3 < 4
mov rdx, -1 < -1
mov rsi, -1 < 0
mov rdi, -1 < 1
mov r8, 3.0 < 2
mov r9, 3 < 3.0
mov r10, 3.0 < 4.0
mov r11, -1.0 < -1
mov r12, -1 < 0.0
mov r13, -1.0 < 1.0
hlt
mov rax, 3 <= 2
mov rbx, 3 <= 3
mov rcx, 3 <= 4
mov rdx, -1 <= -1
mov rsi, -1 <= 0
mov rdi, -1 <= 1
mov r8, 3.0 <= 2
mov r9, 3 <= 3.0
mov r10, 3.0 <= 4.0
mov r11, -1.0 <= -1
mov r12, -1 <= 0.0
mov r13, -1.0 <= 1.0
hlt
mov rax, 3 > 2
mov rbx, 3 > 3
mov rcx, 3 > 4
mov rdx, -1 > -1
mov rsi, -1 > 0
mov rdi, -1 > 1
mov r8, 3.0 > 2
mov r9, 3 > 3.0
mov r10, 3.0 > 4.0
mov r11, -1.0 > -1
mov r12, -1 > 0.0
mov r13, -1.0 > 1.0
hlt
mov rax, 3 >= 2
mov rbx, 3 >= 3
mov rcx, 3 >= 4
mov rdx, -1 >= -1
mov rsi, -1 >= 0
mov rdi, -1 >= 1
mov r8, 3.0 >= 2
mov r9, 3 >= 3.0
mov r10, 3.0 >= 4.0
mov r11, -1.0 >= -1
mov r12, -1 >= 0.0
mov r13, -1.0 >= 1.0
hlt
mov rax, 3 +< 2
mov rbx, 3 +< 3
mov rcx, 3 +< 4
mov rdx, -1 +< -1
mov rsi, -1 +< 0
mov rdi, -1 +< 1
hlt
mov rax, 3 +<= 2
mov rbx, 3 +<= 3
mov rcx, 3 +<= 4
mov rdx, -1 +<= -1
mov rsi, -1 +<= 0
mov rdi, -1 +<= 1
hlt
mov rax, 3 +> 2
mov rbx, 3 +> 3
mov rcx, 3 +> 4
mov rdx, -1 +> -1
mov rsi, -1 +> 0
mov rdi, -1 +> 1
hlt
mov rax, 3 +>= 2
mov rbx, 3 +>= 3
mov rcx, 3 +>= 4
mov rdx, -1 +>= -1
mov rsi, -1 +>= 0
mov rdi, -1 +>= 1
hlt
mov rax, 3 == 2
mov rbx, 3 == 3
mov rcx, 3 == 4
mov rdx, -1 == -1
mov rsi, -1 == 0
mov rdi, -1 == 1
mov r8, 3.0 == 2
mov r9, 3 == 3.0
mov r10, 3.0 == 4.0
mov r11, -1.0 == -1
mov r12, -1 == 0.0
mov r13, -1.0 == 1.0
hlt
mov rax, 3 != 2
mov rbx, 3 != 3
mov rcx, 3 != 4
mov rdx, -1 != -1
mov rsi, -1 != 0
mov rdi, -1 != 1
mov r8, 3.0 != 2
mov r9, 3 != 3.0
mov r10, 3.0 != 4.0
mov r11, -1.0 != -1
mov r12, -1 != 0.0
mov r13, -1.0 != 1.0
hlt
mov rax, 0x12De639fCd11a4cb | 0xf1c89e98dAa39A38
mov rbx, 0xf1c89e98daa39A38 | 0x12de639fcd11a4Cb
mov rcx, 0x12De639fcd11a4cb | 0
mov rdx, 0xf1c89e98daa39a38 | -1
hlt
mov rax, 0x12De639fCd11a4cb & 0xf1c89e98dAa39A38
mov rbx, 0xf1c89e98daa39A38 & 0x12de639fcd11a4Cb
mov rcx, 0x12De639fcd11a4cb & 0
mov rdx, 0xf1c89e98daa39a38 & -1
hlt
mov rax, 0x12De639fCd11a4cb ^ 0xf1c89e98dAa39A38
mov rbx, 0xf1c89e98daa39A38 ^ 0x12de639fcd11a4Cb
mov rcx, 0x12De639fcd11a4cb ^ 0
mov rdx, 0xf1c89e98daa39a38 ^ -1
hlt
mov rax, 0 && 0
mov rbx, 0 && 2
mov rcx, 1 && 0
mov rdx, 2 && -1
hlt
mov rax, 0 || 0
mov rbx, 0 || 2
mov rcx, 1 || 0
mov rdx, 2 || -1
hlt
mov rax, -453
mov rbx, --453
mov rcx, -17.4
mov rdx, --17.4
mov rsi, -0
mov rdi, --0
hlt
mov rax, ~453
mov rbx, ~~453
mov rcx, ~-243
mov rdx, ~~-243
mov rsi, ~0
mov rdi, ~~0
hlt
mov rax, !453
mov rbx, !!453
mov rcx, !-243
mov rdx, !!-243
mov rsi, !0
mov rdi, !!0
hlt
mov rax, $int(45)
mov rbx, $int(-45)
mov rcx, $int(45.3)
mov rdx, $int(45.8)
mov rsi, $int(-45.3)
mov rdi, $int(-45.8)
hlt
mov rax, $float(45.3)
mov rbx, $float(-45.8)
mov rcx, $float(45)
mov rdx, $float(-45)
mov rsi, $float(0)
hlt
mov rax, $floor(35)
mov rbx, $floor(-3322)
mov rcx, $floor(7.32)
mov rdx, $floor(-7.32)
mov rsi, $floor(9.99)
mov rdi, $floor(-9.99)
mov r8, $floor(5.5)
mov r9, $floor(-5.5)
hlt
mov rax, $ceil(35)
mov rbx, $ceil(-3322)
mov rcx, $ceil(7.32)
mov rdx, $ceil(-7.32)
mov rsi, $ceil(9.99)
mov rdi, $ceil(-9.99)
mov r8, $ceil(5.5)
mov r9, $ceil(-5.5)
hlt
mov rax, $round(35)
mov rbx, $round(-3322)
mov rcx, $round(7.32)
mov rdx, $round(-7.32)
mov rsi, $round(9.99)
mov rdi, $round(-9.99)
mov r8, $round(5.5)
mov r9, $round(-5.5)
hlt
mov rax, $trunc(35)
mov rbx, $trunc(-3322)
mov rcx, $trunc(7.32)
mov rdx, $trunc(-7.32)
mov rsi, $trunc(9.99)
mov rdi, $trunc(-9.99)
mov r8, $trunc(5.5)
mov r9, $trunc(-5.5)
hlt
mov rax, $repr64(3.14)
mov rbx, $round($repr64(3.14))
mov rcx, $repr32(3.14)
mov rdx, $round($repr32(3.14))
hlt
mov rax, $float64(0x4005be76c8b43958)
mov ebx, $float64(0x4005be76c8b43958)
mov rcx, $float32(0x401de3b6)
mov edx, $float32(0x401de3b6)
hlt
mov rax, $prec64(9.02101)
mov ebx, $prec64(9.02101)
mov rcx, $prec32(9.01501)
mov edx, $prec32(9.01501)
mov rsi, $prec64(8.71321)
mov edi, $prec64(8.71321)
mov r8, $prec32(8.71321)
mov r9d, $prec32(8.71321)
mov r10, $prec32(8.2499999)
mov r11d, $prec32(8.2499999)
mov r12, $prec32($float64(0x501ffffff94a0359)) ; this test shows how u64->f64->f32->f64->u64 is insufficient
mov r13d, $prec32($float64(0x501ffffff94a0359))
hlt
mov rax, 0 ? 23 : 54
mov rbx, 1 ? 23 : 54
mov rcx, 0 ? 23.534 : 53
mov rdx, -22 ? 23.534 : 53
mov rsi, 0 ? 2 : 54.666
mov rdi, -1 ? 2 : 54.666
mov r8, 0 ? 23.21 : 54.775
mov r9, 834 ? 23.21 : 54.775
hlt
mov rax, 'a'
mov rbx, '2' + 4
mov rcx, 2 + 'A'
mov rdx, 'AuFDenXy'
mov esi, 'WxYz'
mov di, 'mN'
mov r8, 'ABCdef'
mov r9d, 'AvX'
hlt
mov eax, 0
mov ebx, -432
syscall
times 22 nop
)");
	ASSERT(p);
	u64 ticks;

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 13);
	ASSERT_EQ(p->rax(), 721);
	ASSERT_EQ(p->rbx(), 733);
	ASSERT_EQ(p->rcx(), 726);
	ASSERT_EQ(p->rdx(), 728);
	ASSERT_EQ(p->rsi(), 775);
	ASSERT_EQ(p->rdi(), 0x4086e80000000000u);
	ASSERT_EQ(p->r8(), 0x0000000044374000u);
	ASSERT_EQ(p->r9(), 0x4086b00000000000u);
	ASSERT_EQ(p->r10(), 0x0000000044358000u);
	ASSERT_EQ(p->r11(), 0x4086c00000000000u);
	ASSERT_EQ(p->r12(), 0x0000000044360000u);
	ASSERT_EQ(p->r13(), 0x4088380000000000u);
	ASSERT_EQ(p->r14(), 0x000000004441c000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 9);
	ASSERT_EQ(p->rax(), 28);
	ASSERT_EQ(p->rbx(), -15);
	ASSERT_EQ(p->rcx(), -6);
	ASSERT_EQ(p->rdx(), 284);
	ASSERT_EQ(p->rsi(), 0x4047c00000000000u);
	ASSERT_EQ(p->rdi(), 0xc047c00000000000u);
	ASSERT_EQ(p->r8(), 0x4086630000000000u);
	ASSERT_EQ(p->r9(), 0xc086630000000000u);
	ASSERT_EQ(p->r10(), 0x4053880000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 11);
	ASSERT_EQ(p->rax(), 20);
	ASSERT_EQ(p->rbx(), -13);
	ASSERT_EQ(p->rcx(), -17);
	ASSERT_EQ(p->rdx(), 2);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);
	ASSERT_EQ(p->r8(), 0x4053492492492492u);
	ASSERT_EQ(p->r9(), 0xc02284bda12f684cu);
	ASSERT_EQ(p->r10(), 0x4047d29e4129e413u);
	ASSERT_EQ(p->r11(), 0xc0662ccccccccccdu);
	ASSERT_EQ(p->r12(), 0x401e9a69a69a69a7u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 7);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), -5);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), -2);
	ASSERT_EQ(p->rsi(), 21);
	ASSERT_EQ(p->rdi(), -21);
	ASSERT_EQ(p->r8(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 7);
	ASSERT_EQ(p->rax(), 20);
	ASSERT_EQ(p->rbx(), 2635249153387078788);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 838488366986797799);
	ASSERT_EQ(p->r8(), 2);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 7);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 4);
	ASSERT_EQ(p->rcx(), 86);
	ASSERT_EQ(p->rdx(), -46);
	ASSERT_EQ(p->rsi(), 21);
	ASSERT_EQ(p->rdi(), 17);
	ASSERT_EQ(p->r8(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 9);
	ASSERT_EQ(p->rax(), 11);
	ASSERT_EQ(p->rbx(), 5);
	ASSERT_EQ(p->rcx(), -5);
	ASSERT_EQ(p->rdx(), -11);
	ASSERT_EQ(p->rsi(), 0x4028200000000000u);
	ASSERT_EQ(p->rdi(), 0x4018400000000000u);
	ASSERT_EQ(p->r8(), 0x402f000000000000u);
	ASSERT_EQ(p->r9(), 0x4016000000000000u);
	ASSERT_EQ(p->r10(), 0x4029400000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 9);
	ASSERT_EQ(p->rax(), -48);
	ASSERT_EQ(p->rbx(), -12);
	ASSERT_EQ(p->rcx(), 81);
	ASSERT_EQ(p->rdx(), -6);
	ASSERT_EQ(p->rsi(), 0x4026000000000000u);
	ASSERT_EQ(p->rdi(), 0x402b800000000000u);
	ASSERT_EQ(p->r8(), 0xc016000000000000u);
	ASSERT_EQ(p->r9(), 0xc02f000000000000u);
	ASSERT_EQ(p->r10(), 0x4027400000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 10);
	ASSERT_EQ(p->rax(), 56);
	ASSERT_EQ(p->rbx(), 114);
	ASSERT_EQ(p->rcx(), 232);
	ASSERT_EQ(p->rdx(), 24);
	ASSERT_EQ(p->rsi(), -1425929142272);
	ASSERT_EQ(p->rdi(), 0x8000000000000000u);
	ASSERT_EQ(p->r8(), 0);
	ASSERT_EQ(p->r9(), 0);
	ASSERT_EQ(p->r10(), 0);
	ASSERT_EQ(p->r11(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 11);
	ASSERT_EQ(p->rax(), 56);
	ASSERT_EQ(p->rbx(), 28);
	ASSERT_EQ(p->rcx(), 14);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), -1);
	ASSERT_EQ(p->rdi(), 0);
	ASSERT_EQ(p->r8(), -1);
	ASSERT_EQ(p->r9(), 0);
	ASSERT_EQ(p->r10(), 0);
	ASSERT_EQ(p->r11(), -1);
	ASSERT_EQ(p->r12(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 11);
	ASSERT_EQ(p->rax(), 56);
	ASSERT_EQ(p->rbx(), 28);
	ASSERT_EQ(p->rcx(), 14);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 0xffffffffu);
	ASSERT_EQ(p->rdi(), 0);
	ASSERT_EQ(p->r8(), 0);
	ASSERT_EQ(p->r9(), 0);
	ASSERT_EQ(p->r10(), 0);
	ASSERT_EQ(p->r11(), 0);
	ASSERT_EQ(p->r12(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 0);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 1);
	ASSERT_EQ(p->rdi(), 1);
	ASSERT_EQ(p->r8(), 0);
	ASSERT_EQ(p->r9(), 0);
	ASSERT_EQ(p->r10(), 1);
	ASSERT_EQ(p->r11(), 0);
	ASSERT_EQ(p->r12(), 1);
	ASSERT_EQ(p->r13(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), 1);
	ASSERT_EQ(p->rsi(), 1);
	ASSERT_EQ(p->rdi(), 1);
	ASSERT_EQ(p->r8(), 0);
	ASSERT_EQ(p->r9(), 1);
	ASSERT_EQ(p->r10(), 1);
	ASSERT_EQ(p->r11(), 1);
	ASSERT_EQ(p->r12(), 1);
	ASSERT_EQ(p->r13(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 1);
	ASSERT_EQ(p->rbx(), 0);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);
	ASSERT_EQ(p->r8(), 1);
	ASSERT_EQ(p->r9(), 0);
	ASSERT_EQ(p->r10(), 0);
	ASSERT_EQ(p->r11(), 0);
	ASSERT_EQ(p->r12(), 0);
	ASSERT_EQ(p->r13(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 1);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 1);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);
	ASSERT_EQ(p->r8(), 1);
	ASSERT_EQ(p->r9(), 1);
	ASSERT_EQ(p->r10(), 0);
	ASSERT_EQ(p->r11(), 1);
	ASSERT_EQ(p->r12(), 0);
	ASSERT_EQ(p->r13(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 0);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), 1);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 1);
	ASSERT_EQ(p->rbx(), 0);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 1);
	ASSERT_EQ(p->rdi(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 1);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 1);
	ASSERT_EQ(p->rsi(), 1);
	ASSERT_EQ(p->rdi(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 1);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);
	ASSERT_EQ(p->r8(), 0);
	ASSERT_EQ(p->r9(), 1);
	ASSERT_EQ(p->r10(), 0);
	ASSERT_EQ(p->r11(), 1);
	ASSERT_EQ(p->r12(), 0);
	ASSERT_EQ(p->r13(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 1);
	ASSERT_EQ(p->rbx(), 0);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), 0);
	ASSERT_EQ(p->rsi(), 1);
	ASSERT_EQ(p->rdi(), 1);
	ASSERT_EQ(p->r8(), 1);
	ASSERT_EQ(p->r9(), 0);
	ASSERT_EQ(p->r10(), 1);
	ASSERT_EQ(p->r11(), 0);
	ASSERT_EQ(p->r12(), 1);
	ASSERT_EQ(p->r13(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0xF3DEFF9FDFB3BEFBu);
	ASSERT_EQ(p->rbx(), 0xF3DEFF9FDFB3BEFBu);
	ASSERT_EQ(p->rcx(), 0x12De639fcd11a4cbu);
	ASSERT_EQ(p->rdx(), 0xffffffffffffffffu);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0x10C80298C8018008u);
	ASSERT_EQ(p->rbx(), 0x10C80298C8018008u);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 0xf1c89e98daa39a38);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0xE316FD0717B23EF3u);
	ASSERT_EQ(p->rbx(), 0xE316FD0717B23EF3u);
	ASSERT_EQ(p->rcx(), 0x12De639fcd11a4cbu);
	ASSERT_EQ(p->rdx(), 0x0E376167255C65C7u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 0);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 1);
	ASSERT_EQ(p->rdx(), 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), -453);
	ASSERT_EQ(p->rbx(), 453);
	ASSERT_EQ(p->rcx(), 0xc031666666666666u);
	ASSERT_EQ(p->rdx(), 0x4031666666666666u);
	ASSERT_EQ(p->rsi(), 0);
	ASSERT_EQ(p->rdi(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 0xFFFFFFFFFFFFFE3Au);
	ASSERT_EQ(p->rbx(), 453);
	ASSERT_EQ(p->rcx(), 242);
	ASSERT_EQ(p->rdx(), -243);
	ASSERT_EQ(p->rsi(), 0xffffffffffffffffu);
	ASSERT_EQ(p->rdi(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 0);
	ASSERT_EQ(p->rbx(), 1);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->rdx(), 1);
	ASSERT_EQ(p->rsi(), 1);
	ASSERT_EQ(p->rdi(), 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 6);
	ASSERT_EQ(p->rax(), 45);
	ASSERT_EQ(p->rbx(), -45);
	ASSERT_EQ(p->rcx(), 45);
	ASSERT_EQ(p->rdx(), 45);
	ASSERT_EQ(p->rsi(), -45);
	ASSERT_EQ(p->rdi(), -45);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 5);
	ASSERT_EQ(p->rax(), 0x4046a66666666666u);
	ASSERT_EQ(p->rbx(), 0xc046e66666666666u);
	ASSERT_EQ(p->rcx(), 0x4046800000000000u);
	ASSERT_EQ(p->rdx(), 0xc046800000000000u);
	ASSERT_EQ(p->rsi(), 0x0000000000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rax(), 35);
	ASSERT_EQ(p->rbx(), -3322);
	ASSERT_EQ(p->rcx(), 0x401c000000000000u);
	ASSERT_EQ(p->rdx(), 0xc020000000000000u);
	ASSERT_EQ(p->rsi(), 0x4022000000000000u);
	ASSERT_EQ(p->rdi(), 0xc024000000000000u);
	ASSERT_EQ(p->r8(), 0x4014000000000000u);
	ASSERT_EQ(p->r9(), 0xc018000000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rax(), 35);
	ASSERT_EQ(p->rbx(), -3322);
	ASSERT_EQ(p->rcx(), 0x4020000000000000u);
	ASSERT_EQ(p->rdx(), 0xc01c000000000000u);
	ASSERT_EQ(p->rsi(), 0x4024000000000000u);
	ASSERT_EQ(p->rdi(), 0xc022000000000000u);
	ASSERT_EQ(p->r8(), 0x4018000000000000u);
	ASSERT_EQ(p->r9(), 0xc014000000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rax(), 35);
	ASSERT_EQ(p->rbx(), -3322);
	ASSERT_EQ(p->rcx(), 0x401c000000000000u);
	ASSERT_EQ(p->rdx(), 0xc01c000000000000u);
	ASSERT_EQ(p->rsi(), 0x4024000000000000u);
	ASSERT_EQ(p->rdi(), 0xc024000000000000u);
	ASSERT_EQ(p->r8(), 0x4018000000000000u);
	ASSERT_EQ(p->r9(), 0xc018000000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rax(), 35);
	ASSERT_EQ(p->rbx(), -3322);
	ASSERT_EQ(p->rcx(), 0x401c000000000000u);
	ASSERT_EQ(p->rdx(), 0xc01c000000000000u);
	ASSERT_EQ(p->rsi(), 0x4022000000000000u);
	ASSERT_EQ(p->rdi(), 0xc022000000000000u);
	ASSERT_EQ(p->r8(), 0x4014000000000000u);
	ASSERT_EQ(p->r9(), 0xc014000000000000u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0x40091eb851eb851fu);
	ASSERT_EQ(p->rbx(), 0x40091eb851eb851fu);
	ASSERT_EQ(p->rcx(), 0x000000004048f5c3u);
	ASSERT_EQ(p->rdx(), 0x000000004048f5c3u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax(), 0x4005be76c8b43958u);
	ASSERT_EQ(p->rbx(), 0x00000000402df3b6u);
	ASSERT_EQ(p->rcx(), 0x4003bc76c0000000u);
	ASSERT_EQ(p->rdx(), 0x401de3b6);
	
	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 12);
	ASSERT_EQ(p->rax(), 0x40220ac1d29dc726u);
	ASSERT_EQ(p->rbx(), 0x000000004110560fu);
	ASSERT_EQ(p->rcx(), 0x402207af60000000u);
	ASSERT_EQ(p->rdx(), 0x0000000041103d7bu);
	ASSERT_EQ(p->rsi(), 0x40216d29dc725c3eu);
	ASSERT_EQ(p->rdi(), 0x00000000410b694fu);
	ASSERT_EQ(p->r8(), 0x40216d29e0000000u);
	ASSERT_EQ(p->r9(), 0x00000000410b694fu);
	ASSERT_EQ(p->r10(), 0x4020800000000000u);
	ASSERT_EQ(p->r11(), 0x0000000041040000u);
	ASSERT_EQ(p->r12(), 0x5020000000000000u);
	ASSERT_EQ(p->r13(), 0x000000007f800000u);
	
	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rax(), 54);
	ASSERT_EQ(p->rbx(), 23);
	ASSERT_EQ(p->rcx(), 53);
	ASSERT_EQ(p->rdx(), 0x403788b439581062u);
	ASSERT_EQ(p->rsi(), 0x404b553f7ced9168u);
	ASSERT_EQ(p->rdi(), 2);
	ASSERT_EQ(p->r8(), 0x404b633333333333u);
	ASSERT_EQ(p->r9(), 0x403735c28f5c28f6u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rax(), (u8)'a');
	ASSERT_EQ(p->rbx(), (u8)'6');
	ASSERT_EQ(p->rcx(), (u8)'C');
	ASSERT_EQ(p->rdx(), 0x79586e6544467541u);
	ASSERT_EQ(p->rsi(), 0x000000007a597857u);
	ASSERT_EQ(p->di(), 0x4e6du);
	ASSERT_EQ(p->r8(), 0x0000666564434241u);
	ASSERT_EQ(p->r9d(), 0x00587641u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 2);
	ASSERT(!p->running());
	ASSERT_EQ(p->error(), ErrorCode::None);
	ASSERT_EQ(p->return_value(), -432);

	ASM_LNK("t1: equ 0 / 1");
	ASSERT_THROWS(ASM_LNK("t1: equ 0 / 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 / 0"), AssembleException);

	ASM_LNK("t1: equ 0 % 1");
	ASM_LNK("t1: equ 3 % 5");
	ASSERT_THROWS(ASM_LNK("t1: equ 0 % 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 % 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3.0 % 5"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3 % 5.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3.0 % 5.0"), AssembleException);

	ASM_LNK("t1: equ 0 +/ 1");
	ASM_LNK("t1: equ 3 +/ 5");
	ASSERT_THROWS(ASM_LNK("t1: equ 0 +/ 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 +/ 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3.0 +/ 5"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3 +/ 5.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3.0 +/ 5.0"), AssembleException);

	ASM_LNK("t1: equ 0 +% 1");
	ASM_LNK("t1: equ 3 +% 5");
	ASSERT_THROWS(ASM_LNK("t1: equ 0 +% 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 +% 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3.0 +% 5"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3 +% 5.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 3.0 +% 5.0"), AssembleException);

	// equ directive specifically is size-agnostic (nothing to do with expr)
	ASM_LNK("t1: equ 0");
	ASSERT_THROWS(ASM_LNK("t1: equ qword 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ dword 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ word 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ byte 0"), AssembleException);

	ASM_LNK("t1: equ 0.0");
	ASSERT_THROWS(ASM_LNK("t1: equ qword 0.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ dword 0.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ word 0.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ byte 0.0"), AssembleException);

	ASM_LNK("t1: equ 0");
	ASM_LNK("t1: equ 0x0");
	ASM_LNK("t1: equ 0o0");
	ASM_LNK("t1: equ 0b0");
	ASSERT_THROWS(ASM_LNK("t1: equ 00"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ -00"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 01"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ -01"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0x"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0o"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0b"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0_0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 00_"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0_0_"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0x_"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0o_"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0b_"), AssembleException);

	ASM_LNK("t1: equ 0");
	ASSERT_THROWS(ASM_LNK("t1: equ 0a"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0xx"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0ox"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 0bx"), AssembleException);

	ASM_LNK("t1: equ 0.0");
	ASSERT_THROWS(ASM_LNK("t1: equ 0.0a"), AssembleException);

	ASM_LNK("t1: equ 2 << 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 2 << 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 2.0 << 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 2.0 << 3.0"), AssembleException);

	ASM_LNK("t1: equ 2 >> 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 2 >> 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 2.0 >> 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 2.0 >> 3.0"), AssembleException);

	ASM_LNK("t1: equ 2 +>> 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 2 +>> 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 2.0 +>> 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 2.0 +>> 3.0"), AssembleException);

	ASM_LNK("t1: equ 1 +< 2");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +< 2"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 +< 2.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +< 2.0"), AssembleException);

	ASM_LNK("t1: equ 1 +<= 2");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +<= 2"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 +<= 2.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +<= 2.0"), AssembleException);

	ASM_LNK("t1: equ 1 +> 2");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +> 2"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 +> 2.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +> 2.0"), AssembleException);

	ASM_LNK("t1: equ 1 +>= 2");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +>= 2"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 +>= 2.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 +>= 2.0"), AssembleException);

	ASM_LNK("t1: equ 1 & 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 & 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 & 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 & 3.0"), AssembleException);

	ASM_LNK("t1: equ 1 | 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 | 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 | 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 | 3.0"), AssembleException);

	ASM_LNK("t1: equ 1 ^ 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 ^ 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 ^ 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 ^ 3.0"), AssembleException);

	ASM_LNK("t1: equ 1 && 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 && 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 && 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 && 3.0"), AssembleException);

	ASM_LNK("t1: equ 1 || 3");
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 || 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1 || 3.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 || 3.0"), AssembleException);

	ASM_LNK("t1: equ ~23");
	ASSERT_THROWS(ASM_LNK("t1: equ ~23.0"), AssembleException);

	ASM_LNK("t1: equ !0");
	ASSERT_THROWS(ASM_LNK("t1: equ !0.0"), AssembleException);

	ASM_LNK("t1: equ $repr64(0.0)");
	ASSERT_THROWS(ASM_LNK("t1: equ $repr64(0)"), AssembleException);

	ASM_LNK("t1: equ $repr32(0.0)");
	ASSERT_THROWS(ASM_LNK("t1: equ $repr32(0)"), AssembleException);
	
	ASM_LNK("t1: equ $float64(0)");
	ASSERT_THROWS(ASM_LNK("t1: equ $float64(0.0)"), AssembleException);

	ASM_LNK("t1: equ $float32(0)");
	ASSERT_THROWS(ASM_LNK("t1: equ $float32(0.0)"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ $float32(-1)"), AssembleException);

	ASM_LNK("t1: equ $prec64(1.0)");
	ASSERT_THROWS(ASM_LNK("t1: equ $prec64(1)"), AssembleException);

	ASM_LNK("t1: equ $prec32(1.0)");
	ASSERT_THROWS(ASM_LNK("t1: equ $prec32(1)"), AssembleException);

	ASM_LNK("t1: equ 0 ? 1 : 0");
	ASM_LNK("t1: equ 1 ? 1 : 0");
	ASSERT_THROWS(ASM_LNK("t1: equ 0.0 ? 1 : 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t1: equ 1.0 ? 1 : 0"), AssembleException);
}
void symbol_linkage_tests()
{
	ASM_LNK("t1: equ t2\nt2: equ 0");
	ASSERT_THROWS(ASM_LNK("t1: equ t2\nt2: equ t1"), AssembleException);

	ASSERT_THROWS(ASM_LNK("t1: equ t2"), AssembleException);

	ASSERT_THROWS(ASM_LNK("t2: equ t1"), AssembleException);
	ASSERT_THROWS(ASM_LNK("extern t1\nt2: equ t1"), LinkException);
	ASSERT_THROWS(ASM_LNK("global t1"), AssembleException);
	ASM_LNK("t1: equ 53");
	ASM_LNK("global t1\nt1: equ 53");
	ASSERT_THROWS(ASM_LNK("t2: equ t1", "t1: equ 53"), AssembleException);
	ASSERT_THROWS(ASM_LNK("t2: equ t1", "global t1\nt1: equ 53"), AssembleException);
	ASSERT_THROWS(ASM_LNK("extern t1\nt2: equ t1", "t1: equ 53"), LinkException);
	ASM_LNK("extern t1\nt2: equ t1", "global t1\nt1: equ 53");
}
void times_tests()
{
	auto p = ASM_LNK(R"(
segment .text

times 27 nop
hlt
times 1 nop
hlt
times 0 nop
hlt
times -1 nop ; negative count is same as 0
hlt
times -1433 nop
hlt

mov rax, uppercase
mov rbx, lowercase
mov rcx, digits
mov rdx, end
hlt

mov rax, 0
mov rbx, 0
syscall

segment .rodata

uppercase:
times 26 db 'A' + $i

lowercase:
times 26 db 'a' + $I ; all $whatever utilities should be case insensitive

digits: times 10 db '0' + $i ; should be able to be on same line as label

end: db 0 ; for string read test
)");
	ASSERT(p);
	u64 ticks;

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 27);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 1);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 0);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rax() + 26, p->rbx());
	ASSERT_EQ(p->rbx() + 26, p->rcx());
	ASSERT_EQ(p->rcx() + 10, p->rdx());
	for (std::size_t i = 0; i < 26; ++i)
	{
		u8 t;
		ASSERT(p->read_mem<u8>(p->rax() + i, t));
		ASSERT_EQ(t, (u8)('A' + i));
	}
	for (std::size_t i = 0; i < 26; ++i)
	{
		u8 t;
		ASSERT(p->read_mem<u8>(p->rbx() + i, t));
		ASSERT_EQ(t, (u8)('a' + i));
	}
	for (std::size_t i = 0; i < 10; ++i)
	{
		u8 t;
		ASSERT(p->read_mem<u8>(p->rcx() + i, t));
		ASSERT_EQ(t, (u8)('0' + i));
	}
	{
		u8 t;
		ASSERT(p->read_mem<u8>(p->rdx(), t));
		ASSERT_EQ(t, 0);
	}
	{
		std::string str = "pre-existing data";
		ASSERT(p->read_str(p->rax(), str));
		ASSERT_EQ(str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
	}

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 2);
	ASSERT(!p->running());
	ASSERT_EQ(p->error(), ErrorCode::None);
	ASSERT_EQ(p->return_value(), 0);

	ASM_LNK("segment .text\ntimes 2 nop");
	ASSERT_THROWS(ASM_LNK("segment .text\ntimes 2.0 nop"), AssembleException);
}
void addr_tests()
{
	auto p = ASM_LNK(R"(
segment .text
mov rax, 412
mov rbx, 323
mov r14, -30
mov r15, -55
mov ecx, 0
hlt
lea rcx, [rax + rbx]
lea rdx, [1*rbx + 1*rax]
lea rsi, zmmword ptr [rax + 2*rbx]
lea rdi, [rax + 4*rbx]
lea r8, [rax + 8*rbx]
lea r9, word ptr [rax + rbx + 100]
lea r10, [rax + 2*rbx + 120]
lea r11, qword ptr [2*rax + rbx - 130]
lea r12, [rax + 4*rbx + 0]
lea r13, [8*rax + 1*rbx - 143]
hlt
lea rcx, [2*rax - rax + rbx]
lea rdx, [rax*3 + 20]
lea rsi, zmmword ptr [5*rbx + 20]
lea rdi, [rax*9 - 10]
hlt
lea rcx, [r14 + r15]
lea edx, ymmword ptr [r14 + r15]
lea si, [r14 + r15]
lea rdi, [r14d + r15d + r15d]
lea r8d, [r14d + r15d + r15d]
lea r9w, xmmword ptr [r14d + r15d + r15d]
lea r10, [r14w + 2*r15w*2]
lea r11d, [r14w + 2*r15w*2]
lea r12w, word ptr [r14w + 2*r15w*2]
hlt
lea rcx, [3   *(2 *r15 + r15)]
lea edx, [3*    (2*r15 + r15)   ]
lea si, ymmword ptr [   3*(2*r15 + r15)]
lea rdi, [3*-(2*-r14d - --++-+-+r14d)]
lea r8d, [3*-(2*-r14d - --++-+-+r14d)]
lea r9w, qword ptr [3*-(2*-r14d - --++-+-+r14d)]
lea r10, [3*-(2*-eax - --++-+-+eax) - 1*1*3*1*1*eax*1*1*3*1*1 + r14w + 8*r15w]
lea r11d, byte ptr [3*-(2 *- eax - --++ -+- +eax) - 1*1*3*1*1*eax*1*1*3*1*1 + r14w + 8*r15w]
lea r12w, [3*-(2*-eax- --++-+-+eax) - 1*1*3*1*1*eax*1*1*3*1*1 + r14w + 8*r15w]
hlt
lea rcx, xmmword ptr [(rax + 212) * 2]
lea rdx, [(rax - 222) * 2]
lea rsi, [(22+rax)*4]
lea rdi, dword ptr [(29 -rax) * -4]
lea r8, [  8 * (     rax + 21)]
lea r9, [8 * (rax - 20)]
lea r10, [2 * (7 + rax)]
lea r11, byte ptr [-  2 * (7 - rax)]
hlt
lea rcx, [rax]
lea rdx, [rbx]
lea rsi, [1*rax]
lea rdi, [rbx*1]
hlt
lea rcx, [-423]
lea rdx, [qword -423  ]
lea rsi, [ dword - 423]
lea rdi, [word    -423    ]
hlt
mov eax, 0
mov ebx, 0
syscall
times 24 nop
)");
	ASSERT(p);
	u64 ticks;

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 5);
	ASSERT_EQ(p->rax(), 412);
	ASSERT_EQ(p->rbx(), 323);
	ASSERT_EQ(p->rcx(), 0);
	ASSERT_EQ(p->r14(), 0xffffffffffffffe2u);
	ASSERT_EQ(p->r15(), 0xffffffffffffffc9u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 10);
	ASSERT_EQ(p->rcx(), 412 + 323);
	ASSERT_EQ(p->rdx(), 412 + 323);
	ASSERT_EQ(p->rsi(), 412 + 2 * 323);
	ASSERT_EQ(p->rdi(), 412 + 4 * 323);
	ASSERT_EQ(p->r8(), 412 + 8 * 323);
	ASSERT_EQ(p->r9(), 412 + 323 + 100);
	ASSERT_EQ(p->r10(), 412 + 2 * 323 + 120);
	ASSERT_EQ(p->r11(), 2 * 412 + 323 - 130);
	ASSERT_EQ(p->r12(), 412 + 4 * 323);
	ASSERT_EQ(p->r13(), 8 * 412 + 323 - 143);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rcx(), 412 + 323);
	ASSERT_EQ(p->rdx(), 3 * 412 + 20);
	ASSERT_EQ(p->rsi(), 5 * 323 + 20);
	ASSERT_EQ(p->rdi(), 9 * 412 - 10);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 9);
	ASSERT_EQ(p->rcx(), 0xffffffffffffffabu);
	ASSERT_EQ(p->rdx(), 0x00000000ffffffabu);
	ASSERT_EQ(p->si(), 0xffab);
	ASSERT_EQ(p->rdi(), 0x00000000ffffff74u);
	ASSERT_EQ(p->r8(), 0x00000000ffffff74u);
	ASSERT_EQ(p->r9w(), 0xff74u);
	ASSERT_EQ(p->r10(), 0x000000000000ff06u);
	ASSERT_EQ(p->r11(), 0x000000000000ff06u);
	ASSERT_EQ(p->r12w(), 0xff06u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 9);
	ASSERT_EQ(p->rcx(), 0xfffffffffffffe11u);
	ASSERT_EQ(p->rdx(), 0x00000000fffffe11u);
	ASSERT_EQ(p->si(), 0xfe11u);
	ASSERT_EQ(p->rdi(), 0x00000000fffffef2u);
	ASSERT_EQ(p->r8(), 0x00000000fffffef2u);
	ASSERT_EQ(p->r9w(), 0xfef2u);
	ASSERT_EQ(p->r10(), 0x000000000000fe2au);
	ASSERT_EQ(p->r11(), 0x000000000000fe2au);
	ASSERT_EQ(p->r12w(), 0xfe2au);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 8);
	ASSERT_EQ(p->rcx(), 1248u);
	ASSERT_EQ(p->rdx(), 380u);
	ASSERT_EQ(p->rsi(), 1736u);
	ASSERT_EQ(p->rdi(), 1532u);
	ASSERT_EQ(p->r8(), 3464u);
	ASSERT_EQ(p->r9(), 3136u);
	ASSERT_EQ(p->r10(), 838u);
	ASSERT_EQ(p->r11(), 810u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rcx(), 412);
	ASSERT_EQ(p->rdx(), 323);
	ASSERT_EQ(p->rsi(), 412);
	ASSERT_EQ(p->rdi(), 323);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 4);
	ASSERT_EQ(p->rcx(), 0xfffffffffffffe59u);
	ASSERT_EQ(p->rdx(), 0xfffffffffffffe59u);
	ASSERT_EQ(p->rsi(), 0x00000000fffffe59u);
	ASSERT_EQ(p->rdi(), 0x000000000000fe59u);

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 2);
	ASSERT(!p->running());
	ASSERT_EQ(p->error(), ErrorCode::None);
	ASSERT_EQ(p->return_value(), 0);

	ASM_LNK("segment .text\nlea rax, [rax + rbx]");
	ASM_LNK("segment .text\nlea rax, [eax + ebx]");
	ASM_LNK("segment .text\nlea rax, [ax + bx]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax + rbx + rcx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [eax + ebx + ecx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [ax + bx +rcx]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [2*rax + 1*rbx]");
	ASM_LNK("segment .text\nlea rax, [1*rax + 2*rbx]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [2*rax + 2*rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [2*rax + 4*rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [2*rax + 8*rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [4*rax + 2*rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [8*rax + 2*rbx]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [1*rax]");
	ASM_LNK("segment .text\nlea rax, [2*rax]");
	ASM_LNK("segment .text\nlea rax, [3*rax]");
	ASM_LNK("segment .text\nlea rax, [4*rax]");
	ASM_LNK("segment .text\nlea rax, [5*rax]");
	ASM_LNK("segment .text\nlea rax, [8*rax]");
	ASM_LNK("segment .text\nlea rax, [9*rax]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [6*rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [7*rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [10*rax]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [rax + rbx]");
	ASM_LNK("segment .text\nlea rax, [eax + ebx]");
	ASM_LNK("segment .text\nlea rax, [ax + bx]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax + ebx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax + bx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [rax + bl]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [eax + bx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [ax + bl]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [al + bl]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [ah + bl]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [al + bh]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [ah + bh]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [rax * rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [eax * ebx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea ax, [ax * bx]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [rax * 2]");
	ASM_LNK("segment .text\nlea rax, [2 * rax]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [2.0 * rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax * 2.0]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [qword rax]");
	ASM_LNK("segment .text\nlea rax, [dword eax]");
	ASM_LNK("segment .text\nlea rax, [word ax]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [qword eax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [qword ax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [qword al]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [qword ah]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [dword rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [dword ax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [dword al]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [dword ah]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [word rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [word eax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [word al]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [word ah]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte rax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte ax]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte al]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte ah]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [0]");
	ASM_LNK("segment .text\nlea rax, [qword 0]");
	ASM_LNK("segment .text\nlea rax, [dword 0]");
	ASM_LNK("segment .text\nlea rax, [word 0]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte 0]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, []"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [qword]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [dword]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [word]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [byte]"), AssembleException);

	ASM_LNK("segment .text\nlea rax, [rax + rbx]");
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax * rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax / rbx]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [rax / 2]"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .text\nlea rax, [2 / rbx]"), AssembleException);
}
void static_assert_tests()
{
	ASM_LNK("static_assert 1");
	ASM_LNK("static_assert 2");
	ASM_LNK("static_assert 39485");
	ASM_LNK("static_assert -52353");
	ASM_LNK("static_assert $repr64(12.4)"); // result of $repr64 is int
	ASM_LNK("static_assert $repr32(-1.0)"); // result of $repr32 is int
	ASSERT_THROWS(ASM_LNK("static_assert 0"), AssembleException);

	// these fail just because of typing
	ASSERT_THROWS(ASM_LNK("static_assert 0.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert 1.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert -12.5"), AssembleException);

	// tests for optional second arg
	ASM_LNK("static_assert 21, 'message'");
	ASSERT_THROWS(ASM_LNK("static_assert 21, 12"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert 21, 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert 21, -5"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert 21, 1.2"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert 21, 0.0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("static_assert 21, -243"), AssembleException);
}
void align_tests()
{
	auto p = ASM_LNK(R"(
segment .text
start:
mov r13, after_last_nop
mov r14, rodata_seg_start
mov r15, before_align
mov rax, val
mov rbx, also_val
mov rcx, aft_val
hlt

mov rax, test_1_1
mov rbx, test_1_2
mov rcx, test_1_3
mov rdx, test_1_4
mov rsi, test_1_5
hlt

mov rax, test_2_1
mov rbx, test_2_2
mov rcx, test_2_3
mov rdx, test_2_4
mov rsi, test_2_5
hlt

mov eax, 0
mov ebx, 0x654
syscall
times 4 nop

times -($-start) & 7 nop ; add nop to pad text segment to multiple of 8 bytes
nop_size_test:
count: equ 5
after_count:
static_assert $-nop_size_test == 0
static_assert $-after_count == 0
static_assert after_count-nop_size_test == 0
times count nop
.aft:
static_assert $-.aft == 0
static_assert $-nop_size_test == count
; we're now at align 8 + count
static_assert ($-start) % 8 == count
after_last_nop:

segment .rodata
rodata_seg_start: equ $$
before_align:
align 8
val:
also_val: db 1
aft_val:

test_1_1: align 1
test_1_2: align 2
test_1_3: align 4
test_1_4: align 8
test_1_5:

test_2_1: align 8
test_2_2: align 4
test_2_3: align 2
test_2_4: align 1
test_2_5:
)");
	ASSERT(p);
	u64 ticks, temp;

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 6);
	ASSERT_NEQ(p->r13(), p->r14());
	ASSERT_EQ(p->r14(), p->r15());
	ASSERT_EQ(p->r14(), p->rax());
	ASSERT_EQ(p->rax(), p->rbx());
	ASSERT(p->r15() % 8 == 0); // segments aligned to highest required alignment
	ASSERT(p->rax() % 8 == 0);
	ASSERT(p->rcx() % 8 == 1);
	ASSERT_EQ(p->rbx() + 1, p->rcx());
	temp = p->rcx();

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 5);
	ASSERT_EQ(temp + 0, p->rax());
	ASSERT_EQ(temp + 0, p->rbx());
	ASSERT_EQ(temp + 1, p->rcx());
	ASSERT_EQ(temp + 3, p->rdx());
	ASSERT_EQ(temp + 7, p->rsi());

	ASSERT(p->running());
	ticks = p->tick(200000);
	ASSERT_EQ(ticks, 5);
	ASSERT_EQ(temp + 7, p->rax());
	ASSERT_EQ(temp + 7, p->rbx());
	ASSERT_EQ(temp + 7, p->rcx());
	ASSERT_EQ(temp + 7, p->rdx());
	ASSERT_EQ(temp + 7, p->rsi());

	ASSERT(p->running());
	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 2);
	ASSERT(!p->running());
	ASSERT_EQ(p->error(), ErrorCode::None);
	ASSERT_EQ(p->return_value(), 0x654);

	ticks = p->tick(20000);
	ASSERT_EQ(ticks, 0);

	ASM_LNK("segment .rodata\nalign 1");
	ASM_LNK("segment .rodata\nalign 2");
	ASM_LNK("segment .rodata\nalign 4");
	ASM_LNK("segment .rodata\nalign 8");
	ASM_LNK("segment .rodata\nalign 16");
	ASM_LNK("segment .rodata\nalign 32");
	ASM_LNK("segment .rodata\nalign 64");
	ASSERT_THROWS(ASM_LNK("segment .rodata\nalign -1"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .rodata\nalign 0"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .rodata\nalign 3"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .rodata\nalign 10"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .rodata\nalign 100"), AssembleException);
	ASSERT_THROWS(ASM_LNK("segment .rodata\nalign 12.6"), AssembleException);
}

void asm_tests()
{
	RUN_TEST(nop_tests);
	RUN_TEST(load_imm_tests);
	RUN_TEST(expr_tests);
	RUN_TEST(symbol_linkage_tests);
	RUN_TEST(times_tests);
	RUN_TEST(addr_tests);
	RUN_TEST(static_assert_tests);
	RUN_TEST(align_tests);
}
