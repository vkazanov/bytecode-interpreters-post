#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>

#include "pigletvm.h"

#define MAX_TRACE_LEN 16
#define STACK_MAX 256
#define MEMORY_SIZE 65536

#define NEXT_OP()                               \
    (*vm.ip++)
#define NEXT_ARG()                                      \
    ((void)(vm.ip += 2), (vm.ip[-2] << 8) + vm.ip[-1])
#define PEEK_ARG()                              \
    ((vm.ip[0] << 8) + vm.ip[1])
#define POP()                                   \
    (*(--vm.stack_top))
#define PUSH(val)                               \
    (*vm.stack_top = (val), vm.stack_top++)
#define PEEK()                                  \
    (*(vm.stack_top - 1))
#define TOS_PTR()                               \
    (vm.stack_top - 1)


/*
 * switch or threaded vm
 * */

static struct {
    /* Current instruction pointer */
    uint8_t *ip;

    /* Fixed-size stack */
    uint64_t stack[STACK_MAX];
    uint64_t *stack_top;

    /* Operational memory */
    uint64_t memory[MEMORY_SIZE];

    /* A single register containing the result */
    uint64_t result;
} vm;

static void vm_reset(uint8_t *bytecode)
{
    vm = (typeof(vm)) {
        .stack_top = vm.stack,
        .ip = bytecode
    };
}

interpret_result vm_interpret(uint8_t *bytecode)
{
    vm_reset(bytecode);

    for (;;) {
        uint8_t instruction = NEXT_OP();
        switch (instruction) {
        case OP_PUSHI: {
            /* get the argument, push it onto stack */
            uint16_t arg = NEXT_ARG();
            PUSH(arg);
            break;
        }
        case OP_LOADI: {
            /* get the argument, use it to get a value onto stack */
            uint16_t addr = NEXT_ARG();
            uint64_t val = vm.memory[addr];
            PUSH(val);
            break;
        }
        case OP_LOADADDI: {
            /* get the argument, add the value from the address to the top of the stack */
            uint16_t addr = NEXT_ARG();
            uint64_t val = vm.memory[addr];
            *TOS_PTR() += val;
            break;
        }
        case OP_STOREI: {
            /* get the argument, use it to get a value of the stack into a memory cell */
            uint16_t addr = NEXT_ARG();
            uint64_t val = POP();
            vm.memory[addr] = val;
            break;
        }
        case OP_LOAD: {
            /* pop an address, use it to get a value onto stack */
            uint16_t addr = POP();
            uint64_t val = vm.memory[addr];
            PUSH(val);
            break;
        }
        case OP_STORE: {
            /* pop a value, pop an adress, put a value into an address */
            uint64_t val = POP();
            uint16_t addr = POP();
            vm.memory[addr] = val;
            break;
        }
        case OP_DUP:{
            /* duplicate the top of the stack */
            PUSH(PEEK());
            break;
        }
        case OP_DISCARD: {
            /* discard the top of the stack */
            (void)POP();
            break;
        }
        case OP_ADD: {
            /* Pop 2 values, add 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() += arg_right;
            break;
        }
        case OP_ADDI: {
            /* Add immediate value to the top of the stack */
            uint16_t arg_right = NEXT_ARG();
            *TOS_PTR() += arg_right;
            break;
        }
        case OP_SUB: {
            /* Pop 2 values, subtract 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() -= arg_right;
            break;
        }
        case OP_DIV: {
            /* Pop 2 values, divide 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            /* Don't forget to handle the div by zero error */
            if (arg_right == 0)
                return ERROR_DIVISION_BY_ZERO;
            *TOS_PTR() /= arg_right;
            break;
        }
        case OP_MUL: {
            /* Pop 2 values, multiply 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() *= arg_right;
            break;
        }
        case OP_JUMP:{
            /* Use arg as a jump target  */
            uint16_t target = PEEK_ARG();
            vm.ip = bytecode + target;
            break;
        }
        case OP_JUMP_IF_TRUE:{
            /* Use arg as a jump target  */
            uint16_t target = NEXT_ARG();
            if (POP())
                vm.ip = bytecode + target;
            break;
        }
        case OP_JUMP_IF_FALSE:{
            /* Use arg as a jump target  */
            uint16_t target = NEXT_ARG();
            if (!POP())
                vm.ip = bytecode + target;
            break;
        }
        case OP_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() == arg_right;
            break;
        }
        case OP_LESS:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() < arg_right;
            break;
        }
        case OP_LESS_OR_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() <= arg_right;
            break;
        }
        case OP_GREATER:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() > arg_right;
            break;
        }
        case OP_GREATER_OR_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() >= arg_right;
            break;
        }
        case OP_GREATER_OR_EQUALI:{
            uint64_t arg_right = NEXT_ARG();
            *TOS_PTR() = PEEK() >= arg_right;
            break;
        }
        case OP_POP_RES: {
            /* Pop the top of the stack, set it as a result value */
            uint64_t res = POP();
            vm.result = res;
            break;
        }
        case OP_DONE: {
            return SUCCESS;
        }
        case OP_PRINT:{
            uint64_t arg = POP();
            printf("%" PRIu64 "\n", arg);
            break;
        }
        case OP_ABORT: {
            return ERROR_END_OF_STREAM;
        }
        default:
            return ERROR_UNKNOWN_OPCODE;
        }
    }

    return ERROR_END_OF_STREAM;
}

