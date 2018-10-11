#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "pigletvm-stack.h"

#define MAX_CODE_LEN 4096
#define MAX_LINE_LEN 256

static char *error_to_msg[] = {
    [SUCCESS] = "success",
    [ERROR_DIVISION_BY_ZERO] = "division by zero",
    [ERROR_UNKNOWN_OPCODE] = "unknown opcode",
    [ERROR_END_OF_STREAM] = "end of stream",
};

static struct opcode_to_disinfo {
    bool has_arg;
    char *name;
} opcode_to_disinfo[] = {
    [OP_ABORT] = {0, "ABORT"},
    [OP_PUSHI] = {1, "PUSHI"},
    [OP_DISCARD] = {0, "DISCARD"},
    [OP_ADD] = {0, "ADD"},
    [OP_SUB] = {0, "SUB"},
    [OP_DIV] = {0, "DIV"},
    [OP_MUL] = {0, "MUL"},
    [OP_JUMP] = {1, "JUMP"},
    [OP_JUMP_IF_TRUE] = {1, "JUMP_IF_TRUE"},
    [OP_JUMP_IF_FALSE] = {1, "JUMP_IF_FALSE"},
    [OP_EQUAL] = {0, "EQUAL"},
    [OP_LESS] = {0, "LESS"},
    [OP_LESS_OR_EQUAL] = {0, "LESS_OR_EQUAL"},
    [OP_GREATER] = {0, "GREATER"},
    [OP_GREATER_OR_EQUAL] = {0, "GREATER_OR_EQUAL"},
    [OP_POP_RES] = {0, "POP_RES"},
    [OP_DONE] = {0, "DONE"},
    [OP_PRINT] = {0, "PRINT"},
};

typedef struct asm_line {
    enum kind {OP_KIND, JUMP_KIND, LABEL_KIND} kind;
    union {
        /* a usual op, with an arg or without it */
        struct op {
            uint8_t opcode;
            bool has_arg;
            uint16_t arg;
        } op;
        /* a jump op, with a name of a label to jump to */
        struct {
            uint8_t opcode;
            char *label_name;
            uint16_t target;
        } jump;
        /* a label to be used in jumps */
        struct {
            char *label_name;
        } label;
    } as;
    /* a list of lines */
    struct asm_line *next;
} asm_line;

static void opname_to_opcode(const char *opname, uint8_t *op, bool *has_arg)
{
    for (int i = 0; i < OP_NUMBER_OF_OPS; i++) {
        if (strcasecmp(opcode_to_disinfo[i].name, opname) == 0) {
            *op = i;
            *has_arg = opcode_to_disinfo[i].has_arg;
            return;
        }
    }

    fprintf(stderr, "Unknown operation name: %s\n", opname);
    exit(EXIT_FAILURE);
}

static size_t print_instruction(uint8_t *bytecode, size_t offset)
{
    uint8_t op = bytecode[offset++];
    char *op_name = opcode_to_disinfo[op].name;
    bool has_arg = opcode_to_disinfo[op].has_arg;

    printf("%s", op_name);
    if (has_arg) {
        uint16_t arg = bytecode[offset++] << 8;
        arg += bytecode[offset++];
        printf(" %" PRIu16, arg);
    }
    printf("\n");

    return offset;
}

static int disassemble(uint8_t *bytecode)
{
    size_t offset = 0;
    while (bytecode[offset])
        offset = print_instruction(bytecode, offset);
    return EXIT_SUCCESS;
}

static int run(uint8_t *bytecode)
{
    interpret_result res = vm_interpret(bytecode);
    if (res != SUCCESS) {
        fprintf(stderr, "Runtime error: %s\n", error_to_msg[res]);
        return EXIT_FAILURE;
    }
    uint64_t result_value = vm_get_result();
    printf("Result value: %" PRIu64 "\n", result_value);
    return EXIT_SUCCESS;
}

