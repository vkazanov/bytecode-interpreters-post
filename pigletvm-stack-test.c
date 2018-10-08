#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "pigletvm-stack.h"


int main(int argc, char *argv[])
{
    (void) argc; (void) argv;

    {
        /* Push and pop the result */
        uint8_t code[] = { OP_PUSHI, 5, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 5);
    }

    {
        /* Addition */
        uint8_t code[] = { OP_PUSHI, 10, OP_PUSHI, 5, OP_ADD, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 15);
    }

    {
        /* Subtraction */
        uint8_t code[] = { OP_PUSHI, 10, OP_PUSHI, 6, OP_SUB, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 4);
    }

    {
        /* Division */
        uint8_t code[] = { OP_PUSHI, 10, OP_PUSHI, 5, OP_DIV, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 2);
    }

    {
        /* Division with error */
        uint8_t code[] = { OP_PUSHI, 10, OP_PUSHI, 0, OP_DIV, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == ERROR_DIVISION_BY_ZERO);
    }

    {
        /* Multiplication */
        uint8_t code[] = { OP_PUSHI, 10, OP_PUSHI, 2, OP_MUL, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 20);
    }

    {
        /* Expression: 2*(11+3) */
        uint8_t code[] = { OP_PUSHI, 2, OP_PUSHI, 11, OP_PUSHI, 3, OP_ADD, OP_MUL, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 28);
    }

    return EXIT_SUCCESS;
}
