#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "piglet-matcher.h"

#define MAX_LINE_LEN 256
#define MAX_CODE_LEN 4096
#define MAX_EVENT_LEN 4096

/* static char *error_to_msg[] = { */
/*     [SUCCESS] = "success", */
/*     [ERROR_DIVISION_BY_ZERO] = "division by zero", */
/*     [ERROR_UNKNOWN_OPCODE] = "unknown opcode", */
/*     [ERROR_END_OF_STREAM] = "end of stream", */
/* }; */

typedef struct opinfo {
    bool has_arg;
    char *name;
    bool is_jump;
    bool is_split;
} opinfo;

opinfo opcode_to_disinfo[] = {
    [OP_ABORT] = {0, "ABORT", 0, 0},
    [OP_NAME] = {1, "NAME", 0, 0},
    [OP_SCREEN] = {1, "SCREEN", 0, 0},
    [OP_NEXT] = {0, "NEXT", 0, 0},
    [OP_JUMP] = {0, "JUMP", 1, 0},
    [OP_SPLIT] = {0, "SPLIT", 0, 1},
    [OP_MATCH] = {0, "MATCH", 0, 0},
};

typedef struct labelinfo {
    char *name;
    uint16_t address;
    struct labelinfo *next;
} labelinfo;

typedef struct asm_line {
    enum kind {OP_KIND, JUMP_KIND, SPLIT_KIND, LABEL_KIND} kind;
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
        /* a jump op, with 2 labels to jump to */
        struct {
            uint8_t opcode;
            char *left_label_name;
            uint16_t left_target_address;
            char *right_label_name;
            uint16_t right_target_address;
        } split;
        /* a label to be used in jumps */
        struct {
            char *label_name;
        } label;
    } as;

    /* a list of lines */
    struct asm_line *next;
} asm_line;

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
    case SPLIT_KIND:{
        bytecode[pc++] = line->as.split.opcode;

        uint16_t left_target = line->as.split.left_target_address;
        bytecode[pc++] = (left_target & 0xff00) >> 8;
        bytecode[pc++] = (left_target & 0x00ff);

        uint16_t right_target = line->as.split.right_target_address;
        bytecode[pc++] = (right_target & 0xff00) >> 8;
        bytecode[pc++] = (right_target & 0x00ff);
        break;
    }
    default:{
        fprintf(stderr, "Unknown op kind: %d\n", line->kind);
        exit(EXIT_FAILURE);
    }
    }
    return pc;
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

static void parse_split_argument(asm_line *parsed_line, char **args_raw)
{
    {
        char *arg = calloc(strlen(args_raw[1]), 1);
        strip_line(args_raw[1], arg);
        if (is_label_name(arg)) {
            parsed_line->as.split.left_label_name = strdup(arg);
        } else {
            uint16_t arg_val = 0;
            if (sscanf(arg, "%" SCNu16, &arg_val) != 1) {
                fprintf(stderr, "Invalid address supplied: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            parsed_line->as.split.left_target_address = arg_val;
        }
    }

    {
        char *arg = calloc(strlen(args_raw[0]), 1);
        strip_line(args_raw[0], arg);
        if (is_label_name(arg)) {
            parsed_line->as.split.right_label_name = strdup(arg);
        } else {
            uint16_t arg_val = 0;
            if (sscanf(arg, "%" SCNu16, &arg_val) != 1) {
                fprintf(stderr, "Invalid address supplied: %s\n", arg);
                exit(EXIT_FAILURE);
            }
            parsed_line->as.split.right_target_address = arg_val;
        }

    }
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
        } else if (info->is_split) {
            parsed_line->kind = SPLIT_KIND;
            parsed_line->as.split.opcode = op;

            /* See if there an immediate arg left on the line to be put into bytecode */
            int args_left = 2;
            char *raw_args[2] = {0};
            for (;;) {
                char *arg = strtok_r(NULL, " ", &saveptr);
                if (!arg && args_left) {
                    fprintf(stderr, "Not enough arguments supplied: %s\n", raw_line);
                    exit(EXIT_FAILURE);
                } else if (arg && !args_left) {
                    fprintf(stderr, "Too many arguments supplied: %s\n", raw_line);
                    exit(EXIT_FAILURE);
                } else if (!arg && !args_left) {
                    /* This is fine */
                    break;
                }
                args_left--;
                raw_args[args_left] = arg;
            }

            parse_split_argument(parsed_line, raw_args);
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
        case SPLIT_KIND:{
            pc += 5;
            break;
        }
        default:{
            fprintf(stderr, "Unknown kind: %d\n", line->kind);;
            exit(EXIT_FAILURE);
        }
        }
    }
}

static labelinfo *find_labelinfo(char *target_label_name, labelinfo *labelinfo_list)
{
    labelinfo *target_info = NULL;
    for (target_info = labelinfo_list; target_info; target_info = target_info->next)
        if (strcmp(target_label_name, target_info->name) == 0)
            break;

    if (!target_info) {
        fprintf(stderr, "Cannot resolve a label: %s\n", target_label_name);
        exit(EXIT_FAILURE);
    }

    return target_info;
}

