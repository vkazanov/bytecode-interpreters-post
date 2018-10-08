#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pigletvm-stack.h"

static void disassemble(uint8_t *bytecode)
{
    (void) bytecode;
}

static void run(uint8_t *bytecode)
{
    (void) bytecode;
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

    if (0 == strcmp(cmd, "dis")) {
        disassemble(bytecode);
    } else if (0 == strcmp(cmd, "run")) {
        run(bytecode);
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
    }

    free(bytecode);
    return EXIT_SUCCESS;
}
