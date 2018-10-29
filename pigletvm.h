#include <inttypes.h>

#define MAX_CODE_LEN 4096

typedef enum interpret_result {
    SUCCESS,
    ERROR_DIVISION_BY_ZERO,
    ERROR_RUNTIME_EXCEPTION,
    ERROR_UNKNOWN_OPCODE,
    ERROR_END_OF_STREAM,
} interpret_result;

typedef enum {
    /* just a sentinel ending the instruction stream, should never be reached by the interpreter */
    OP_ABORT,

    /* push the immediate argument onto the stack */
    OP_PUSHI,
    /* load a value onto the stack from a memory cell addressed by an immediate argument  */
    OP_LOADI,
    /* load a value onto the stack from a memory cell addressed by an immediate argument, add it to
     * the top of the stack */
    OP_LOADADDI,
    /* pop a value and store it into a memory cell addressed by an immediate argument  */
    OP_STOREI,
    /* pop an address of the stack, use it to get a value from a memory cell */
    OP_LOAD,
    /* pop an address of the stack, pop a value from the stack, store into the address */
    OP_STORE,


    /* check the current top of the stack and add a copy on top of it */
    OP_DUP,
    /* pop 2 values from the stack, add and push the result onto the stack */
    OP_DISCARD,


    /* pop 2 values, add and push back onto stack  */
    OP_ADD,
    /* add an immediate value to the top of the stack */
    OP_ADDI,
    /* pop 2 values from the stack, subtract and push the result onto the stack */
    OP_SUB,
    /* pop 2 values from the stack, divide and push the result onto the stack */
    OP_DIV,
    /* pop 2 values from the stack, multiply and push the result onto the stack */
    OP_MUL,


    /* jump to an absolute bytecode address (the immediate argument) */
    OP_JUMP,
    /* pop the top of the stack, jump to an absolute bytecode address if true */
    OP_JUMP_IF_TRUE,
    /* pop the top of the stack, jump to an absolute bytecode address if false */
    OP_JUMP_IF_FALSE,

    /* equality ops: pop two values from the stack, compare then, push the result (0/1) back onto
     * the stack */
    OP_EQUAL,
    OP_LESS,
    OP_LESS_OR_EQUAL,
    OP_GREATER,
    OP_GREATER_OR_EQUAL,
    OP_GREATER_OR_EQUALI,

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

interpret_result vm_interpret_no_range_check(uint8_t *bytecode);

interpret_result vm_interpret_threaded(uint8_t *bytecode);

uint64_t vm_get_result(void);

interpret_result vm_interpret_trace(uint8_t *bytecode);

uint64_t vm_trace_get_result(void);
