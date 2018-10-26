global main

extern strlen

stin:   equ 0
stdout: equ 1

segment .text

main:
    ; we'll use r14/15 for a loop index
    push r14
    push r15
    
    ; store cmd lines args on stack
    push rdi ; rdi = argc = [rsp + 8]
    push rsi ; rsi = argv = [rsp + 0]

    xor r15, r15
    jmp .aft
    .top:
        ; get string to print
        mov r14, [rsp]
        mov r14, [r14 + 8*r15]
        
        ; get length
        mov rdi, r14
        call strlen
        
        ; print string
        mov rcx, r14
        mov rdx, rax
        mov rax, sys_write
        mov rbx, stdout
        syscall
        
        ; new line
        mov rax, sys_write
        mov rcx, new_line
        mov rdx, 1
        syscall
        
        inc r15
    .aft:
        cmp r15, [rsp + 8]
        jl .top
    
    ; undo arg pushes
    add rsp, 16
    
    ; restore call-safe regs
    pop r15
    pop r14
    ret

segment .rodata

new_line: db 10