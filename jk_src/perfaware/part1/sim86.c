#include <assert.h>
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

#define REG_VALUE_COUNT 9

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

    REG_IP,
} Reg;
// clang-format on

static Reg register_print_order[REG_VALUE_COUNT] = {
    REG_AX,
    REG_BX,
    REG_CX,
    REG_DX,
    REG_SP,
    REG_BP,
    REG_SI,
    REG_DI,
    REG_IP,
};

#define REGISTER_FILE_BYTE_COUNT (REG_VALUE_COUNT * 2)

static uint8_t register_file[REGISTER_FILE_BYTE_COUNT] = {0};
static uint8_t prev_register_file[REGISTER_FILE_BYTE_COUNT] = {0};

#define MEMORY_BYTE_COUNT (1 << 20)
static uint8_t memory[MEMORY_BYTE_COUNT];

// Number of bytes read from the binary file
static size_t code_byte_count;

typedef enum Flag {
    FLAG_ZERO,
    FLAG_SIGN,
    FLAG_COUNT,
} Flag;

#define FLAG_ZERO_BIT (1 << FLAG_ZERO)
#define FLAG_SIGN_BIT (1 << FLAG_SIGN)

static char const *flag_strings[FLAG_COUNT] = {
    "Z",
    "S",
};

static uint8_t flags = 0;
static uint8_t prev_flags = 0;

// Usage: reg_w_to_register_file_index[reg][w]
static uint8_t reg_w_to_register_file_index[REG_VALUE_COUNT][2] = {
    {0x0, 0x0},   // al, ax
    {0x2, 0x2},   // cl, cx
    {0x4, 0x4},   // dl, dx
    {0x6, 0x6},   // bl, bx
    {0x1, 0x8},   // ah, sp
    {0x3, 0xa},   // ch, bp
    {0x5, 0xc},   // dh, si
    {0x7, 0xe},   // bh, di
    {0x10, 0x10}, // ip, ip
};

// Usage: reg_w_to_string[reg][w]
static char const *reg_w_to_string[REG_VALUE_COUNT][2] = {
    {"al", "ax"},
    {"cl", "cx"},
    {"dl", "dx"},
    {"bl", "bx"},
    {"ah", "sp"},
    {"ch", "bp"},
    {"dh", "si"},
    {"bh", "di"},
    {"error_ip_must_be_wide", "ip"},
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
    } u;
} Operand;

typedef enum BinopType {
    BINOP_UNKNOWN,
    BINOP_MOV,
    BINOP_ADD,
    BINOP_SUB,
    BINOP_CMP,
    BINOP_TYPE_COUNT,
} BinopType;

static BinopType octal_code_to_binop[8] = {
    BINOP_ADD,
    BINOP_UNKNOWN,
    BINOP_UNKNOWN,
    BINOP_UNKNOWN,
    BINOP_UNKNOWN,
    BINOP_SUB,
    BINOP_UNKNOWN,
    BINOP_CMP,
};

static char const *binop_names[BINOP_TYPE_COUNT] = {
    "unknown_binop",
    "mov",
    "add",
    "sub",
    "cmp",
};

#define DEST 0
#define SRC 1

typedef struct Binop {
    BinopType type;
    Operand operands[2];
    bool wide;
} Binop;

typedef enum JumpType {
    JUMP_JO,
    JUMP_JNO,
    JUMP_JB,
    JUMP_JAE,
    JUMP_JE,
    JUMP_JNE,
    JUMP_JBE,
    JUMP_JA,
    JUMP_JS,
    JUMP_JNS,
    JUMP_JPE,
    JUMP_JPO,
    JUMP_JL,
    JUMP_JGE,
    JUMP_JLE,
    JUMP_JG,
    JUMP_LOOPNE,
    JUMP_LOOPE,
    JUMP_LOOP,
    JUMP_JCXZ,
    JUMP_TYPE_COUNT,
} JumpType;

static char const *jump_names[JUMP_TYPE_COUNT] = {
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
    "loopne",
    "loope",
    "loop",
    "jcxz",
};

typedef struct Jump {
    JumpType type;
    int8_t offset;
} Jump;

typedef enum InstructionType {
    INST_BINOP,
    INST_JUMP,
} InstructionType;

typedef struct Instruction {
    InstructionType type;
    union {
        Binop binop;
        Jump jump;
    } u;
} Instruction;

static bool next_byte(uint8_t *dest)
{
    uint16_t *ip = (uint16_t *)&register_file[reg_w_to_register_file_index[REG_IP][true]];
    if (*ip < code_byte_count) {
        *dest = memory[(*ip)++];
        return true;
    } else {
        return false;
    }
}

