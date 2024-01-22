#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPCODE_MOV_REG_RM_MASK 0xfc
#define OPCODE_MOV_REG_RM 0x88

#define OPCODE_MOV_IMMEDIATE_RM_MASK 0xfe
#define OPCODE_MOV_IMMEDIATE_RM 0xc6

#define OPCODE_MOV_IMMEDIATE_REG_MASK 0xf0
#define OPCODE_MOV_IMMEDIATE_REG 0xb0

#define OPCODE_MOV_MEMORY_ACCUMULATOR_MASK 0xfc
#define OPCODE_MOV_MEMORY_ACCUMULATOR 0xa0

#define OPCODE_OCTAL_REG_RM_MASK 0xc4
#define OPCODE_OCTAL_REG_RM 0x0

#define OPCODE_OCTAL_IMMEDIATE_RM_MASK 0xfc
#define OPCODE_OCTAL_IMMEDIATE_RM 0x80

#define OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR_MASK 0xc6
#define OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR 0x4

#define OPCODE_CONDITIONAL_JUMP_MASK 0xf0
#define OPCODE_CONDITIONAL_JUMP 0x70

#define OPCODE_LOOP_MASK 0xfc
#define OPCODE_LOOP 0xe0

#define MOD_MEMORY_NO_DISP 0x0
#define MOD_MEMORY_8_BIT_DISP 0x1
#define MOD_MEMORY_16_BIT_DISP 0x2
#define MOD_REGISTER 0x3

char *program_name = NULL;

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

char *octal_code_names[8] = {
        "add",
        "not_supported",
        "not_supported",
        "not_supported",
        "not_supported",
        "sub",
        "not_supported",
        "cmp",
};

char *conditional_jump_names[16] = {
        "jo",
        "jno",
        "jb",
        "jae",
        "je",
        "jne",
        "jbe",
        "ja",
        "js",
        "jns",
        "jpe",
        "jpo",
        "jl",
        "jge",
        "jle",
        "jg",
};

char *loop_names[4] = {
        "loopne",
        "loope",
        "loop",
        "jcxz",
};

/**
 * Read n bytes from file and write them to dest. If the file ends before we can read n bytes, print
 * an error informing the user that their file ended in the middle of an instruction and exit the
 * program with code 1.
 */
void read_instruction_bytes(FILE *file, int n, uint8_t *dest)
{
    if (fread(dest, 1, n, file) != n) {
        fprintf(stderr, "%s: File ended in the middle of an instruction\n", program_name);
        exit(1);
    }
}

#define RM_STRING_BUF_SIZE 32

typedef struct RmByteInfo {
    uint8_t mod;
    uint8_t x;
    uint8_t rm;
    uint16_t disp;
    char rm_string[RM_STRING_BUF_SIZE];
} RmByteInfo;

/**
 * Read a byte following the "mod x r/m" pattern. Also read the displacement bytes if any. Write
 * those values to the info struct, plus a disassembled string in info->rm_string.
 *
 * x is an octal value, the middle 3 bits of the byte. What it means depends on the calling context.
 */
void read_rm_byte(FILE *file, bool w, RmByteInfo *info)
{
    uint8_t byte;
    read_instruction_bytes(file, 1, &byte);
    info->mod = byte >> 6;
    info->x = (byte >> 3) & 0x7;
    info->rm = byte & 0x7;
    if (info->rm == 0x6 && info->mod == MOD_MEMORY_NO_DISP) { // Direct address
        uint16_t disp;
        read_instruction_bytes(file, 2, (uint8_t *)&disp);
        sprintf(info->rm_string, "[%hu]", disp);
    } else if (info->mod == MOD_REGISTER) {
        strncpy(info->rm_string, register_names[info->rm][w], RM_STRING_BUF_SIZE);
    } else {
        int rm_string_index = 0;
        rm_string_index +=
                sprintf(&info->rm_string[rm_string_index], "[%s", effective_address_base[info->rm]);
        if (info->mod == MOD_MEMORY_8_BIT_DISP) {
            uint8_t disp;
            read_instruction_bytes(file, 1, &disp);
            if (disp != 0) {
                rm_string_index += sprintf(&info->rm_string[rm_string_index], " + %hhd", disp);
            }
        }
        if (info->mod == MOD_MEMORY_16_BIT_DISP) {
            uint16_t disp;
            read_instruction_bytes(file, 2, (uint8_t *)&disp);
            if (disp != 0) {
                rm_string_index += sprintf(&info->rm_string[rm_string_index], " + %hd", disp);
            }
        }
        rm_string_index += sprintf(&info->rm_string[rm_string_index], "]");
    }
}

/**
 * Read bytes for an immediate value and write the value to a string, formatted as a signed integer
 *
 * @param immediate_string Buffer to write the result to. Must be large enough for any signed
 *     integer of the size indicated by is_16_bit.
 * @param is_16_bit true if the immediate is a 16 bit values, false if it's 8 bit
 */
