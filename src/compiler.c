#include "compiler.h"
#include "chunk.h"
#include "common.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_TERNARY,    // ?:
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int32_t depth;
} Local;

typedef struct {
  Local *locals;
  uint32_t localCount;
  int32_t scopeDepth;
  int32_t loopStart;
  uint32_t loopDepth;
} Compiler;

typedef struct {
  Token current;
  Token previous;
  bool panicMode;
  bool hadError;
} Parser;

Parser parser;
Compiler *current = NULL;

Chunk *compilingChunk;

static Chunk *currentChunk() { return compilingChunk; }

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode) {
    return;
  }

  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); };

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
};

static void advance() {
  parser.previous = parser.current;

  while (true) {
    parser.current = scanToken();

    if (parser.current.type != TOKEN_ERROR) {
      break;
    }

    errorAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  }

  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

/// Emits 4 bytes
static void emitWord(uint8_t byte1, uint8_t byte2, uint8_t byte3,
                     uint8_t byte4) {
  emitByte(byte1);
  emitByte(byte2);
  emitByte(byte3);
  emitByte(byte4);
}

static size_t emitJump(OpCode instruction) {
  emitByte(instruction);
  emitBytes(0xff, 0xff);

  return currentChunk()->count - 2;
}

static void emitLoop(size_t loopStart) {
  emitByte(OP_LOOP);

  size_t offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) {
    error("The loop body is too large");
  }

  emitBytes((offset >> 8) & 0xff, offset & 0xff);
}

static uint16_t makeConstant(Value value) {
  const uint16_t LIMIT = (1 << 16) - 1;
  uint16_t constantIndex = addConstant(currentChunk(), value);

  if (constantIndex == LIMIT) {
    error("Too many constants in one chunk");
    return 0;
  }

  return constantIndex;
}

static void emitConstant(Value value) {
  uint16_t index = makeConstant(value);

  if (index <= UINT8_MAX) {
    emitBytes(OP_CONSTANT, index);
  } else {
    emitByte(OP_CONSTANT_LONG);
    emitBytes(index >> 8, index & UINT8_MAX);
  }
}

static void patchJump(size_t offset) {
  Chunk *chunk = currentChunk();
  uint8_t *code = chunk->code;
  size_t jump = chunk->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump.");
  }

  code[offset] = (jump >> 8) & 0xff;
  code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->loopStart = -1;
  compiler->locals = malloc(sizeof(Local) * UINT16_MAX);
  current = compiler;
}

