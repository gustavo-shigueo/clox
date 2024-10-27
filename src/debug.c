#include "debug.h"
#include "chunk.h"
#include "line.h"
#include "value.h"
#include <stdio.h>

void disassembleChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (size_t offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static size_t simpleInstruction(const char *name, size_t offset) {
  printf("%s\n", name);
  return offset + 1;
}

static size_t popNInstruction(const char *name, Chunk *chunk, size_t offset) {
  uint8_t count = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, count);

  return offset + 2;
}

static size_t oneByteInstruction(const char *name, Chunk *chunk,
                                 size_t offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);

  return offset + 2;
}

static size_t twoBytesInstruction(const char *name, Chunk *chunk,
                                  size_t offset) {
  uint16_t slot = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
  printf("%-16s %4d\n", name, slot);

  return offset + 3;
}

static size_t constantInstruction(const char *name, Chunk *chunk,
                                  size_t offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static size_t longConstantInstruction(const char *name, Chunk *chunk,
                                      size_t offset) {
  uint16_t byte_a = chunk->code[offset + 1] << 8;
  uint16_t byte_b = chunk->code[offset + 2];

  uint16_t constant = byte_a | byte_b;

  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}

static size_t jumpInstruction(const char *name, Chunk *chunk, int32_t sign,
                              size_t offset) {
  uint16_t byte_a = chunk->code[offset + 1] << 8;
  uint16_t byte_b = chunk->code[offset + 2];

  uint16_t jump = byte_a | byte_b;

  printf("%-16s %4zu -> %zu\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

size_t disassembleInstruction(Chunk *chunk, size_t offset) {
  printf("%04zu ", offset);

  uint32_t line = getLine(&chunk->lines, offset);
  if (offset > 0 && line == getLine(&chunk->lines, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4d ", line);
  }

  uint8_t instruction = chunk->code[offset];

  switch (instruction) {
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);

    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);

    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);

    case OP_EQUAL_EQUAL:
      return simpleInstruction("OP_EQUAL_EQUAL", offset);

    case OP_NOT_EQUAL:
      return simpleInstruction("OP_NOT_EQUAL", offset);

    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);

    case OP_LESS_EQUAL:
      return simpleInstruction("OP_LESS_EQUAL", offset);

    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);

    case OP_GREATER_EQUAL:
      return simpleInstruction("OP_GREATER_EQUAL", offset);

    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);

    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);

    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);

    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);

    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);

    case OP_CONSTANT_LONG:
      return longConstantInstruction("OP_CONSTANT_LONG", chunk, offset);

    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);

    case OP_DEFINE_GLOBAL_LONG:
      return longConstantInstruction("OP_DEFINE_GLOBAL_LONG", chunk, offset);

    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);

    case OP_GET_GLOBAL_LONG:
      return longConstantInstruction("OP_GET_GLOBAL_LONG", chunk, offset);

    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);

    case OP_SET_GLOBAL_LONG:
      return longConstantInstruction("OP_SET_GLOBAL_LONG", chunk, offset);

    case OP_GET_LOCAL:
      return oneByteInstruction("OP_GET_LOCAL", chunk, offset);

    case OP_GET_LOCAL_LONG:
      return twoBytesInstruction("OP_GET_LOCAL_LONG", chunk, offset);

    case OP_SET_LOCAL:
      return oneByteInstruction("OP_SET_LOCAL", chunk, offset);

    case OP_SET_LOCAL_LONG:
      return twoBytesInstruction("OP_SET_LOCAL_LONG", chunk, offset);

    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);

    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);

    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);

    case OP_JUMP_IF_TRUE:
      return jumpInstruction("OP_JUMP_IF_TRUE", chunk, 1, offset);

    case OP_JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", chunk, 1, offset);

    case OP_JUMP:
      return jumpInstruction("OP_JUMP", chunk, 1, offset);

    case OP_LOOP:
      return jumpInstruction("OP_LOOP", chunk, -1, offset);

    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);

    case OP_POP:
      return simpleInstruction("OP_POP", offset);

    case OP_POPN:
      return popNInstruction("OP_POPN", chunk, offset);

    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
