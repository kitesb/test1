#ifndef _IPCDATA_H
#define _IPCDATA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>


#define MAX_MSG_SIZE    8192
#define MAX_PAYLOAD_SIZE (MAX_MSG_SIZE - sizeof(IPCMsgHeader))
#define QUEUE_WAIT_MS    100
#define WORK_SLEEP_MS    100
#define STATUS_INTERVAL_SEC   2


// 消息类型枚举
typedef enum {
    MSG_HEARTBEAT = 0x01,
} IPCMsgType;

// 消息头（固定 8 字节）
typedef struct {
    uint32_t magic;    // 固定魔数，如 0x1255CC78
    uint32_t length;   // 消息总长度
    uint32_t type;     // 消息类型
    uint32_t checksum; // 可选校验和
} IPCMsgHeader;

// 示例消息体（可变长度数据用柔性数组）
typedef struct {
    IPCMsgHeader header; 
    char data[1];      // 实际分配 length - sizeof(MessageHeader)
} _IPCMsg;


void* send_thread_func(void* arg);
void* recv_thread_func(void* arg);
int tcp_client_init(const char* server_ip, int server_port);




#endif

