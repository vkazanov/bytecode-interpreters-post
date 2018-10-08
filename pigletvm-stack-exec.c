#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pigletvm-stack.h"

static char *error_to_msg[] = {
    [SUCCESS] = "success",
    [ERROR_DIVISION_BY_ZERO] = "division by zero",
    [ERROR_UNKNOWN_OPCODE] = "unknown opcode",
    [ERROR_END_OF_STREAM] = "end of stream",
};

static struct opcode_to_disinfo {
    size_t num_args;
    char *name;
} opcode_to_disinfo[] = {
    [OP_ABORT] = {0, "OP_ABORT"},
    [OP_PUSHI] = {1, "OP_PUSHI"},
    [OP_ADD] = {0, "OP_ADD"},
    [OP_SUB] = {0, "OP_SUB"},
    [OP_DIV] = {0, "OP_DIV"},
    [OP_MUL] = {0, "OP_MUL"},
    [OP_POP_RES] = {0, "OP_POP_RES"},
    [OP_DONE] = {0, "OP_DONE"},
};

static void print_arg(char *name, size_t arg_offset, uint8_t *bytecode, size_t num_args)
{
    printf("%s", name);
    for (size_t arg_i = 0; arg_i < num_args; arg_i++ )
        printf(" %d", bytecode[arg_offset++]);
    printf("\n");
}

static size_t print_instruction(uint8_t *bytecode, size_t offset)
{
    uint8_t op = bytecode[offset++];
    char *op_name = opcode_to_disinfo[op].name;
    size_t num_args = opcode_to_disinfo[op].num_args;
    print_arg(op_name, offset, bytecode, num_args);
    return offset + num_args;
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
    return EXIT_SUCCESS;
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

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: <dis|run> <path/to/bytecode>\n");
        exit(EXIT_FAILURE);
    }

    const char *cmd = argv[1];

    int res;
    if (0 == strcmp(cmd, "dis")) {
        const char *path = argv[2];
        uint8_t *bytecode = read_file(path);

        res = disassemble(bytecode);

        free(bytecode);
    } else if (0 == strcmp(cmd, "run")) {
        const char *path = argv[2];
        uint8_t *bytecode = read_file(path);

        res = run(bytecode);

        free(bytecode);
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
        res = EXIT_FAILURE;
    }

    return res;
}
