#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value)))
#define AS_CLOSURE(value) (((ObjClosure *)AS_OBJ(value)))

typedef enum {
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
} ObjType;

struct Object {
  ObjType type;
  Object *next;
};

typedef struct {
  Object object;
  uint32_t arity;
  uint32_t upvalueCount;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef Value (*NativeFn)(uint32_t argumentCount, Value* args);

typedef struct {
  Object object;
  uint32_t arity;
  NativeFn function;
} ObjNative;

struct ObjString {
  Object object;
  size_t length;
  uint32_t hash;
  char chars[];
};

typedef struct _ObjUpvalue {
  Object object;
  Value *location;
  Value closed;
  struct _ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
  Object object;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  uint32_t upvalueCount;
} ObjClosure;

static bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

ObjUpvalue *newUpvalue(Value *slot);
ObjClosure *newClosure(ObjFunction *function);
ObjFunction *newFunction();
ObjNative *newNative(NativeFn function, uint32_t arity);
ObjString *takeString(char *chars, size_t length);
ObjString *copyString(const char *chars, size_t length);
ObjString *concatenate(ObjString *a, ObjString *b);
void printObject(Value value);

#endif
