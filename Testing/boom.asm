global main

extern bsearch, qsort
extern memcpy, memmove
extern memset, memchr

extern malloc, calloc, free, realloc

segment .text
    
main:
    mov edi, 32
    call malloc

    cmp rax, 0
    je .bad_malloc ; make sure that worked

    mov qword ptr [rax +  0], 0xdeadbeef
    mov qword ptr [rax +  8], 0xbadc0de
    mov qword ptr [rax + 16], 0xabacaba
    mov qword ptr [rax + 24], 0x1337

    mov rdi, rax
    mov r15, rax ; store a copy of that pointer in r15

    call free
    
    mov edi, 32
    call calloc

    cmp rax, 0
    je .bad_calloc ; make sure that worked

    cmp rax, r15
    jne .diff_block ; make sure it gave us the same block as before
                    ; this is a test case for the malloc impl
                    ; users should not assume this behavior

    mov r8, rax
    mov rax, [r8 +  0]
    mov rbx, [r8 +  8]
    mov rcx, [r8 + 16]
    mov rdx, [r8 + 24]
    
    ret

.bad_malloc:

    mov rax, sys_write
    mov rbx, 2
    mov rcx, bad_malloc_msg
    mov rdx, bad_malloc_msg_len
    syscall

    ret

.bad_calloc:

    mov rax, sys_write
    mov rbx, 2
    mov rcx, bad_calloc_msg
    mov rdx, bad_calloc_msg_len
    syscall

    ret

.diff_block:

    mov rax, sys_write
    mov rbx, 2
    mov rcx, diff_block_msg
    mov rdx, diff_block_msg_len
    syscall

    ret

segment .bss

align 8
fstat: resb 108

segment .rodata

bad_malloc_msg: db `\n\nMALLOC RETURNED NULL!!\n\n`
bad_malloc_msg_len: equ $-bad_malloc_msg

bad_calloc_msg: db `\n\nCALLOC RETURNED NULL!!\n\n`
bad_calloc_msg_len: equ $-bad_calloc_msg

diff_block_msg: db `\n\nMALLOC/CALLOC GAVE DIFFERENT DATA BLOCKS!!\n\n`
diff_block_msg_len: equ $-diff_block_msg
