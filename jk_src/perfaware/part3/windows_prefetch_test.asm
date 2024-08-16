global process_nodes_control
global process_nodes_prefetch

section .text

process_nodes_control:
    align 64
.loop:
    db 0x0f, 0x1f, 0x00 ; 3-byte nop
%rep 224
    nop
%endrep
    mov rcx, [rcx]
    test rcx, rcx
    jnz .loop
    ret

process_nodes_prefetch:
    align 64
.loop:
    prefetch [rcx]
%rep 224
    nop
%endrep
    mov rcx, [rcx]
    test rcx, rcx
    jnz .loop
    ret
