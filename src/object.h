#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
} ObjType;

struct Object {
  ObjType type;
  Object *next;
};

typedef struct {
  Object object;
  uint32_t arity;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

struct ObjString {
  Object object;
  size_t length;
  uint32_t hash;
  char chars[];
};

static bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

ObjFunction *newFunction();
ObjString *takeString(char *chars, size_t length);
ObjString *copyString(const char *chars, size_t length);
ObjString *concatenate(ObjString *a, ObjString *b);
void printObject(Value value);

#endif
