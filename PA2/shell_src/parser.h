#ifndef PARSING_H
#define PARSING_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MAX_TOKENS 1000

typedef enum {
  TOKEN_COMMAND,
  TOKEN_ARGUMENT,
  TOKEN_VARIABLE,         // $VAR | ~
  TOKEN_PIPE,             // |
  TOKEN_REDIRECT_IN,      // <
  TOKEN_REDIRECT_OUT,     // >
  TOKEN_REDIRECT_APPEND,  // >>
  TOKEN_BACKGROUND,       // &
  TOKEN_SEP,              // ; or \n
  TOKEN_END,
} TokenType;

typedef struct token {
  TokenType type;
  char* data;
} Token;

void free_tokens(Token tokens[]);

typedef enum {
  COMMAND_NULL,
  COMMAND_BUILTIN,      // cd, pwd, exit, fg, bg, jobs
  COMMAND_IMPLEMENTED,  // head, tail, cp, mv, rm
  COMMAND_OTHER
} CommandType;

typedef struct Command {
  char* args[MAX_TOKENS];
  char* stdin;
  char* stdout;
  size_t num_args;
  bool append_stdout;
  CommandType type;
  struct Command* next_command;
} Command;

Command* default_command();

typedef struct Pipeline {
  bool is_in_background;
  Command* first_command;
  struct Pipeline* next_command_line;
  uint64_t num_commands;
} Pipeline;

Pipeline* default_pipeline();
bool pipeline_is_empty(Pipeline* pipeline);

typedef struct {
  Pipeline* first_command_line;
  size_t num_command_lines;
} Input;

void free_input(Input* input);

bool is_background(const Token token);
bool is_special_char(const char c);
bool is_pipe(const Token token);
bool is_redirection(const Token token);
bool is_builtin_command(const Token token);
bool is_implemented_command(const Token token);
bool is_single_builtin_command(Pipeline* pipeline);

int lex(const char* cmd, Token* tokens);
int parse(Token* tokens, Input* input, int last_status);

#endif
