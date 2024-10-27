#include "line.h"
#include "memory.h"
#include <limits.h>

void initLineArray(LineArray *array) {
  array->lines = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeLineArray(LineArray *array, uint32_t line) {
  if (array->count != 0 && array->lines[array->count - 1].line == line &&
      array->lines[array->count - 1].run < UINT32_MAX) {
    array->lines[array->count - 1].run++;
    return;
  }

  if (array->count == array->capacity) {
    size_t oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->lines = GROW_ARRAY(Line, array->lines, oldCapacity, array->capacity);
  }

  Line newLine;
  newLine.line = line;
  newLine.run = 1;

  array->lines[array->count] = newLine;
  array->count++;
}

void freeLineArray(LineArray *array) {
  FREE_ARRAY(Line, array->lines, array->capacity);
  initLineArray(array);
}

uint32_t getLine(LineArray *array, size_t offset) {
  size_t traversed = 0;

  for (size_t i = 0; i < array->count; ++i) {
    traversed += array->lines[i].run;

    if (traversed >= offset) {
      return array->lines[i].line;
    }
  }

  return array->lines[array->count - 1].line;
}
