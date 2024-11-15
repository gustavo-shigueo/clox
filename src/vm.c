#include "vm.h"
#include "chunk.h"
#include "compiler.h"
#include "line.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;

static Value clockNative(uint32_t argumentCount, Value* args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (uint32_t i = vm.frameCount; i > 0; i--) {
    CallFrame *frame = &vm.frames[i - 1];
    ObjFunction *function = frame->function;
    size_t instruction = frame->instructionPointer - function->chunk.code - 1;

    fprintf(stderr, "[line %d] in ", getLine(&function->chunk.lines, instruction));
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

static void defineNative(const char* name, NativeFn function, uint32_t arity) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, arity)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  initTable(&vm.strings);
  initTable(&vm.globals);

  defineNative("clock", clockNative, 0);
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

static bool call(ObjFunction *function, uint8_t argumentCount) {
  if (function->arity != argumentCount) {
    runtimeError(
      "Expected %d arguments but got %d.",
      function->arity,
      argumentCount
    );

    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->instructionPointer = function->chunk.code;
  frame->slots = vm.stackTop - argumentCount - 1;

  return true;
}

static bool callValue(Value callee, uint8_t argumentCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_FUNCTION:
        return call(AS_FUNCTION(callee), argumentCount);

      case OBJ_NATIVE: {
        ObjNative *native = AS_NATIVE(callee);

        if (native->arity != argumentCount) {
          runtimeError(
            "Expected %d arguments but got %d.",
            native->arity,
            argumentCount
          );

          return false;
        }

        Value result = native->function(argumentCount, vm.stackTop - argumentCount);
        vm.stackTop -= argumentCount + 1;
        push(result);
        return true;
      }

      default:
        break;
    }
  }

  runtimeError("Can only call functions and classes.");
  return false;
}

InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->instructionPointer++)
#define READ_SHORT() ((READ_BYTE() << 8) | (READ_BYTE()))
#define READ_WORD() ((READ_SHORT() << 16) | (READ_SHORT()))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (frame->function->chunk.constants.values[READ_SHORT()])
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
    CallFrame *frame = &vm.frames[vm.frameCount - 1];
    for (Value *slot = frame->slots; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");

    disassembleInstruction(
      &frame->function->chunk,
      (size_t)(frame->instructionPointer - frame->function->chunk.code)
    );
#endif

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
      case OP_CALL: {
        uint8_t argumentCount = READ_BYTE();

        if (!callValue(peek(argumentCount), argumentCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        frame = &vm.frames[vm.frameCount - 1];

        break;
      }

      case OP_RETURN: {
        Value value = pop();
        vm.frameCount--;

        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(value);
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }

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
        push(frame->slots[slot]);
        break;
      }

      case OP_GET_LOCAL_LONG: {
        uint16_t slot = READ_SHORT();
        push(frame->slots[slot]);
        break;
      }

      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }

      case OP_SET_LOCAL_LONG: {
        uint16_t slot = READ_SHORT();
        frame->slots[slot] = peek(0);
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
          frame->instructionPointer += offset;
        }

        break;
      }

      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();

        if (!isTruthy(peek(0))) {
          frame->instructionPointer += offset;
        }

        break;
      }

      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->instructionPointer += offset;
        break;
      }

      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->instructionPointer -= offset;
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
  ObjFunction *function = compile(source);

  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  push(OBJ_VAL(function));
  call(function, 0);

  return run();
}
