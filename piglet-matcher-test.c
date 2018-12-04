#include <assert.h>

#include "piglet-matcher.h"

static inline uint32_t make_event(uint32_t event_name, uint32_t event_screen)
{
    return event_screen << 16 | event_name;
}

static inline uint32_t make_start_event(void)
{
    return 0;
}

int main(int argc, char *argv[])
{
#define ENCODE_ARG(arg)                         \
    (((arg) & 0xff00) >> 8), ((arg) & 0x00ff)

    (void) argc; (void) argv;

    /* Match a single event */
    {
        uint8_t bytecode[] = {OP_NEXT, OP_NAME, ENCODE_ARG(1), OP_MATCH};

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_OK == matcher_accept(m, make_event(1, 2)));

        matcher_destroy(m);
    }

    /* Fail to match a single event */
    {
        uint8_t bytecode[] = {OP_NEXT, OP_NAME, ENCODE_ARG(3), OP_MATCH};

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 2)));

        matcher_destroy(m);
    }

    /* Match a single event with a screen*/
    {
        uint8_t bytecode[] = {OP_NEXT, OP_NAME, ENCODE_ARG(1), OP_SCREEN, ENCODE_ARG(2), OP_MATCH};

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_OK == matcher_accept(m, make_event(1, 2)));

        matcher_destroy(m);
    }

    /* Fail to match a single event with a screen*/
    {
        uint8_t bytecode[] = {OP_NEXT, OP_NAME, ENCODE_ARG(1), OP_SCREEN, ENCODE_ARG(2), OP_MATCH};

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));

        matcher_destroy(m);
    }

    /* Match multiple events*/
    {
        uint8_t bytecode[] = {
            OP_NEXT, OP_NAME, ENCODE_ARG(1),
            OP_NEXT, OP_NAME, ENCODE_ARG(2),
            OP_MATCH
        };

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));
        assert(MATCH_OK == matcher_accept(m, make_event(2, 3)));

        matcher_destroy(m);
    }

    /* Match multiple events and fail on the last one*/
    {
        uint8_t bytecode[] = {
            OP_NEXT, OP_NAME, ENCODE_ARG(1),
            OP_NEXT, OP_NAME, ENCODE_ARG(2),
            OP_NEXT, OP_NAME, ENCODE_ARG(3),
            OP_MATCH
        };

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(2, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(4, 3)));

        matcher_destroy(m);
    }

    /* Match an event chain that does not start from the beginning of the stream */
    {
        uint8_t bytecode[] = {
             OP_NEXT, OP_NAME, ENCODE_ARG(1),
             OP_NEXT, OP_NAME, ENCODE_ARG(2),
             OP_NEXT, OP_NAME, ENCODE_ARG(3),
             OP_MATCH
        };

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 9)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 9)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(2, 3)));
        assert(MATCH_OK == matcher_accept(m, make_event(3, 3)));

        matcher_destroy(m);
    }

    /* Match an event chain that does not start from the beginning of the stream, with a half match
     * in the middle */
    {
        uint8_t bytecode[] = {
             OP_NEXT, OP_NAME, ENCODE_ARG(1),
             OP_NEXT, OP_NAME, ENCODE_ARG(2),
             OP_NEXT, OP_NAME, ENCODE_ARG(3),
             OP_MATCH
        };

        matcher * m = matcher_create(bytecode);
        assert(m);

        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 9)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 9)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(2, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(2, 3)));
        assert(MATCH_OK == matcher_accept(m, make_event(3, 3)));

        matcher_destroy(m);
    }

    /* Match either the first event or the second one */
    {
        uint8_t bytecode[] = {
            /* byte position: 0/1,2/3,4 */
            OP_SPLIT, ENCODE_ARG(5), ENCODE_ARG(12),
            /* 5/6,7/8 */
            OP_NEXT, OP_NAME, ENCODE_ARG(1),
            /* 9/10,11 */
            OP_JUMP, ENCODE_ARG(16),
            /* 12/13,14/15 */
            OP_NEXT, OP_NAME, ENCODE_ARG(2),
            /* 16/17,18 */
            OP_NEXT, OP_NAME, ENCODE_ARG(3),
            /* 19 */
            OP_MATCH
        };

        matcher *m = matcher_create(bytecode);
        assert(m);

        /* First branch should match */
        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 9)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(1, 3)));
        assert(MATCH_OK == matcher_accept(m, make_event(3, 3)));

        matcher_reset(m);

        /* 2nd branch should also match */
        assert(MATCH_NEXT == matcher_accept(m, make_start_event()));
        assert(MATCH_NEXT == matcher_accept(m, make_event(9, 9)));
        assert(MATCH_NEXT == matcher_accept(m, make_event(2, 3)));
        assert(MATCH_OK == matcher_accept(m, make_event(3, 3)));


        matcher_destroy(m);
    }

    return 0;

#undef ENCODE_ARG
}
