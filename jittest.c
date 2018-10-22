#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <jit/jit.h>

void build_sum_function(jit_function_t function)
{
    jit_value_t l, r, temp;
    l = jit_value_get_param(function, 0);
    r = jit_value_get_param(function, 1);
    temp = jit_insn_add(function, l, r);
    jit_insn_return(function, temp);
 }

int main(int argc, char *argv[])
{
    (void) argc; (void) argv;

    jit_context_t context = jit_context_create();

    jit_type_t params[2] = {jit_type_ulong, jit_type_ulong};
    jit_type_t signature = jit_type_create_signature(jit_abi_cdecl, jit_type_ulong, params, 2, 1);

    jit_context_build_start(context);

    jit_function_t function = jit_function_create(context, signature);

    build_sum_function(function);

    jit_function_compile(function);
    jit_context_build_end(context);

    uint64_t x = 3, y = 10, result;
    void *args[2] = {&x, &y};
    jit_function_apply(function, args, &result);

    printf("%" PRIu64 "\n", result);

    return 0;
}
