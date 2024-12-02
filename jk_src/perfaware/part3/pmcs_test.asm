global count_nonzeroes

section .text

count_nonzeroes:
    xor rax, rax
    xor r10, r10
.loop:
    mov r11b, [rdx + r10]
    test r11b, r11b
    jz .skip_sum
    inc rax
.skip_sum:
    inc r10
    cmp r10, rcx
    jb .loop
    ret