void read_immediate(FILE *file, bool is_16_bit, char *immediate_string)
{
    if (is_16_bit) {
        uint16_t data;
        read_instruction_bytes(file, 2, (uint8_t *)&data);
        sprintf(immediate_string, "%hd", data);
    } else {
        uint8_t data;
        read_instruction_bytes(file, 1, &data);
        sprintf(immediate_string, "%hhd", data);
    }
}

void disassemble_reg_rm(FILE *file, uint8_t byte, char *name)
{
    bool d = (byte >> 1) & 0x1;
    bool w = byte & 0x1;
    RmByteInfo rm_byte_info;
    read_rm_byte(file, w, &rm_byte_info);
    char *reg_name = register_names[rm_byte_info.x][w];
    printf("%s %s, %s\n",
            name,
            d ? reg_name : rm_byte_info.rm_string,
            d ? rm_byte_info.rm_string : reg_name);
}

void disassemble_immediate_rm(FILE *file, uint8_t byte, char *name_override, bool force_8_bit)
{
    bool w = byte & 0x1;
    char *w_string = w ? "word " : "byte ";
    RmByteInfo rm_byte_info;
    read_rm_byte(file, w, &rm_byte_info);
    char *name = name_override ? name_override : octal_code_names[rm_byte_info.x];
    char immediate_string[8];
    read_immediate(file, !force_8_bit && w, immediate_string);
    printf("%s %s%s, %s\n",
            name,
            rm_byte_info.mod != MOD_REGISTER ? w_string : "",
            rm_byte_info.rm_string,
            immediate_string);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr,
                "%s: Incorrect number of arguments, usage: %s FILE_TO_DECODE\n",
                program_name,
                argv[1]);
        exit(1);
    }

    program_name = argv[0];

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror(program_name);
        exit(1);
    }

    printf("; %s disassembly:\nbits 16\n", argv[1]);

    char immediate_string[8];

    uint8_t byte;
    while (fread(&byte, 1, 1, file) == 1) {
        if ((byte & OPCODE_MOV_REG_RM_MASK) == OPCODE_MOV_REG_RM) {
            disassemble_reg_rm(file, byte, "mov");
        } else if ((byte & OPCODE_MOV_IMMEDIATE_RM_MASK) == OPCODE_MOV_IMMEDIATE_RM) {
            disassemble_immediate_rm(file, byte, "mov", false);
        } else if ((byte & OPCODE_MOV_IMMEDIATE_REG_MASK) == OPCODE_MOV_IMMEDIATE_REG) {
            bool w = (byte >> 3) & 0x1;
            uint8_t reg = byte & 0x7;
            read_immediate(file, w, immediate_string);
            printf("mov %s, %s\n", register_names[reg][w], immediate_string);
        } else if ((byte & OPCODE_MOV_MEMORY_ACCUMULATOR_MASK) == OPCODE_MOV_MEMORY_ACCUMULATOR) {
            bool mem_is_dest = (byte >> 1) & 0x1;
            bool w = byte & 0x1;
            char *reg_name = w ? "ax" : "al";
            read_immediate(file, w, immediate_string);
            if (mem_is_dest) {
                printf("mov [%s], %s\n", immediate_string, reg_name);
            } else {
                printf("mov %s, [%s]\n", reg_name, immediate_string);
            }
        } else if ((byte & OPCODE_OCTAL_REG_RM_MASK) == OPCODE_OCTAL_REG_RM) {
            disassemble_reg_rm(file, byte, octal_code_names[(byte >> 3) & 0x7]);
        } else if ((byte & OPCODE_OCTAL_IMMEDIATE_RM_MASK) == OPCODE_OCTAL_IMMEDIATE_RM) {
            bool s = (byte >> 1) & 0x1;
            disassemble_immediate_rm(file, byte, NULL, s);
        } else if ((byte & OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR_MASK)
                == OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR) {
            bool w = byte & 0x1;
            read_immediate(file, w, immediate_string);
            printf("%s %s, %s\n",
                    octal_code_names[(byte >> 3) & 0x7],
                    w ? "ax" : "al",
                    immediate_string);
        } else if ((byte & OPCODE_CONDITIONAL_JUMP_MASK) == OPCODE_CONDITIONAL_JUMP) {
            read_immediate(file, false, immediate_string);
            printf("%s $+2+%s\n", conditional_jump_names[byte & 0xf], immediate_string);
        } else if ((byte & OPCODE_LOOP_MASK) == OPCODE_LOOP) {
            read_immediate(file, false, immediate_string);
            printf("%s $+2+%s\n", loop_names[byte & 0x3], immediate_string);
        } else {
            fprintf(stderr, "%s: Unknown byte pattern '0x%x'\n", program_name, byte);
            exit(1);
        }
    }

    fclose(file);

    return 0;
}