static void resolve_jumps(asm_line *line_list, labelinfo *labelinfo_list)
{
    /* Look for jumps and get label addresses for 'em */
    /* NOTE: only resolve jumps that have labels as addresses */
    for (asm_line *line = line_list; line; line = line->next) {
        if (line->kind == JUMP_KIND) {
            if (line->as.jump.label_name) {
                labelinfo *target_info =
                    find_labelinfo(line->as.jump.label_name, labelinfo_list);
                line->as.jump.target_address = target_info->address;
            }
        } else if (line->kind == SPLIT_KIND) {
            if (line->as.split.left_label_name) {
                labelinfo *target_info =
                    find_labelinfo(line->as.split.left_label_name,
                                   labelinfo_list);
                line->as.split.left_target_address = target_info->address;
            }
            if (line->as.split.right_label_name) {
                labelinfo *target_info =
                    find_labelinfo(line->as.split.right_label_name,
                                   labelinfo_list);
                line->as.split.right_target_address = target_info->address;
            }

        }
    }
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

static size_t print_instruction(uint8_t *bytecode, size_t offset)
{
    printf("%zu ", offset);
    uint8_t op = bytecode[offset++];
    char *op_name = opcode_to_disinfo[op].name;

    bool has_arg = opcode_to_disinfo[op].has_arg;
    bool is_jump = opcode_to_disinfo[op].is_jump;
    bool is_split = opcode_to_disinfo[op].is_split;

    printf("%s", op_name);
    if (has_arg || is_jump) {
        uint16_t arg = bytecode[offset++] << 8;
        arg += bytecode[offset++];
        printf(" %" PRIu16, arg);
    }  else if (is_split) {
        uint16_t left_arg = bytecode[offset++] << 8;
        left_arg += bytecode[offset++];

        uint16_t right_arg = bytecode[offset++] << 8;
        right_arg += bytecode[offset++];

        printf(" %" PRIu16 " %" PRIu16, left_arg, right_arg);
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

static uint8_t *read_bytecode_file(const char *path)
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

static inline uint32_t make_event(uint32_t event_name, uint32_t event_screen)
{
    return event_screen << 16 | event_name;
}

static bool parse_event_line(char *raw_line, uint32_t *event)
{
    /* Ignore comments and empty lines*/
    if (raw_line[0] == '#' || raw_line[0] == '\n')
        return false;

    fprintf(stderr, "DEBUG: parsing \"%s\"\n", raw_line);

    uint32_t event_name = 0;
    uint32_t screen_name = 0;
    if (sscanf(raw_line, "%" SCNu32 " %" SCNu32, &event_name, &screen_name) != 2)
        return 0;

    fprintf(stderr, "DEBUG: parsed e=%u s=%u\n", event_name, screen_name);

    *event = make_event(event_name, screen_name);

    return true;
}

static uint32_t *read_events_file(const char *path, size_t *event_num)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "File does not exist: %s\n", path);
        exit(EXIT_FAILURE);
    }

    uint32_t *events = calloc(MAX_EVENT_LEN, sizeof(uint32_t));
    if (!events) {
        fprintf(stderr, "Memory allocation failure: %s\n", path);
        exit(EXIT_FAILURE);
    }

    size_t event_i = 0;
    char line_buf[MAX_LINE_LEN];
    while (fgets(line_buf, MAX_LINE_LEN, file)) {
        uint32_t event = 0;
        if (parse_event_line(line_buf, &event)) {
            events[event_i] = event;
            event_i++;
        }
    }
    *event_num = event_i;

    fclose(file);

    return events;
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

static bool match_events(uint8_t *bytecode, uint32_t *events, size_t event_num)
{
    bool res = false;
    matcher *m = matcher_create(bytecode);

    for (size_t e_i = 0; e_i < event_num; e_i++) {
        uint32_t event = events[e_i];
        match_result mresult = matcher_accept(m, event);
        if (mresult == MATCH_OK) {
            res = true;
            break;
        } else if (mresult == MATCH_ERROR) {
            fprintf(stderr, "Match error, abort\n");
            break;
        } else {
            /* MATCH_NEXT, i.e. continue */
        }
    }

    matcher_destroy(m);
    return res;
}

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
    } else if (0 == strcmp(cmd, "run")) {
        if (argc != 4) {
            fprintf(stderr, "Usage: run <path/to/bytecode> <path/to/input>\n");
            exit(EXIT_FAILURE);
        }

        const char *bytecode_path = argv[2];
        uint8_t *bytecode = read_bytecode_file(bytecode_path);

        const char *event_path = argv[3];
        size_t event_num = 0;
        uint32_t *events = read_events_file(event_path, &event_num);

        if (match_events(bytecode, events, event_num)) {
            fprintf(stdout, "MATCHED\n");
            res = EXIT_SUCCESS;
        } else {
            fprintf(stdout, "NO MATCH\n");
            res = EXIT_FAILURE;
        }

        free(bytecode);
        free(events);
    } else if (0 == strcmp(cmd, "dis")) {
        if (argc != 3) {
            fprintf(stderr, "Usage: dis <path/to/bytecode>\n");
            exit(EXIT_FAILURE);
        }
        const char *path = argv[2];
        uint8_t *bytecode = read_bytecode_file(path);

        res = disassemble(bytecode);

        free(bytecode);
    } else {
        fprintf(stderr, "Unknown cmd: %s\n", cmd);;
        res = EXIT_FAILURE;

    }

    return res;
}