static ParseRule *getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void expression();
static void declaration();
static void varDeclaration();
static void statement();
static void printStatement();
static void ifStatement();
static void whileStatement();
static void forStatement();
static void continueStatement();
static void block();
static void expressionStatement();
static void grouping(bool canAssign);
static void unary(bool canAssign);
static void ternary(bool canAssign);
static void binary(bool canAssign);
static void and_(bool canAssign);
static void or_(bool canAssign);
static void number(bool canAssign);
static void string(bool canAssign);
static void literal(bool canAssign);
static void variable(bool canAssign);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_QUESTION_MARK] = {NULL, ternary, PREC_TERNARY},
    [TOKEN_COLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void parsePrecedence(Precedence precedence) {
  advance();

  ParseFn prefixRule = getRule(parser.previous.type)->prefix;

  if (prefixRule == NULL) {
    error("Expected expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static void beginScope() { ++current->scopeDepth; }

static void endScope() {
  --current->scopeDepth;

  Chunk *chunk = currentChunk();
  Local *locals = current->locals;
  int32_t scopeDepth = current->scopeDepth;

  uint8_t popCount = 0;
  while (current->localCount > 0 &&
         locals[current->localCount - 1].depth > scopeDepth) {
    ++popCount;
    --current->localCount;
  }

  if (popCount == 0) {
    return;
  }

  if (popCount == 1) {
    emitByte(OP_POP);
    return;
  }

  while (popCount > 0) {
    if (popCount <= UINT8_MAX) {
      emitBytes(OP_POPN, popCount);
      return;
    }

    emitBytes(OP_POPN, UINT8_MAX);
    popCount -= UINT8_MAX;
  }
}

static uint16_t findIdentifier(Token *name) {
  uint16_t index = UINT16_MAX;

  Value identifier = OBJ_VAL(copyString(name->start, name->length));
  ValueArray constants = currentChunk()->constants;

  for (uint16_t i = 0; i < constants.count; ++i) {
    if (valuesEqual(identifier, constants.values[i])) {
      return i;
    }
  }

  return index;
}

static uint16_t identifierConstant(Token *name) {
  uint16_t index = findIdentifier(name);

  if (index != UINT16_MAX) {
    return index;
  }

  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(const Token *a, const Token *b) {
  if (a->length != b->length) {
    return false;
  }

  return memcmp(a->start, b->start, a->length) == 0;
}

static void markInitialized() {
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static uint16_t resolveLocal(Compiler *compiler, Token *name) {
  Local *locals = compiler->locals;
  for (int32_t i = compiler->localCount - 1; i >= 0; --i) {
    Local *local = locals + i;
    if (identifiersEqual(&local->name, name)) {
      if (local->depth == -1) {
        error("Can't read variable in its own initializer");
      }
      return i;
    }
  }

  return UINT16_MAX;
}

static void addLocal(Token name) {
  printf("%c %d\n", name.start[0], current->scopeDepth);
  if (current->localCount == UINT16_COUNT) {
    error("Too many local variables in function");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
}

static void declareVariable() {
  if (current->scopeDepth == 0) {
    return;
  }

  Token *name = &parser.previous;

  Local *locals = current->locals;
  int32_t scopeDepth = current->scopeDepth;
  for (int32_t i = current->localCount - 1; i >= 0; --i) {
    Local *local = locals + i;

    if (local->depth != -1 && local->depth < scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("There is already a variable with this name in the current scope");
    }
  }

  addLocal(*name);
}

static uint16_t parseVariable(const char *message) {
  consume(TOKEN_IDENTIFIER, message);

  declareVariable();
  if (current->scopeDepth > 0) {
    return 0;
  }

  return identifierConstant(&parser.previous);
}

static void defineVariable(uint16_t index) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  if (index <= UINT8_MAX) {
    emitBytes(OP_DEFINE_GLOBAL, index);
  } else {
    emitByte(OP_DEFINE_GLOBAL_LONG);
    emitBytes(index >> 8, index & UINT8_MAX);
  }
}

static void synchronyze() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) {
      return;
    }

    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_IF:
      case TOKEN_FOR:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        advance();
    }
  }
}

static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode) {
    synchronyze();
  }
}

static void varDeclaration() {
  uint16_t global = parseVariable("Expected variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration");

  defineVariable(global);
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_CONTINUE)) {
    continueStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' at the end of statement");
  emitByte(OP_PRINT);
}

static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

  size_t thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  statement();

  size_t elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(TOKEN_ELSE)) {
    statement();
  }

  patchJump(elseJump);
}

static void whileStatement() {
  int32_t enclosingLoopStart = current->loopStart;
  int32_t enclosingLoopDepth = current->loopDepth;
  size_t loopStart = currentChunk()->count;

  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

  size_t exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  current->loopDepth = current->scopeDepth;
  current->loopStart = loopStart;
  statement();

  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
  current->loopStart = enclosingLoopStart;
  current->loopDepth = enclosingLoopDepth;
}

static void forStatement() {
  int32_t enclosingLoopStart = current->loopStart;
  int32_t enclosingLoopDepth = current->loopDepth;
  beginScope();
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'for'.");
  if (!match(TOKEN_SEMICOLON)) {
    if (match(TOKEN_VAR)) {
      varDeclaration();
    } else {
      expressionStatement();
    }
  }

  int32_t exitJump = -1;
  int32_t loopStart = currentChunk()->count;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after condition.");

    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    size_t bodyJump = emitJump(OP_JUMP);
    size_t incrementStart = currentChunk()->count;

    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after 'for' clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  current->loopDepth = current->scopeDepth;
  current->loopStart = loopStart;
  statement();
  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }

  endScope();
  current->loopStart = enclosingLoopStart;
  current->loopDepth = enclosingLoopDepth;
}

