global main

segment .text

extern bsearch, qsort
extern memcpy, memmove
extern memset, memchr

extern strlen
extern malloc, calloc, free, realloc

global __read_arr

dump:
    mov rdi, 40125
    mov rsi, rdi
    stos byte ptr [4024]
    lods eax
    ret
    mov rax, [rsp*5]

; compares 32-bit integers passed by address
cmp_int:
    ; perform raw subtraction a-b
    mov eax, [rdi]
    sub eax, [rsi]
    
    ; overflow can be a problem, so fix that here from flags
    movl eax, -1
    movg eax,  1
    
    ret

__read_arr:
    push r8d
    push r9d
    push r10d
    push r11d
    push r12d
    push r13d
    push r14d
    push r15d
    
    mov r8d,  [arr_cpy +  0]
    mov r9d,  [arr_cpy +  4]
    mov r10d, [arr_cpy +  8]
    mov r11d, [arr_cpy + 12]
    mov r12d, [arr_cpy + 16]
    mov r13d, [arr_cpy + 20]
    mov r14d, [arr_cpy + 24]
    mov r15d, [arr_cpy + 28]
    debug_cpu
    
    pop r15d
    pop r14d
    pop r13d
    pop r12d
    pop r11d
    pop r10d
    pop r9d
    pop r8d
    
    ret
    
    align 64
    fa: dq 0.0/0.1
    fb: dq 0/0.1
    
    _sqrt_val: equ 10.0
    _dsqrt: dq _sqrt_val
    _fsqrt: dd _sqrt_val
    
    align 64
    darr: dq 1.3, 1.9, 2.0, 2.1, 3.4, 5.7, 7.5, 255.99
    
