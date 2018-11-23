#ifndef PIGLET_MATCHER_H
#define PIGLET_MATCHER_H

#include <inttypes.h>

typedef enum matcher_opcode {
    OP_ABORT,
    OP_EVENT_NAME,
    OP_EVENT_SCREEN,
    OP_NEXT,
    OP_MATCH,
} matcher_opcode;

typedef enum match_result {
    MATCH_NEXT,
    MATCH_FOUND,
    MATCH_ERROR,
} match_result;

typedef struct matcher {
    uint8_t *bytecode;
} matcher;

matcher *matcher_create(uint8_t *bytecode);

void matcher_reset(void);

match_result matcher_accept(uint32_t event);

void matcher_destroy(matcher *matcher);

#endif //PIGLET_MATCHER_H