interpret_result vm_interpret_no_range_check(uint8_t *bytecode)
{
    vm_reset(bytecode);

    for (;;) {
        uint8_t instruction = NEXT_OP();
        switch (instruction & 0x1f) {
        case OP_PUSHI: {
            /* get the argument, push it onto stack */
            uint16_t arg = NEXT_ARG();
            PUSH(arg);
            break;
        }
        case OP_LOADI: {
            /* get the argument, use it to get a value onto stack */
            uint16_t addr = NEXT_ARG();
            uint64_t val = vm.memory[addr];
            PUSH(val);
            break;
        }
        case OP_LOADADDI: {
            /* get the argument, add the value from the address to the top of the stack */
            uint16_t addr = NEXT_ARG();
            uint64_t val = vm.memory[addr];
            *TOS_PTR() += val;
            break;
        }
        case OP_STOREI: {
            /* get the argument, use it to get a value of the stack into a memory cell */
            uint16_t addr = NEXT_ARG();
            uint64_t val = POP();
            vm.memory[addr] = val;
            break;
        }
        case OP_LOAD: {
            /* pop an address, use it to get a value onto stack */
            uint16_t addr = POP();
            uint64_t val = vm.memory[addr];
            PUSH(val);
            break;
        }
        case OP_STORE: {
            /* pop a value, pop an address, put a value into an address */
            uint64_t val = POP();
            uint16_t addr = POP();
            vm.memory[addr] = val;
            break;
        }
        case OP_DUP:{
            /* duplicate the top of the stack */
            PUSH(PEEK());
            break;
        }
        case OP_DISCARD: {
            /* discard the top of the stack */
            (void)POP();
            break;
        }
        case OP_ADD: {
            /* Pop 2 values, add 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() += arg_right;
            break;
        }
        case OP_ADDI: {
            /* Add immediate value to the top of the stack */
            uint16_t arg_right = NEXT_ARG();
            *TOS_PTR() += arg_right;
            break;
        }
        case OP_SUB: {
            /* Pop 2 values, subtract 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() -= arg_right;
            break;
        }
        case OP_DIV: {
            /* Pop 2 values, divide 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            /* Don't forget to handle the div by zero error */
            if (arg_right == 0)
                return ERROR_DIVISION_BY_ZERO;
            *TOS_PTR() /= arg_right;
            break;
        }
        case OP_MUL: {
            /* Pop 2 values, multiply 'em, push the result back to the stack */
            uint64_t arg_right = POP();
            *TOS_PTR() *= arg_right;
            break;
        }
        case OP_JUMP:{
            /* Use arg as a jump target  */
            uint16_t target = PEEK_ARG();
            vm.ip = bytecode + target;
            break;
        }
        case OP_JUMP_IF_TRUE:{
            /* Use arg as a jump target  */
            uint16_t target = NEXT_ARG();
            if (POP())
                vm.ip = bytecode + target;
            break;
        }
        case OP_JUMP_IF_FALSE:{
            /* Use arg as a jump target  */
            uint16_t target = NEXT_ARG();
            if (!POP())
                vm.ip = bytecode + target;
            break;
        }
        case OP_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() == arg_right;
            break;
        }
        case OP_LESS:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() < arg_right;
            break;
        }
        case OP_LESS_OR_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() <= arg_right;
            break;
        }
        case OP_GREATER:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() > arg_right;
            break;
        }
        case OP_GREATER_OR_EQUAL:{
            uint64_t arg_right = POP();
            *TOS_PTR() = PEEK() >= arg_right;
            break;
        }
        case OP_GREATER_OR_EQUALI:{
            uint64_t arg_right = NEXT_ARG();
            *TOS_PTR() = PEEK() >= arg_right;
            break;
        }
        case OP_POP_RES: {
            /* Pop the top of the stack, set it as a result value */
            uint64_t res = POP();
            vm.result = res;
            break;
        }
        case OP_DONE: {
            return SUCCESS;
        }
        case OP_PRINT:{
            uint64_t arg = POP();
            printf("%" PRIu64 "\n", arg);
            break;
        }
        case OP_ABORT: {
            return ERROR_END_OF_STREAM;
        }
        case 26 ... 0x1f:
            return ERROR_UNKNOWN_OPCODE;
        }
    }

    return ERROR_END_OF_STREAM;
}

