#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "line.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;

static void resetStack() { vm.stackTop = vm.stack; }

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.instructionPointer - vm.chunk->code - 1;
  uint32_t line = getLine(&vm.chunk->lines, instruction);
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  initTable(&vm.strings);
  initTable(&vm.globals);
}

void freeVM() {
  freeObjects();
  freeTable(&vm.strings);
  freeTable(&vm.globals);
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  --vm.stackTop;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool isTruthy(Value value) {
  switch (value.type) {
    case VAL_NIL:
      return false;

    case VAL_BOOL:
      return value.as.boolean;

    default:
      return true;
  }
}

InterpretResult run() {
#define READ_BYTE() (*vm.instructionPointer++)
#define READ_SHORT() ((READ_BYTE() << 8) | (READ_BYTE()))
#define READ_WORD() ((READ_SHORT() << 16) | (READ_SHORT()))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[READ_SHORT()])
#define READ_STRING() (AS_STRING(READ_CONSTANT()))
#define READ_STRING_LONG() (AS_STRING(READ_CONSTANT_LONG()))
#define READ_ADDRESS() (((size_t)READ_WORD() << 32) | ((size_t)READ_WORD()))
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
                                                                               \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
                                                                               \
    push(valueType(a op b));                                                   \
  } while (false)

  while (true) {

#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");

    size_t offset = vm.instructionPointer - vm.chunk->code;
    disassembleInstruction(vm.chunk, offset);
#endif

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
      case OP_RETURN:
        return INTERPRET_OK;

      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }

      case OP_CONSTANT_LONG: {
        Value constant = READ_CONSTANT_LONG();
        push(constant);
        break;
      }

      case OP_DEFINE_GLOBAL: {
        ObjString *name = READ_STRING();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }

      case OP_DEFINE_GLOBAL_LONG: {
        ObjString *name = READ_STRING_LONG();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }

      case OP_GET_GLOBAL: {
        ObjString *name = READ_STRING();
        Value value;

        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(value);
        break;
      }

      case OP_GET_GLOBAL_LONG: {
        ObjString *name = READ_STRING_LONG();
        Value value;

        if (!tableGet(&vm.globals, name, &value)) {
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(value);
        break;
      }

      case OP_SET_GLOBAL: {
        ObjString *name = READ_STRING();

        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }

      case OP_SET_GLOBAL_LONG: {
        ObjString *name = READ_STRING_LONG();

        if (tableSet(&vm.globals, name, peek(0))) {
          tableDelete(&vm.globals, name);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }

      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(vm.stack[slot]);
        break;
      }

      case OP_GET_LOCAL_LONG: {
        uint16_t slot = READ_SHORT();
        push(vm.stack[slot]);
        break;
      }

      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        vm.stack[slot] = peek(0);
        break;
      }

      case OP_SET_LOCAL_LONG: {
        uint16_t slot = READ_SHORT();
        vm.stack[slot] = peek(0);
        break;
      }

      case OP_FALSE: {
        push(BOOL_VAL(false));
        break;
      }

      case OP_TRUE: {
        push(BOOL_VAL(true));
        break;
      }

      case OP_NIL: {
        push(NIL_VAL);
        break;
      }

      case OP_NOT: {
        push(BOOL_VAL(!isTruthy(pop())));
        break;
      }

      case OP_EQUAL_EQUAL: {
        Value b = pop();
        Value a = pop();

        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }

      case OP_NOT_EQUAL: {
        Value b = pop();
        Value a = pop();

        push(BOOL_VAL(!valuesEqual(a, b)));
        break;
      }

      case OP_GREATER: {
        BINARY_OP(BOOL_VAL, >);
        break;
      }

      case OP_GREATER_EQUAL: {
        BINARY_OP(BOOL_VAL, >=);
        break;
      }

      case OP_LESS: {
        BINARY_OP(BOOL_VAL, <);
        break;
      }

      case OP_LESS_EQUAL: {
        BINARY_OP(BOOL_VAL, <=);
        break;
      }

      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }

        *(vm.stackTop - 1) = NUMBER_VAL(-AS_NUMBER(*(vm.stackTop - 1)));
        break;

      case OP_ADD: {
        if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          BINARY_OP(NUMBER_VAL, +);
        } else if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          ObjString *b = AS_STRING(pop());
          ObjString *a = AS_STRING(pop());

          push(OBJ_VAL(concatenate(a, b)));
        } else {
          runtimeError("Operands must be two numbers or two strings");
        }
        break;
      }

      case OP_SUBTRACT: {
        BINARY_OP(NUMBER_VAL, -);
        break;
      }

      case OP_MULTIPLY: {
        BINARY_OP(NUMBER_VAL, *);
        break;
      }

      case OP_DIVIDE: {
        BINARY_OP(NUMBER_VAL, /);
        break;
      }

      case OP_JUMP_IF_TRUE: {
        uint16_t offset = READ_SHORT();

        if (isTruthy(peek(0))) {
          vm.instructionPointer += offset;
        }

        break;
      }

      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();

        if (!isTruthy(peek(0))) {
          vm.instructionPointer += offset;
        }

        break;
      }

      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        vm.instructionPointer += offset;
        break;
      }

      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        vm.instructionPointer -= offset;
        break;
      }

      case OP_PRINT:
        printValue(pop());
        printf("\n");
        break;

      case OP_POP:
        pop();
        break;

      case OP_POPN: {
        uint8_t count = READ_BYTE();
        vm.stackTop -= count;
        break;
      }
    }
  }

#undef BINARY_OP
#undef READ_ADDRESS
#undef READ_STRING_LONG
#undef READ_STRING
#undef READ_CONSTANT_LONG
#undef READ_CONSTANT
#undef READ_WORD
#undef READ_SHORT
#undef READ_BYTE
}

InterpretResult interpret(const char *source) {
  Chunk chunk;

  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.instructionPointer = vm.chunk->code;

  InterpretResult result = run();
  freeChunk(&chunk);

  return result;
}
