#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define OPCODE_SHIFT 2

#define OPCODE_MOV 0x22

#define D_MASK 0x2
#define D_SHIFT 1

#define W_MASK 0x1

#define MOD_SHIFT 6

#define MOD_REGISTER 0x3

#define REG_MASK 0x38
#define REG_SHIFT 3

#define RM_MASK 0x7

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

    while (true) {
        uint8_t byte1;
        if (fread(&byte1, 1, 1, file) != 1) {
            break;
        }
        uint8_t opcode = byte1 >> OPCODE_SHIFT;
        bool d = (byte1 & D_MASK) >> D_SHIFT;
        bool w = byte1 & W_MASK;
        switch (opcode) {
        case OPCODE_MOV: {
            uint8_t byte2;
            if (fread(&byte2, 1, 1, file) != 1) {
                break;
            }
            uint8_t mod = byte2 >> MOD_SHIFT;
            uint8_t reg = (byte2 & REG_MASK) >> REG_SHIFT;
            uint8_t rm = byte2 & RM_MASK;
            switch (mod) {
            case MOD_REGISTER: {
                char *dest_name = register_names[d ? reg : rm][w];
                char *src_name = register_names[d ? rm : reg][w];
                printf("mov %s, %s\n", dest_name, src_name);
            } break;
            default:
                fprintf(stderr, "%s: mod '0x%x' not supported\n", argv[0], mod);
            }
        } break;
        default:
            fprintf(stderr, "%s: opcode '0x%x' not supported\n", argv[0], opcode);
            exit(1);
        }
    }

    fclose(file);
    return 0;
}
