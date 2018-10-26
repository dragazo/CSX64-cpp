global main

extern rand, srand
extern atoi, atol, atof
extern pow

segment .text

one: equ 0x1111111111111111

string: db "  1.815e+3"

main:    
    mov edi, 143
    call srand
    
    mov r15, 4000000
    .loop:
        call rand
        ;debug_cpu
        dec r15
        jnz .loop
    
    ret
    
    call rand
    debug_cpu
    
    call rand
    debug_cpu
    
    ret
    
    fld qword ptr [thing]
    fld qword ptr [thing + 8]
    debug_cpu
    fmulp st1, st0
    debug_cpu
    fld qword ptr [other]
    fld qword ptr [other + 8]
    debug_cpu
    fmulp st1, st0
    debug_cpu
    faddp st1, st0
    debug_cpu
    fist dword ptr [a]
    fstp dword ptr [b]
    debug_cpu
    
    ret
    
    faddp st, st
    add qword ptr [thing], qword 7
    ;debug_vpu
    add rax, [dword 0]
    movapd xmm0, [thing]
    movapd xmm1, [other]
    
    addpd xmm2, xmm0, xmm1
    
    debug_vpu
    
    ret
    
    align 16
thing:
    dq 3.14159, 2.71828
 other:   dq 1.2, 3.1
    temp: dq 0,0
    pi: dq 3.14159;__pi__
    fld qword ptr [a]
    fild word ptr [b]
    
    fxch st1
    fscale
    
    ffree st1
    
    fstp qword ptr [res]
    
    ret

fpu_empty:
    ffree st0
    ffree st1
    ffree st2
    ffree st3
    ffree st4
    ffree st5
    ffree st6
    ffree st7
    ret
    
segment .data

align 16
vec_a: dd 1, 14, 13, 5
vec_b: dd 5, 3, 5, 6

a: dq 7.6543
b: dw -2

res: dq 0
