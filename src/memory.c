#include "memory.h"
#include "chunk.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#include <stdlib.h>

static void freeObject(Object *object) {
  switch (object->type) {
    case OBJ_STRING: {
      ObjString *string = (ObjString *)object;
      FREE_STRING(string, string->length);
      break;
    }

    case OBJ_FUNCTION: {
      ObjFunction *function = (ObjFunction *)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
  }
}

void freeObjects() {
  Object *current = vm.objects;

  while (current != NULL) {
    Object *next = current->next;
    freeObject(current);
    current = next;
  }
}

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);

  if (result == NULL) {
    exit(1);
  }

  return result;
}
