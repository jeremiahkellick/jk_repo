global buffer_loop_mov
global buffer_loop_cmp
global buffer_loop_dec
global buffer_loop_nop
global buffer_loop_nop_3
global buffer_loop_nop_9
global loop_predictable
global loop_unpredictable

section .text

buffer_loop_mov:
    xor rax, rax
.loop:
    mov [rdx + rax], al
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_cmp:
    xor rax, rax
.loop:
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_dec:
.loop:
    dec rcx
    jnz .loop
    ret

buffer_loop_nop:
    xor rax, rax
.loop:
    db 0x0f, 0x1f, 0x00
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_nop_3:
    xor rax, rax
.loop:
    nop
    nop
    nop
    inc rax
    cmp rax, rcx
    jb .loop
    ret

buffer_loop_nop_9:
    xor rax, rax
.loop:
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    inc rax
    cmp rax, rcx
    jb .loop
    ret

loop_predictable:
    xor rax, rax
.loop:
    inc rax
    mov r8, 0
    and r8, rax
    jnz .loop
    cmp rax, rcx
    jb .loop
    ret

loop_unpredictable:
    xor rax, rax
.loop:
    inc rax
    mov r8, 1
    and r8, rax
    jnz .loop
    cmp rax, rcx
    jb .loop
    ret
