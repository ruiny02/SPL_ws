#ifndef CLIENT_HELPER_H
#define CLIENT_HELPER_H
#include "helper.h"

Action to_action(const char* action_str);
ParsingError parse_request(Request* request,
                           char* input,
                           const char** active_user);
void free_input(char** input);

bool line_is_empty(const char* line);
bool evaluate(char* input, int32_t sockfd, const char** active_user);

// you have to implement these in pa3_client.c
void send_request(int32_t sockfd, Request* request);
void receive_response(int32_t sockfd, Response* response);
void terminate(int32_t sockfd, const char* active_user);
#endif