/**
 * Read n bytes from file and write them to dest. If the file ends before we can read n bytes, print
 * an error informing the user that their file ended in the middle of an instruction and exit the
 * program with code 1.
 */
static void read_instruction_bytes(size_t n, uint8_t *dest)
{
    for (; n > 0; n--) {
        if (!next_byte(dest++)) {
            fprintf(stderr, "%s: File ended in the middle of an instruction\n", program_name);
            exit(1);
        }
    }
}

/**
 * Read an int16_t value from the given file. If read_both_bytes is true, read two bytes and return
 * them directly. Otherwise, only read one byte and sign-extend it to two.
 */
static int16_t read_int16(bool read_both_bytes)
{
    if (read_both_bytes) {
        int16_t data_full;
        read_instruction_bytes(2, (uint8_t *)&data_full);
        return data_full;
    } else {
        int8_t data_low;
        read_instruction_bytes(1, (uint8_t *)&data_low);
        return (int16_t)data_low;
    }
}

/**
 * Read 2 bytes from the file. Interpret them as a direct address, writing it to the given operand.
 */
static void read_direct_address(Operand *operand)
{
    operand->type = OPERAND_MEMORY;
    operand->u.memory.regs[0] = REG_NONE;
    operand->u.memory.regs[1] = REG_NONE;
    read_instruction_bytes(2, (uint8_t *)&operand->u.memory.disp);
}

/**
 * Read a byte following the "mod x r/m" pattern. Also read the displacement bytes if any. Write
 * those values to the given operand struct.
 *
 * @param file the file to read the byte from
 * @return The middle 3 bits of the byte. What they mean depends on the calling context.
 */
static uint8_t decode_rm_byte(Operand *operand)
{
    uint8_t byte;
    read_instruction_bytes(1, &byte);
    uint8_t mod = byte >> 6;
    uint8_t rm = byte & 0x7;
    if (rm == 0x6 && mod == MOD_MEMORY_NO_DISP) { // Direct address
        read_direct_address(operand);
    } else if (mod == MOD_REGISTER) {
        operand->type = OPERAND_REG;
        operand->u.reg = (Reg)rm;
    } else {
        operand->type = OPERAND_MEMORY;
        for (int i = 0; i < OPERAND_MEMORY_REG_COUNT; i++) {
            operand->u.memory.regs[i] = rm_to_memory_operand_regs[rm][i];
        }
        if (mod == MOD_MEMORY_8_BIT_DISP || mod == MOD_MEMORY_16_BIT_DISP) {
            operand->u.memory.disp = read_int16(mod == MOD_MEMORY_16_BIT_DISP);
        }
    }
    return (byte >> 3) & 0x7;
}

static void decode_reg_rm(uint8_t byte, Binop *binop)
{
    binop->wide = byte & 0x1;
    int rm_operand = (byte >> 1) & 0x1;
    int reg_operand = !rm_operand;
    uint8_t reg = decode_rm_byte(&binop->operands[rm_operand]);
    binop->operands[reg_operand].type = OPERAND_REG;
    binop->operands[reg_operand].u.reg = (Reg)reg;
}

/**
 * Decode rm byte (with optional displacement) followed by an immediate
 *
 * @return The middle 3 bits of the rm byte. What they mean depends on the calling context.
 */
static uint8_t decode_immediate_rm(uint8_t byte, bool sign_extend, Binop *binop)
{
    binop->wide = byte & 0x1;
    uint8_t middle_bits = decode_rm_byte(&binop->operands[DEST]);
    binop->operands[SRC].type = OPERAND_IMMEDIATE;
    binop->operands[SRC].u.immediate = read_int16(!sign_extend && binop->wide);
    return middle_bits;
}

