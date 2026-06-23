#ifndef _IPC_TYPES_H
#define _IPC_TYPES_H

/*--- 项目头文件（无） ---*/

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


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
    uint32_t magic;    // 固定魔数 0x1255CC78
    uint32_t length;   // 消息总长度
    uint32_t type;     // 消息类型
    uint32_t checksum; // 可选校验
} IPCMsgHeader;

// 示例消息体（可变长度，实际使用时将 data 作为变长数组）
typedef struct {
    IPCMsgHeader header;
    char data[1];      // 实际分配 length - sizeof(MessageHeader)
} _IPCMsg;


#endif
