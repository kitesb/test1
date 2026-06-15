#ifndef _DEALSOC_INTERNAL_H
#define _DEALSOC_INTERNAL_H

#include "dealsoc.h"
#include "dealsoc_types.h"
#include "soc_context.h"
#include <stddef.h>   /* offsetof, size_t */

/*===========================================================================
 * pin pool 描述符 — 参数化 dut_common_pin_t / dut_power_pin_t 的差异
 *===========================================================================*/

/*
 * 两级 pin pool 外数组的描述符。
 * 参数化 dut_common_pin_t / dut_power_pin_t 的差异，
 * 让 realloc_pin_pool_outer 能泛用管理两者。
 */
typedef struct {
    size_t      inner_ptr_offset;  /* offsetof(type, inner_ptr_field) */
    size_t      meta_size;         /* 元数据字节数，复用清零用 */
    void      **outer_array;       /* 指向 ctx->dut_*_pins 变量 */
    uint32_t   *alloc_count;       /* 指向 ctx->alloc_*_pin_num */
    size_t      elem_size;         /* sizeof(outer element) */
    const char *pool_name;         /* DBG_ERR 格式字符串 */
} pin_pool_desc_t;

/* 提取 outer element 中的内数组 _dut_pin_t* 指针 */
static inline _dut_pin_t **
pin_pool_inner_ptr(const pin_pool_desc_t *desc, void *outer_elem)
{
    return (_dut_pin_t **)((char *)outer_elem + desc->inner_ptr_offset);
}

/* DevMgmt 接口预留: 获取 socket 板存在标志, 0=无板 1=有板 */
#define DEV_GET_SOCKET_BOARD_STATUS() (0)

/*===========================================================================
 * 宏定义
 *===========================================================================*/
#define RET_WITH_DEL_JSON(rootx, ret)\
                cJSON_Delete(rootx);\
                return ret;

/**
 * @brief 读取整个文件到 malloc 分配的 null-terminated buffer
 *
 * 以二进制模式打开文件, 获取大小, 分配 buffer,
 * 读取全部内容, 并以 '\0'结尾.
 * 调用方负责 free 返回的 buffer.
 *
 * @param path  文件路径
 * @return      成功时返回 malloc 分配的 buffer (调用方负责 free),
 *              失败时返回 NULL
 */
char *read_file_to_string(const char *path);

/*===========================================================================
 * socket_json_parse 模块内部私有头文件
 * 仅供模块内 .c 文件包含, 外部调用方不应使用
 *===========================================================================*/

/* --- socket 解析 (socket_parser.c) --- */

int parse_socket_json(SocContext *ctx, const char *json_path);
void free_socket_data(SocContext *ctx);

/* --- config 解析 (config_parser.c) --- */

RES_API int parse_config_json(SocContext *ctx, const char *json_path);

/* --- 编排 (socket_json_parse.c) --- */

int pipeline_run_cycle(SocContext *ctx);

/* 返回值枚举 */
#define CYCLE_OK      0
#define CYCLE_RESTART 1
#define CYCLE_ERROR   2

#endif /* _DEALSOC_INTERNAL_H */
