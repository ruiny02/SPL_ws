#include "helper.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#define LOG_10_2 302.0 / 1000


Command* default_command() {
  Command* command = malloc(sizeof *command);
  *command = (Command){
      .args = {NULL},
      .stdin = NULL,
      .stdout = NULL,
      .num_args = 0,
      .append_stdout = false,
      .type = COMMAND_NULL,
      .next_command = NULL,
  };
  return command;
}

static void free_command(Command* command) {
  if (command == NULL) return;
  free(command->stdin);
  free(command->stdout);

  for (size_t i = 0; i < command->num_args; i++) {
    free(command->args[i]);
    command->args[i] = NULL;
  }

  free(command);
}

static void free_pipeline_tree(Pipeline* pipeline) {
  while (pipeline != NULL) {
    Pipeline* next_pipeline = pipeline->next_command_line;

    Command* command = pipeline->first_command;
    while (command != NULL) {
      Command* next_command = command->next_command;
      free_command(command);
      command = next_command;
    }

    free(pipeline);
    pipeline = next_pipeline;
  }
}

void free_input(Input* input) {
  if (input == NULL) return;

  free_pipeline_tree(input->first_command_line);
  input->first_command_line = NULL;
  input->num_command_lines = 0;
}


bool is_special_char(const char c) {
  return c == '<' || c == '>' || c == '|' || c == '\'' || c == '"' || c == '&' || c == ';' || c == '\n' || c == '\0' || c == ' ';
}

bool is_separator(const Token token) {
  return token.type == TOKEN_SEP;
}

bool is_background(const Token token) {
  return token.type == TOKEN_BACKGROUND;
}

bool is_pipe(const Token token) {
  return token.type == TOKEN_PIPE;
}

bool is_redirection(const Token token) {
  return token.type == TOKEN_REDIRECT_IN || token.type == TOKEN_REDIRECT_OUT ||
         token.type == TOKEN_REDIRECT_APPEND;
}

bool is_end(const Token token) {
  return token.type == TOKEN_END;
}

