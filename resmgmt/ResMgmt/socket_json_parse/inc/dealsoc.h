#ifndef _DEALSOC_H
#define _DEALSOC_H

#include "general.h"
#include "dut_api_types.h"
#include "soc_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * socket_json_parse 模块 — 编译解析引擎公共 API
 *===========================================================================*/

/** @brief 模块初始化, 创建 context 并启动后台工作线程 */
int32_t socket_json_parse_init(uint32_t argc, char *argv);

/** @brief 触发编译解析 (异步, 信号后台线程) */
int32_t socket_json_parse_start(const char *socket_file_path);

/** @brief 设置 config JSON 路径 */
int32_t socket_json_parse_set_config_path(const char *path, uint32_t path_len);

/** @brief 同步编译入口 (fork+exec 外部编译器) */
int32_t socket_json_parse_compile(const char *input_path, char *output_path, uint32_t output_len);

/*===========================================================================
 * Context 单例 + 解析等待 (供 ResSocket.c / 抽象层调用)
 *===========================================================================*/

SocContext *soc_context_get_instance(void);
int soc_wait_socket_parse(SocContext *ctx);
int soc_wait_both_parse(SocContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* _DEALSOC_H */
