#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>

typedef enum {
    /* match a single char to an immediate argument from the string and advance ip and cp, or
     * abort*/
    OP_CHAR,
    /* jump to and match either left expression or the right one, abort if nothing matches*/
    OP_OR,
    /* do an absolute jump to an offset in the immediate argument */
    OP_JUMP,
    /* stop execution and report a successful match */
    OP_MATCH,
} opcode;

typedef enum match_result {
    MATCH_OK,
    MATCH_FAIL,
    MATCH_ERROR,
} match_result;

match_result vm_match_recur(uint8_t *bytecode, uint8_t *ip, char *sp)
{
    for (;;) {
        uint8_t instruction = *ip++;
        switch (instruction) {
        case OP_CHAR:{
            char cur_c = *sp;
            char arg_c = (char)*ip ;
            /* no match? FAILed to match */
            if (arg_c != cur_c)
                return MATCH_FAIL;
            /* advance both current instruction and character pointers */
            ip++;
            sp++;
            continue;
        }
        case OP_JUMP:{
            /* read the offset and jump to the instruction */
            uint8_t offset = *ip;
            ip = bytecode + offset;
            continue;
        }
        case OP_OR:{
            /* get both branch offsets */
            uint8_t left_offset = *ip++;
            uint8_t right_offset = *ip;
            /* check if following the first offset get a match */
            uint8_t *left_ip = bytecode + left_offset;
            if (vm_match_recur(bytecode, left_ip, sp) == MATCH_OK)
                return MATCH_OK;
            /* no match? Check the second branch */
            ip = bytecode + right_offset;
            continue;
        }
        case OP_MATCH:{
            /* success */
            return MATCH_OK;
        }
        }
        return MATCH_ERROR;
    }
}

match_result vm_match(uint8_t *bytecode, char *str)
{
    printf("Start matching a string: %s\n", str);
    return vm_match_recur(bytecode, bytecode, str);
}

int main(int argc, char *argv[])
{
    (void) argc; (void) argv;

    {
        /* Match a string "abc" against regexp "abc" */
        uint8_t code[] = {OP_CHAR, 'a', OP_CHAR, 'b', OP_CHAR, 'c', OP_MATCH};
        char *str = "abc";

        match_result result = vm_match(code, str);
        assert(result == MATCH_OK);
    }

    {
        /* Fail matching a string "def" against regexp "deg" */
        uint8_t code[] = {OP_CHAR, 'd', OP_CHAR, 'e', OP_CHAR, 'g', OP_MATCH};
        char *str = "def";

        match_result result = vm_match(code, str);
        assert(result == MATCH_FAIL);
    }

    {
        /* Match strings "abc" and "bc" against regexp "a?bc" */
        uint8_t code[] = {
            OP_OR, 3, 7,
            OP_CHAR, 'a', OP_JUMP, 7,
            OP_CHAR, 'b', OP_CHAR, 'c',
            OP_MATCH
        };
        char *str1 = "abc";
        char *str2 = "bc";

        match_result result = vm_match(code, str1);
        assert(result == MATCH_OK);

        result = vm_match(code, str2);
        assert(result == MATCH_OK);
    }

    {
        /* Match strings "abc" and "dec" against regexp "(ab|de)c" */
        uint8_t code[] = {
            OP_OR, 3, 9,
            OP_CHAR, 'a', OP_CHAR, 'b', OP_JUMP, 13,
            OP_CHAR, 'd', OP_CHAR, 'e',
            OP_CHAR, 'c',
            OP_MATCH
        };
        char *str1 = "abc";
        char *str2 = "dec";
        char *str_wrong = "dc";

        match_result result = vm_match(code, str1);
        assert(result == MATCH_OK);

        result = vm_match(code, str2);
        assert(result == MATCH_OK);

        result = vm_match(code, str_wrong);
        assert(result == MATCH_FAIL);
    }

    return EXIT_SUCCESS;
}