interpret_result vm_interpret_threaded(uint8_t *bytecode)
{
    vm_reset(bytecode);

    const void *labels[] = {
        [OP_PUSHI] = &&op_pushi,
        [OP_LOADI] = &&op_loadi,
        [OP_LOADADDI] = &&op_loadaddi,
        [OP_STORE] = &&op_store,
        [OP_STOREI] = &&op_storei,
        [OP_LOAD] = &&op_load,
        [OP_DUP] = &&op_dup,
        [OP_DISCARD] = &&op_discard,
        [OP_ADD] = &&op_add,
        [OP_ADDI] = &&op_addi,
        [OP_SUB] = &&op_sub,
        [OP_DIV] = &&op_div,
        [OP_MUL] = &&op_mul,
        [OP_JUMP] = &&op_jump,
        [OP_JUMP_IF_TRUE] = &&op_jump_if_true,
        [OP_JUMP_IF_FALSE] = &&op_jump_if_false,
        [OP_EQUAL] = &&op_equal,
        [OP_LESS] = &&op_less,
        [OP_LESS_OR_EQUAL] = &&op_less_or_equal,
        [OP_GREATER] = &&op_greater,
        [OP_GREATER_OR_EQUAL] = &&op_greater_or_equal,
        [OP_GREATER_OR_EQUALI] = &&op_greater_or_equali,
        [OP_POP_RES] = &&op_pop_res,
        [OP_DONE] = &&op_done,
        [OP_PRINT] = &&op_print,
        [OP_ABORT] = &&op_abort,
    };

    goto *labels[NEXT_OP()];

op_pushi: {
        /* get the argument, push it onto stack */
        uint16_t arg = NEXT_ARG();
        PUSH(arg);
        goto *labels[NEXT_OP()];
    }
op_loadi: {
    /* get the argument, use it to get a value onto stack */
        uint16_t addr = NEXT_ARG();
        uint64_t val = vm.memory[addr];
        PUSH(val);
        goto *labels[NEXT_OP()];
    }
op_loadaddi: {
        /* get the argument, add the value from the address to the top of the stack */
        uint16_t addr = NEXT_ARG();
        uint64_t val = vm.memory[addr];
        *TOS_PTR() += val;
        goto *labels[NEXT_OP()];
    }
op_storei: {
        /* get the argument, use it to get a value of the stack into a memory cell */
        uint16_t addr = NEXT_ARG();
        uint64_t val = POP();
        vm.memory[addr] = val;
        goto *labels[NEXT_OP()];
    }
op_load: {
        /* pop an address, use it to get a value onto stack */
        uint16_t addr = POP();
        uint64_t val = vm.memory[addr];
        PUSH(val);
        goto *labels[NEXT_OP()];
    }
op_store: {
        /* pop a value, pop an adress, put a value into an address */
        uint64_t val = POP();
        uint16_t addr = POP();
        vm.memory[addr] = val;
        goto *labels[NEXT_OP()];
    }
op_dup:{
        /* duplicate the top of the stack */
        PUSH(PEEK());
        goto *labels[NEXT_OP()];
    }
op_discard: {
        /* discard the top of the stack */
        (void)POP();
        goto *labels[NEXT_OP()];
    }
op_add: {
        /* Pop 2 values, add 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        *TOS_PTR() += arg_right;
        goto *labels[NEXT_OP()];
    }
op_addi: {
        /* Add immediate value to the top of the stack */
        uint16_t arg_right = NEXT_ARG();
        *TOS_PTR() += arg_right;
        goto *labels[NEXT_OP()];
    }
op_sub: {
        /* Pop 2 values, subtract 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        *TOS_PTR() -= arg_right;
        goto *labels[NEXT_OP()];
    }
op_div: {
        /* Pop 2 values, divide 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        /* Don't forget to handle the div by zero error */
        if (arg_right == 0)
            return ERROR_DIVISION_BY_ZERO;
        *TOS_PTR() /= arg_right;
        goto *labels[NEXT_OP()];
    }
op_mul: {
        /* Pop 2 values, multiply 'em, push the result back to the stack */
        uint64_t arg_right = POP();
        *TOS_PTR() *= arg_right;
        goto *labels[NEXT_OP()];
    }
op_jump:{
        /* Use arg as a jump target  */
        uint16_t target = PEEK_ARG();
        vm.ip = bytecode + target;
        goto *labels[NEXT_OP()];
    }
op_jump_if_true:{
        /* Use arg as a jump target  */
        uint16_t target = NEXT_ARG();
        if (POP())
            vm.ip = bytecode + target;
        goto *labels[NEXT_OP()];
    }
op_jump_if_false:{
        /* Use arg as a jump target  */
        uint16_t target = NEXT_ARG();
        if (!POP())
            vm.ip = bytecode + target;
        goto *labels[NEXT_OP()];
    }
op_equal:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() == arg_right;
        goto *labels[NEXT_OP()];
    }
