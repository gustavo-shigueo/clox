#ifndef clox_line_h
#define clox_line_h

#include "common.h"

typedef struct {
  uint32_t run;
  uint32_t line;
} Line;

typedef struct {
  size_t count;
  size_t capacity;
  Line *lines;
} LineArray;

void initLineArray(LineArray *array);
void writeLineArray(LineArray *array, uint32_t line);
void freeLineArray(LineArray *array);
uint32_t getLine(LineArray *array, size_t offset);

#endif // !clox_line_h
