#ifndef _LOG_MGMT_H
#define _LOG_MGMT_H

/*--- 项目头文件 ---*/
#include "general.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <stdbool.h>
#include <stdio.h>

#ifndef  PTR_NULL
#define PTR_NULL    NULL
#endif

// ---- 基础 I/O 宏 ----
#define ERR_PRINT(format, ...)  fprintf(stderr, format, ##__VA_ARGS__);

#if 1
#define  DBG_printf(format, ...)  printf(format, ##__VA_ARGS__)
#else
#define  DBG_printf(format, ...)
#endif

// ---------------  日志文件相关定义  start
// 确定好日志的存储地址后，定义如下路径  
#define  LOG_FILE_PATH   "./log/"
#define  LOG_FILE_NAME   "ResLog_0504_0001.txt"
#define  LOG_FILE_PATH_LEN  6
#define  LOG_FILE_NAME_LEN  20
#define  LOG_FILE_LEN    (LOG_FILE_PATH_LEN + LOG_FILE_NAME_LEN + 2)
#define  LOG_FILE_DATE   (LOG_FILE_PATH_LEN + 7)      // 日期的起始地址
#define  LOG_FILE_NAME_NUM    12  // 文件名起始地址
#define  LOG_FILE_NUM    (LOG_FILE_PATH_LEN + LOG_FILE_NAME_NUM) // 序号的起始地址，不含路径



// 日志打印
#define DBG_DEBUG(level, format, ...)  printf_log2(level, format, ##__VA_ARGS__)
// error级别的日志打印
#define DBG_ERR(format, ...)     printf_log2(LOG_ERR,  format, ##__VA_ARGS__)
// warning级别的日志打印
#define DBG_WARN(format, ...)    printf_log2(LOG_WARN, format, ##__VA_ARGS__)
// debug级别的日志打印
#define DBG_DBG(format, ...)     printf_log2(LOG_DBG,  format, ##__VA_ARGS__)
// info级别的日志打印
#define DBG_INFO(format, ...)    printf_log2(LOG_INFO,  format, ##__VA_ARGS__)
// 函数进入及关键的日志打印
#define DBG_ENTER(format, ...)   printf_log2(LOG_ENTER,  format, ##__VA_ARGS__)
// 函数正常返回的日志打印
#define DBG_EXIT(format, ...)    printf_log2(LOG_EXIT,  format, ##__VA_ARGS__)

typedef enum{
    LOG_ERR   = 1,
    LOG_WARN  = 2,
    LOG_DBG   = 3,
    LOG_INFO  = 4,
    LOG_ENTER = 5,
    LOG_EXIT  = 6,

}_ENUM_LOG_LEVEL;



void deal_log_file(int8_t *pFileName);
void printf_log2(uint32_t level, int8_t *format, ...);

extern uint32_t gLogLevel;
extern int8_t   LOG_FILE[];

#endif
