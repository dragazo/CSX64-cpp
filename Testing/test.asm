global main

stdin:  equ 0
stdout: equ 1

segment .text

main:
    mov rax, sys_write
    mov rbx, stdout
    mov rcx, q
    mov rdx, q_len
    syscall
    
    mov rax, sys_read
    mov rbx, stdin
    mov rcx, name
    mov rdx, name_len
    syscall
    
    ; store length of resulting name in r8
    mov r8, rax
    ;sub r8, 2 ; get rid of the new line char (-2 because \r\n)
    dec r8
    
    mov rax, sys_write
    mov rbx, stdout
    mov rcx, a
    mov rdx, a_len
    syscall
    ;fldl2e
    ;debug_full
    mov rax, sys_write
    mov rcx, name
    mov rdx, r8
    syscall
    
    mov rax, sys_write
    mov rcx, b
    mov rdx, b_len
    syscall
    
    ret

segment .rodata

q: db "what's your name? "
q_len: equ $-q

a: db "so your name is "
a_len: equ $-a
b: db '?', 10, "what a cool name!", 10
b_len: equ $-b

segment .bss

name: resb 32
name_len: equ $-name
resb name_len
