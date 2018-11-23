#include "piglet-matcher.h"

#include <stdlib.h>
#include <assert.h>

matcher *matcher_create(uint8_t *bytecode)
{
    matcher * m = malloc(sizeof(*m));
    if (!m)
        return NULL;

    *m = (typeof(*m)) {
        .bytecode = bytecode,
        .ip = &bytecode[0]
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

match_result matcher_accept(matcher *m, uint32_t next_event)
{
#define NEXT_OP()                               \
    (*m->ip++)
#define NEXT_ARG()                              \
    ((void)(m->ip += 2), (m->ip[-2] << 8) + m->ip[-1])

    for (;;) {
        uint8_t instruction = NEXT_OP();
        switch (instruction) {
        case OP_ABORT:{
            return MATCH_ERROR;
        }
        case OP_NAME:{
            uint16_t name = NEXT_ARG();
            if (event_name(next_event) != name)
                return MATCH_FAIL;
            break;
        }
        case OP_SCREEN:{
            uint16_t screen = NEXT_ARG();
            if (event_screen(next_event) != screen)
                return MATCH_FAIL;
            break;
        }
        case OP_NEXT:{
            return MATCH_NEXT;
        }
        case OP_MATCH:{
            return MATCH_OK;
        }
        default:{
            return MATCH_ERROR;
        }
        }
    }

#undef NEXT_OP
#undef PEEK_ARG
}

void matcher_destroy(matcher *m)
{
    free(m);
}
