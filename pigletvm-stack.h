#include <inttypes.h>

typedef enum interpret_result {
    SUCCESS,
    ERROR_DIVISION_BY_ZERO,
    ERROR_UNKNOWN_OPCODE,
    ERROR_END_OF_STREAM,
} interpret_result;

typedef enum {
    /* just a sentinel ending the instruction stream, should never be reached by the interpreter */
    OP_ABORT,
    /* push the immediate argument onto the stack */
    OP_PUSHI,
    /* pop 2 values from the stack, add and push the result onto the stack */
    OP_ADD,
    /* pop 2 values from the stack, subtract and push the result onto the stack */
    OP_SUB,
    /* pop 2 values from the stack, divide and push the result onto the stack */
    OP_DIV,
    /* pop 2 values from the stack, multiply and push the result onto the stack */
    OP_MUL,
    /* jump to an absolute bytecode address (the immediate argument) */
    OP_JUMP,
    /* pop the top of the stack and set it as execution result */
    OP_POP_RES,
    /* properly stop execution */
    OP_DONE,
    /* pop the top of the stack and print it */
    OP_PRINT,
    /* just a helper to count operation number */
    OP_NUMBER_OF_OPS
} opcode;

interpret_result vm_interpret(uint8_t *bytecode);

uint64_t vm_get_result(void);
