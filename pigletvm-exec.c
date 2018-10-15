#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include "pigletvm.h"

#define MAX_CODE_LEN 4096
#define MAX_LINE_LEN 256

#define TIMER_DEF(start_var,end_var) struct timeval (start_var); struct timeval (end_var)
#define TIMER_START(start_var) gettimeofday(&(start_var), NULL)
#define TIMER_END(start_var,end_var,msg)                                \
    do{                                                                 \
        gettimeofday(&(end_var), NULL);                                 \
        fprintf(stderr, "PROFILE: %s took %ldms", msg,                  \
                (((end_var).tv_sec * 1000000L + (end_var).tv_usec) -    \
                   ((start_var).tv_sec * 1000000L + (start_var).tv_usec)) / 1000); \
    } while (0);

static char *error_to_msg[] = {
    [SUCCESS] = "success",
    [ERROR_DIVISION_BY_ZERO] = "division by zero",
    [ERROR_UNKNOWN_OPCODE] = "unknown opcode",
    [ERROR_END_OF_STREAM] = "end of stream",
};

typedef struct opinfo {
    bool has_arg;
    char *name;
    bool is_jump;
} opinfo;

opinfo opcode_to_disinfo[] = {
    [OP_ABORT] = {0, "ABORT", 0},
    [OP_PUSHI] = {1, "PUSHI", 0},
    [OP_LOADI] = {1, "LOADI", 0},
    [OP_STOREI] = {1, "STOREI", 0},
    [OP_LOAD] = {0, "LOAD", 0},
    [OP_STORE] = {0, "STORE", 0},
    [OP_DUP] = {0, "DUP", 0},
    [OP_DISCARD] = {0, "DISCARD", 0},
    [OP_ADD] = {0, "ADD", 0},
    [OP_SUB] = {0, "SUB", 0},
    [OP_DIV] = {0, "DIV", 0},
    [OP_MUL] = {0, "MUL", 0},
    [OP_JUMP] = {1, "JUMP", 1},
    [OP_JUMP_IF_TRUE] = {1, "JUMP_IF_TRUE", 1},
    [OP_JUMP_IF_FALSE] = {1, "JUMP_IF_FALSE", 1},
    [OP_EQUAL] = {0, "EQUAL", 0},
    [OP_LESS] = {0, "LESS", 0},
    [OP_LESS_OR_EQUAL] = {0, "LESS_OR_EQUAL", 0},
    [OP_GREATER] = {0, "GREATER", 0},
    [OP_GREATER_OR_EQUAL] = {0, "GREATER_OR_EQUAL", 0},
    [OP_POP_RES] = {0, "POP_RES", 0},
    [OP_DONE] = {0, "DONE", 0},
    [OP_PRINT] = {0, "PRINT", 0},
};

typedef struct labelinfo {
    char *name;
    uint16_t address;
    struct labelinfo *next;
} labelinfo;

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
            uint16_t target_address;
        } jump;
        /* a label to be used in jumps */
        struct {
            char *label_name;
        } label;
    } as;

    /* a list of lines */
    struct asm_line *next;
} asm_line;

