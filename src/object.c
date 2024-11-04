#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

#define ALLOCATE_STRING(length)                                                \
  (ObjString *)allocateObject(sizeof(ObjString) + (length + 1) * sizeof(char), \
                              OBJ_STRING)

static Object *allocateObject(size_t size, ObjType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm.objects;
  vm.objects = object;
  return object;
}

ObjFunction *newFunction() {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->name = NULL;
  initChunk(&function->chunk);

  return function;
}

ObjNative *newNative(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

static ObjString *allocateString(const char *chars, size_t length,
                                 uint32_t hash) {
  ObjString *string = (ObjString *)allocateObject(
      sizeof(ObjString) + (length + 1) * sizeof(char), OBJ_STRING);

  string->length = length;
  string->hash = hash;
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';

  tableSet(&vm.strings, string, NIL_VAL);

  return string;
}

static uint32_t hashString(const char *chars, size_t length) {
  uint32_t hash = 2166136261u;

  for (size_t i = 0; i < length; ++i) {
    hash ^= (uint8_t)chars[i];
    hash *= 16777619;
  }

  return hash;
}

ObjString *concatenate(ObjString *a, ObjString *b) {
  size_t length = a->length + b->length;
  ObjString *result = ALLOCATE_STRING(length);

  result->length = length;
  memcpy(result->chars, a->chars, a->length);
  memcpy(result->chars + a->length, b->chars, b->length);
  result->chars[length] = '\0';

  uint32_t hash = hashString(result->chars, length);
  result->hash = hash;

  ObjString *interned =
      tableFindString(&vm.strings, result->chars, length, hash);
  if (interned != NULL) {
    FREE_STRING(result, length);
    return interned;
  }

  return result;
}

ObjString *takeString(char *chars, size_t length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, size_t length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    return interned;
  }

  return allocateString(chars, length, hash);
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;

    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;

    case OBJ_NATIVE:
      printf("<native fn>");
      break;
  }
}
