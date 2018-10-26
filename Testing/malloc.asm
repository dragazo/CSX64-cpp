global main

extern atexit
extern malloc, calloc, realloc, free

segment .text

print1:
    mov eax, sys_write
    mov ebx, 1
    mov rcx, str1
    mov rdx, str1_len
    syscall
    ret
print2:
    mov eax, sys_write
    mov ebx, 1
    mov rcx, str2
    mov rdx, str2_len
    syscall
    ret

main:
    debug_cpu
    ret
    
    movdqa xmm0, [vec_a]
    movdqa xmm1, [vec_b]
    pmullw xmm2, xmm0, xmm1
    debug_vpu
    ret
    
    mov rdi, 1
    call malloc
    ;debug_cpu
    mov rdi, rax
    call free
    
    mov rdi, 9
    call malloc
    ;debug_cpu
    mov rdi, rax
    ;call free
    
    mov rdi, 45
    call malloc
    ;debug_cpu
    mov rdi, rax
    call free
    
    mov rdi, 195
    call malloc
    ;debug_cpu
    mov rdi, rax
    call free
    
    mov rdi, 5024587
    call malloc
    ;debug_cpu
    mov byte ptr [rax + 5024587 - 1], 4
    mov rdi, rax
    call free
    
    mov rdi, 195
    call malloc
    ;debug_cpu
    mov rdi, rax
    call free
    
    xor eax, eax
    ret
    
    mov rdi, print1
    call atexit
    mov rdi, print2
    call atexit
    mov rdi, print1
    call atexit
    mov rdi, print2
    call atexit
    mov rdi, print1
    call atexit
    mov rdi, print2
    call atexit
    
    mov rdi, 1
    call malloc
    push rax
    
    mov rdi, 42
    call malloc
    push rax
    
    mov rdi, 4 * 1024 * 1024
    call malloc
    push rax
    
    ; test write
    mov [rax + 15634], qword 3.4
    
    pop rdi
    call free
    
    pop rdi
    call free
    
    pop rdi
    mov rsi, 456
    call realloc
    
    mov rdi, rax
    call free
    
    ret

segment .rodata

str1: db "str1", 10
str1_len: equ $-str1

str2: db "str2", 10
str2_len: equ $-str2

align 16
vec_a: dw 1, 4, 8, 5, 4, 7, 3, 6
vec_b: dw 8, 2, 7, 5, 1, 8, 6, 4
