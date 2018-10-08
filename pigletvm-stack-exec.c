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

static int disassemble(uint8_t *bytecode)
{
    (void) bytecode;
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
    if (argc != 3) {
        fprintf(stderr, "Usage: <dis|run> <path/to/bytecode>\n");
        exit(EXIT_FAILURE);
    }

    const char *cmd = argv[1];
    const char *path = argv[2];
    uint8_t *bytecode = read_file(path);

    int res;
    if (0 == strcmp(cmd, "dis")) {
        res = disassemble(bytecode);
    } else if (0 == strcmp(cmd, "run")) {
        res = run(bytecode);
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
        res = EXIT_FAILURE;
    }

    free(bytecode);
    return res;
}
