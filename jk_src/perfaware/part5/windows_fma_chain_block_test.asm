global blocks_2
global blocks_4
global blocks_8
global blocks_12
global blocks_16

default rel

section .data

one dq 1.0
two dq 2.0

section .text

blocks_2:
    vxorpd ymm0, ymm0
    movsd xmm1, [two]
    movsd xmm2, [one]

    lea rax, [rsp - 128]
    and rax, ~0x1f

    vmovupd [rax], ymm0
    vmovupd [rax + 32], ymm0
    vmovupd [rax + 64], ymm0
    vmovupd [rax + 96], ymm0

    align 64
.repeat_loop:
    mov r10, rax
    mov r11, r10
    sub r11, 128

    mov r8, rdx
    .chain_loop:

        mov r9, 16
        .block_loop:
            vmovsd xmm0, [r10]
            add r10, 8

            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2

            vmovsd [r11], xmm0
            add r11, 8

            dec r9
            jnz .block_loop

        sub r11, 128
        mov r10, r11
        dec r8
        jnz .chain_loop

    dec rcx
    jnz .repeat_loop

    ret

blocks_4:
    vxorpd ymm0, ymm0
    movsd xmm1, [two]
    movsd xmm2, [one]

    lea rax, [rsp - 128]
    and rax, ~0x1f

    vmovupd [rax], ymm0
    vmovupd [rax + 32], ymm0
    vmovupd [rax + 64], ymm0
    vmovupd [rax + 96], ymm0

    align 64
.repeat_loop:
    mov r10, rax
    mov r11, r10
    sub r11, 128

    mov r8, rdx
    .chain_loop:

        mov r9, 16
        .block_loop:
            vmovsd xmm0, [r10]
            add r10, 8

            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2

            vmovsd [r11], xmm0
            add r11, 8

            dec r9
            jnz .block_loop

        sub r11, 128
        mov r10, r11
        dec r8
        jnz .chain_loop

    dec rcx
    jnz .repeat_loop

    ret

blocks_8:
    vxorpd ymm0, ymm0
    movsd xmm1, [two]
    movsd xmm2, [one]

    lea rax, [rsp - 128]
    and rax, ~0x1f

    vmovupd [rax], ymm0
    vmovupd [rax + 32], ymm0
    vmovupd [rax + 64], ymm0
    vmovupd [rax + 96], ymm0

    align 64
.repeat_loop:
    mov r10, rax
    mov r11, r10
    sub r11, 128

    mov r8, rdx
    .chain_loop:

        mov r9, 16
        .block_loop:
            vmovsd xmm0, [r10]
            add r10, 8

            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2

            vmovsd [r11], xmm0
            add r11, 8

            dec r9
            jnz .block_loop

        sub r11, 128
        mov r10, r11
        dec r8
        jnz .chain_loop

    dec rcx
    jnz .repeat_loop

    ret

blocks_12:
    vxorpd ymm0, ymm0
    movsd xmm1, [two]
    movsd xmm2, [one]

    lea rax, [rsp - 128]
    and rax, ~0x1f

    vmovupd [rax], ymm0
    vmovupd [rax + 32], ymm0
    vmovupd [rax + 64], ymm0
    vmovupd [rax + 96], ymm0

    align 64
.repeat_loop:
    mov r10, rax
    mov r11, r10
    sub r11, 128

    mov r8, rdx
    .chain_loop:

        mov r9, 16
        .block_loop:
            vmovsd xmm0, [r10]
            add r10, 8

            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2

            vmovsd [r11], xmm0
            add r11, 8

            dec r9
            jnz .block_loop

        sub r11, 128
        mov r10, r11
        dec r8
        jnz .chain_loop

    dec rcx
    jnz .repeat_loop

    ret

blocks_16:
    vxorpd ymm0, ymm0
    movsd xmm1, [two]
    movsd xmm2, [one]

    lea rax, [rsp - 128]
    and rax, ~0x1f

    vmovupd [rax], ymm0
    vmovupd [rax + 32], ymm0
    vmovupd [rax + 64], ymm0
    vmovupd [rax + 96], ymm0

    align 64
.repeat_loop:
    mov r10, rax
    mov r11, r10
    sub r11, 128

    mov r8, rdx
    .chain_loop:

        mov r9, 16
        .block_loop:
            vmovsd xmm0, [r10]
            add r10, 8

            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2
            vfmadd213sd xmm0, xmm1, xmm2

            vmovsd [r11], xmm0
            add r11, 8

            dec r9
            jnz .block_loop

        sub r11, 128
        mov r10, r11
        dec r8
        jnz .chain_loop

    dec rcx
    jnz .repeat_loop

    ret
