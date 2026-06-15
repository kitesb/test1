#ifndef _REDIRECT_H
#define _REDIRECT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void redirect_stdio_to_socket(void);
void redirect_stdio_to_file(void);
bool start_stdout_to_socket(void);
bool start_stderr_to_socket(void);
bool stop_stdout_to_socket(void);
bool stop_stderr_to_socket(void);
void redirect_stdin_to_socket(void);
int  write_data_to_stdin(char *pData, int mLen);
void close_redirect_stdin(void);

#ifdef __cplusplus
}
#endif

#endif
