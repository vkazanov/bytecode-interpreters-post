#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "piglet-matcher.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: <asm|dis|run> [arg1 [arg2 ...]]\n");
        exit(EXIT_FAILURE);
    }

    const char *cmd = argv[1];

    int res = EXIT_SUCCESS;
    if (0 == strcmp(cmd, "asm")) {
        if (argc != 4) {
            fprintf(stderr, "Usage: asm <path/to/asm> <path/to/output/bytecode>\n");
            exit(EXIT_FAILURE);
        }
        /* TODO: asm */
    } else if (0 == strcmp(cmd, "run")) {
        if (argc != 4) {
            fprintf(stderr, "Usage: run <path/to/bytecode> <path/to/input>\n");
            exit(EXIT_FAILURE);
        }
        /* TODO: run */
    } else if (0 == strcmp(cmd, "dis")) {
        if (argc != 3) {
            fprintf(stderr, "Usage: dis <path/to/bytecode>\n");
            exit(EXIT_FAILURE);
        }
        /* TODO: disasm */
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
        res = EXIT_FAILURE;
    }

    return res;
}