static asm_line *parse_line(char *line)
{
    /* Ignore comments and empty lines*/
    if (line[0] == '#' || line[0] == '\n')
        return NULL;

    char *saveptr = NULL;
    char *opname = strtok_r(line, " ", &saveptr);
    if (!opname) {
        fprintf(stderr, "Cannot parse string: %s\n", line);
        exit(EXIT_FAILURE);
    }

    /* Strip opname, find it's info, put it into bytecode */

    char opname_stripped[MAX_LINE_LEN];
    char *d = opname_stripped;

    /* Strip */
    do
        while(isspace(*opname))
            opname++;
    while((*d++ = *opname++));

    asm_line *parsed_line = malloc(sizeof(*line));

    /* Get info */
    uint8_t op;
    bool has_arg;
    opname_to_opcode(opname_stripped, &op, &has_arg);
    parsed_line->kind = OP_KIND;
    parsed_line->as.op.opcode = op;
    parsed_line->as.op.has_arg = has_arg;

    /* See if there an immediate arg left on the line to be put into bytecode */
    for (;;) {
        char *arg = strtok_r(NULL, " ", &saveptr);
        if (!arg && has_arg) {
            fprintf(stderr, "Not enough arguments supplied: %s\n", line);
            exit(EXIT_FAILURE);
        } else if (arg && !has_arg) {
            fprintf(stderr, "Too many arguments supplied: %s\n", line);
            exit(EXIT_FAILURE);
        } else if (!arg && !has_arg) {
            /* This is fine */
            break;
        }

        uint16_t arg_val = 0;
        if (sscanf(arg, "%" SCNu16, &arg_val) != 1) {
            fprintf(stderr, "Invalid argument supplied: %s\n", arg);
            exit(EXIT_FAILURE);
        }

        parsed_line->as.op.arg = arg_val;
        has_arg = false;
    }

    return parsed_line;
}

static size_t assemble_line(asm_line *line, uint8_t *bytecode, size_t pc)
{
    bytecode[pc++] = line->as.op.opcode;
    if (line->as.op.has_arg) {
        uint16_t arg = line->as.op.arg;
        bytecode[pc++] = (arg & 0xff00) >> 8;
        bytecode[pc++] = (arg & 0x00ff);
    }
    return pc;
}

static uint8_t *assemble(const char *path, size_t *bytecode_len)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "File does not exist: %s\n", path);
        exit(EXIT_FAILURE);
    }

    uint8_t *bytecode = malloc(MAX_CODE_LEN);
    if (!bytecode) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    /* Parse lines */
    char line_buf[MAX_LINE_LEN];
    asm_line *lines_list = NULL;
    asm_line *last_line = NULL;
    while (fgets(line_buf, MAX_LINE_LEN, file)) {
        asm_line *line = parse_line(line_buf);
        if (!line)
            continue;
        if (!lines_list) {
            lines_list = line;
            last_line = line;
        } else {
            last_line->next = line;
            last_line = line;
        }
    }

    /* Compile lines */
    size_t pc = 0;
    for (asm_line *line = lines_list; line; line = line->next)
        pc = assemble_line(line, bytecode, pc);
    *bytecode_len = pc;

    fclose(file);
    return bytecode;
}

static uint8_t *read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "File does not exist: %s\n", path);
        exit(EXIT_FAILURE);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = (size_t)ftell(file);
    rewind(file);

    uint8_t *buf = malloc(file_size + 1);
    if (!buf) {
        fprintf(stderr, "Memory allocation failure: %s\n", path);
        exit(EXIT_FAILURE);
    }

    size_t bytes_read = fread(buf, sizeof(uint8_t), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Failed to read: %s\n", path);
        exit(EXIT_FAILURE);
    }

    buf[bytes_read] = '\0';
    fclose(file);
    return buf;
}

static void write_file(const uint8_t *bytecode, const size_t bytecode_len, const char *path)
{
    FILE *file = fopen(path, "wb");
    if (fwrite(bytecode, bytecode_len, 1, file) != 1) {
        fprintf(stderr, "Failed to write to a file: %s\n", path);
        exit(EXIT_FAILURE);
    }
    fclose(file);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: <dis|run|asm> <path/to/bytecode> [<path/to/output>]\n");
        exit(EXIT_FAILURE);
    }

    const char *cmd = argv[1];

    int res;
    if (0 == strcmp(cmd, "dis")) {
        if (argc != 3) {
            fprintf(stderr, "Usage: dis <path/to/bytecode>\n");
            exit(EXIT_FAILURE);
        }

        const char *path = argv[2];
        uint8_t *bytecode = read_file(path);

        res = disassemble(bytecode);

        free(bytecode);
    } else if (0 == strcmp(cmd, "run")) {
        if (argc != 3) {
            fprintf(stderr, "Usage: run <path/to/bytecode>\n");
            exit(EXIT_FAILURE);
        }

        const char *path = argv[2];
        uint8_t *bytecode = read_file(path);

        res = run(bytecode);

        free(bytecode);
    } else if (0 == strcmp(cmd, "asm")) {
        if (argc != 4) {
            fprintf(stderr, "Usage: asm <path/to/asm> <path/to/output/bytecode>\n");
            exit(EXIT_FAILURE);
        }

        const char *input_path = argv[2];
        const char *output_path = argv[3];

        size_t bytecode_len = 0;
        uint8_t *bytecode = assemble(input_path, &bytecode_len);
        write_file(bytecode, bytecode_len, output_path);

        res = EXIT_SUCCESS;
        free(bytecode);
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
        res = EXIT_FAILURE;
    }

    return res;
}
