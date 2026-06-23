#ifndef _DUT_GROUP_TYPES_H
#define _DUT_GROUP_TYPES_H

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

/* 内存池余量: new_spec + 25% + 8 */
#define POOL_MARGIN(n)         ((n) + ((n) >> 2) + 8)

/* 默认最大 DUT 数（无 JSON 时使用），实际按 POOL_MARGIN 分配余量 */
#define RES_MAX_DUTS_PER_SITE  512

/* n 个 DUT 需要的位图字节数 */
#define RES_BITMAP_BYTES(n)    (((n) - 1) / 8 + 1)

/* 最大 site 数（预留多 site 扩展） */
#define MAX_SITE_NUM           8

/*===========================================================================
 * 枚举：错误码
 *===========================================================================*/

typedef enum {
	DUT_GROUP_OK           =  0,
	DUT_GROUP_ERR_PARAM    = -1,
	DUT_GROUP_ERR_NOT_INIT = -2,
	DUT_GROUP_ERR_ALLOC    = -3,
	DUT_GROUP_ERR_SITE     = -4,
	DUT_GROUP_ERR_GROUP    = -5,
	DUT_GROUP_ERR_DUT_NO   = -6,
} dut_group_error_t;

/*===========================================================================
 * 枚举：DUT 分组类型
 *===========================================================================*/

typedef enum {
	DUT_GROUP_DDUT  = 0, /**< 硬件 DSA 板上实际放置的 DUT（由 JSON 确认） */
	DUT_GROUP_SDUT  = 1, /**< 待测 DUT（物理在位、可测的） */
	DUT_GROUP_MDUT  = 2, /**< Rejection 后剩余的 DUT */
	DUT_GROUP_EDUT  = 3, /**< Exclusion 后剩余的 DUT（连接层面语义） */
	DUT_GROUP_CDUT  = 4, /**< Exclusion 后剩余的 DUT（测量层面语义） */
	DUT_GROUP_PDUT  = 5, /**< 上一次 Meas test 判 PASS 的 DUT */
	DUT_GROUP_FDUT  = 6, /**< 上一次 Meas test 判 FAIL 的 DUT */
	DUT_GROUP_GFDUT = 7, /**< 累计所有 FAIL 过的 DUT */
	DUT_GROUP_MAX   = 8  /**< 分组总数（用于入参校验） */
} dut_group_type_t;

#ifdef __cplusplus
}
#endif

#endif /* _DUT_GROUP_TYPES_H */
