#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "pigletvm-stack.h"

#define STACK_MAX 256
#define MEMORY_SIZE 256

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

static inline void vm_reset(void)
{
    puts("Reset vm state");
    vm = (typeof(vm)) { NULL };
    vm.stack_top = vm.stack;
}

static inline void vm_stack_push(uint64_t value)
{
    *vm.stack_top = value;
    vm.stack_top++;
}

static inline uint64_t vm_stack_pop(void)
{
    vm.stack_top--;
    return *vm.stack_top;
}

interpret_result vm_interpret(uint8_t *bytecode)
{
    vm_reset();

    puts("Start interpreting");
    vm.ip = bytecode;
    for (;;) {
        uint8_t instruction = *vm.ip++;
        switch (instruction) {
        case OP_PUSHI: {
            /* get the argument, push it onto stack */
            uint8_t arg = *vm.ip++;
            vm_stack_push(arg);
            break;
        }
        case OP_ADD: {
            /* Pop 2 values, add 'em, push the result back to the stack */
            uint64_t arg_right = vm_stack_pop();
            uint64_t arg_left = vm_stack_pop();
            uint64_t res = arg_left + arg_right;
            vm_stack_push(res);
            break;
        }
        case OP_SUB: {
            /* Pop 2 values, subtract 'em, push the result back to the stack */
            uint64_t arg_right = vm_stack_pop();
            uint64_t arg_left = vm_stack_pop();
            uint64_t res = arg_left - arg_right;
            vm_stack_push(res);
            break;
        }
        case OP_DIV: {
            /* Pop 2 values, divide 'em, push the result back to the stack */
            uint64_t arg_right = vm_stack_pop();
            /* Don't forget to handle the div by zero error */
            if (arg_right == 0)
                return ERROR_DIVISION_BY_ZERO;
            uint64_t arg_left = vm_stack_pop();
            uint64_t res = arg_left / arg_right;
            vm_stack_push(res);
            break;
        }
        case OP_MUL: {
            /* Pop 2 values, multiply 'em, push the result back to the stack */
            uint64_t arg_right = vm_stack_pop();
            uint64_t arg_left = vm_stack_pop();
            uint64_t res = arg_left * arg_right;
            vm_stack_push(res);
            break;
        }
        case OP_POP_RES: {
            /* Pop the top of the stack, set it as a result value */
            uint64_t res = vm_stack_pop();
            vm.result = res;
            break;
        }
        case OP_DONE: {
            return SUCCESS;
        }
        default:
            return ERROR_UNKNOWN_OPCODE;
        }
    }

    return SUCCESS;
}

uint64_t vm_get_result(void)
{
    return vm.result;
}