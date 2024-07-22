global read_1x
global read_2x
global read_3x
global read_4x
global write_1x
global write_2x
global write_3x
global write_4x

section .text

read_1x:
    align 64
.loop:
    mov rax, [rdx]
    sub rcx, 1
    jge .loop
    ret

read_2x:
    align 64
.loop:
    mov rax, [rdx]
    mov rax, [rdx]
    sub rcx, 2
    jge .loop
    ret

read_3x:
    align 64
.loop:
    mov rax, [rdx]
    mov rax, [rdx]
    mov rax, [rdx]
    sub rcx, 3
    jge .loop
    ret

read_4x:
    align 64
.loop:
    mov rax, [rdx]
    mov rax, [rdx]
    mov rax, [rdx]
    mov rax, [rdx]
    sub rcx, 4
    jge .loop
    ret

write_1x:
    align 64
.loop:
    mov [rdx], rax
    sub rcx, 1
    jge .loop
    ret

write_2x:
    align 64
.loop:
    mov [rdx], rax
    mov [rdx], rax
    sub rcx, 2
    jge .loop
    ret

write_3x:
    align 64
.loop:
    mov [rdx], rax
    mov [rdx], rax
    mov [rdx], rax
    sub rcx, 3
    jge .loop
    ret

write_4x:
    align 64
.loop:
    mov [rdx], rax
    mov [rdx], rax
    mov [rdx], rax
    mov [rdx], rax
    sub rcx, 4
    jge .loop
    ret