main:
    fstenv [fstat]
    fldenv [fstat]
    fstenv [fstat]
    fldenv [fstat]
    fwait
    
    debug_cpu
    mov edi, 32
    call malloc
    mov qword ptr [rax +  0], 0xdeadbeef
    mov qword ptr [rax +  8], 0xbadc0de
    mov qword ptr [rax + 16], 0xabacaba
    mov qword ptr [rax + 24], 0x1337
    mov rdi, rax
    call free
    
    mov edi, 32
    call calloc
    mov r8, rax
    
    mov rax, [r8 +  0]
    mov rbx, [r8 +  8]
    mov rcx, [r8 + 16]
    mov rdx, [r8 + 24]
    debug_cpu
    
    ret
    
    movapd zmm0, [darr]
    cvtpd2ps ymm1, zmm0
    
    cvttpd2dq ymm2, zmm0
    cvttps2dq ymm3, ymm1
    
    debug_full
    ret
    
    
    mov eax, 0x69457
    
    ;cvtpd2dq zmm0, xmmword ptr [fa]
    
    cvtsi2sd xmm0, eax
    cvtsi2ss xmm1, rax
    
    cvtsd2si ebx, xmm0
    cvtss2si rcx, xmm1
    
    cvtss2sd xmm2, xmm1
    cvtsd2si edx, xmm2
    cvtsd2si rsi, xmm2
    
    debug_full
    xor eax, eax
    ret
    
    mov eax, sys_write
    mov ebx, 1
    
    movsd xmm0, [fa]
    movsd xmm1, [fb]
    comisd xmm0, xmm1
    
    jp .unord
    ja .great
    jb .less
    je .eq
    jmp .end
    
    .unord:
    mov ecx, unord_msg
    mov edx, unord_msg.len
    syscall
    jmp .end
    
    .great:
    mov ecx, great_msg
    mov edx, great_msg.len
    syscall
    jmp .end
    
    .less:
    mov ecx, less_msg
    mov edx, less_msg.len
    syscall
    jmp .end
    
    .eq:
    mov ecx, eq_msg
    mov edx, eq_msg.len
    syscall
    jmp .end
    
    .end:
    xor eax, eax
    ret
    
    
    movsd xmm0, [_dsqrt]
    movss xmm1, [_fsqrt]
    
    sqrtsd xmm2, xmm0
    sqrtss xmm3, xmm1
    
    mulsd xmm4, xmm2, xmm2
    mulss xmm5, xmm3, xmm3
    
    sqrtsd xmm6, [_dsqrt]
    sqrtss xmm7, [_fsqrt]
    
    debug_vpu
    
    xor eax, eax
    ret
    
    mov rcx, 0x800000000000000c
    bsf rax, rcx
    bsr rbx, rcx
    debug_cpu
    ;ret
    
    movsd xmm1, [fa]
    movsd xmm2, [fb]
    cmpltsd xmm0, xmm1, xmm2
    debug_vpu
    ret
    
    mov rdi, fail_msg
    call strlen
    debug_cpu
    
    mov rdi, fail_msg
    mov rsi, 't'
    mov rdx, rax
    call memchr
    debug_cpu
    
    mov al, [rax]
    debug_cpu
    
    ret
    
    ;mov ebx, 0x0000000e
    ;mov ecx, 0x80000000
    
    ;mov eax, ebx
    ;sub eax, ecx
    ;debug_cpu
    
    ;sub eax, 0
    ;debug_cpu
    
    ;ret
    
    ;mov rdi, arr
    ;mov rsi, arr
    ;mov rcx, 100
    
    ;debug_cpu
    ;repe cmpsb
    ;cmps
    ;mov rdx, mrmr
    ;debug_cpu
    ;ret
    
    mov rdi, arr
    mov rsi, -1
    mov rdx, arr_len * 4
    call memset
    
    
    
    
    
    
    
    ; copy arr to arr_cpy (testing)
    mov rdi, arr_cpy
    mov rsi, arr
    mov rdx, arr_len * 4
    call memmove
    ;loop .top
    
    call __read_arr
    
    ; sort arr
    mov rdi, arr_cpy
    mov rsi, arr_len
    mov rdx, 4
    mov rcx, cmp_int
    call qsort
    
    call __read_arr
    
    xor rax, rax
    ret
    
    
    
    
    
    
    
    ; search in arr
    mov rdi, find
    mov rsi, arr_cpy
    mov rdx, arr_len
    mov rcx, 4
    mov r8, cmp_int
    call bsearch
    
    mov rbx, arr_cpy ; rax is ptr returned
    mov rcx, rax ; rbx is arr start
    sub rcx, rbx ; rcx is arr index
    shr rcx, 2
    debug_cpu
    
    xor rax, rax
    ret
    
    
    
    
    
    
    ; open the file
    mov eax, sys_open
    mov ebx, path
    mov ecx, O_CREAT | O_TRUNC | O_WRONLY
    syscall
    
    ; make sure it succeeded
    cmp eax, 0
    jl .fail
    
    mov [fd], eax
    
    ; write the text
    mov ebx, eax
    mov eax, sys_write
    mov ecx, text
    mov edx, text_len
    
    syscall
    
    ; return 0
    xor eax, eax
    ret
    
    .fail:
    mov eax, sys_write
    mov ebx, 1
    mov ecx, fail_msg
    mov edx, fail_msg_len
    syscall
    
    ; return 1
    mov eax, 1
    ret

segment .data

align 8
dval: dq 7.543
fval: dd 3.652

align 4
find: dd -11

dq -1, -1, -1 ,-1

;arr: dd 5, 599, 24, 14, 255, 65, 5, 25
arr: dd 1, 222, 3, 14, 5, 46, 0x80000000, -7
arr_len: equ ($-arr) / 4

mrmr: dq -1, -1, -1 ,-1

path: db "file.txt", 0

text: db "hello,", 10, "this is dog!", 10, 0
text_len: equ $-text-1

fail_msg: db "failed to open the file", 10, 0
fail_msg_len: equ $-fail_msg-1

segment .bss

align 8
fstat: resb 108

align 8
val_arr: resq 4

align 4
mxcsr: resd 1

align 4
fd: resd 1

;arr_cpy: resd 20
arr_cpy: equ arr + 16
    
segment .rodata

great_msg: db `greater than\n`
.len: equ $-great_msg

less_msg: db `less than\n`
.len: equ $-less_msg

eq_msg: db `equal to\n`
.len: equ $-eq_msg

unord_msg: db `unordered\n`
.len: equ $-unord_msg
