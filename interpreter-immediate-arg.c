#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

struct {
    uint8_t *ip;
    uint64_t accumulator;
} vm;

typedef enum {
    /* increment the register */
    OP_INC,
    /* decrement the register */
    OP_DEC,
    /* add the immediate argument to the register */
    OP_ADDI,
    /* subtract the immediate argument from the register */
    OP_SUBI,
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
    memset(&vm, 0, sizeof(vm));
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
        case OP_ADDI: {
            /* get the argument */
            uint8_t arg = *vm.ip++;
            vm.accumulator += arg;
            break;
        }
        case OP_SUBI: {
            /* get the argument */
            uint8_t arg = *vm.ip++;
            vm.accumulator -= arg;
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
        /* notice the value after OP_ADDI */
        uint8_t code[] = { OP_ADDI, 10, OP_DEC, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm.accumulator);

        assert(result == SUCCESS);
        assert(vm.accumulator == 9);
    }

    {
        /* notice values after OP_ADDI and OP_SUBI  */
        uint8_t code[] = { OP_ADDI, 10, OP_SUBI, 3, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm.accumulator);

        assert(result == SUCCESS);
        assert(vm.accumulator == 7);
    }

    return EXIT_SUCCESS;
}
