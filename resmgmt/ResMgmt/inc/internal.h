#ifndef _INTERNAL_H
#define _INTERNAL_H

#include "general.h"
#include "stdio.h"
#include <stdbool.h>

/* 内存池余量: new_spec + 25% + 8 */
#define POOL_MARGIN(n)  ((n) + ((n) >> 2) + 8)

typedef enum{
    RET_ERR          = -1,
    RET_OK           = 0,
    RET_PTHREAD_ERR  = -2,
    RET_TCP_ERR      = -3,

}_RET_CODE;



int init_ipc_msg(void);
int init_ipc_queue(void);

#endif


