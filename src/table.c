#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"

#include <stdlib.h>
#include <string.h>

#define TABLE_MAX_LOAD 0.75

static Entry *findEntry(Entry *entries, uint32_t capacity, ObjString *key) {
  Entry *tombstone = NULL;
  uint32_t index = key->hash % capacity;

  while (true) {
    Entry *entry = &entries[index];
    ObjString *entryKey = entry->key;

    if (entryKey == NULL) {
      if (IS_NIL(entry->value)) {
        return tombstone != NULL ? tombstone : entry;
      } else {
        if (tombstone == NULL) {
          tombstone = entry;
        }
      }
    } else if (entryKey == key) {
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

static void adjustCapacity(Table *table, uint32_t capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);

  for (uint32_t i = 0; i < capacity; ++i) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  uint32_t oldCapacity = table->capacity;
  Entry *oldEntries = table->entries;

  for (uint32_t i = 0; i < oldCapacity; ++i) {
    Entry *entry = &oldEntries[i];
    ObjString *key = entry->key;

    if (key == NULL) {
      continue;
    }

    Entry *dest = findEntry(entries, capacity, key);
    dest->key = key;
    dest->value = entry->value;
    ++table->count;
  }

  FREE_ARRAY(Entry, oldEntries, oldCapacity);

  table->entries = entries;
  table->capacity = capacity;
}

void tableAllTo(Table *from, Table *to) {
  uint32_t fromCapacity = from->capacity;
  Entry *fromEntries = from->entries;

  for (uint32_t i = 0; i < fromCapacity; ++i) {
    Entry *entry = &fromEntries[i];

    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

void initTable(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

bool tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0) {
    return false;
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);

  if (entry->key == NULL) {
    return false;
  }

  *value = entry->value;
  return true;
}

bool tableSet(Table *table, ObjString *key, Value value) {
  if (table->count >= table->capacity * TABLE_MAX_LOAD) {
    uint32_t capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;

  if (isNewKey && IS_NIL(entry->value)) {
    ++table->count;
  }

  entry->key = key;
  entry->value = value;

  return isNewKey;
}

bool tableDelete(Table *table, ObjString *key) {
  if (table->count == 0) {
    return false;
  }

  Entry *entry = findEntry(table->entries, table->capacity, key);

  if (entry->key == NULL) {
    return false;
  }

  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

ObjString *tableFindString(Table *table, const char *chars, size_t length,
                           uint32_t hash) {
  if (table->count == 0) {
    return NULL;
  }

  uint32_t capacity = table->capacity;
  uint32_t index = hash % capacity;
  Entry *entries = table->entries;

  while (true) {
    Entry *entry = &entries[index];
    ObjString *key = entry->key;

    if (key == NULL) {
      if (IS_NIL(entry->value)) {
        return NULL;
      }
    } else if (key->length == length && key->hash == hash &&
               memcmp(chars, key->chars, length) == 0) {
      return key;
    }

    index = (index + 1) % capacity;
  }
}