op_less:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() < arg_right;
        goto *labels[NEXT_OP()];
    }
op_less_or_equal:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() <= arg_right;
        goto *labels[NEXT_OP()];
    }
op_greater:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() > arg_right;
        goto *labels[NEXT_OP()];
    }
op_greater_or_equal:{
        uint64_t arg_right = POP();
        *TOS_PTR() = PEEK() >= arg_right;
        goto *labels[NEXT_OP()];
    }
op_greater_or_equali:{
        uint64_t arg_right = NEXT_ARG();
        *TOS_PTR() = PEEK() >= arg_right;
        goto *labels[NEXT_OP()];
    }
op_pop_res: {
        /* Pop the top of the stack, set it as a result value */
        uint64_t res = POP();
        vm.result = res;
        goto *labels[NEXT_OP()];
    }
op_done: {
        goto end;
    }
op_print:{
        uint64_t arg = POP();
        printf("%" PRIu64 "\n", arg);
        goto *labels[NEXT_OP()];
    }
op_abort: {
        return ERROR_END_OF_STREAM;
    }
end:
    return SUCCESS;
}


uint64_t vm_get_result(void)
{
    return vm.result;
}

#undef NEXT_OP
#undef NEXT_ARG
#undef PEEK_ARG
#undef POP
#undef PUSH
#undef PEEK
#undef TOS_PTR

/*
 * trace-based vm interpreter
 * */

#define POP()                                   \
    (*(--vm_trace.stack_top))
#define PUSH(val)                               \
    (*vm_trace.stack_top = (val), vm_trace.stack_top++)
#define PEEK()                                  \
    (*(vm_trace.stack_top - 1))
#define TOS_PTR()                               \
    (vm_trace.stack_top - 1)
#define NEXT_HANDLER(code)                      \
    (((code)++), (code)->handler((code)))
#define ARG_AT_PC(bytecode, pc)                                         \
    (((uint64_t)(bytecode)[(pc) + 1] << 8) + (bytecode)[(pc) + 2])

typedef struct scode scode;

typedef void trace_op_handler(scode *code);

struct scode {
    uint64_t arg;
    trace_op_handler *handler;
};

typedef scode trace[MAX_TRACE_LEN];

static struct {
    uint8_t *bytecode;
    size_t pc;
    jmp_buf buf;
    bool is_running;
    interpret_result error;

    trace trace_cache[MAX_CODE_LEN];

    /* Fixed-size stack */
    uint64_t stack[STACK_MAX];
    uint64_t *stack_top;

    /* Operational memory */
    uint64_t memory[MEMORY_SIZE];

    /* A single register containing the result */
    uint64_t result;

} vm_trace;

static void op_abort_handler(scode *code)
{
    (void) code;

    vm_trace.is_running = false;
    vm_trace.error = ERROR_END_OF_STREAM;
}

static void op_pushi_handler(scode *code)
{
    PUSH(code->arg);

    NEXT_HANDLER(code);
}

static void op_loadi_handler(scode *code)
{
    uint64_t addr = code->arg;
    uint64_t val = vm_trace.memory[addr];
    PUSH(val);

    NEXT_HANDLER(code);
}

