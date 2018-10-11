#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "pigletvm.h"


int main(int argc, char *argv[])
{
#define ENCODE_ARG(arg)                         \
    (((arg) & 0xff00) >> 8), ((arg) & 0x00ff)

    (void) argc; (void) argv;

    {
        /* Push and pop the result */
        uint8_t code[] = { OP_PUSHI, ENCODE_ARG(5u), OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 5);
    }

    {
        /* Push and duplicate the result, check if both args made it */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(6),
            OP_PUSHI, ENCODE_ARG(5),
            OP_DUP,
            OP_ADD,
            OP_POP_RES,
            OP_DONE
        };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 10);
    }

    {
        /* Store a value into memory, load it, the pop */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(111),
            OP_STOREI, ENCODE_ARG(5),
            OP_LOADI, ENCODE_ARG(5),
            OP_POP_RES,
            OP_DONE
        };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 111);
    }

    {
        /* Store a value into memory, load it, the pop */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(10), /* address */
            OP_DUP,                    /* address for loading */
            OP_PUSHI, ENCODE_ARG(112), /* value */
            OP_STORE,
            OP_LOAD,
            OP_POP_RES,
            OP_DONE
        };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 112);
    }

    {
        /* Push and discard the result */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(5),
            OP_PUSHI, ENCODE_ARG(7),
            OP_DISCARD,
            OP_POP_RES,
            OP_DONE
        };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 5);
    }

    {
        /* Addition */
        uint8_t code[] = { OP_PUSHI, ENCODE_ARG(10), OP_PUSHI, ENCODE_ARG(5), OP_ADD, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 15);
    }

    {
        /* Subtraction */
        uint8_t code[] = { OP_PUSHI, ENCODE_ARG(10), OP_PUSHI, ENCODE_ARG(6), OP_SUB, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 4);
    }

    {
        /* Division */
        uint8_t code[] = { OP_PUSHI, ENCODE_ARG(10), OP_PUSHI, ENCODE_ARG(5), OP_DIV, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 2);
    }

    {
        /* Division with error */
        uint8_t code[] = { OP_PUSHI, ENCODE_ARG(10), OP_PUSHI, ENCODE_ARG(0), OP_DIV, OP_POP_RES, OP_DONE };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == ERROR_DIVISION_BY_ZERO);
    }

    {
        /* Multiplication */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(10), OP_PUSHI, ENCODE_ARG(2), OP_MUL, OP_POP_RES, OP_DONE
        };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 20);
    }

    {
        /* Expression: 2*(11+3) */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(2),
            OP_PUSHI, ENCODE_ARG(11),
            OP_PUSHI, ENCODE_ARG(3),
            OP_ADD,
            OP_MUL,
            OP_POP_RES,
            OP_DONE
        };
        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 28);
    }

    {
        /* Absolute jump */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(3),
            OP_PUSHI, ENCODE_ARG(1),
            OP_ADD,
            OP_JUMP, ENCODE_ARG(14),

            /* skip */
            OP_PUSHI, ENCODE_ARG(2),
            OP_ADD,

            /* jump here (byte No 14)*/
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 4);
    }

    {
        /* Jump if true, with condition positive */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(13), /* result to be returned */
            OP_PUSHI, ENCODE_ARG(1),  /* condition to be checked */
            OP_JUMP_IF_TRUE, ENCODE_ARG(12),

            /* to be skipped */
            OP_PUSHI, ENCODE_ARG(2),

            /* jump here if true (byte No 12)*/
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 13);
    }

    {
        /* Jump if true, with condition negative */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(13),
            OP_PUSHI, ENCODE_ARG(0),  /* condition to be checked */
            OP_JUMP_IF_TRUE, ENCODE_ARG(12),

            /* to be executed */
            OP_PUSHI, ENCODE_ARG(2),  /* result to be returned */

            /* jump here if true (byte No 12)*/
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 2);
    }

    {
        /* Jump if false, with condition negative */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(13), /* result to be returned */
            OP_PUSHI, ENCODE_ARG(0),  /* condition to be checked */
            OP_JUMP_IF_FALSE, ENCODE_ARG(12),

            /* to be skipped */
            OP_PUSHI, ENCODE_ARG(2),

            /* jump here if true (byte No 12)*/
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 13);
    }

    {
        /* Jump if false, with condition positive */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(13),
            OP_PUSHI, ENCODE_ARG(1),  /* condition to be checked */
            OP_JUMP_IF_FALSE, ENCODE_ARG(12),

            /* to be skipped */
            OP_PUSHI, ENCODE_ARG(2),  /* result to be returned */

            /* jump here if true (byte No 12)*/
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 2);
    }

    {
        /* equality test 1 */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(2),
            OP_PUSHI, ENCODE_ARG(2),
            OP_EQUAL,
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 1);
    }

    {
        /* equality test 2*/
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(1),
            OP_PUSHI, ENCODE_ARG(2),
            OP_EQUAL,
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 0);
    }

    {
        /* less test */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(1),
            OP_PUSHI, ENCODE_ARG(2),
            OP_LESS,
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 1);
    }

    {
        /* less or equal test */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(2),
            OP_PUSHI, ENCODE_ARG(2),
            OP_LESS_OR_EQUAL,
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 1);
    }

    {
        /* greater test */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(3),
            OP_PUSHI, ENCODE_ARG(2),
            OP_GREATER,
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 1);
    }

    {
        /* greater or equal test */
        uint8_t code[] = {
            OP_PUSHI, ENCODE_ARG(2),
            OP_PUSHI, ENCODE_ARG(2),
            OP_GREATER_OR_EQUAL,
            OP_POP_RES,
            OP_DONE
        };

        interpret_result result = vm_interpret(code);
        printf("vm state: %" PRIu64 "\n", vm_get_result());

        assert(result == SUCCESS);
        assert(vm_get_result() == 1);
    }

    return EXIT_SUCCESS;

#undef ENCODE_ARG
}
