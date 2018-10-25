#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <setjmp.h>
#include <jit/jit.h>
#include <jit/jit-dump.h>

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

/*
 * Jit-based bytecode interpreter
 *  */

typedef void trace_handler(void);

typedef struct trace_record {
    jit_function_t function;
    trace_handler *handler;
} trace_record;

static struct {
    uint8_t *bytecode;
    size_t pc;
    bool is_running;
    interpret_result error;

    trace_record trace_cache[MAX_CODE_LEN];

    /* Stack */
    uint64_t stack[STACK_MAX];
    uint64_t *stack_top;

    /* Operational memory */
    uint64_t memory[MEMORY_SIZE];

    /* A single register containing the result */
    uint64_t result;
} vm_jit;

/* Need addressable err values */
static interpret_result error_eos = ERROR_END_OF_STREAM;
static interpret_result error_dbz = ERROR_DIVISION_BY_ZERO;
static interpret_result error_re = ERROR_RUNTIME_EXCEPTION;

/* Arg types */
static jit_type_t jit_memory_ptr_type;
static jit_type_t jit_type_cstring;
static jit_type_t jit_stack_ptr_type;
static jit_type_t jit_stack_ptr_ptr_type;
static jit_type_t jit_pc_ptr_type;
static jit_type_t jit_is_running_ptr_type;
static jit_type_t jit_result_ptr_type;


/* The main jitted function signature */
static jit_type_t jit_function_signature;

/* Stdlib helper signature */
static jit_type_t jit_printf_signature;

static void op_abort_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    jit_value_t errno_value_ptr = jit_value_create_nint_constant(
        function, jit_result_ptr_type, (long)&error_eos
    );
    jit_insn_throw(function, errno_value_ptr);
}

static void op_done_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    jit_value_t is_running_ptr = jit_value_get_param(function, 2);
    jit_value_t false_value = jit_value_create_nint_constant(function, jit_type_ubyte, false);
    jit_insn_store_relative(function, is_running_ptr, 0, false_value);
    jit_insn_return(function, NULL);
}

static void op_pushi_compiler(jit_function_t function, uint64_t arg)
{
    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t arg_value = jit_value_create_long_constant(function, jit_type_ulong, arg);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Store the new stack value */
    jit_insn_store_relative(function, stack_ptr, 0, arg_value);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);
}

static void op_loadi_compiler(jit_function_t function, uint64_t arg)
{
    const long memory_offset = (long)jit_type_get_size(jit_type_ulong) * arg;
    jit_value_t memory_ptr = jit_value_create_nint_constant(
        function, jit_memory_ptr_type, (long)&vm_jit.memory
    );

    /* Get the value from memory */
    jit_value_t val_value = jit_insn_load_relative(function, memory_ptr, memory_offset, jit_type_ulong);

    /* Push it onto stack */
    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);
    jit_insn_store_relative(function, stack_ptr, 0, val_value);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

}

static void op_loadaddi_compiler(jit_function_t function, uint64_t arg)
{
    /* Get the right-hand value from memory */
    const long memory_offset = (long)jit_type_get_size(jit_type_ulong) * arg;
    jit_value_t memory_ptr = jit_value_create_nint_constant(
        function, jit_memory_ptr_type, (long)&vm_jit.memory
    );
    jit_value_t rvalue = jit_insn_load_relative(function, memory_ptr, memory_offset, jit_type_ulong);

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Peek the current value */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Add and put back as a top of the stack */
    jit_value_t result = jit_insn_add(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_storei_compiler(jit_function_t function, uint64_t arg)
{
    const long memory_offset = (long)jit_type_get_size(jit_type_ulong) * arg;
    jit_value_t memory_ptr = jit_value_create_nint_constant(
        function, jit_memory_ptr_type, (long)&vm_jit.memory
    );

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Get the top of the stack and store it into memory */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_value_t val_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);
    jit_insn_store_relative(function, memory_ptr, memory_offset, val_value);

    /* Move the top of the stack */
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);
}

static void op_load_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Get the top of the stack as an address */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_value_t addr_value = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Get the value from the address */
    jit_value_t memory_ptr = jit_value_create_nint_constant(
        function, jit_memory_ptr_type, (long)&vm_jit.memory
    );
    jit_value_t val_value = jit_insn_load_elem(function, memory_ptr, addr_value, jit_type_ulong);

    /* Replace the value on the top of the stack with the value from memory */
    jit_insn_store_relative(function, stack_ptr_peek, 0, val_value);
}