static void continueStatement() {
  int32_t enclosingDepth = current->scopeDepth;
  uint32_t enclosingLocalCount = current->localCount;

  if (current->loopStart == -1) {
    error("Cannot use continue outside of loop");
    return;
  }

  consume(TOKEN_SEMICOLON, "Expected ';'.");

  while (current->scopeDepth > current->loopDepth) {
    endScope();
  }

  emitLoop(current->loopStart);
  current->scopeDepth = enclosingDepth;
  current->localCount = enclosingLocalCount;
}

static void block() {
  while (!check(TOKEN_EOF) && !check(TOKEN_RIGHT_BRACE)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expected '}' after block");
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expected ';' at the end of statement");
  emitByte(OP_POP);
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE:
      emitByte(OP_FALSE);
      return;
    case TOKEN_TRUE:
      emitByte(OP_TRUE);
      return;
    case TOKEN_NIL:
      emitByte(OP_NIL);
      return;
    default:
      return; // unreachable
  }
}

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(
      copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
  OpCode getOp, getLongOp, setOp, setLongOp;
  uint16_t index = resolveLocal(current, &name);

  if (index == UINT16_MAX) {
    index = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    getLongOp = OP_GET_GLOBAL_LONG;
    setOp = OP_SET_GLOBAL;
    setLongOp = OP_SET_GLOBAL_LONG;
  } else {
    getOp = OP_GET_LOCAL;
    getLongOp = OP_GET_LOCAL_LONG;
    setOp = OP_SET_LOCAL;
    setLongOp = OP_SET_LOCAL_LONG;
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();

    if (index <= UINT8_MAX) {
      emitBytes(setOp, index);
    } else {
      emitByte(setOp);
      emitBytes(index >> 8, index & UINT8_MAX);
    }
  } else {
    if (index <= UINT8_MAX) {
      emitBytes(getOp, index);
    } else {
      emitByte(getOp);
      emitBytes(index >> 8, index & UINT8_MAX);
    }
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void number(bool canAssign) {
  Value value = NUMBER_VAL(strtod(parser.previous.start, NULL));
  emitConstant(value);
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
    case TOKEN_MINUS:
      emitByte(OP_NEGATE);
      break;
    case TOKEN_BANG:
      emitByte(OP_NOT);
      break;
    default:
      return; // unreachable
  }
}

static void ternary(bool canAssign) {
  Chunk *chunk = currentChunk();

  size_t thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  parsePrecedence(PREC_TERNARY);

  size_t elseJump = emitJump(OP_JUMP);
  emitByte(OP_POP);

  patchJump(thenJump);

  consume(TOKEN_COLON, "Expected ':' in ternary expression");
  parsePrecedence(PREC_TERNARY);
  patchJump(elseJump);
}

static void binary(bool canAssign) {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
    case TOKEN_PLUS:
      emitByte(OP_ADD);
      break;
    case TOKEN_MINUS:
      emitByte(OP_SUBTRACT);
      break;
    case TOKEN_STAR:
      emitByte(OP_MULTIPLY);
      break;
    case TOKEN_SLASH:
      emitByte(OP_DIVIDE);
      break;
    case TOKEN_BANG_EQUAL:
      emitByte(OP_NOT_EQUAL);
      break;
    case TOKEN_EQUAL_EQUAL:
      emitByte(OP_EQUAL_EQUAL);
      break;
    case TOKEN_GREATER:
      emitByte(OP_GREATER);
      break;
    case TOKEN_GREATER_EQUAL:
      emitByte(OP_GREATER_EQUAL);
      break;
    case TOKEN_LESS:
      emitByte(OP_LESS);
      break;
    case TOKEN_LESS_EQUAL:
      emitByte(OP_LESS_EQUAL);
      break;
    default:
      return; // unreachable
  }
}

static void and_(bool canAssign) {
  size_t endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

static void or_(bool canAssign) {
  size_t endJump = emitJump(OP_JUMP_IF_TRUE);

  emitByte(OP_POP);
  parsePrecedence(PREC_OR);

  patchJump(endJump);
}

static void emitReturn() { emitByte(OP_RETURN); }

static void endCompiler() {
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }
#endif

  emitReturn();
  free(current->locals);
}

bool compile(const char *source, Chunk *chunk) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler);
  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  endCompiler();
  return !parser.hadError;
}
