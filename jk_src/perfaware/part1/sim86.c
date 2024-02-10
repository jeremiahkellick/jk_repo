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

#define MEMORY_BYTE_COUNT (1 << 16)
static uint8_t memory[MEMORY_BYTE_COUNT];

// Number of bytes read from the binary file
static size_t code_byte_count;

typedef enum Flag {
    FLAG_CARRY,
    FLAG_PARITY,
    FLAG_ZERO,
    FLAG_SIGN,
    FLAG_OVERFLOW,
    FLAG_COUNT,
} Flag;

static char const *flag_strings[FLAG_COUNT] = {
    "C",
    "P",
    "Z",
    "S",
    "O",
};

static uint8_t flags = 0;
static uint8_t prev_flags = 0;

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

typedef enum BinopCategory {
    BC_MEMORY_ACCUMULATOR,
    BC_ACCUMULATOR_MEMORY,
    BC_REGISTER_REGISTER,
    BC_REGISTER_MEMORY,
    BC_MEMORY_REGISTER,
    BC_REGISTER_IMMEDIATE,
    BC_MEMORY_IMMEDIATE,
    BC_ACCUMULATOR_IMMEDIATE,
    BINOP_CATEGORY_COUNT,
} BinopCategory;

typedef enum ClocksColumn {
    COLUMN_CLOCKS,
    COLUMN_TRANSFERS,
    CLOCKS_COLUMN_COUNT,
} ClocksColumn;