static void op_store_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Get the top of the stack as a value */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_value_t val_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* Get address from the stack  */
    stack_ptr_moved = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t addr_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* Move the top of the stack pointer */
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Store the value in the given address */
    jit_value_t memory_ptr = jit_value_create_nint_constant(
        function, jit_memory_ptr_type, (long)&vm_jit.memory
    );
    jit_insn_store_elem(function, memory_ptr, addr_value, val_value);
}

static void op_dup_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Peek the current TOS */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_value_t dup_value = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Store the new stack value */
    jit_insn_store_relative(function, stack_ptr, 0, dup_value);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);
}

static void op_discard_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);
}

static void op_add_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Add up values */
    jit_value_t result = jit_insn_add(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_addi_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);

    /* Get the left-hand value */
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_value_create_long_constant(function, jit_type_ulong, arg);

    /* Add up values */
    jit_value_t result = jit_insn_add(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_sub_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Subtract values */
    jit_value_t result = jit_insn_sub(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_div_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Divide values */
    jit_value_t result = jit_insn_div(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_mul_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Multiply values */
    jit_value_t result = jit_insn_mul(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_jump_if_true_compiler(jit_function_t function, uint64_t arg)
{
    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the condition and jump to the end if not true */
    jit_value_t condition_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);
    jit_label_t done_label = jit_label_undefined;
    jit_insn_branch_if_not(function, condition_value, &done_label);

    /* Change pc if true */
    jit_value_t pc_ptr = jit_value_get_param(function, 1);
    jit_value_t target_pc = jit_value_create_long_constant(function, jit_type_ulong, arg);
    jit_insn_store_relative(function, pc_ptr, 0, target_pc);

    jit_insn_label(function, &done_label);
}

static void op_jump_if_false_compiler(jit_function_t function, uint64_t arg)
{
    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the condition and jump to the end if not true */
    jit_value_t condition_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);
    jit_label_t done_label = jit_label_undefined;
    jit_insn_branch_if(function, condition_value, &done_label);

    /* Change pc if true */
    jit_value_t pc_ptr = jit_value_get_param(function, 1);
    jit_value_t target_pc = jit_value_create_long_constant(function, jit_type_ulong, arg);
    jit_insn_store_relative(function, pc_ptr, 0, target_pc);

    jit_insn_label(function, &done_label);
}

static void op_equal_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* equal */
    jit_value_t result = jit_insn_eq(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_less_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* less than */
    jit_value_t result = jit_insn_lt(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_less_or_equal_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* less or equal */
    jit_value_t result = jit_insn_le(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_greater_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* greater than */
    jit_value_t result = jit_insn_gt(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_greater_or_equal_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the right-hand value */
    jit_value_t rvalue = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* We only need to peek the top of the stak */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr_moved, -stack_ptrdiff);
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* greater than */
    jit_value_t result = jit_insn_ge(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_greater_or_equali_compiler(jit_function_t function, uint64_t arg)
{
    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Peek the top of the stack value */
    jit_value_t stack_ptr_peek = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);

    /* Get the left-hand value */
    jit_value_t lvalue = jit_insn_load_relative(function, stack_ptr_peek, 0, jit_type_ulong);

    /* Argument is a right-hand comparison value */
    jit_value_t rvalue = jit_value_create_long_constant(function, jit_type_ulong, arg);

    /* greater than */
    jit_value_t result = jit_insn_ge(function, lvalue, rvalue);
    jit_insn_store_relative(function, stack_ptr_peek, 0, result);
}

static void op_print_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = (long)jit_type_get_size(jit_type_ulong);
    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);

    /* Move the top of the stack value */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, -stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the value to be printed */
    jit_value_t val_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);

    /* Call printf */
    jit_value_t format_str_ptr = jit_value_create_long_constant(function, jit_type_cstring, (long)"%zu\n");
    jit_value_t args[] = {format_str_ptr, val_value};
    jit_insn_call_native(function, "printf", printf, jit_printf_signature, args, 2, JIT_CALL_NOTHROW);
}

static void op_pop_res_compiler(jit_function_t function, uint64_t arg)
{
    (void) arg;

    const long stack_ptrdiff = -(long)jit_type_get_size(jit_type_ulong);

    jit_value_t stack_ptr_ptr = jit_value_get_param(function, 0);
    jit_value_t stack_ptr = jit_insn_load_relative(function, stack_ptr_ptr, 0, jit_stack_ptr_type);
    jit_value_t result_ptr = jit_value_get_param(function, 3);

    /* Move the top of the stack */
    jit_value_t stack_ptr_moved = jit_insn_add_relative(function, stack_ptr, stack_ptrdiff);
    jit_insn_store_relative(function, stack_ptr_ptr, 0, stack_ptr_moved);

    /* Get the result value of the stack and store it into the result register */
    jit_value_t result_value = jit_insn_load_relative(function, stack_ptr_moved, 0, jit_type_ulong);
    jit_insn_store_relative(function, result_ptr, 0, result_value);
}

static void prejump_compiler(jit_function_t function, uint64_t arg)
{
    jit_value_t pc_ptr = jit_value_get_param(function, 1);
    jit_value_t target_pc = jit_value_create_long_constant(function, jit_type_ulong, arg);
    jit_insn_store_relative(function, pc_ptr, 0, target_pc);
}

typedef void jit_op_compiler(jit_function_t function, uint64_t arg);

typedef struct jit_opinfo {
    bool has_arg;
    bool is_branch;
    bool is_abs_jump;
    bool is_final;
    jit_op_compiler *compiler;
} jit_opinfo;

static const jit_opinfo jit_opcode_to_opinfo[] = {
    [OP_ABORT] = {false, false, false, true, op_abort_compiler},
    [OP_PUSHI] = {true, false, false, false, op_pushi_compiler},
    [OP_LOADI] = {true, false, false, false, op_loadi_compiler},
    [OP_LOADADDI] = {true, false, false, false, op_loadaddi_compiler},
    [OP_STOREI] = {true, false, false, false, op_storei_compiler},
    [OP_LOAD] = {false, false, false, false, op_load_compiler},
    [OP_STORE] = {false, false, false, false, op_store_compiler},
    [OP_DUP] = {false, false, false, false, op_dup_compiler},
    [OP_DISCARD] = {false, false, false, false, op_discard_compiler},
    [OP_ADD] = {false, false, false, false, op_add_compiler},
    [OP_ADDI] = {true, false, false, false, op_addi_compiler},
    [OP_SUB] = {false, false, false, false, op_sub_compiler},
    [OP_DIV] = {false, false, false, false, op_div_compiler},
    [OP_MUL] = {false, false, false, false, op_mul_compiler},
    [OP_JUMP] = {true, false, true, false, NULL},
    [OP_JUMP_IF_TRUE] = {true, true, false, false, op_jump_if_true_compiler},
    [OP_JUMP_IF_FALSE] = {true, true, false, false, op_jump_if_false_compiler},
    [OP_EQUAL] = {false, false, false, false, op_equal_compiler},
    [OP_LESS] = {false, false, false, false, op_less_compiler},
    [OP_LESS_OR_EQUAL] = {false, false, false, false, op_less_or_equal_compiler},
    [OP_GREATER] = {false, false, false, false, op_greater_compiler},
    [OP_GREATER_OR_EQUAL] = {false, false, false, false, op_greater_or_equal_compiler},
    [OP_GREATER_OR_EQUALI] = {true, false, false, false, op_greater_or_equali_compiler},
    [OP_POP_RES] = {false, false, false, false, op_pop_res_compiler},
    [OP_DONE] = {false, false, false, true, op_done_compiler},
    [OP_PRINT] = {false, false, false, false, op_print_compiler},
};

static void trace_jit_runner(void)
{
    size_t pc = vm_jit.pc;

    jit_function_t function = vm_jit.trace_cache[pc].function;

    uint64_t **stack_top_ptr_ptr = &vm_jit.stack_top;
    uint64_t *pc_ptr = &vm_jit.pc;
    bool *is_running_ptr = &vm_jit.is_running;
    uint64_t *result_ptr = &vm_jit.result;

    void *args[] = {&stack_top_ptr_ptr, &pc_ptr, &is_running_ptr, &result_ptr};
    if (jit_function_apply(function, args, NULL) == 0) {
        /* The only user-defined exception we have is ERROR_END_OF_STREAM on OP_ABORT */
        vm_jit.is_running = false;
        interpret_result *error_code_ptr = jit_exception_get_last_and_clear();
        vm_jit.error = *error_code_ptr;
    };
}

static void trace_jit_compile(void)
{
    uint8_t *bytecode = vm_jit.bytecode;
    size_t pc_to_compile = vm_jit.pc;

    /* Init the function to be compiled */
    jit_context_t context = jit_context_create();
    jit_context_build_start(context);
    jit_function_t function = jit_function_create(context, jit_function_signature);

    const jit_opinfo *info = &jit_opcode_to_opinfo[bytecode[pc_to_compile]];
    while (!info->is_final && !info->is_branch) {
        if (info->is_abs_jump) {
            /* Absolute jumps need special care: we just jump continue compiling functions starting
             * with the target pc of the instruction*/
            uint64_t target = ARG_AT_PC(bytecode, pc_to_compile);
            pc_to_compile = target;
        } else {
            /* Get the compiling function */
            jit_op_compiler *compiler = info->compiler;

            /* Optional arg */
            uint64_t arg = 0;
            if (info->has_arg) {
                arg = ARG_AT_PC(bytecode, pc_to_compile);
                pc_to_compile += 2;
            }

            /* Add the body of the instruction to the function to be compiled */
            compiler(function, arg);

            /* Skip one more byte to the next instruction */
            pc_to_compile++;
        }

        /* Get the next info */
        info = &jit_opcode_to_opinfo[bytecode[pc_to_compile]];
    }

    if (info->is_final) {
        /* we just compile the finalizing handler, nothing special here */
        info->compiler(function, 0);
    } else if (info->is_branch) {
        /* we need to change pc in case the branch is not taken, thus a prejump handler */
        prejump_compiler(function, pc_to_compile + 3);

        /* Now the branching instruction itself */
        info = &jit_opcode_to_opinfo[bytecode[pc_to_compile]];
        info->compiler(function, ARG_AT_PC(bytecode, pc_to_compile));
    } else {
        /* This is impossible, we only end traces on last instructions or branches */
        assert(false);
    }

    /* Finalize and compile the function */
    jit_context_build_end(context);
    jit_dump_function(stderr, function, "uncompiled"); /* debug */
    jit_function_compile(function);

    /* Replace the compiling handler with the running handler */
    vm_jit.trace_cache[vm_jit.pc].function = function;
    vm_jit.trace_cache[vm_jit.pc].handler = trace_jit_runner;

    /* ...and run that thing */
    vm_jit.trace_cache[vm_jit.pc].handler();
}

static void *vm_jit_builtin_exception_handler(int exception_type)
{
    if (exception_type == JIT_RESULT_DIVISION_BY_ZERO)
        return &error_dbz;
    else
        return &error_re;
}

static void vm_jit_reset(uint8_t *bytecode)
{
    vm_jit = (typeof(vm_jit)) {
        .stack_top = vm_jit.stack,
        .bytecode = bytecode,
        .is_running = true
    };

    for (size_t trace_i = 0; trace_i < MAX_CODE_LEN; trace_i++ )
        vm_jit.trace_cache[trace_i].handler = trace_jit_compile;

    /* Init various libjit helper types and signature */
    jit_pc_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);
    jit_memory_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);
    jit_stack_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);
    jit_stack_ptr_ptr_type = jit_type_create_pointer(jit_stack_ptr_type, 1);
    jit_is_running_ptr_type = jit_type_create_pointer(jit_type_sys_bool, 1);
    jit_result_ptr_type = jit_type_create_pointer(jit_type_ulong, 1);

    {
        jit_type_t params[] = {jit_stack_ptr_ptr_type, jit_pc_ptr_type, jit_is_running_ptr_type, jit_result_ptr_type};
        jit_function_signature = jit_type_create_signature(
            jit_abi_cdecl, jit_type_void, params, sizeof(params) / sizeof(params[0]), 1
        );
    }

    {
        jit_type_cstring = jit_type_create_pointer(jit_type_sys_char, 1);
        jit_type_t params[] = {jit_type_cstring, jit_type_ulong};
        jit_printf_signature = jit_type_create_signature(
            jit_abi_cdecl, jit_type_int, params, sizeof(params) / sizeof(params[0]), 1
        );
    }

    /* set the builtin exception handler */
    jit_exception_set_handler(vm_jit_builtin_exception_handler);
}

interpret_result vm_interpret_jit(uint8_t *bytecode)
{
    vm_jit_reset(bytecode);

    while(vm_jit.is_running)
        vm_jit.trace_cache[vm_jit.pc].handler();

    return vm_jit.error;
}

uint64_t vm_jit_get_result(void)
{
    return vm_jit.result;
}


/* Local Variables: */
/* rmsbolt-command: "gcc -O3" */
/* rmsbolt-asm-format: "intel" */
/* End: */
