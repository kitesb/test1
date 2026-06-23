#ifndef _SOCKET_INTERNAL_H
#define _SOCKET_INTERNAL_H

/*--- 项目头文件 ---*/
#include "socket_types.h"
#include "soc_context.h"
#include "HashTable.h"

/*--- 第三方库头文件 ---*/
#include <cJSON.h>

/*--- 系统头文件 ---*/
#include <stddef.h>   /* offsetof, size_t */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 内部类型 — 仅 socket_json_parse 模块内部使用
 *===========================================================================*/

typedef struct {
    char     pin_name[32];
    char     pin_num[16];
    uint32_t dut_pin_num;
} pin_name_t;

typedef struct {
    char pin_name[32];
    char vs_name[16];
} symbol_entry_t;

typedef struct {
    char      name[16];
    char      pps_type[16];
    uint32_t *channels;
    uint32_t  channel_count;
} vs_def_t;

typedef struct {
    char      site_name[16];
    char      vs_name[16];
    char      pps_type[16];
    uint32_t *channels;
    uint32_t  channel_count;
} vs_site_entry_t;

typedef struct {
    uint32_t system_dut_num;
    uint32_t site_num;
    uint32_t dut_num;
} system_dut_t;

typedef struct {
    char     type[16];
    uint32_t number;
    uint32_t mode;
} separate_entity_t;

typedef struct {
    uint32_t           *vs_map;
    separate_entity_t  *entities;
    uint32_t            entity_count;
    uint32_t           *dut_sequence;
    uint32_t            dut_seq_len;
    pin_name_t         *pin_names;
    uint32_t            pin_name_count;
    symbol_entry_t     *symbols;
    uint32_t            symbol_count;
    vs_def_t           *vs_defs;
    uint32_t            vs_def_count;
    vs_site_entry_t    *vs_site_defs;
    uint32_t            vs_site_def_count;
    vs_def_t           *hvpps_defs;
    uint32_t            hvpps_def_count;
    vs_site_entry_t    *hvpps_site_defs;
    uint32_t            hvpps_site_def_count;
    system_dut_t       *system_duts;
    uint32_t            system_dut_count;
} socket_extra_t;

/*===========================================================================
 * struct soc_context_impl 完整定义 (内部文件可见, 外部不可见)
 *===========================================================================*/
struct soc_context_impl {
    /* --- 解析数据（context 拥有） --- */
    dut_common_pin_t *dut_common_pins;
    dut_power_pin_t  *dut_power_pins;

    /* --- 哈希表（解析周期内临时存在） --- */
    hash_table_t hash_common;
    hash_table_t hash_power;

    /* --- 逻辑配置 --- */
    logic_config_t logic_config;

    /* --- 元数据 --- */
    soc_meta_t meta;

    socket_extra_t *extra;

    /* --- 内存池追踪 --- */
    uint32_t alloc_common_pin_num;
    uint32_t alloc_power_pin_num;
    uint32_t alloc_dut_count;

    /* --- 同步原语（context 拥有） --- */
    pthread_mutex_t *parse_mutex;
    pthread_cond_t  *parse_cond;

    /* --- 中断/重启标志 --- */
    atomic_int restart_needed;
    atomic_int parse_pending;

    /* --- 后台线程 --- */
    pthread_t  work_thread;
    atomic_int stop_flag;

    /* --- 创建时配置（不可变） --- */
    soc_config_t config;
};

/*===========================================================================
 * 宏定义
 *===========================================================================*/

/* 内存池余量: new_spec + 25% + 8 */
#define POOL_MARGIN(n)  ((n) + ((n) >> 2) + 8)

/* 返回值枚举 */
#define CYCLE_OK      0
#define CYCLE_RESTART 1
#define CYCLE_ERROR   2

/* stop_flag 取值: 请求工作线程退出 */
#define SOC_THREAD_STOP_REQUESTED 2

/* DevMgmt 接口预留: 获取 socket 板存在标志, 0=无板 1=有板 */
#define DEV_GET_SOCKET_BOARD_STATUS() (1)

#define RET_WITH_DEL_JSON(rootx, ret)\
                cJSON_Delete(rootx);\
                return ret;

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

/*===========================================================================
 * 内部函数声明
 *===========================================================================*/

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

/* --- socket 解析 (socket_parser.c / socket_pins.c / socket_optional.c) --- */

/* socket_parser.c — 核心编排器 + 内存管理 */
int  parse_socket_json(SocContext *ctx, const char *json_path);
void free_socket_data(SocContext *ctx);

/* socket_pins.c — power/common pin 解析 */
int parse_power_pins(SocContext *ctx, cJSON *power_pins, uint32_t power_pin_num, uint32_t dut_count);
int parse_common_pins(SocContext *ctx, cJSON *common_pins, uint32_t common_pin_num, uint32_t dut_count);

/* socket_optional.c — 可选字段 + scalars */
int parse_multiple_scalars(SocContext *ctx, cJSON *multiple);
int parse_parallel_vs_map(SocContext *ctx, cJSON *multiple);
int parse_separate_entities(SocContext *ctx, cJSON *multiple);
int parse_dut_sequence(SocContext *ctx, cJSON *multiple);
int parse_optional_pin_names(SocContext *ctx, cJSON *root);
int parse_optional_symbol(SocContext *ctx, cJSON *root);
int parse_optional_vs_definitions(SocContext *ctx, cJSON *root);
int parse_optional_vs_site_definitions(SocContext *ctx, cJSON *root);
int parse_optional_hvpps_definitions(SocContext *ctx, cJSON *multiple);
int parse_optional_hvpps_site_definitions(SocContext *ctx, cJSON *multiple);
int parse_optional_system_dut(SocContext *ctx, cJSON *multiple);

/* --- config 解析 (config_parser.c) --- */

RES_API int parse_config_json(SocContext *ctx, const char *json_path);

/* --- 编排 (socket_pipeline.c / socket_compile.c / socket_thread.c) --- */

int pipeline_run_cycle(SocContext *ctx);
int socket_json_compile(SocContext *ctx, const char *input_path,
                         char *output_path, uint32_t output_len);
int init_thread_soc(SocContext *ctx);

/* 单例注册 (由 socket_json_parse_init 在完全初始化后调用) */
void soc_context_register_instance(SocContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _SOCKET_INTERNAL_H */