bool is_builtin_command(const Token token) {
  const char* builtin_commands[] = {"cd", "exit", "pwd", "jobs",
                                    "fg", "bg",   NULL};

  for (int i = 0; builtin_commands[i] != NULL; i++) {
    if (strcmp(token.data, builtin_commands[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool is_implemented_command(const Token token) {
  const char* implemented_commands[] = {
      "pa2", "pa2_head", "pa2_tail", "pa2_cat", 
      "pa2_cp", "pa2_mv", "pa2_rm", "pa2_echo", NULL};

  for (int i = 0; implemented_commands[i] != NULL; i++) {
    if (strcmp(token.data, implemented_commands[i]) == 0) {
      return true;
    }
  }
  return false;
}

bool is_command(Token token) {
  return token.type == TOKEN_COMMAND;
}


bool is_argument(Token token) {
  return token.type == TOKEN_ARGUMENT;
}

bool is_variable(Token token) {
  return token.type == TOKEN_VARIABLE;
}

bool is_single_builtin_command(Pipeline* pipeline) {
  return pipeline->num_commands == 1 &&
         pipeline->first_command->type == COMMAND_BUILTIN;
}

const char* token_type_string(TokenType type) {
  switch (type) {
    case TOKEN_COMMAND:
      return "command";
    case TOKEN_ARGUMENT:
      return "argument";
    case TOKEN_VARIABLE:
      return "variable";
    case TOKEN_SEP:
      return "separator";
    case TOKEN_BACKGROUND:
      return "background";
    case TOKEN_PIPE:
      return "pipe";
    case TOKEN_REDIRECT_IN:
      return "redirect_in";
    case TOKEN_REDIRECT_OUT:
      return "redirect_out";
    case TOKEN_REDIRECT_APPEND:
      return "redirect_append";
    case TOKEN_END:
      return "end";
    default:
      return "unknown";
  }
}

char* dereference_variable(const char* var_name, status_t last_status) {
  // status of last command ($? or $status)
  if (var_name[0] == '?' || (strncmp(var_name, "status", 7) == 0 && var_name[6] == '\0')) {
      size_t max_digits = (size_t) ceil((double) (sizeof(status_t) * CHAR_BIT) * LOG_10_2) + 2;
      char* data = malloc(max_digits);
      snprintf(data, max_digits, "%d", last_status);
      return data;
  } else {
      char* value = getenv(var_name);
      return strdup(value != NULL ? value : "");
  }
}

char* to_argument(Token token, status_t last_status) {
  if (is_variable(token)) {
    return dereference_variable(token.data, last_status);
  } else {
    return strdup(token.data);
  }
}
status_t lex(const char* cmd, Token* tokens) {
  int token_i = 0;

  for (const char* curr_char = cmd; *curr_char != '\0'; curr_char++) {
    if (isspace(*curr_char) && *curr_char != '\n')
      continue;    

    if (token_i >= MAX_TOKENS - 1) {
      tokens[token_i] = (Token){.type = TOKEN_END, .data = NULL};
      fprintf(stderr, "warning: Too many tokens\n");
      return 2;
    }

    switch (*curr_char) {
      case '#':
        while (*curr_char != '\n' && *curr_char != '\0' && *curr_char != ';') curr_char++;
        curr_char--;
        break;
      case '$': {
        const char* var_start = ++curr_char;
        while (!is_special_char(*curr_char)) {
          curr_char++;
        }
        tokens[token_i++] = (Token){
            .type = TOKEN_VARIABLE,
            .data = strndup(var_start, curr_char - var_start),
        };
        curr_char--;
      } break;
      case '~':
        tokens[token_i++] = (Token){
            .type = TOKEN_VARIABLE,
            .data = strdup("HOME"),
        };
        break;
      case ';':
        tokens[token_i++] = (Token){.type = TOKEN_SEP, .data = ";"};
        break;
      case '\n':
        tokens[token_i++] = (Token){.type = TOKEN_SEP, .data = "\n"};
        break;
      case '|':
        tokens[token_i++] = (Token){.type = TOKEN_PIPE, .data = "|"};
        break;
      case '<':
        tokens[token_i++] = (Token){.type = TOKEN_REDIRECT_IN, .data = "<"};
        break;
      case '>':
        if (*(curr_char + 1) == '>') {
          tokens[token_i++] =
              (Token){.type = TOKEN_REDIRECT_APPEND, .data = ">>"};
          curr_char++;
        } else {
          tokens[token_i++] = (Token){.type = TOKEN_REDIRECT_OUT, .data = ">"};
        }
        break;
      case '&':
        tokens[token_i++] = (Token){.type = TOKEN_BACKGROUND, .data = "&"};
        break;
      case '\'':
      case '\"': {
        // not required for PA2, it just makes checking commands much easier
        // since some commands almost require quotes (i.e awk)
        char quote = *curr_char;
        const char* start = ++curr_char;
        while (*curr_char != quote && *curr_char != '\0') {
          curr_char++;
        }
        tokens[token_i++] = (Token){
            .type = TOKEN_ARGUMENT,
            .data = strndup(start, curr_char - start),
        };
      } break;
      default: {
        const char* command_start = curr_char;

        TokenType type = (token_i == 0 || is_pipe(tokens[token_i - 1]) || is_separator(tokens[token_i - 1]))
                             ? TOKEN_COMMAND
                             : TOKEN_ARGUMENT;

        while (!is_special_char(*curr_char)) {
          curr_char++;
        }

        tokens[token_i] =
            (Token){.type = type,
                    .data = strndup(command_start, curr_char - command_start + 1)};
        tokens[token_i++].data[curr_char - command_start] = '\0';
      
        curr_char--;        
        break;
      }
    }  
  }

  tokens[token_i] = (Token){.type = TOKEN_END, .data = NULL};  

  switch (tokens[token_i - 1].type) {
    case TOKEN_PIPE:
      fprintf(stderr, "warning: Trailing pipe with no command after it\n");
      return 1;
    case TOKEN_REDIRECT_IN:
    case TOKEN_REDIRECT_OUT:
    case TOKEN_REDIRECT_APPEND:
      fprintf(stderr, "warning: Trailing redirection with no filename after it\n");
      return 1;
    default:
      break;
  }

  return 0;
}

void free_tokens(Token tokens[]) {
  for (int i = 0; tokens[i].type != TOKEN_END; i++) {
    if (tokens[i].type == TOKEN_COMMAND || tokens[i].type == TOKEN_ARGUMENT || tokens[i].type == TOKEN_VARIABLE) {
      free(tokens[i].data);
      tokens[i].data = NULL;
    }
  }
}

Pipeline* default_pipeline() {
  Pipeline* pipeline = malloc(sizeof *pipeline);
  *pipeline = (Pipeline){
      .is_in_background = false,
      .first_command = default_command(),
      .next_command_line = NULL,
      .num_commands = 0,
  };
  return pipeline;
}

bool pipeline_is_empty(Pipeline* pipeline) {
  return pipeline == NULL || pipeline->num_commands == 0 || 
         (pipeline->num_commands == 1 && pipeline->first_command->num_args >= 1 && pipeline->first_command->args[0][0] == '#');
}

bool is_last_token(Token* tokens, int i) {
  for (int j = i + 1; tokens[j].type != TOKEN_END; j++) {
    if (tokens[j].type != TOKEN_SEP) return false;
  }
  return true;
}

status_t parse(Token* tokens, Input* input, status_t last_status) {
  status_t status = 0;

  input->first_command_line = default_pipeline();
  Pipeline* current_pipeline = input->first_command_line;
  input->num_command_lines = 1;
  Command* current_command = current_pipeline->first_command;

  int i = 0;
  bool stop = false;

  while (!stop) {
    if (is_pipe(tokens[i]) || is_redirection(tokens[i]) ||
        is_background(tokens[i]) || is_separator(tokens[i]) || 
        is_end(tokens[i])) {        
      switch (tokens[i].type) {
        case TOKEN_END:
          if (current_command->num_args > 0) {
            current_command->args[current_command->num_args] = NULL;
          }
          stop = true;
          // fall through
        case TOKEN_SEP:      
          if (!(current_pipeline->num_commands == 0 || (
            current_pipeline->num_commands == 1 &&
            current_command->num_args == 0)) && !stop
          ) {
            current_pipeline->next_command_line = default_pipeline();
            current_command->args[current_command->num_args] = NULL;
            current_pipeline = current_pipeline->next_command_line;
            current_pipeline->next_command_line = NULL;
            input->num_command_lines++;
            current_command = current_pipeline->first_command;
          }
          break;
        case TOKEN_PIPE:
          if (current_command->num_args == 0) {
            fprintf(stderr, "warning: Missing command before pipe\n");
            status = 1;
            goto cleanup;
          }
          if (!is_command(tokens[i + 1])) {
            fprintf(stderr, "warning: Missing command after pipe\n");
            status = 1;
            goto cleanup;
          }
          current_command->args[current_command->num_args] = NULL;
          current_command->next_command = default_command();
          current_command = current_command->next_command;
          break;
        case TOKEN_REDIRECT_IN:
          i++;

          if (!(is_argument(tokens[i]) || is_variable(tokens[i]))) {
            fprintf(stderr, "warning: Expected a string, but found '%s'\n", token_type_string(tokens[i].type));
            status = 1;
            goto cleanup;
          }

          current_command->stdin = to_argument(tokens[i], last_status);          
          break;
        case TOKEN_REDIRECT_APPEND:
          current_command->append_stdout = true;
          // fall through
        case TOKEN_REDIRECT_OUT:
          i++;

          if (!(is_argument(tokens[i]) || is_variable(tokens[i]))) {
            fprintf(stderr, "warning: Expected a string, but found '%s'\n", token_type_string(tokens[i].type));
            status = 1;
            goto cleanup;
          }
          current_command->stdout = to_argument(tokens[i], last_status);
          break;
        case TOKEN_BACKGROUND:
          current_pipeline->is_in_background = true;
          break;
        default:
          fprintf(stderr, "fatal: Invalid token: %s\n", tokens[i].data);
          exit(1);
      }
    } else {
      if (current_command->num_args == 0) {
        if (tokens[i].type != TOKEN_COMMAND) {
          fprintf(stderr, "warning: Expected a string, but found '%s'\n", token_type_string(tokens[i].type));
          status = 1;
          goto cleanup;
        }

        current_pipeline->num_commands++;

        if (is_builtin_command(tokens[i])) {
          current_command->type = COMMAND_BUILTIN;
        } else if (is_implemented_command(tokens[i])) {
          current_command->type = COMMAND_IMPLEMENTED;
        } else {
          current_command->type = COMMAND_OTHER;
        }
      } 
      current_command->args[current_command->num_args++] = to_argument(tokens[i], last_status); 
    }
    i+=1;
    if (is_end(tokens[i])) stop = true;
  }

cleanup:
  if (status != 0) free_input(input);
  return status;
}