static opinfo *opname_to_opcode_info(const char *opname, uint8_t *op)
{
    for (int i = 0; i < OP_NUMBER_OF_OPS; i++) {
        if (strcasecmp(opcode_to_disinfo[i].name, opname) == 0) {
            *op = i;
            return &opcode_to_disinfo[i];
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

static void strip_line(char *source, char *target)
{
    do
        while(isspace(*source))
            source++;
    while((*target++ = *source++));
}

static bool is_label_name(char *name)
{
    /* always start with a letter */
    if (!isalpha(*name++))
        return false;
    /* everything is alphanumeric */
    while (*name) {
        if (!isalnum(*name++))
            return false;
    }
    return true;
}

static void parse_jump_argument(asm_line *parsed_line, char *arg_raw)
{
    char *arg = calloc(strlen(arg_raw), 1);
    strip_line(arg_raw, arg);
    if (is_label_name(arg)) {
        parsed_line->as.jump.label_name = strdup(arg);
    } else {
        uint16_t arg_val = 0;
        if (sscanf(arg, "%" SCNu16, &arg_val) != 1) {
            fprintf(stderr, "Invalid address supplied: %s\n", arg);
            exit(EXIT_FAILURE);
        }
        parsed_line->as.jump.target_address = arg_val;
    }
}

static void parse_op_argument(asm_line *parsed_line, char *arg)
{
    uint16_t arg_val = 0;
    if (sscanf(arg, "%" SCNu16, &arg_val) != 1) {
        fprintf(stderr, "Invalid argument supplied: %s\n", arg);
        exit(EXIT_FAILURE);
    }

    parsed_line->as.op.arg = arg_val;
}

static asm_line *parse_line(char *raw_line)
{
    /* Ignore comments and empty lines*/
    if (raw_line[0] == '#' || raw_line[0] == '\n')
        return NULL;

    char *saveptr = NULL;
    char *opname_raw = strtok_r(raw_line, " ", &saveptr);
    if (!opname_raw) {
        fprintf(stderr, "Cannot parse string: %s\n", raw_line);
        exit(EXIT_FAILURE);
    }

    /* Allocate the target line */
    asm_line *parsed_line = calloc(sizeof(*parsed_line), 1);
    if (!parsed_line) {
        fprintf(stderr, "Memory allocation failure\n ");
        exit(EXIT_FAILURE);
    }

    /* Strip opname, find it's info, put it into bytecode */
    char opname[MAX_LINE_LEN];
    strip_line(opname_raw, opname);

    /* Label? */
    size_t opname_len = strlen(opname) - 1;
    if (opname[opname_len] == ':') {
        /* Cannot have any more args */
        char *arg = strtok_r(NULL, " ", &saveptr);
        if (arg) {
            fprintf(stderr, "Labels do not have arguments: %s\n", raw_line);
            exit(EXIT_FAILURE);
        }

        /* Strip the colon */
        opname[opname_len] = '\0';

        parsed_line->kind = LABEL_KIND;
        parsed_line->as.label.label_name = strdup(opname);

    } else {
        /* Either an op or a jump */

        /* Get info */
        uint8_t op;
        const opinfo *info = opname_to_opcode_info(opname, &op);

        if (info->is_jump) {
            parsed_line->kind = JUMP_KIND;
            parsed_line->as.jump.opcode = op;

            /* See if there an immediate arg left on the line to be put into bytecode */
            bool should_get_arg = true;
            for (;;) {
                char *arg = strtok_r(NULL, " ", &saveptr);
                if (!arg && should_get_arg) {
                    fprintf(stderr, "Not enough arguments supplied: %s\n", raw_line);
                    exit(EXIT_FAILURE);
                } else if (arg && !should_get_arg) {
                    fprintf(stderr, "Too many arguments supplied: %s\n", raw_line);
                    exit(EXIT_FAILURE);
                } else if (!arg && !should_get_arg) {
                    /* This is fine */
                    break;
                }
                parse_jump_argument(parsed_line, arg);

                should_get_arg = false;
            }
        } else {
            parsed_line->kind = OP_KIND;
            parsed_line->as.op.opcode = op;
            parsed_line->as.op.has_arg = info->has_arg;

            /* See if there an immediate arg left on the line to be put into bytecode */
            bool should_get_arg = info->has_arg;
            for (;;) {
                char *arg = strtok_r(NULL, " ", &saveptr);
                if (!arg && should_get_arg) {
                    fprintf(stderr, "Not enough arguments supplied: %s\n", raw_line);
                    exit(EXIT_FAILURE);
                } else if (arg && !should_get_arg) {
                    fprintf(stderr, "Too many arguments supplied: %s\n", raw_line);
                    exit(EXIT_FAILURE);
                } else if (!arg && !should_get_arg) {
                    /* This is fine */
                    break;
                }

                parse_op_argument(parsed_line, arg);

                should_get_arg = false;
            }
        }
    }

    return parsed_line;
}

static void collect_labelinfo(asm_line *line_list, labelinfo **list)
{
    size_t pc = 0;
    for (asm_line *line = line_list; line; line = line->next) {
        switch (line->kind) {
        case OP_KIND:{
            pc++;
            if (line->as.op.has_arg)
                pc += 2;
            break;
        }
        case LABEL_KIND:{
            labelinfo * label = calloc(sizeof(*label), 1);
            label->name = line->as.label.label_name;
            label->address = pc;
            *list = label;
            list = &label->next;
            break;
        }
        case JUMP_KIND:{
            pc += 3;
            break;
        }
        default:{
            fprintf(stderr, "Unknown kind: %d\n", line->kind);;
            exit(EXIT_FAILURE);
        }
        }
    }
}

static void resolve_jumps(asm_line *line_list, labelinfo *labelinfo_list)
{
    /* Look for jumps and get label addresses for 'em */
    for (asm_line *line = line_list; line; line = line->next) {
        if (line->kind != JUMP_KIND)
            continue;

        /* only resolve jumps that have labels as addresses */
        if (line->as.jump.label_name == NULL)
            continue;

        /* Search through the list */
        char *target_label_name = line->as.jump.label_name;
        labelinfo *target_info = NULL;
        for (target_info = labelinfo_list; target_info; target_info = target_info->next)
            if (strcmp(target_label_name, target_info->name) == 0)
                break;

        /* Did not find anything, fail */
        if (!target_info) {
            fprintf(stderr, "Cannot resolve a label: %s\n", target_label_name);
            exit(EXIT_FAILURE);
        }

        line->as.jump.target_address = target_info->address;
    }
}

static size_t assemble_line(asm_line *line, uint8_t *bytecode, size_t pc)
{
    switch (line->kind) {
    case OP_KIND:{
        bytecode[pc++] = line->as.op.opcode;
        if (line->as.op.has_arg) {
            uint16_t arg = line->as.op.arg;
            bytecode[pc++] = (arg & 0xff00) >> 8;
            bytecode[pc++] = (arg & 0x00ff);
        }
        break;
    }
    case LABEL_KIND:{
        break;
    }
    case JUMP_KIND:{
        bytecode[pc++] = line->as.jump.opcode;
        uint16_t target = line->as.jump.target_address;
        bytecode[pc++] = (target & 0xff00) >> 8;
        bytecode[pc++] = (target & 0x00ff);
        break;
    }
    default:{
        fprintf(stderr, "Unknown op kind: %d\n", line->kind);
        exit(EXIT_FAILURE);
    }
    }
    return pc;
}

static void assemble(const char *path, uint8_t *bytecode, size_t *bytecode_len)
{
    asm_line *lines = NULL;

    /* Parse the flie */
    {
        FILE *file = fopen(path, "r");
        if (file == NULL) {
            fprintf(stderr, "File does not exist: %s\n", path);
            exit(EXIT_FAILURE);
        }

        /* Parse lines */
        char line_buf[MAX_LINE_LEN];
        asm_line **list_tail = &lines;
        while (fgets(line_buf, MAX_LINE_LEN, file)) {
            asm_line *line = parse_line(line_buf);
            if (!line)
                continue;
            *list_tail = line;
            list_tail = &line->next;
        }
        fclose(file);
    }

    /* Collect label addresses */
    {
        labelinfo *labelinfo_list = NULL;
        collect_labelinfo(lines, &labelinfo_list);

        /* Add proper addresses to jumps */
        resolve_jumps(lines, labelinfo_list);
    }

    /* Compile lines to bytecode*/
    {
        size_t pc = 0;
        for (asm_line *line = lines; line; line = line->next)
            pc = assemble_line(line, bytecode, pc);
        *bytecode_len = pc;
    }
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
        fprintf(stderr, "Usage: <asm|dis|run|runtimes> [arg1 [arg2 ...]]\n");
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

        TIMER_DEF(start_time, end_time);
        TIMER_START(start_time);

        res = run(bytecode);

        TIMER_END(start_time, end_time, "code finished");

        free(bytecode);
    } else if (0 == strcmp(cmd, "runtimes")) {
        if (argc != 4) {
            fprintf(stderr, "Usage: runtimes <path/to/bytecode> <number of iterations>\n");
            exit(EXIT_FAILURE);
        }

        const char *path = argv[2];
        uint8_t *bytecode = read_file(path);

        int num_iterations = 0;
        if (sscanf(argv[3], "%d", &num_iterations) != 1) {
            fprintf(stderr, "Failed to parse number of iterations: %s\n", argv[3]);
            exit(EXIT_FAILURE);
        };

        TIMER_DEF(start_time, end_time);
        TIMER_START(start_time);

        for (int i = 0; i < num_iterations; i++)
            res = run(bytecode);

        TIMER_END(start_time, end_time, "code finished");

        free(bytecode);
    } else if (0 == strcmp(cmd, "asm")) {
        if (argc != 4) {
            fprintf(stderr, "Usage: asm <path/to/asm> <path/to/output/bytecode>\n");
            exit(EXIT_FAILURE);
        }

        const char *input_path = argv[2];
        const char *output_path = argv[3];

        size_t bytecode_len = 0;
        uint8_t *bytecode = calloc(MAX_CODE_LEN, 1);
        if (!bytecode) {
            fprintf(stderr, "Failed to allocate memory\n");
            exit(EXIT_FAILURE);
        }

        assemble(input_path, bytecode, &bytecode_len);
        write_file(bytecode, bytecode_len, output_path);

        res = EXIT_SUCCESS;
        free(bytecode);
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
        res = EXIT_FAILURE;
    }

    return res;
}
