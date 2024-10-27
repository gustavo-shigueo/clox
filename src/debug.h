#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassembleChunk(Chunk *chunk, const char *name);
size_t disassembleInstruction(Chunk *chunk, size_t offset);

#endif // !clox_debug_h
