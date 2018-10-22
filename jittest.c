#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <setjmp.h>

#include <jit/jit.h>
#include <jit/jit-dump.h>

struct {
    uint64_t pc;
    uint64_t stack[5];
} vm = {0, {1, 2, 3, 4, 5}};

jit_type_t longjmp_signature;

void exception_handler(int exception_type)
{
    fprintf(stderr, "handling an exception: %d\n", exception_type);
}

void compile_add_pc(jit_function_t function, uint64_t diff)
{
    jit_value_t pc_ptr = jit_value_get_param(function, 1);
    jit_value_t pc_value = jit_insn_load_relative(function, pc_ptr, 0, jit_type_ulong);
    jit_value_t diff_value = jit_value_create_long_constant(function, jit_type_ulong, diff);
    jit_value_t pc_new = jit_insn_add(function, pc_value, diff_value);

    jit_insn_store_relative(function, pc_ptr, 0, pc_new);
}

void compile_sum(jit_function_t function, uint64_t arg)
{
    jit_value_t l, r, temp;
    l = jit_value_get_param(function, 2);
    r = jit_value_get_param(function, 3);
    temp = jit_insn_add(function, l, r);

    jit_insn_return(function, temp);
}

void compile_push(jit_function_t function, uint64_t arg)
{
    jit_type_t stack_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, stack_ptr_type);

    jit_dump_value(stderr, function, stack_ptr, "\noriginal=");
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, jit_type_get_size(stack_ptr_type));
    jit_dump_value(stderr, function, stack_ptr_moved, "\nmoved=");
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);
}

void compile_exception(jit_function_t function)
{
    jit_value_t l = jit_value_get_param(function, 2);
    jit_value_t r = jit_value_get_param(function, 3);
    jit_value_t temp = jit_insn_div(function, l, r);
    jit_insn_return(function, temp);
}

int main(int argc, char *argv[])
{
    (void) argc; (void) argv;

    jit_type_t pc_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);
    jit_type_t stack_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);
    jit_type_t stack_ptr_ptr_type = jit_type_create_pointer(stack_ptr_type, 1);
    jit_context_t context = jit_context_create();

    jit_context_build_start(context);

    jit_type_t params[] = {stack_ptr_ptr_type, pc_ptr_type, jit_type_ulong, jit_type_ulong};
    jit_type_t signature = jit_type_create_signature(
        jit_abi_cdecl, jit_type_ulong, params, sizeof(params) / sizeof(params[0]), 1
    );
    jit_function_t function = jit_function_create(context, signature);

    compile_add_pc(function, 11);
    compile_push(function, 0);
    compile_push(function, 0);
    compile_push(function, 0);
    compile_exception(function);
    compile_sum(function, 0);

    jit_context_build_end(context);

    jit_dump_function(stderr, function, "uncompiled");
    jit_function_compile(function);

    uint64_t x = 10, y = 0, result = 0;
    uint64_t *pc_ptr = &vm.pc, *stack_ptr = &vm.stack[0];
    uint64_t **stack_ptr_ptr = &stack_ptr;

    jit_exception_set_handler(exception_handler);

    void *args[5] = {&stack_ptr_ptr, &pc_ptr, &x, &y};
    if (jit_function_apply(function, args, &result) == 0) {
        puts("EXCEPTION!");
    };

    printf("result=%" PRIu64 "\n", result);
    printf("pc=%" PRIu64 "\n", vm.pc);
    printf("stack top=%" PRIu64 "\n", *stack_ptr);

    return 0;
}
