#ifndef _RES_DUT_GROUP_INTERNAL_H
#define _RES_DUT_GROUP_INTERNAL_H

/*--- 项目头文件 ---*/
#include "general.h"
#include "dut_group_types.h"

/*--- 第三方库头文件 ---*/
#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 结构体：DUT 分组
 *===========================================================================*/

typedef struct {
	uint32_t  num_duts;     /**< 当前实际 DUT 数量 */
	uint32_t  alloc_duts;   /**< 已分配内存可容纳的最大 DUT 数量（POOL_MARGIN 后） */
	uint32_t  bitmap_bytes; /**< 位图有效字节数 = RES_BITMAP_BYTES(num_duts) */

	uint8_t  *ddut;  /**< DDUT 分组位图（硬件板上实际放置） */
	uint8_t  *sdut;  /**< SDUT 分组位图（待测） */
	uint8_t  *mdut;  /**< MDUT 分组位图（Rejection 后剩余） */
	uint8_t  *edut;  /**< EDUT 分组位图（Exclusion 后剩余，连接层面） */
	uint8_t  *cdut;  /**< CDUT 分组位图（Exclusion 后剩余，测量层面） */
	uint8_t  *pdut;  /**< PDUT 分组位图（上次判 PASS） */
	uint8_t  *fdut;  /**< FDUT 分组位图（上次判 FAIL） */
	uint8_t  *gfdut; /**< GFDUT 分组位图（累计所有 FAIL） */
} Res_Dut_group;

typedef struct {
	uint32_t  num_sites;              /**< site 数量（当前固定为 1） */
	uint32_t  sites[MAX_SITE_NUM];    /**< EWS 下发的 site 编号数组 */
} Res_Site;

typedef struct {
	Res_Dut_group   dut_group;     /**< DUT 分组数据 */
	Res_Site        site;          /**< site 信息（预留多 site） */
	uint32_t        current_site;  /**< 当前 site 编号 */
	pthread_mutex_t mutex;         /**< 保护所有 DUT 分组操作的互斥锁 */
	bool            initialized;   /**< 模块初始化标记 */
} Res_Manager;

/*===========================================================================
 * 内部函数声明（无 RES_API，实现层可独立重构）
 *===========================================================================*/

/**
 * @brief  查询模块是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool dut_group_is_init(void);

/**
 * @brief  DUT 分组管理子模块初始化入口
 *         按 max_duts + POOL_MARGIN 余量分配 8 组 bitmap
 *         支持重复调用，内部先释放旧内存再重新分配
 * @param  max_duts 最大 DUT 数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_ALLOC 内存分配失败
 */
int32_t dut_group_init(uint32_t max_duts);

/**
 * @brief  查询指定 DUT 分组中置位的 DUT 编号
 *         采用 bitmap 按 bit 位存储，天然升序遍历
 * @param  dut_group 需要操作的分组类型（dut_group_type_t）
 * @param  dut_list  输出 DUT 编号数组地址（msize == 0 时可传 NULL）
 * @param  msize     传入的 dut_list 数组大小（0 = 仅返回计数）
 * @return > 0 实际的 DUT 个数，< 0 dut_group_error_t 错误码
 */
int32_t dut_group_get(uint32_t dut_group, uint32_t *dut_list, uint32_t msize);

/**
 * @brief  批量增加 DUT 到指定分组（将对应 bit 置 1）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_nos   要增加的 DUT 编号数组（1-indexed）
 * @param  count     要增加的 DUT 个数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_DUT_NO 编号越界
 */
int32_t dut_group_add(uint32_t dut_group,
                      const uint32_t *dut_nos, uint32_t count);

/**
 * @brief  批量从指定分组删除 DUT（将对应 bit 清 0）
 *         删除不存在的 DUT 不报错（幂等）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_nos   要删除的 DUT 编号数组（1-indexed）
 * @param  count     要删除的 DUT 个数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_DUT_NO 编号越界
 */
int32_t dut_group_remove(uint32_t dut_group,
                         const uint32_t *dut_nos, uint32_t count);

/**
 * @brief  清除整个 DUT 分组（所有 bit 置 0）
 * @param  dut_group 需要操作的分组类型
 * @return DUT_GROUP_OK 成功
 */
int32_t dut_group_clear(uint32_t dut_group);

/**
 * @brief  dut_count 变化通知（由 socket_json 子模块在解析 JSON 后调用）
 *         若 new_dut_count ≤ alloc_duts：清零各 bitmap 后复用
 *         若 new_dut_count > alloc_duts：释放旧内存，按 POOL_MARGIN 重新分配
 * @param  new_dut_count 新的 DUT 数量（>= 1）
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_ALLOC 重新分配失败
 */
int32_t dut_group_resize(uint32_t new_dut_count);

#ifdef __cplusplus
}
#endif

#endif /* _RES_DUT_GROUP_INTERNAL_H */
