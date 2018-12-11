#ifndef PIGLET_MATCHER_H
#define PIGLET_MATCHER_H

#include <inttypes.h>

#define MAX_THREAD_NUM 256

typedef enum matcher_opcode {
    OP_ABORT,
    OP_NAME,
    OP_SCREEN,
    OP_NEXT,
    OP_JUMP,
    OP_SPLIT,
    OP_MATCH,
    OP_NUMBER_OF_OPS,
} matcher_opcode;

typedef enum match_result {
    MATCH_NEXT,
    MATCH_OK,
    MATCH_ERROR,
} match_result;

typedef struct matcher_thread {
    uint8_t *ip;
} matcher_thread;

typedef struct matcher {
    uint8_t *bytecode;

    /* Threads to be processed using the current event */
    matcher_thread current_threads[MAX_THREAD_NUM];
    uint8_t current_thread_num;

    /* Threads to be processed using the event to follow */
    matcher_thread next_threads[MAX_THREAD_NUM];
    uint8_t next_thread_num;

} matcher;

matcher *matcher_create(uint8_t *bytecode);

match_result matcher_accept(matcher *m, uint32_t event);

void matcher_reset(matcher *m);

void matcher_destroy(matcher *matcher);

#endif //PIGLET_MATCHER_H
