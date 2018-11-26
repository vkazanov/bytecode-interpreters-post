#include "piglet-matcher.h"

#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

matcher *matcher_create(uint8_t *bytecode)
{
    matcher * m = malloc(sizeof(*m));
    if (!m) return NULL;

    *m = (typeof(*m)) {
        .bytecode = bytecode,

        .current_threads = { 0 },
        .current_thread_num = 0,

        .next_threads = { 0 },
        .next_thread_num = 0,
    };

    return m;
}

static inline uint32_t event_name(uint32_t event)
{
    return event & 0xff;
}

static inline uint32_t event_screen(uint32_t event)
{
    return event >> 16 & 0xff;
}

static inline void add_current_thread(matcher *m, matcher_thread thread)
{
    m->current_threads[m->current_thread_num] = thread;
    m->current_thread_num++;
}

static inline void add_next_thread(matcher *m, matcher_thread thread)
{
    m->next_threads[m->next_thread_num] = thread;
    m->next_thread_num++;
}

static inline void swap_current_and_next(matcher *m)
{
    m->current_thread_num = m->next_thread_num;
    for (size_t thread_i = 0; thread_i < m->next_thread_num; thread_i++ )
        m->current_threads[thread_i] = m->next_threads[thread_i];
    m->next_thread_num = 0;
}

static inline matcher_thread initial_thread(matcher *m)
{
    return (matcher_thread){ .ip = &m->bytecode[0]};
}

match_result matcher_accept(matcher *m, uint32_t next_event)
{
#define NEXT_OP(thread)                         \
    (*(thread).ip++)
#define NEXT_ARG(thread)                                                \
    ((void)((thread).ip += 2), ((thread).ip[-2] << 8) + (thread).ip[-1])

    /* On every event initiate a new thread */
    add_current_thread(m, initial_thread(m));

    for (size_t thread_i = 0; thread_i < m->current_thread_num; thread_i++ ) {
        matcher_thread current_thread = m->current_threads[thread_i];

        bool thread_done = false;
        while (!thread_done) {
            uint8_t instruction = NEXT_OP(current_thread);
            switch (instruction) {
            case OP_ABORT:{
                return MATCH_ERROR;
            }
            case OP_NAME:{
                uint16_t name = NEXT_ARG(current_thread);
                if (event_name(next_event) != name)
                    thread_done = true;
                break;
            }
            case OP_SCREEN:{
                uint16_t screen = NEXT_ARG(current_thread);
                if (event_screen(next_event) != screen)
                    thread_done = true;
                break;
            }
            case OP_NEXT:{
                add_next_thread(m, current_thread);
                thread_done = true;
                break;
            }
            case OP_MATCH:{
                return MATCH_OK;
            }
            default:{
                return MATCH_ERROR;
            }
            }
        }
    }

    /* Prepare for the next event - swap current and next thread lists */
    swap_current_and_next(m);

    return MATCH_NEXT;

#undef NEXT_OP
#undef PEEK_ARG
}

void matcher_destroy(matcher *m)
{
    free(m);
}
