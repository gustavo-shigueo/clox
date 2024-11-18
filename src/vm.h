#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * (UINT8_MAX + 1))

typedef struct {
  ObjClosure* closure;
  uint8_t* instructionPointer;
  Value* slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  uint32_t frameCount;

  Value stack[STACK_MAX];
  Value *stackTop;
  Table strings;
  Table globals;
  Object *objects;
  ObjUpvalue *openUpvalues;
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
