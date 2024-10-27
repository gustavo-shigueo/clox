#include "scanner.h"
#include "common.h"
#include <string.h>

typedef struct {
  const char *start;
  const char *current;
  uint32_t line;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool isAtEnd() { return *scanner.current == '\0'; }

static Token makeToken(TokenType type) {
  Token token;
  token.line = scanner.line;
  token.start = scanner.start;
  token.length = (uint32_t)(scanner.current - scanner.start);
  token.type = type;

  return token;
}

static Token errorToken(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = strlen(message);
  token.line = scanner.line;

  return token;
}

static char peek() { return *scanner.current; }

static char peekNext() {
  if (isAtEnd()) {
    return '\0';
  }

  return scanner.current[1];
}

static char advance() { return *scanner.current++; }

static bool match(char c) {
  if (isAtEnd()) {
    return false;
  }

  if (*scanner.current != c) {
    return false;
  }

  scanner.current++;
  return true;
}

static void skipWhitespace() {
  while (true) {
    char c = peek();

    switch (c) {
      case ' ':
      case '\t':
      case '\r':
        advance();
        break;

      case '\n':
        scanner.line++;
        advance();
        break;

      case '/':
        if (peekNext() == '/') {
          while (!isAtEnd() && peek() != '\n') {
            advance();
          }
        } else {
          return;
        }

        break;

      default:
        return;
    }
  }
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static Token string() {
  while (!isAtEnd() && peek() != '"') {
    if (advance() == '\n') {
      scanner.line++;
    }
  }

  if (isAtEnd()) {
    return errorToken("Unterminated string.");
  }

  advance();
  return makeToken(TOKEN_STRING);
}

static Token number() {
  while (isDigit(peek()) || peek() == '_') {
    advance();
  }

  if (peek() == '.' && isDigit(peekNext())) {
    advance();

    while (isDigit(peek()) || peek() == '_') {
      advance();
    }
  }

  return makeToken(TOKEN_NUMBER);
}

static TokenType checkKeyword(size_t start, size_t length, const char *rest,
                              TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
  switch (*scanner.start) {
    case 'a':
      return checkKeyword(1, 2, "nd", TOKEN_AND);

    case 'c':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'l':
            return checkKeyword(2, 3, "ass", TOKEN_CLASS);

          case 'o':
            return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
        }
      }

    case 'e':
      return checkKeyword(1, 3, "lse", TOKEN_ELSE);

    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a':
            return checkKeyword(2, 3, "lse", TOKEN_FALSE);

          case 'o':
            return checkKeyword(2, 1, "r", TOKEN_FOR);

          case 'u':
            return checkKeyword(2, 1, "n", TOKEN_FUN);
        }
      }
      break;

    case 'i':
      return checkKeyword(1, 1, "f", TOKEN_IF);

    case 'n':
      return checkKeyword(1, 2, "il", TOKEN_NIL);

    case 'o':
      return checkKeyword(1, 1, "r", TOKEN_OR);

    case 'p':
      return checkKeyword(1, 4, "rint", TOKEN_PRINT);

    case 'r':
      return checkKeyword(1, 5, "eturn", TOKEN_RETURN);

    case 's':
      return checkKeyword(1, 4, "uper", TOKEN_SUPER);

    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h':
            return checkKeyword(2, 2, "is", TOKEN_THIS);
          case 'r':
            return checkKeyword(2, 2, "ue", TOKEN_TRUE);
        }
      }
      break;

    case 'v':
      return checkKeyword(1, 2, "ar", TOKEN_VAR);

    case 'w':
      return checkKeyword(1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) {
    advance();
  }

  return makeToken(identifierType());
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;

  if (isAtEnd()) {
    return makeToken(TOKEN_EOF);
  }

  char c = advance();

  if (isDigit(c)) {
    return number();
  }

  if (isAlpha(c)) {
    return identifier();
  }

  switch (c) {
    case '(':
      return makeToken(TOKEN_LEFT_PAREN);

    case ')':
      return makeToken(TOKEN_RIGHT_PAREN);

    case '{':
      return makeToken(TOKEN_LEFT_BRACE);

    case '}':
      return makeToken(TOKEN_RIGHT_BRACE);

    case ';':
      return makeToken(TOKEN_SEMICOLON);

    case ',':
      return makeToken(TOKEN_COMMA);

    case '.':
      return makeToken(TOKEN_DOT);

    case '-':
      return makeToken(TOKEN_MINUS);

    case '+':
      return makeToken(TOKEN_PLUS);

    case '/':
      return makeToken(TOKEN_SLASH);

    case '*':
      return makeToken(TOKEN_STAR);

    case '?':
      return makeToken(TOKEN_QUESTION_MARK);

    case ':':
      return makeToken(TOKEN_COLON);

    case '!':
      return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);

    case '=':
      return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);

    case '<':
      return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);

    case '>':
      return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

    case '"':
      return string();
  }

  return errorToken("Unexpected character.");
}