static void op_loadaddi_handler(scode *code)
{
    uint64_t addr = code->arg;
    uint64_t val = vm_trace.memory[addr];
    *TOS_PTR() += val;

    NEXT_HANDLER(code);
}

static void op_storei_handler(scode *code)
{
    uint16_t addr = code->arg;
    uint64_t val = POP();
    vm_trace.memory[addr] = val;

    NEXT_HANDLER(code);
}

static void op_load_handler(scode *code)
{
    uint16_t addr = POP();
    uint64_t val = vm_trace.memory[addr];
    PUSH(val);

    NEXT_HANDLER(code);

}

static void op_store_handler(scode *code)
{
    uint64_t val = POP();
    uint16_t addr = POP();
    vm_trace.memory[addr] = val;

    NEXT_HANDLER(code);
}

static void op_dup_handler(scode *code)
{
    PUSH(PEEK());

    NEXT_HANDLER(code);
}

static void op_discard_handler(scode *code)
{
    (void) POP();

    NEXT_HANDLER(code);
}

static void op_add_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() += arg_right;

    NEXT_HANDLER(code);
}

static void op_addi_handler(scode *code)
{
    uint16_t arg_right = code->arg;
    *TOS_PTR() += arg_right;

    NEXT_HANDLER(code);
}

static void op_sub_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() -= arg_right;

    NEXT_HANDLER(code);
}

static void op_div_handler(scode *code)
{
    uint64_t arg_right = POP();
    /* Don't forget to handle the div by zero error */
    if (arg_right != 0) {
        *TOS_PTR() /= arg_right;
    } else {
        vm_trace.is_running = false;
        vm_trace.error = ERROR_DIVISION_BY_ZERO;
        longjmp(vm_trace.buf, 1);
    }

    NEXT_HANDLER(code);
}

static void op_mul_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() *= arg_right;

    NEXT_HANDLER(code);
}

static void op_jump_handler(scode *code)
{
    uint64_t target = code->arg;
    vm_trace.pc = target;
}

static void op_jump_if_true_handler(scode *code)
{
    if (POP()) {
        uint64_t target = code->arg;
        vm_trace.pc = target;
        return;
    }
}

static void op_jump_if_false_handler(scode *code)
{
    if (!POP()) {
        uint64_t target = code->arg;
        vm_trace.pc =  target;
        return;
    }
}

static void op_equal_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() = PEEK() == arg_right;

    NEXT_HANDLER(code);
}

static void op_less_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() = PEEK() < arg_right;

    NEXT_HANDLER(code);
}

static void op_less_or_equal_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() = PEEK() <= arg_right;

    NEXT_HANDLER(code);
}
static void op_greater_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() = PEEK() > arg_right;

    NEXT_HANDLER(code);
}

static void op_greater_or_equal_handler(scode *code)
{
    uint64_t arg_right = POP();
    *TOS_PTR() = PEEK() >= arg_right;

    NEXT_HANDLER(code);
}

static void op_greater_or_equali_handler(scode *code)
{
    uint64_t arg_right = code->arg;
    *TOS_PTR() = PEEK() >= arg_right;

    NEXT_HANDLER(code);
}

static void op_pop_res_handler(scode *code)
{
    uint64_t res = POP();
    vm_trace.result = res;

    NEXT_HANDLER(code);
}

static void op_done_handler(scode *code)
{
    (void) code;

    vm_trace.is_running = false;
    vm_trace.error = SUCCESS;
}

static void op_print_handler(scode *code)
{
    uint64_t arg = POP();
    printf("%" PRIu64 "\n", arg);

    NEXT_HANDLER(code);
}

typedef struct trace_opinfo {
    bool has_arg;
    bool is_branch;
    bool is_abs_jump;
    bool is_final;
    trace_op_handler *handler;
} trace_opinfo;

