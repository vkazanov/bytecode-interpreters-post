#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

struct {
    uint8_t *ip;
    uint64_t accumulator;
} vm;

typedef enum {
    /* increment the register */
    OP_INC,
    /* decrement the register */
    OP_DEC,
    /* stop execution */
    OP_DONE
} opcode;

typedef enum interpret_result {
    SUCCESS,
    ERROR_UNKNOWN_OPCODE,
} interpret_result;

void vm_reset(void)
{
    puts("Reset vm state");
    vm = (typeof(vm)) { NULL };
}

interpret_result vm_interpret(uint8_t *bytecode)
{
    vm_reset();

    puts("Start interpreting");
    vm.ip = bytecode;
    for (;;) {
        uint8_t instruction = *vm.ip++;
        switch (instruction) {
        case OP_INC: {
            vm.accumulator++;
            break;
        }
        case OP_DEC: {
            vm.accumulator--;
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

int main(int argc, char *argv[])
{
    (void) argc; (void) argv;

    {
        uint8_t code[] = { OP_INC, OP_INC, OP_DEC, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm.accumulator);

        assert(result == SUCCESS);
        assert(vm.accumulator == 1);
    }

    {
        uint8_t code[] = { OP_INC, OP_DEC, OP_DEC, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm.accumulator);

        assert(result == SUCCESS);
        assert(vm.accumulator == UINT64_MAX);
    }

    return EXIT_SUCCESS;
}