static int binop_clocks[BINOP_TYPE_COUNT][BINOP_CATEGORY_COUNT][CLOCKS_COLUMN_COUNT] = {
    {0}, // BINOP_UNKNOWN
    {
        // BINOP_MOV
        {10, 1}, // BC_MEMORY_ACCUMULATOR
        {10, 1}, // BC_ACCUMULATOR_MEMORY
        {2, 0}, // BC_REGISTER_REGISTER
        {8, 1}, // BC_REGISTER_MEMORY
        {9, 1}, // BC_MEMORY_REGISTER
        {4, 0}, // BC_REGISTER_IMMEDIATE
        {10, 1}, // BC_MEMORY_IMMEDIATE
        {0, 0}, // BC_ACCUMULATOR_IMMEDIATE
    },
    {
        // BINOP_ADD
        {0, 0}, // BC_MEMORY_ACCUMULATOR
        {0, 0}, // BC_ACCUMULATOR_MEMORY
        {3, 0}, // BC_REGISTER_REGISTER
        {9, 1}, // BC_REGISTER_MEMORY
        {16, 2}, // BC_MEMORY_REGISTER
        {4, 0}, // BC_REGISTER_IMMEDIATE
        {17, 2}, // BC_MEMORY_IMMEDIATE
        {4, 0}, // BC_ACCUMULATOR_IMMEDIATE
    },
    {
        // BINOP_SUB
        {0, 0}, // BC_MEMORY_ACCUMULATOR
        {0, 0}, // BC_ACCUMULATOR_MEMORY
        {3, 0}, // BC_REGISTER_REGISTER
        {9, 1}, // BC_REGISTER_MEMORY
        {16, 2}, // BC_MEMORY_REGISTER
        {4, 0}, // BC_REGISTER_IMMEDIATE
        {17, 2}, // BC_MEMORY_IMMEDIATE
        {4, 0}, // BC_ACCUMULATOR_IMMEDIATE
    },
    {
        // BINOP_CMP
        {0, 0}, // BC_MEMORY_ACCUMULATOR
        {0, 0}, // BC_ACCUMULATOR_MEMORY
        {3, 0}, // BC_REGISTER_REGISTER
        {9, 1}, // BC_REGISTER_MEMORY
        {9, 1}, // BC_MEMORY_REGISTER
        {4, 0}, // BC_REGISTER_IMMEDIATE
        {10, 1}, // BC_MEMORY_IMMEDIATE
        {4, 0}, // BC_ACCUMULATOR_IMMEDIATE
    },
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
    BinopCategory category;
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

static int effective_address_clocks(Binop *binop)
{
    OperandMemory *memop = NULL;
    for (int i = 0; i < 2; i++) {
        if (binop->operands[i].type == OPERAND_MEMORY) {
            memop = &binop->operands[i].u.memory;
        }
    }
    if (memop == NULL) {
        return 0;
    }

    int num_regs = 0;
    for (int i = 0; i < OPERAND_MEMORY_REG_COUNT; i++) {
        if (memop->regs[i] != REG_NONE) {
            num_regs++;
        }
    }

    switch (num_regs) {
    case 0: // Displacement only
        return 6;
    case 1: {
        if (memop->disp == 0) { // Base or index only
            return 5;
        } else { // Displacement + base or index
            return 9;
        }
    } break;
    case 2: {
        bool fast = (memop->regs[0] == REG_BP && memop->regs[1] == REG_DI)
                || (memop->regs[0] == REG_BX && memop->regs[1] == REG_SI);
        if (memop->disp == 0) { // Base + index
            return fast ? 7 : 8;
        } else { // Displacement + base + index
            return fast ? 11 : 12;
        }
    } break;
    default:
        return 0;
    }
}

static void *register_address(Reg reg, bool wide)
{
    return &register_file[reg_w_to_register_file_index[reg][wide]];
}

static bool next_byte(uint8_t *dest)
{
    uint16_t *ip = register_address(REG_IP, true);
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

static int16_t get_int16(void *address, bool sign_extend)
{
    if (sign_extend) {
        return (int16_t)(*(int8_t *)address);
    } else {
        return *(int16_t *)address;
    }
}

/**
 * Read an int16_t value from the given file. If read_both_bytes is true, read two bytes and return
 * them directly. Otherwise, only read one byte and sign-extend it to two.
 */
static int16_t read_int16(bool sign_extend)
{
    uint16_t *ip = register_address(REG_IP, true);
    int16_t value = get_int16(&memory[*ip], sign_extend);
    *ip += sign_extend ? 1 : 2;
    return value;
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
            operand->u.memory.disp = read_int16(mod != MOD_MEMORY_16_BIT_DISP);
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

    if (binop->operands[DEST].type == OPERAND_REG) {
        switch (binop->operands[SRC].type) {
        case OPERAND_REG:
            binop->category = BC_REGISTER_REGISTER;
            break;
        case OPERAND_MEMORY:
            binop->category = BC_REGISTER_MEMORY;
            break;
        case OPERAND_IMMEDIATE:
            binop->category = BC_REGISTER_IMMEDIATE;
            break;
        }
    } else { // dest is memory operand
        binop->category =
                binop->operands[SRC].type == OPERAND_REG ? BC_MEMORY_REGISTER : BC_MEMORY_IMMEDIATE;
    }
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
    binop->category =
            binop->operands[DEST].type == OPERAND_REG ? BC_REGISTER_IMMEDIATE : BC_MEMORY_IMMEDIATE;
    binop->operands[SRC].type = OPERAND_IMMEDIATE;
    binop->operands[SRC].u.immediate = read_int16(sign_extend || !binop->wide);
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
        Binop *binop = &inst->u.binop;
        decode_reg_rm(byte, binop);
        for (int i = 0; i < 2; i++) {
            if (binop->operands[i].type == OPERAND_REG && binop->operands[i].u.reg == REG_AL
                    && binop->operands[!i].type == OPERAND_MEMORY) {
                binop->category = i ? BC_MEMORY_ACCUMULATOR : BC_ACCUMULATOR_MEMORY;
            }
        }
    } else if ((byte & OPCODE_MOV_IMMEDIATE_RM_MASK) == OPCODE_MOV_IMMEDIATE_RM) {
        inst->type = INST_BINOP;
        inst->u.binop.type = BINOP_MOV;
        decode_immediate_rm(byte, false, &inst->u.binop);
    } else if ((byte & OPCODE_MOV_IMMEDIATE_REG_MASK) == OPCODE_MOV_IMMEDIATE_REG) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        binop->type = BINOP_MOV;
        binop->category = BC_REGISTER_IMMEDIATE;
        binop->wide = (byte >> 3) & 0x1;
        binop->operands[DEST].type = OPERAND_REG;
        binop->operands[DEST].u.reg = (Reg)(byte & 0x7);
        binop->operands[SRC].type = OPERAND_IMMEDIATE;
        binop->operands[SRC].u.immediate = read_int16(!binop->wide);
    } else if ((byte & OPCODE_MOV_MEMORY_ACCUMULATOR_MASK) == OPCODE_MOV_MEMORY_ACCUMULATOR) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        binop->type = BINOP_MOV;
        binop->wide = byte & 0x1;
        int accumulator_operand = (byte >> 1) & 0x1;
        int memory_operand = !accumulator_operand;

        binop->operands[accumulator_operand].type = OPERAND_REG;
        binop->operands[accumulator_operand].u.reg = REG_AX;

        binop->category = accumulator_operand ? BC_MEMORY_ACCUMULATOR : BC_ACCUMULATOR_MEMORY;

        read_direct_address(&binop->operands[memory_operand]);
    } else if ((byte & OPCODE_OCTAL_REG_RM_MASK) == OPCODE_OCTAL_REG_RM) {
        inst->type = INST_BINOP;
        inst->u.binop.type = octal_code_to_binop[(byte >> 3) & 0x7];
        Binop *binop = &inst->u.binop;
        decode_reg_rm(byte, binop);
        if (binop->operands[DEST].type == OPERAND_REG && binop->operands[DEST].u.reg == REG_AX
                && binop->operands[SRC].type == OPERAND_IMMEDIATE) {
            binop->category = BC_ACCUMULATOR_IMMEDIATE;
        }
    } else if ((byte & OPCODE_OCTAL_IMMEDIATE_RM_MASK) == OPCODE_OCTAL_IMMEDIATE_RM) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        uint8_t octal_code = decode_immediate_rm(byte, (byte >> 1) & 0x1, binop);
        binop->type = octal_code_to_binop[octal_code];
        if (binop->operands[DEST].type == OPERAND_REG && binop->operands[DEST].u.reg == REG_AX
                && binop->operands[SRC].type == OPERAND_IMMEDIATE) {
            binop->category = BC_ACCUMULATOR_IMMEDIATE;
        }
    } else if ((byte & OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR_MASK)
            == OPCODE_OCTAL_IMMEDIATE_ACCUMULATOR) {
        inst->type = INST_BINOP;
        Binop *binop = &inst->u.binop;
        binop->type = octal_code_to_binop[(byte >> 3) & 0x7];
        binop->category = BC_ACCUMULATOR_IMMEDIATE;
        binop->wide = byte & 0x1;
        binop->operands[SRC].type = OPERAND_IMMEDIATE;
        binop->operands[SRC].u.immediate = read_int16(!binop->wide);
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
                            printf("+");
                        }
                        printf("%s", reg_w_to_string[op->u.memory.regs[i]][true]);
                    }
                }
                if (op->u.memory.disp != 0) {
                    if (num_regs > 0) {
                        printf("+%hd", op->u.memory.disp);
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

static bool get_flag(Flag flag)
{
    return (flags >> flag) & 0x1;
}

static void set_flag(Flag flag, bool value)
{
    if (value) {
        flags |= 0x1 << flag;
    } else {
        flags &= ~(0x1 << flag);
    }
}

static bool get_parity(int16_t value)
{
    uint8_t least_significant_byte = *(uint8_t *)&value;
    int t0 = least_significant_byte ^ (least_significant_byte >> 4);
    int t1 = t0 ^ (t0 >> 2);
    return !((t1 ^ (t1 >> 1)) & 0x1);
}

static uint16_t simulate_instruction(Instruction *inst)
{
    uint16_t mem_address = 0;
    switch (inst->type) {
    case INST_BINOP: {
        Binop *binop = &inst->u.binop;

        // Get addresses and values of operands
        void *operand_addresses[2] = {0};
        for (int i = 0; i < 2; i++) {
            Operand *operand = &binop->operands[i];
            switch (operand->type) {
            case OPERAND_REG:
                operand_addresses[i] = register_address(operand->u.reg, binop->wide);
                break;
            case OPERAND_MEMORY: {
                for (int j = 0; j < OPERAND_MEMORY_REG_COUNT; j++) {
                    mem_address += *(uint16_t *)register_address(operand->u.memory.regs[j], true);
                }
                mem_address += operand->u.memory.disp;
                operand_addresses[i] = &memory[mem_address];
            } break;
            case OPERAND_IMMEDIATE:
                assert(i != DEST && "Destination was an immediate");
                operand_addresses[i] = &operand->u.immediate;
                break;
            }
            assert(operand_addresses[i]);
        }
        int16_t dest_value = get_int16(operand_addresses[DEST], !binop->wide);
        int16_t src_value = get_int16(operand_addresses[SRC], !binop->wide);

        // Perform operation  to get result
        int16_t result;
        if (binop->type == BINOP_MOV) {
            result = src_value;
        } else {
            int16_t dest_invert_add = dest_value;
            if (binop->type == BINOP_ADD) {
                result = dest_value + src_value;
                // Because CF for a + b is the same as CF for ~a - b, we can invert this and use the
                // same code as sub and cmp to compute the carry flag
                // Also used for OF flag to invert the comparison of dest's and src's sign bits,
                // such that for add, same sign = 1, and for sub/cmp, same sign = 0.
                dest_invert_add = ~dest_invert_add;
            } else {
                result = dest_value - src_value;
            }

            // Update flags
            int16_t diff = dest_invert_add ^ src_value;
            set_flag(FLAG_CARRY,
                    // See carry-flag.txt for an explanation of the following expression
                    (((diff & src_value) | (~diff & (dest_invert_add - src_value))) >> 15) & 0x1);
            set_flag(FLAG_PARITY, get_parity(result));
            set_flag(FLAG_ZERO, result == 0);
            set_flag(FLAG_SIGN, result < 0);
            set_flag(FLAG_OVERFLOW, ((diff & (dest_value ^ result)) >> 15) & 0x1);
        }

        // If not a cmp, write result to dest
        if (binop->type != BINOP_CMP) {
            memcpy(operand_addresses[DEST], &result, binop->wide ? 2 : 1);
        }
    } break;
    case INST_JUMP: {
        Jump *jump = &inst->u.jump;
        uint16_t *cx = register_address(REG_CX, true);
        bool condition = false;
        switch (jump->type) {
        case JUMP_JA:
            condition = !(get_flag(FLAG_CARRY) || get_flag(FLAG_ZERO));
            break;
        case JUMP_JAE:
            condition = !get_flag(FLAG_CARRY);
            break;
        case JUMP_JB:
            condition = get_flag(FLAG_CARRY);
            break;
        case JUMP_JBE:
            condition = get_flag(FLAG_CARRY) || get_flag(FLAG_ZERO);
            break;
        case JUMP_JE:
            condition = get_flag(FLAG_ZERO);
            break;
        case JUMP_JG:
            condition = !((get_flag(FLAG_SIGN) ^ get_flag(FLAG_OVERFLOW)) || get_flag(FLAG_ZERO));
            break;
        case JUMP_JGE:
            condition = !(get_flag(FLAG_SIGN) ^ get_flag(FLAG_OVERFLOW));
            break;
        case JUMP_JL:
            condition = get_flag(FLAG_SIGN) ^ get_flag(FLAG_OVERFLOW);
            break;
        case JUMP_JLE:
            condition = (get_flag(FLAG_SIGN) ^ get_flag(FLAG_OVERFLOW)) || get_flag(FLAG_ZERO);
            break;
        case JUMP_JNE:
            condition = !get_flag(FLAG_ZERO);
            break;
        case JUMP_JNO:
            condition = !get_flag(FLAG_OVERFLOW);
            break;
        case JUMP_JPO:
            condition = !get_flag(FLAG_PARITY);
            break;
        case JUMP_JNS:
            condition = !get_flag(FLAG_SIGN);
            break;
        case JUMP_JO:
            condition = get_flag(FLAG_OVERFLOW);
            break;
        case JUMP_JPE:
            condition = get_flag(FLAG_PARITY);
            break;
        case JUMP_JS:
            condition = get_flag(FLAG_SIGN);
            break;
        case JUMP_LOOPNE:
            condition = --(*cx) != 0 && !get_flag(FLAG_ZERO);
            break;
        case JUMP_LOOPE:
            condition = --(*cx) != 0 && get_flag(FLAG_ZERO);
            break;
        case JUMP_LOOP:
            condition = --(*cx) != 0;
            break;
        case JUMP_JCXZ:
            condition = *cx == 0;
            break;
        case JUMP_TYPE_COUNT:
            printf("\n");
            fprintf(stderr, "%s: Invalid jump type\n", program_name);
            exit(1);
        }
        if (condition) {
            uint16_t *ip = register_address(REG_IP, true);
            *ip += jump->offset;
        }
    } break;
    }

    return mem_address;
}

int main(int argc, char **argv)
{
    program_name = argv[0];

    // Parse arguments
    char *file_path = NULL;
    bool execute = false;
    bool dump = false;
    {
        bool help = false;
        bool usage_error = false;
        bool options_ended = false;
        int non_option_arguments = 0;
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-' && argv[i][1] != '\0' && !options_ended) { // Option argument
                if (argv[i][1] == '-') {
                    if (argv[i][2] == '\0') { // -- encountered
                        options_ended = true;
                    } else { // Double hyphen option
                        if (strcmp(argv[i], "--execute") == 0) {
                            execute = true;
                        } else if (strcmp(argv[i], "--dump") == 0) {
                            dump = true;
                        } else if (strcmp(argv[i], "--help") == 0) {
                            help = true;
                        } else {
                            fprintf(stderr, "%s: Invalid option '%s'\n", program_name, argv[i]);
                            usage_error = true;
                        }
                    }
                } else { // Single-hypen option(s)
                    for (char *c = &argv[i][1]; *c != '\0'; c++) {
                        switch (*c) {
                        case 'e':
                            execute = true;
                            break;
                        case 'd':
                            dump = true;
                            break;
                        default:
                            fprintf(stderr,
                                    "%s: Invalid option '%c' in '%s'\n",
                                    program_name,
                                    *c,
                                    argv[i]);
                            usage_error = true;
                            break;
                        }
                    }
                }
            } else { // Regular argument
                non_option_arguments++;
                file_path = argv[i];
            }
        }
        if (dump && !execute) {
            fprintf(stderr, "%s: -d (--dump) requires -e (--execute)\n", program_name);
            usage_error = true;
        }
        if (!help && non_option_arguments != 1) {
            fprintf(stderr,
                    "%s: Expected 1 non-option argument, got %d\n",
                    program_name,
                    non_option_arguments);
            usage_error = true;
        }
        if (help || usage_error) {
            printf("NAME\n"
                   "\tsim86 - decodes and simulates a subset of 8086 machine code\n\n"
                   "SYNOPSIS\n"
                   "\tsim86 file\n"
                   "\tsim86 -e[-d] file\n\n"
                   "DESCRIPTION\n"
                   "\tsim86 decodes and simulates a subset of 8086 machine code. By default,\n"
                   "\tit prints a disassembly of FILE to stdout. -e (--execute) tells it to\n"
                   "\texecute the instructions. Instructions are loaded from FILE into the\n"
                   "\tsimulator's memory at address 0, which is also where the instruction\n"
                   "\tpointer (ip register) starts. Execution ends when the instruction\n"
                   "\tpointer leaves the region of memory that was loaded from FILE.\n\n"
                   "OPTIONS\n"
                   "\t-d, --dump\n"
                   "\t\tDump the simulator's memory to ./dump.data when execution ends.\n"
                   "\t\tOnly valid if the -e option is also specified.\n\n"
                   "\t-e, --execute\n"
                   "\t\tEnables instruction execution. If not specified, sim86 only\n"
                   "\t\tperforms disassembly.\n\n"
                   "\t--help\tDisplay this help text and exit.\n");
            exit(usage_error);
        }
    }

    { // Load file into memory
        FILE *file = fopen(file_path, "rb");
        if (file == NULL) {
            fprintf(stderr,
                    "%s: Could not open '%s': %s\n",
                    program_name,
                    file_path,
                    strerror(errno));
            exit(1);
        }
        code_byte_count = fread(memory, 1, MEMORY_BYTE_COUNT, file);
        fclose(file);
    }

    Instruction inst;
    if (execute) {
        int clocks = 0;
        printf("--- %s execution ---\n", file_path);
        while (save_state(), decode_instruction(&inst)) {
            print_instruction(&inst);
            printf(" ; ");
            uint16_t mem_address = simulate_instruction(&inst);

            // Compute and print clock count change
            if (inst.type == INST_BINOP) {
                Binop *binop = &inst.u.binop;
                int inst_clocks = binop_clocks[binop->type][binop->category][COLUMN_CLOCKS];
                int ea_clocks = effective_address_clocks(binop);
                int transfers = binop_clocks[binop->type][binop->category][COLUMN_TRANSFERS];
                int p_clocks = 0;
                if (mem_address % 2 != 0) {
                    p_clocks += transfers * 4;
                }
                int incr = inst_clocks + ea_clocks + p_clocks;
                clocks += incr;
                printf("Clocks: +%d = %d ", incr, clocks);
                if (ea_clocks != 0 || p_clocks != 0) {
                    printf("(%d", inst_clocks);
                    if (ea_clocks != 0) {
                        printf(" + %dea", ea_clocks);
                    }
                    if (p_clocks != 0) {
                        printf(" + %dp", p_clocks);
                    }
                    printf(") ");
                }
                printf("| ");
            }

            print_diff();
            printf("\n");
        }
        printf("\nFinal registers:\n");
        for (int i = 0; i < REG_VALUE_COUNT; i++) {
            Reg reg = register_print_order[i];
            uint16_t value = *(uint16_t *)register_address(reg, true);
            if (value != 0) {
                printf("      %s: 0x%04hx (%hu)\n", reg_w_to_string[reg][true], value, value);
            }
        }
        if (flags != 0) {
            printf("   flags: ");
            print_flags(flags);
            printf("\n");
        }
        printf("\n");
        if (dump) {
            char const *dump_file_path = "dump.data";
            FILE *dump_file = fopen(dump_file_path, "wb");
            if (dump_file == NULL) {
                fprintf(stderr,
                        "%s: Could not open '%s': %s\n",
                        program_name,
                        dump_file_path,
                        strerror(errno));
                exit(1);
            }
            fwrite(memory, 1, MEMORY_BYTE_COUNT, dump_file);
            fclose(dump_file);
        }
    } else {
        printf("; %s disassembly:\nbits 16\n", file_path);
        while (decode_instruction(&inst)) {
            print_instruction(&inst);
            printf("\n");
        }
    }

    return 0;
}
