#ifndef _IPC_API_H
#define _IPC_API_H

/*--- 项目头文件 ---*/
#include "ipc_types.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件（无） ---*/

void* send_thread_func(void* arg);
void* recv_thread_func(void* arg);
int tcp_client_init(const char* server_ip, int server_port);

int init_ipc_msg(void);
int init_ipc_queue(void);

void tst_send(char *pData);
void tst_recv();

#endif