static bool decode_instruction(Instruction *inst)
{
    memset(inst, 0, sizeof(*inst));

    uint8_t byte;
    if (!next_byte(&byte)) {
        return false;
    }

    if ((byte & OPCODE_MOV_REG_RM_MASK) == OPCODE_MOV_REG_RM) {
        inst->type = INST_BINOP;
        inst->u.binop.type = BINOP_MOV;
        decode_reg_rm(byte, &inst->u.binop);
    } else if ((byte & OPCODE_MOV_IMMEDIATE_RM_MASK) == OPCODE_MOV_IMMEDIATE_RM) {
        inst->type = INST_BINOP;
        inst->u.binop.type = BINOP_MOV;
        decode_immediate_rm(byte, false, &inst->u.binop);
    } else if ((byte & OPCODE_MOV_IMMEDIATE_REG_MASK) == OPCODE_MOV_IMMEDIATE_REG) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        binop->type = BINOP_MOV;
        binop->wide = (byte >> 3) & 0x1;
        binop->operands[DEST].type = OPERAND_REG;
        binop->operands[DEST].u.reg = (Reg)(byte & 0x7);
        binop->operands[SRC].type = OPERAND_IMMEDIATE;
        binop->operands[SRC].u.immediate = read_int16(binop->wide);
    } else if ((byte & OPCODE_MOV_MEMORY_ACCUMULATOR_MASK) == OPCODE_MOV_MEMORY_ACCUMULATOR) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        binop->type = BINOP_MOV;
        binop->wide = byte & 0x1;
        int accumulator_operand = (byte >> 1) & 0x1;
        int memory_operand = !accumulator_operand;

        binop->operands[accumulator_operand].type = OPERAND_REG;
        binop->operands[accumulator_operand].u.reg = REG_AX;

        read_direct_address(&binop->operands[memory_operand]);
    } else if ((byte & OPCODE_OCTAL_REG_RM_MASK) == OPCODE_OCTAL_REG_RM) {
        inst->type = INST_BINOP;
        inst->u.binop.type = octal_code_to_binop[(byte >> 3) & 0x7];
        decode_reg_rm(byte, &inst->u.binop);
    } else if ((byte & OPCODE_OCTAL_IMMEDIATE_RM_MASK) == OPCODE_OCTAL_IMMEDIATE_RM) {
        uint8_t octal_code = decode_immediate_rm(byte, (byte >> 1) & 0x1, &inst->u.binop);
        inst->type = INST_BINOP;
        inst->u.binop.type = octal_code_to_binop[octal_code];
    } else if ((byte & OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR_MASK)
            == OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        binop->type = octal_code_to_binop[(byte >> 3) & 0x7];
        binop->wide = byte & 0x1;
        binop->operands[SRC].type = OPERAND_IMMEDIATE;
        binop->operands[SRC].u.immediate = read_int16(binop->wide);
    } else if ((byte & OPCODE_CONDITIONAL_JUMP_MASK) == OPCODE_CONDITIONAL_JUMP) {
        inst->type = INST_JUMP;
        inst->u.jump.type = (JumpType)(byte & 0xf);
        read_instruction_bytes(1, (uint8_t *)&inst->u.jump.offset);
    } else if ((byte & OPCODE_LOOP_MASK) == OPCODE_LOOP) {
        inst->type = INST_JUMP;
        inst->u.jump.type = (JumpType)((byte & 0x3) + 0x10);
        read_instruction_bytes(1, (uint8_t *)&inst->u.jump.offset);
    } else {
        fprintf(stderr, "%s: Unknown byte pattern '0x%x'\n", program_name, byte);
        exit(1);
    }

    return true;
}

static void print_instruction(Instruction *inst)
{
    switch (inst->type) {
    case INST_JUMP: {
        printf("%s ", jump_names[inst->u.jump.type]);
        printf("$%+hhd", inst->u.jump.offset + 2);
    } break;
    case INST_BINOP: {
        Binop *binop = &inst->u.binop;
        // Print mnemonic
        printf("%s ", binop_names[binop->type]);

        // Print a data size specifier if it would otherwise be ambiguous,
        if (binop->operands[0].type != OPERAND_REG && binop->operands[1].type != OPERAND_REG) {
            printf(binop->wide ? "word " : "byte ");
        }

        // Print the operands
        for (int operand_index = 0; operand_index < 2; operand_index++) {
            if (operand_index != 0) {
                printf(", ");
            }
            Operand *op = &binop->operands[operand_index];
            switch (op->type) {
            case OPERAND_REG:
                printf("%s", reg_w_to_string[op->u.reg][binop->wide]);
                break;
            case OPERAND_MEMORY: {
                printf("[");
                int num_regs = 0;
                for (int i = 0; i < OPERAND_MEMORY_REG_COUNT; i++) {
                    if (op->u.memory.regs[i] != REG_NONE) {
                        num_regs++;
                        if (num_regs == 2) {
                            printf(" + ");
                        }
                        printf("%s", reg_w_to_string[op->u.memory.regs[i]][true]);
                    }
                }
                if (op->u.memory.disp != 0) {
                    if (num_regs > 0) {
                        printf(" + %hd", op->u.memory.disp);
                    } else {
                        printf("%hu", op->u.memory.disp);
                    }
                }
                printf("]");
            } break;
            case OPERAND_IMMEDIATE:
                printf("%hd", op->u.immediate);
                break;
            }
        }
    } break;
    }
}

