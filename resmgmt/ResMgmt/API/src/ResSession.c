/*===========================================================================
 * ResSession.c — 会话信息查询 API 薄包装层
 *
 * 职责: 入参校验 → 委托 res_resource 内部函数
 * 不包含: 存储、解析逻辑（全部委托给内部层）
 *===========================================================================*/

/*--- 模块头文件 ---*/
#include "ResAPI.h"
#include "session.h"       /* 内部类型 + 内部函数声明 */

/*--- 系统头文件 ---*/
#include <stddef.h>        /* NULL */

/*===========================================================================
 * 会话信息查询 API
 *===========================================================================*/

/**
 * @brief  获取所有 ON 的 function key 数组
 * @param  func_key 输出数组地址（msize == 0 时可传 NULL）
 * @param  msize    传入数组大小（0 = 仅返回计数）
 * @return > 0 实际 func_key 个数，< 0 session_error_t 错误码
 */
RES_API int32_t Res_GetFuncKey(uint32_t *func_key, uint32_t msize)
{
    if (!session_is_init()) {
        return SESSION_ERR_NOT_INIT;
    }
    if (msize > 0 && func_key == NULL) {
        return SESSION_ERR_PARAM;
    }

    return session_get_func_key(func_key, msize);
}

/**
 * @brief  获取测试程序名
 * @param  program_name 输出缓冲区
 * @param  max_len      缓冲区大小
 * @return SESSION_OK 成功，SESSION_ERR_PARAM / SESSION_ERR_NOT_INIT 失败
 */
RES_API int32_t Res_GetProgramName(char *program_name, uint32_t max_len)
{
    if (!session_is_init()) {
        return SESSION_ERR_NOT_INIT;
    }
    if (max_len > 0 && program_name == NULL) {
        return SESSION_ERR_PARAM;
    }

    return session_get_program_name(program_name, max_len);
}

/**
 * @brief  获取测试项名称
 * @param  test_name 输出缓冲区
 * @param  max_len   缓冲区大小
 * @return SESSION_OK 成功，SESSION_ERR_PARAM / SESSION_ERR_NOT_INIT 失败
 */
RES_API int32_t Res_GetTestName(char *test_name, uint32_t max_len)
{
    if (!session_is_init()) {
        return SESSION_ERR_NOT_INIT;
    }
    if (max_len > 0 && test_name == NULL) {
        return SESSION_ERR_PARAM;
    }

    return session_get_test_name(test_name, max_len);
}

/**
 * @brief  查询 site 编号数组（多 site 预留）
 * @param  site_nos 输出数组地址（msize == 0 时可传 NULL）
 * @param  msize    传入数组大小（0 = 仅返回计数）
 * @return > 0 实际 site 个数，< 0 session_error_t 错误码
 */
RES_API int32_t Res_QuerySiteNos(uint32_t *site_nos, uint32_t msize)
{
    if (!session_is_init()) {
        return SESSION_ERR_NOT_INIT;
    }
    if (msize > 0 && site_nos == NULL) {
        return SESSION_ERR_PARAM;
    }

    return session_query_site_nos(site_nos, msize);
}

/**
 * @brief  查询当前 site 编号（取 site_nos[0]）
 * @return >= 0 site 编号，< 0 session_error_t 错误码
 */
RES_API int32_t Res_QuerySiteNo(void)
{
    if (!session_is_init()) {
        return SESSION_ERR_NOT_INIT;
    }

    return session_query_site_no();
}
