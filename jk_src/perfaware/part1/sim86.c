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

static char *program_name = NULL;

#define REG_VALUE_COUNT 8

// clang-format off
typedef enum Reg {
    REG_NONE = -1,

//  wide == 0   wide == 1
    REG_AL = 0, REG_AX = 0,
    REG_CL = 1, REG_CX = 1,
    REG_DL = 2, REG_DX = 2,
    REG_BL = 3, REG_BX = 3,
    REG_AH = 4, REG_SP = 4,
    REG_CH = 5, REG_BP = 5,
    REG_DH = 6, REG_SI = 6,
    REG_BH = 7, REG_DI = 7,
} Reg;
// clang-format on

static uint8_t register_file[16] = {0};

// Usage: reg_w_to_register_file_index[reg][w]
static uint8_t reg_w_to_register_file_index[REG_VALUE_COUNT][2] = {
    {0x0, 0x0}, // al, ax
    {0x2, 0x2}, // cl, cx
    {0x4, 0x4}, // dl, dx
    {0x6, 0x6}, // bl, bx
    {0x1, 0x8}, // ah, sp
    {0x3, 0xa}, // ch, bp
    {0x5, 0xc}, // dh, si
    {0x7, 0xe}, // bh, di
};

// Usage: reg_w_to_string[reg][w]
static char *reg_w_to_string[REG_VALUE_COUNT][2] = {
    {"al", "ax"},
    {"cl", "cx"},
    {"dl", "dx"},
    {"bl", "bx"},
    {"ah", "sp"},
    {"ch", "bp"},
    {"dh", "si"},
    {"bh", "di"},
};

#define RM_VALUE_COUNT 8
#define OPERAND_MEMORY_REG_COUNT 2

// Usage: rm_to_memory_operand_regs[rm][register_index]
static Reg rm_to_memory_operand_regs[RM_VALUE_COUNT][OPERAND_MEMORY_REG_COUNT] = {
    {REG_BX, REG_SI},
    {REG_BX, REG_DI},
    {REG_BP, REG_SI},
    {REG_BP, REG_DI},
    {REG_SI, REG_NONE},
    {REG_DI, REG_NONE},
    {REG_BP, REG_NONE},
    {REG_BX, REG_NONE},
};

typedef enum InstructionType {
    INST_NONE,
    INST_MOV,
    INST_ADD,
    INST_SUB,
    INST_CMP,
    INST_CONDITIONAL_JUMP,
    INST_LOOP,
    INST_TYPE_COUNT,
} InstructionType;

static InstructionType octal_code_instruction_types[8] = {
    INST_ADD,
    INST_NONE,
    INST_NONE,
    INST_NONE,
    INST_NONE,
    INST_SUB,
    INST_NONE,
    INST_CMP,
};

static char *instruction_type_names[INST_TYPE_COUNT] = {
    "UNKNOWN",
    "mov",
    "add",
    "sub",
    "cmp",
    "UNPRINTABLE",
    "UNPRINTABLE",
};

