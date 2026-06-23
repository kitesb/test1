/*--- 模块头文件 ---*/
#include "session.h"

/*--- 项目头文件（无） ---*/

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <string.h>

/*===========================================================================
 * 静态全局变量 — 会话信息单例
 *===========================================================================*/

static SessionContext g_session;

/*===========================================================================
 * 内部函数实现
 *===========================================================================*/

/**
 * @brief  查询模块是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool session_is_init(void)
{
    return g_session.initialized;
}

/**
 * @brief  会话信息初始化（深拷贝）
 *         数据在 init 阶段写入，之后只读，无需锁保护
 * @param  param 入参结构体指针
 * @return SESSION_OK 成功，SESSION_ERR_PARAM param 为 NULL
 */
int32_t session_init(const SessionInitParam *param)
{
    if (param == NULL) {
        return SESSION_ERR_PARAM;
    }

    memset(&g_session, 0, sizeof(g_session));

    /* func_key 数组深拷贝 */
    if (param->func_keys != NULL && param->func_key_count > 0) {
        uint32_t n = param->func_key_count;
        if (n > RES_MAX_FUNC_KEY_COUNT) {
            n = RES_MAX_FUNC_KEY_COUNT;
        }
        memcpy(g_session.func_keys, param->func_keys, n * sizeof(uint32_t));
        g_session.func_key_count = n;
    }

    /* program_name 深拷贝 */
    if (param->program_name != NULL) {
        strncpy(g_session.program_name, param->program_name,
                RES_MAX_PROGRAM_NAME_LEN - 1);
        g_session.program_name[RES_MAX_PROGRAM_NAME_LEN - 1] = '\0';
    }

    /* test_name 深拷贝 */
    if (param->test_name != NULL) {
        strncpy(g_session.test_name, param->test_name,
                RES_MAX_TEST_NAME_LEN - 1);
        g_session.test_name[RES_MAX_TEST_NAME_LEN - 1] = '\0';
    }

    /* site_no 存储 */
    g_session.site_nos[0] = param->site_no;
    g_session.site_count = 1;

    g_session.initialized = true;
    return SESSION_OK;
}

/**
 * @brief  获取所有 ON 的 function key 数组
 *         msize == 0 时不写数组，仅返回实际个数
 * @param  func_key 输出数组地址
 * @param  msize    传入数组大小
 * @return > 0 实际 func_key 个数，< 0 session_error_t 错误码
 */
int32_t session_get_func_key(uint32_t *func_key, uint32_t msize)
{
    if (!g_session.initialized) {
        return SESSION_ERR_NOT_INIT;
    }
    if (msize > 0 && func_key == NULL) {
        return SESSION_ERR_PARAM;
    }

    uint32_t copy_count = g_session.func_key_count;
    if (copy_count > msize) {
        copy_count = msize;
    }
    if (func_key != NULL && copy_count > 0) {
        memcpy(func_key, g_session.func_keys, copy_count * sizeof(uint32_t));
    }

    return (int32_t)g_session.func_key_count;
}

/**
 * @brief  获取测试程序名
 * @param  program_name 输出缓冲区
 * @param  max_len      缓冲区大小
 * @return SESSION_OK 成功，SESSION_ERR_PARAM / SESSION_ERR_NOT_INIT 失败
 */
int32_t session_get_program_name(char *program_name, uint32_t max_len)
{
    if (!g_session.initialized) {
        return SESSION_ERR_NOT_INIT;
    }
    if (max_len > 0 && program_name == NULL) {
        return SESSION_ERR_PARAM;
    }
    if (max_len == 0) {
        return SESSION_OK;
    }

    strncpy(program_name, g_session.program_name, max_len - 1);
    program_name[max_len - 1] = '\0';
    return SESSION_OK;
}

/**
 * @brief  获取测试项名称
 * @param  test_name 输出缓冲区
 * @param  max_len   缓冲区大小
 * @return SESSION_OK 成功，SESSION_ERR_PARAM / SESSION_ERR_NOT_INIT 失败
 */
int32_t session_get_test_name(char *test_name, uint32_t max_len)
{
    if (!g_session.initialized) {
        return SESSION_ERR_NOT_INIT;
    }
    if (max_len > 0 && test_name == NULL) {
        return SESSION_ERR_PARAM;
    }
    if (max_len == 0) {
        return SESSION_OK;
    }

    strncpy(test_name, g_session.test_name, max_len - 1);
    test_name[max_len - 1] = '\0';
    return SESSION_OK;
}

/**
 * @brief  查询 site 编号数组（多 site 预留，当前始终返回 1 个）
 * @param  site_nos 输出数组地址
 * @param  msize    传入数组大小
 * @return > 0 实际 site 个数，< 0 session_error_t 错误码
 */
int32_t session_query_site_nos(uint32_t *site_nos, uint32_t msize)
{
    if (!g_session.initialized) {
        return SESSION_ERR_NOT_INIT;
    }
    if (msize > 0 && site_nos == NULL) {
        return SESSION_ERR_PARAM;
    }

    uint32_t copy_count = g_session.site_count;
    if (copy_count > msize) {
        copy_count = msize;
    }
    if (site_nos != NULL && copy_count > 0) {
        memcpy(site_nos, g_session.site_nos, copy_count * sizeof(uint32_t));
    }

    return (int32_t)g_session.site_count;
}

/**
 * @brief  查询当前 site 编号（取 site_nos[0]）
 * @return >= 0 site 编号，< 0 session_error_t 错误码
 */
int32_t session_query_site_no(void)
{
    if (!g_session.initialized) {
        return SESSION_ERR_NOT_INIT;
    }

    return (int32_t)g_session.site_nos[0];
}
