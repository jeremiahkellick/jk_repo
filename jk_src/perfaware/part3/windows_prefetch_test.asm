global process_nodes_control
global process_nodes_prefetch

section .text

process_nodes_control:
    align 64
.loop:
    mov r8, [rcx + 8]
    %rep 512
    add rcx, r8
    sub rcx, r8
    %endrep
    mov rcx, [rcx]
    db 0x0f, 0x1f, 0x00 ; 3-byte nop
    test rcx, rcx
    jnz .loop
    ret

process_nodes_prefetch:
    align 64
.loop:
    mov r8, [rcx + 8]
    %rep 512
    add rcx, r8
    sub rcx, r8
    %endrep
    mov rcx, [rcx]
    prefetch [rcx]
    test rcx, rcx
    jnz .loop
    ret
