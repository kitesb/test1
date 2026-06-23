#ifndef _SESSION_INTERNAL_H
#define _SESSION_INTERNAL_H

/*--- 项目头文件 ---*/
#include "general.h"
#include "session_types.h"

/*--- 第三方库头文件 ---*/
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 结构体：会话上下文（全局单例，存储命令行解析结果）
 *===========================================================================*/

typedef struct {
    uint32_t  func_keys[RES_MAX_FUNC_KEY_COUNT];  /**< 所有 ON 的 func_key */
    uint32_t  func_key_count;                      /**< func_key 实际个数 */
    char      program_name[RES_MAX_PROGRAM_NAME_LEN]; /**< 测试程序名 */
    char      test_name[RES_MAX_TEST_NAME_LEN];       /**< 测试项名 */
    uint32_t  site_nos[RES_MAX_FUNC_KEY_COUNT];    /**< site 编号数组（多 site 预留） */
    uint32_t  site_count;                           /**< site 实际个数（当前始终为 1） */
    bool      initialized;                          /**< 模块初始化标记 */
} SessionContext;

/*===========================================================================
 * 内部函数声明（无 RES_API 前缀，实现层可独立重构）
 *===========================================================================*/

/**
 * @brief  查询模块是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool session_is_init(void);

/**
 * @brief  会话信息初始化（深拷贝，断开对 argv 栈的依赖）
 * @param  param 入参结构体指针，含 func_key 数组、程序名、测试项名、site 编号
 * @return SESSION_OK 成功，SESSION_ERR_PARAM param 为 NULL
 */
int32_t session_init(const SessionInitParam *param);

/**
 * @brief  获取所有 ON 的 function key 数组
 * @param  func_key 输出数组地址（msize == 0 时可传 NULL）
 * @param  msize    传入数组大小（0 = 仅返回计数）
 * @return > 0 实际 func_key 个数，< 0 session_error_t 错误码
 */
int32_t session_get_func_key(uint32_t *func_key, uint32_t msize);

/**
 * @brief  获取测试程序名
 * @param  program_name 输出缓冲区
 * @param  max_len      缓冲区大小
 * @return SESSION_OK 成功，SESSION_ERR_PARAM / SESSION_ERR_NOT_INIT 失败
 */
int32_t session_get_program_name(char *program_name, uint32_t max_len);

/**
 * @brief  获取测试项名称
 * @param  test_name 输出缓冲区
 * @param  max_len   缓冲区大小
 * @return SESSION_OK 成功，SESSION_ERR_PARAM / SESSION_ERR_NOT_INIT 失败
 */
int32_t session_get_test_name(char *test_name, uint32_t max_len);

/**
 * @brief  查询 site 编号数组（多 site 预留）
 * @param  site_nos 输出数组地址（msize == 0 时可传 NULL）
 * @param  msize    传入数组大小（0 = 仅返回计数）
 * @return > 0 实际 site 个数，< 0 session_error_t 错误码
 */
int32_t session_query_site_nos(uint32_t *site_nos, uint32_t msize);

/**
 * @brief  查询当前 site 编号（取 site_nos[0]）
 * @return >= 0 site 编号，< 0 session_error_t 错误码
 */
int32_t session_query_site_no(void);

#ifdef __cplusplus
}
#endif

#endif /* _SESSION_INTERNAL_H */
