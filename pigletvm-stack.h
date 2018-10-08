#include <inttypes.h>

typedef enum interpret_result {
    SUCCESS,
    ERROR_DIVISION_BY_ZERO,
    ERROR_UNKNOWN_OPCODE,
} interpret_result;

typedef enum {
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
    /* pop the top of the stack and set it as execution result */
    OP_POP_RES,
    /* stop execution */
    OP_DONE,
} opcode;

interpret_result vm_interpret(uint8_t *bytecode);

uint64_t vm_get_result(void);
