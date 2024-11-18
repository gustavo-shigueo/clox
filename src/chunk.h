#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "line.h"
#include "value.h"
#include <stdint.h>

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,

  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_LONG,
  OP_GET_GLOBAL,
  OP_GET_GLOBAL_LONG,
  OP_SET_GLOBAL,
  OP_SET_GLOBAL_LONG,

  OP_GET_LOCAL,
  OP_GET_LOCAL_LONG,
  OP_SET_LOCAL,
  OP_SET_LOCAL_LONG,

  OP_GET_UPVALUE,
  OP_GET_UPVALUE_LONG,
  OP_SET_UPVALUE,
  OP_SET_UPVALUE_LONG,

  OP_CLOSE_UPVALUE,

  OP_NIL,
  OP_TRUE,
  OP_FALSE,

  OP_NEGATE,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,

  OP_EQUAL_EQUAL,
  OP_NOT_EQUAL,
  OP_GREATER,
  OP_GREATER_EQUAL,
  OP_LESS,
  OP_LESS_EQUAL,

  OP_NOT,

  OP_JUMP_IF_TRUE,
  OP_JUMP_IF_FALSE,
  OP_JUMP,

  OP_LOOP,

  OP_PRINT,
  OP_POP,
  OP_POPN,

  OP_CALL,
  OP_RETURN,
  OP_CLOSURE,
  OP_CLOSURE_LONG,
} OpCode;

typedef struct {
  size_t count;
  size_t capacity;
  uint8_t *code;
  ValueArray constants;
  LineArray lines;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, uint32_t line);
void freeChunk(Chunk *chunk);
uint16_t addConstant(Chunk *chunk, Value value);

#endif // !clox_chunk_h