static char *conditional_jump_names[16] = {
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

static char *loop_names[4] = {
    "loopne",
    "loope",
    "loop",
    "jcxz",
};

typedef struct OperandMemory {
    int16_t disp;
    Reg regs[OPERAND_MEMORY_REG_COUNT];
} OperandMemory;

typedef enum OperandType {
    OPERAND_REG,
    OPERAND_MEMORY,
    OPERAND_IMMEDIATE,
} OperandType;

typedef struct Operand {
    OperandType type;
    union {
        Reg reg;
        OperandMemory memory;
        int16_t immediate;
    };
} Operand;

#define OPERAND_COUNT 2
#define DEST 0
#define SRC 1

typedef struct Instruction {
    Operand operands[OPERAND_COUNT];
    InstructionType type;
    uint8_t subtype;
    int8_t jump_offset;
    bool wide;
} Instruction;

/**
 * Read n bytes from file and write them to dest. If the file ends before we can read n bytes, print
 * an error informing the user that their file ended in the middle of an instruction and exit the
 * program with code 1.
 */
static void read_instruction_bytes(FILE *file, int n, uint8_t *dest)
{
    if (fread(dest, 1, n, file) != n) {
        fprintf(stderr, "%s: File ended in the middle of an instruction\n", program_name);
        exit(1);
    }
}

/**
 * Read an int16_t value from the given file. If read_both_bytes is true, read two bytes and return
 * them directly. Otherwise, only read one byte and sign-extend it to two.
 */
static int16_t read_int16(FILE *file, bool read_both_bytes)
{
    if (read_both_bytes) {
        int16_t data_full;
        read_instruction_bytes(file, 2, (uint8_t *)&data_full);
        return data_full;
    } else {
        int8_t data_low;
        read_instruction_bytes(file, 1, (uint8_t *)&data_low);
        return (int16_t)data_low;
    }
}

/**
 * Read 2 bytes from the file. Interpret them as a direct address, writing it to the given operand.
 */
static void read_direct_address(FILE *file, Operand *operand)
{
    operand->type = OPERAND_MEMORY;
    operand->memory.regs[0] = REG_NONE;
    operand->memory.regs[1] = REG_NONE;
    read_instruction_bytes(file, 2, (uint8_t *)&operand->memory.disp);
}

/**
 * Read a byte following the "mod x r/m" pattern. Also read the displacement bytes if any. Write
 * those values to the given operand struct.
 *
 * @param file the file to read the byte from
 * @return The middle 3 bits of the byte. What they mean depends on the calling context.
 */
static uint8_t decode_rm_byte(FILE *file, Operand *operand)
{
    uint8_t byte;
    read_instruction_bytes(file, 1, &byte);
    uint8_t mod = byte >> 6;
    uint8_t rm = byte & 0x7;
    if (rm == 0x6 && mod == MOD_MEMORY_NO_DISP) { // Direct address
        read_direct_address(file, operand);
    } else if (mod == MOD_REGISTER) {
        operand->type = OPERAND_REG;
        operand->reg = rm;
    } else {
        operand->type = OPERAND_MEMORY;
        for (int i = 0; i < OPERAND_MEMORY_REG_COUNT; i++) {
            operand->memory.regs[i] = rm_to_memory_operand_regs[rm][i];
        }
        if (mod == MOD_MEMORY_8_BIT_DISP || mod == MOD_MEMORY_16_BIT_DISP) {
            operand->memory.disp = read_int16(file, mod == MOD_MEMORY_16_BIT_DISP);
        }
    }
    return (byte >> 3) & 0x7;
}

static void decode_reg_rm(FILE *file, uint8_t byte, Instruction *inst)
{
    inst->wide = byte & 0x1;
    int rm_operand = (byte >> 1) & 0x1;
    int reg_operand = !rm_operand;
    uint8_t reg = decode_rm_byte(file, &inst->operands[rm_operand]);
    inst->operands[reg_operand].type = OPERAND_REG;
    inst->operands[reg_operand].reg = reg;
}

/**
 * Decode rm byte (with optional displacement) followed by an immediate
 *
 * @return The middle 3 bits of the rm byte. What they mean depends on the calling context.
 */
static uint8_t decode_immediate_rm(FILE *file, uint8_t byte, bool sign_extend, Instruction *inst)
{
    inst->wide = byte & 0x1;
    uint8_t middle_bits = decode_rm_byte(file, &inst->operands[DEST]);
    inst->operands[SRC].type = OPERAND_IMMEDIATE;
    inst->operands[SRC].immediate = read_int16(file, !sign_extend && inst->wide);
    return middle_bits;
}

static bool decode_instruction(FILE *file, Instruction *inst)
{
    memset(inst, 0, sizeof(*inst));

    uint8_t byte;
    if (fread(&byte, 1, 1, file) != 1) {
        return false;
    }

    if ((byte & OPCODE_MOV_REG_RM_MASK) == OPCODE_MOV_REG_RM) {
        inst->type = INST_MOV;
        decode_reg_rm(file, byte, inst);
    } else if ((byte & OPCODE_MOV_IMMEDIATE_RM_MASK) == OPCODE_MOV_IMMEDIATE_RM) {
        inst->type = INST_MOV;
        decode_immediate_rm(file, byte, false, inst);
    } else if ((byte & OPCODE_MOV_IMMEDIATE_REG_MASK) == OPCODE_MOV_IMMEDIATE_REG) {
        inst->type = INST_MOV;
        inst->wide = (byte >> 3) & 0x1;
        inst->operands[DEST].type = OPERAND_REG;
        inst->operands[DEST].reg = byte & 0x7;
        inst->operands[SRC].type = OPERAND_IMMEDIATE;
        inst->operands[SRC].immediate = read_int16(file, inst->wide);
    } else if ((byte & OPCODE_MOV_MEMORY_ACCUMULATOR_MASK) == OPCODE_MOV_MEMORY_ACCUMULATOR) {
        inst->type = INST_MOV;
        inst->wide = byte & 0x1;
        int accumulator_operand = (byte >> 1) & 0x1;
        int memory_operand = !accumulator_operand;

        inst->operands[accumulator_operand].type = OPERAND_REG;
        inst->operands[accumulator_operand].reg = REG_AX;

        read_direct_address(file, &inst->operands[memory_operand]);
    } else if ((byte & OPCODE_OCTAL_REG_RM_MASK) == OPCODE_OCTAL_REG_RM) {
        inst->type = octal_code_instruction_types[(byte >> 3) & 0x7];
        decode_reg_rm(file, byte, inst);
    } else if ((byte & OPCODE_OCTAL_IMMEDIATE_RM_MASK) == OPCODE_OCTAL_IMMEDIATE_RM) {
        uint8_t octal_code = decode_immediate_rm(file, byte, (byte >> 1) & 0x1, inst);
        inst->type = octal_code_instruction_types[octal_code];
    } else if ((byte & OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR_MASK)
            == OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR) {
        inst->type = octal_code_instruction_types[(byte >> 3) & 0x7];
        inst->wide = byte & 0x1;
        inst->operands[SRC].type = OPERAND_IMMEDIATE;
        inst->operands[SRC].immediate = read_int16(file, inst->wide);
    } else if ((byte & OPCODE_CONDITIONAL_JUMP_MASK) == OPCODE_CONDITIONAL_JUMP) {
        inst->type = INST_CONDITIONAL_JUMP;
        inst->subtype = byte & 0xf;
        read_instruction_bytes(file, 1, (uint8_t *)&inst->jump_offset);
    } else if ((byte & OPCODE_LOOP_MASK) == OPCODE_LOOP) {
        inst->type = INST_LOOP;
        inst->subtype = byte & 0x3;
        read_instruction_bytes(file, 1, (uint8_t *)&inst->jump_offset);
    } else {
        fprintf(stderr, "%s: Unknown byte pattern '0x%x'\n", program_name, byte);
        exit(1);
    }

    return true;
}

static void print_instruction(Instruction *inst)
{
    switch (inst->type) {
    case INST_CONDITIONAL_JUMP:
    case INST_LOOP: {
        char **names = inst->type == INST_CONDITIONAL_JUMP ? conditional_jump_names : loop_names;
        printf("%s ", names[inst->subtype]);
        printf("$%+hhd\n", inst->jump_offset + 2);
    } break;
    default:
        // Print mnemonic
        printf("%s ", instruction_type_names[inst->type]);

        // Print a data size specifier if it would otherwise be ambiguous,
        if (inst->operands[0].type != OPERAND_REG && inst->operands[1].type != OPERAND_REG) {
            printf(inst->wide ? "word " : "byte ");
        }

        // Print the operands
        for (int operand_index = 0; operand_index < OPERAND_COUNT; operand_index++) {
            if (operand_index != 0) {
                printf(", ");
            }
            Operand *const op = &inst->operands[operand_index];
            switch (op->type) {
            case OPERAND_REG:
                printf("%s", reg_w_to_string[op->reg][inst->wide]);
                break;
            case OPERAND_MEMORY: {
                printf("[");
                int num_regs = 0;
                for (int i = 0; i < OPERAND_MEMORY_REG_COUNT; i++) {
                    if (op->memory.regs[i] != REG_NONE) {
                        num_regs++;
                        if (num_regs == 2) {
                            printf(" + ");
                        }
                        printf("%s", reg_w_to_string[op->memory.regs[i]][true]);
                    }
                }
                if (op->memory.disp != 0) {
                    if (num_regs > 0) {
                        printf(" + %hd", op->memory.disp);
                    } else {
                        printf("%hu", op->memory.disp);
                    }
                }
                printf("]");
            } break;
            case OPERAND_IMMEDIATE:
                printf("%hd", op->immediate);
                break;
            }
        }
        printf("\n");
        break;
    }
}

void usage_error(void)
{
    fprintf(stderr, "Usage: %s [-d] binary_file\n", program_name);
    exit(1);
}

int main(int argc, char **argv)
{
    program_name = argv[0];

    if (argc < 2 || argc > 3) {
        usage_error();
    }

    // Parse arguments
    bool disassemble = false;
    char *file_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == 'd' && argv[i][2] == '\0') {
            if (disassemble) {
                usage_error();
            }
            disassemble = true;
        } else {
            if (file_path != NULL) {
                usage_error();
            }
            file_path = argv[i];
        }
    }
    if (file_path == NULL) {
        usage_error();
    }

    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror(program_name);
        exit(1);
    }

    Instruction inst;
    if (disassemble) {
        printf("; %s disassembly:\nbits 16\n", file_path);
    } else {
        printf("--- %s execution ---\n", file_path);
    }
    while (decode_instruction(file, &inst)) {
        print_instruction(&inst);
    }

    fclose(file);

    return 0;
}
