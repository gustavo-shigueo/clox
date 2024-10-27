#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)

typedef enum {
  OBJ_STRING,
} ObjType;

struct Object {
  ObjType type;
  Object *next;
};

struct ObjString {
  Object object;
  size_t length;
  uint32_t hash;
  char chars[];
};

static bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

ObjString *takeString(char *chars, size_t length);
ObjString *copyString(const char *chars, size_t length);
ObjString *concatenate(ObjString *a, ObjString *b);
void printObject(Value value);

#endif