static const trace_opinfo trace_opcode_to_opinfo[] = {
    [OP_ABORT] = {false, false, false, true, op_abort_handler},
    [OP_PUSHI] = {true, false, false, false, op_pushi_handler},
    [OP_LOADI] = {true, false, false, false, op_loadi_handler},
    [OP_LOADADDI] = {true, false, false, false, op_loadaddi_handler},
    [OP_STOREI] = {true, false, false, false, op_storei_handler},
    [OP_LOAD] = {false, false, false, false, op_load_handler},
    [OP_STORE] = {false, false, false, false, op_store_handler},
    [OP_DUP] = {false, false, false, false, op_dup_handler},
    [OP_DISCARD] = {false, false, false, false, op_discard_handler},
    [OP_ADD] = {false, false, false, false, op_add_handler},
    [OP_ADDI] = {true, false, false, false, op_addi_handler},
    [OP_SUB] = {false, false, false, false, op_sub_handler},
    [OP_DIV] = {false, false, false, false, op_div_handler},
    [OP_MUL] = {false, false, false, false, op_mul_handler},
    [OP_JUMP] = {true, false, true, false, op_jump_handler},
    [OP_JUMP_IF_TRUE] = {true, true, false, false, op_jump_if_true_handler},
    [OP_JUMP_IF_FALSE] = {true, true, false, false, op_jump_if_false_handler},
    [OP_EQUAL] = {false, false, false, false, op_equal_handler},
    [OP_LESS] = {false, false, false, false, op_less_handler},
    [OP_LESS_OR_EQUAL] = {false, false, false, false, op_less_or_equal_handler},
    [OP_GREATER] = {false, false, false, false, op_greater_handler},
    [OP_GREATER_OR_EQUAL] = {false, false, false, false, op_greater_or_equal_handler},
    [OP_GREATER_OR_EQUALI] = {true, false, false, false, op_greater_or_equali_handler},
    [OP_POP_RES] = {false, false, false, false, op_pop_res_handler},
    [OP_DONE] = {false, false, false, true, op_done_handler},
    [OP_PRINT] = {false, false, false, false, op_print_handler},
};

static void trace_tail_handler(scode *code)
{
    vm_trace.pc = code->arg;
}

static void trace_prejump_handler(scode *code)
{
    vm_trace.pc = code->arg;

    NEXT_HANDLER(code);
}

static void trace_compile_handler(scode *trace_head)
{
    uint8_t *bytecode = vm_trace.bytecode;
    size_t pc = vm_trace.pc;
    size_t trace_size = 0;

    const trace_opinfo *info = &trace_opcode_to_opinfo[bytecode[pc]];
    scode *trace_tail = trace_head;
    while (!info->is_final && !info->is_branch && trace_size < MAX_TRACE_LEN - 2) {
        if (info->is_abs_jump) {
            /* Absolute jumps need special care: we just jump continue parsing starting with the
             * target pc of the instruction*/
            uint64_t target = ARG_AT_PC(bytecode, pc);
            pc = target;
        } else {
            /* For usual handlers we just set the handler and optionally skip argument bytes*/
            trace_tail->handler = info->handler;

            if (info->has_arg) {
                uint64_t arg = ARG_AT_PC(bytecode, pc);
                trace_tail->arg = arg;
                pc += 2;
            }
            pc++;

            trace_size++;
            trace_tail++;
        }

        /* Get the next info and move the scode pointer */
        info = &trace_opcode_to_opinfo[bytecode[pc]];
    }

    if (info->is_final) {
        /* last instruction */
        trace_tail->handler = info->handler;
    } else if (info->is_branch) {
        /* jump handler */

        /* add a tail to skip the jump instruction - if the branch is not taken */
        trace_tail->handler = trace_prejump_handler;
        trace_tail->arg = pc + 3;

        /* now, the jump handler itself */
        trace_tail++;
        trace_tail->handler = info->handler;
        trace_tail->arg = ARG_AT_PC(bytecode, pc);
    } else {
        /* the trace is too long, add a tail handler */
        trace_tail->handler = trace_tail_handler;
        trace_tail->arg = pc;
    }

    /* now, run the chain */
    trace_head->handler(trace_head);
}

static void vm_trace_reset(uint8_t *bytecode)
{
    vm_trace = (typeof(vm_trace)) {
        .stack_top = vm_trace.stack,
        .bytecode = bytecode,
        .is_running = true
    };
    for (size_t trace_i = 0; trace_i < MAX_CODE_LEN; trace_i++ )
        vm_trace.trace_cache[trace_i][0].handler = trace_compile_handler;
}

interpret_result vm_interpret_trace(uint8_t *bytecode)
{
    vm_trace_reset(bytecode);

    if (!setjmp(vm_trace.buf)) {
        while(vm_trace.is_running) {
            scode *code = &vm_trace.trace_cache[vm_trace.pc][0];
            code->handler(code);
        }
    }

    return vm_trace.error;
}

uint64_t vm_trace_get_result(void)
{
    return vm_trace.result;
}
