#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX UINT8_MAX

typedef struct {
  Chunk *chunk;
  uint8_t *instructionPointer;
  Value stack[STACK_MAX];
  Value *stackTop;
  Table strings;
  Table globals;
  Object *objects;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(const char *source);

void push(Value value);
Value pop();

extern VM vm;

#endif // !clox_vm_h