static void print_flags(uint8_t flags_value)
{
    for (int i = 0; i < FLAG_COUNT; i++) {
        if ((flags_value >> i) & 0x1) {
            printf("%s", flag_strings[i]);
        }
    }
}

static void save_state(void)
{
    memcpy(prev_register_file, register_file, sizeof(register_file));
    prev_flags = flags;
}

static void print_diff(void)
{
    for (int i = 0; i < REG_VALUE_COUNT; i++) {
        Reg reg = register_print_order[i];
        int register_file_index = reg_w_to_register_file_index[reg][true];
        uint16_t prev = *(uint16_t *)&prev_register_file[register_file_index];
        uint16_t current = *(uint16_t *)&register_file[register_file_index];
        if (prev != current) {
            printf("%s:0x%hx->0x%hx ", reg_w_to_string[reg][true], prev, current);
        }
    }
    if (prev_flags != flags) {
        printf("flags:");
        print_flags(prev_flags);
        printf("->");
        print_flags(flags);
    }
}

void simulate_instruction(Instruction *inst)
{
    switch (inst->type) {
    case INST_BINOP: {
        Binop *binop = &inst->u.binop;

        // Get dest_address and set value to the value of the source operand
        void *dest_address = NULL;
        int16_t value = 0;
        {
            Operand *dest = &binop->operands[DEST];
            switch (dest->type) {
            case OPERAND_REG:
                dest_address =
                        &register_file[reg_w_to_register_file_index[dest->u.reg][binop->wide]];
                break;
            case OPERAND_MEMORY:
                goto not_implemented;
            case OPERAND_IMMEDIATE:
                assert(0 && "Destination was an immediate");
            }
            Operand *src = &binop->operands[SRC];
            void *src_address = NULL;
            switch (src->type) {
            case OPERAND_REG:
                src_address = &register_file[reg_w_to_register_file_index[src->u.reg][binop->wide]];
                break;
            case OPERAND_MEMORY:
                goto not_implemented;
            case OPERAND_IMMEDIATE:
                src_address = &src->u.immediate;
                break;
            }
            assert(dest_address);
            assert(src_address);
            value = *(int16_t *)src_address;
        }

        // Perform operation on value
        switch (binop->type) {
        case BINOP_MOV:
            break;
        case BINOP_ADD:
            value += *(int16_t *)dest_address;
            break;
        case BINOP_SUB:
        case BINOP_CMP:
            value = *(int16_t *)dest_address - value;
            break;
        case BINOP_UNKNOWN:
        case BINOP_TYPE_COUNT:
            assert(0 && "Invalid binop");
        }

        // If not a mov, update flags based on value
        if (binop->type != BINOP_MOV) {
            if (value == 0) {
                flags |= FLAG_ZERO_BIT;
            } else {
                flags &= ~FLAG_ZERO_BIT;
            }
            if (value < 0) {
                flags |= FLAG_SIGN_BIT;
            } else {
                flags &= ~FLAG_SIGN_BIT;
            }
        }

        // If not a cmp, write value to dest
        if (binop->type != BINOP_CMP) {
            memcpy(dest_address, &value, binop->wide ? 2 : 1);
        }
    } break;
    case INST_JUMP:
        fprintf(stderr, "%s: Not implemented\n", program_name);
        exit(1);
    }
    return;

not_implemented:
    printf("\n");
    fprintf(stderr, "%s: Instruction not implemented\n", program_name);
    exit(1);
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

    { // Load file into memory
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
            perror(program_name);
            exit(1);
        }
        code_byte_count = fread(memory, 1, MEMORY_BYTE_COUNT, file);
        fclose(file);
    }

    Instruction inst;
    if (disassemble) {
        printf("; %s disassembly:\nbits 16\n", file_path);
        while (decode_instruction(&inst)) {
            print_instruction(&inst);
            printf("\n");
        }
    } else {
        printf("--- %s execution ---\n", file_path);
        while (save_state(), decode_instruction(&inst)) {
            print_instruction(&inst);
            printf(" ; ");
            simulate_instruction(&inst);
            print_diff();
            printf("\n");
        }
        printf("\nFinal registers:\n");
        for (int i = 0; i < REG_VALUE_COUNT; i++) {
            Reg reg = register_print_order[i];
            uint16_t value = *(uint16_t *)&register_file[reg_w_to_register_file_index[reg][true]];
            if (value != 0) {
                printf("      %s: 0x%04hx (%hu)\n", reg_w_to_string[reg][true], value, value);
            }
        }
        printf("   flags: ");
        print_flags(flags);
        printf("\n\n");
    }

    return 0;
}
