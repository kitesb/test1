#ifndef _SOC_CONTEXT_H
#define _SOC_CONTEXT_H

#include "general.h"
#include "HashTable.h"
#include "dealsoc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Context 创建时配置结构体
 *===========================================================================*/
typedef struct {
    uint32_t        default_dut_count;
    uint32_t        default_common_pin_num;
    uint32_t        default_power_pin_num;
    logic_config_t  default_logic_config;
    char            default_config_json_path[MAX_FILE_PATH_LEN];
    const char     *compiler_path;
} soc_config_t;

/*===========================================================================
 * 默认配置值宏
 *===========================================================================*/
#define SOC_CONFIG_INIT { \
    .default_dut_count       = 384,  \
    .default_common_pin_num  = 34,   \
    .default_power_pin_num   = 5,    \
    .default_logic_config    = {32, 4, 14, 37, 4, 256, 512}, \
    .default_config_json_path = "config-local.json", \
    .compiler_path           = NULL, \
}

/*===========================================================================
 * Context 私有结构体 — 完整定义 (模块内部可见)
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
    pthread_t work_thread;
    int       thread_running;

    /* --- 创建时配置（不可变） --- */
    soc_config_t config;
};

/*===========================================================================
 * 不透明句柄 — 封装所有解析状态
 *===========================================================================*/
typedef struct soc_context_impl SocContext;

/*===========================================================================
 * 生命周期 API
 *===========================================================================*/

/**
 * @brief  创建 context（mutex/cond 指针指向全局变量，和 ResSocket.c 共享）
 * @param  config  创建时配置, NULL 则使用 SOC_CONFIG_INIT 默认值
 * @return 堆分配的 context 指针, 失败返回 NULL
 */
RES_API SocContext *soc_context_create(const soc_config_t *config);

/**
 * @brief  销毁 context, 自动停止线程, 释放所有资源.
 *         安全处理 NULL.
 * @param  ctx  context 指针
 */
RES_API void soc_context_destroy(SocContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _SOC_CONTEXT_H */
