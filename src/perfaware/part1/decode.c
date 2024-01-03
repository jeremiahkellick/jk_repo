#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPCODE_MOV_RM_REG_MASK 0xfc
#define OPCODE_MOV_RM_REG 0x88

#define OPCODE_MOV_IMMEDIATE_REG_MASK 0xf0
#define OPCODE_MOV_IMMEDIATE_REG 0xb0

#define MOD_MEMORY_8_BIT_DISP 0x1
#define MOD_MEMORY_16_BIT_DISP 0x2
#define MOD_REGISTER 0x3

// Usage: register_names[reg][w]
char *register_names[8][2] = {
        {"al", "ax"},
        {"cl", "cx"},
        {"dl", "dx"},
        {"bl", "bx"},
        {"ah", "sp"},
        {"ch", "bp"},
        {"dh", "si"},
        {"bh", "di"},
};

char *effective_address_base[8] = {
        "bx + si",
        "bx + di",
        "bp + si",
        "bp + di",
        "si",
        "di",
        "bp",
        "bx",
};

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr,
                "%s: Incorrect number of arguments, usage: %s FILE_TO_DECODE\n",
                argv[0],
                argv[1]);
        exit(1);
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror(argv[0]);
        exit(1);
    }

    printf("; %s disassembly:\nbits 16\n", argv[1]);

    uint8_t byte1;
    while (fread(&byte1, 1, 1, file) == 1) {
        bool d = (byte1 >> 1) & 0x1;
        bool w = byte1 & 0x1;
        if ((byte1 & OPCODE_MOV_RM_REG_MASK) == OPCODE_MOV_RM_REG) {
            uint8_t byte2;
            if (fread(&byte2, 1, 1, file) != 1) {
                goto file_ended_mid_instruction;
            }
            uint8_t mod = byte2 >> 6;
            uint8_t reg = (byte2 >> 3) & 0x7;
            char *reg_name = register_names[reg][w];
            uint8_t rm = byte2 & 0x7;
            char rm_name[32];
            if (mod == MOD_REGISTER) {
                strncpy(rm_name, register_names[rm][w], 32);
            } else {
                int rm_name_idx = 0;
                rm_name_idx += sprintf(&rm_name[rm_name_idx], "[%s", effective_address_base[rm]);
                if (mod == MOD_MEMORY_8_BIT_DISP) {
                    uint8_t disp;
                    if (fread(&disp, 1, 1, file) != 1) {
                        goto file_ended_mid_instruction;
                    }
                    if (disp != 0) {
                        rm_name_idx += sprintf(&rm_name[rm_name_idx], " + %hhd", disp);
                    }
                }
                if (mod == MOD_MEMORY_16_BIT_DISP) {
                    uint16_t disp;
                    if (fread(&disp, 1, 2, file) != 2) {
                        goto file_ended_mid_instruction;
                    }
                    if (disp != 0) {
                        rm_name_idx += sprintf(&rm_name[rm_name_idx], " + %hd", disp);
                    }
                }
                rm_name_idx += sprintf(&rm_name[rm_name_idx], "]");
            }
            printf("mov %s, %s\n", d ? reg_name : rm_name, d ? rm_name : reg_name);
        } else if ((byte1 & OPCODE_MOV_IMMEDIATE_REG_MASK) == OPCODE_MOV_IMMEDIATE_REG) {
            bool w = (byte1 >> 3) & 0x1;
            uint8_t reg = byte1 & 0x7;
            if (w) {
                uint16_t data;
                if (fread(&data, 1, 2, file) != 2) {
                    goto file_ended_mid_instruction;
                }
                printf("mov %s, %hd\n", register_names[reg][w], data);
            } else {
                uint8_t data;
                if (fread(&data, 1, 1, file) != 1) {
                    goto file_ended_mid_instruction;
                }
                printf("mov %s, %hhd\n", register_names[reg][w], data);
            }
        } else {
            fprintf(stderr, "%s: Unknown byte1 pattern '0x%x'\n", argv[0], byte1);
            goto error_exit;
        }
    }

    fclose(file);
    return 0;

file_ended_mid_instruction:
    fprintf(stderr, "%s: File ended in the middle of an instruction\n", argv[0]);
error_exit:
    fclose(file);
    return 1;
}
