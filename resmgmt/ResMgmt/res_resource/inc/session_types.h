#ifndef _SESSION_TYPES_H
#define _SESSION_TYPES_H

/*--- 项目头文件 ---*/
#include "general.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件（无） ---*/

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 编译期常量
 *===========================================================================*/

/* 字符串最大长度 */
#define RES_MAX_PROGRAM_NAME_LEN   128
#define RES_MAX_TEST_NAME_LEN      128

/* func_key / site_nos 数组最大容量 */
#define RES_MAX_FUNC_KEY_COUNT     64

/*===========================================================================
 * 枚举：错误码
 *===========================================================================*/

typedef enum {
    SESSION_OK           =  0,
    SESSION_ERR_PARAM    = -1,
    SESSION_ERR_NOT_INIT = -2,
    SESSION_ERR_BUF_SZ   = -3,
} session_error_t;

/*===========================================================================
 * 结构体：session_init 入参
 *===========================================================================*/

typedef struct {
    const uint32_t *func_keys;      /**< func_key 数组指针（可为 NULL） */
    uint32_t        func_key_count; /**< func_key 个数 */
    const char     *program_name;   /**< 测试程序名（可为 NULL） */
    const char     *test_name;      /**< 测试项名（可为 NULL） */
    uint32_t        site_no;        /**< site 编号 */
} SessionInitParam;

#ifdef __cplusplus
}
#endif

#endif /* _SESSION_TYPES_H */
