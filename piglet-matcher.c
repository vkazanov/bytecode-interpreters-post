#include "piglet-matcher.h"

#include <stdlib.h>
#include <assert.h>

matcher *matcher_create(uint8_t *bytecode)
{
    matcher * m = malloc(sizeof(*m));
    if (!m)
        return NULL;

    *m = (typeof(*m)) {
        .bytecode = bytecode
    };

    return m;
}

void matcher_reset(void)
{

}

match_result matcher_accept(uint32_t event)
{
    return MATCH_NEXT;
}

void matcher_destroy(matcher *m)
{
    free(m);
}
