/*===========================================================================
 * ResDutGroup.c — DUT 分组管理 API 薄包装层
 *
 * 职责: 入参校验 → 委托 res_resource 内部函数
 * 不包含: bitmap 操作、内存管理、锁（全部委托给内部层）
 *===========================================================================*/

/*--- 模块头文件 ---*/
#include "ResAPI.h"
#include "dut_group.h"       /* 内部类型 + 内部函数声明 */

/*--- 系统头文件 ---*/
#include <stddef.h>          /* NULL */

/*===========================================================================
 * DUT 分组管理 API
 *===========================================================================*/

/**
 * @brief  DUT 分组管理子模块初始化入口
 *         按 RES_MAX_DUTS_PER_SITE(512) + POOL_MARGIN 余量分配 8 组 bitmap
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_ALLOC 内存分配失败
 */
RES_API int32_t Res_DutGroupInit(void)
{
	return dut_group_init(RES_MAX_DUTS_PER_SITE);
}

/**
 * @brief  查询指定 DUT 分组中置位的 DUT 编号
 * @param  site_no   当前 site 编号（预留）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_list  输出 DUT 编号数组地址（msize == 0 时可传 NULL）
 * @param  msize     传入的 dut_list 数组大小（0 = 仅返回计数）
 * @return > 0 实际的 DUT 个数，< 0 错误码
 */
RES_API int32_t Res_GetDutGroup(uint32_t site_no, uint32_t dut_group,
                                 uint32_t *dut_list, uint32_t msize)
{
	(void)site_no;

	if (!dut_group_is_init()) {
		return DUT_GROUP_ERR_NOT_INIT;
	}
	if (dut_group >= DUT_GROUP_MAX) {
		return DUT_GROUP_ERR_GROUP;
	}
	if (msize > 0 && dut_list == NULL) {
		return DUT_GROUP_ERR_PARAM;
	}

	return dut_group_get(dut_group, dut_list, msize);
}

/**
 * @brief  批量增加 DUT 到指定分组（将对应 bit 置 1）
 * @param  site_no   当前 site 编号（预留）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_nos   要增加的 DUT 编号数组（1-indexed）
 * @param  count     要增加的 DUT 个数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_DUT_NO 编号越界
 */
RES_API int32_t Res_AddDutToGroup(uint32_t site_no, uint32_t dut_group,
                                  const uint32_t *dut_nos, uint32_t count)
{
	(void)site_no;

	if (!dut_group_is_init()) {
		return DUT_GROUP_ERR_NOT_INIT;
	}
	if (dut_group >= DUT_GROUP_MAX) {
		return DUT_GROUP_ERR_GROUP;
	}
	if (dut_nos == NULL || count == 0) {
		return DUT_GROUP_ERR_PARAM;
	}

	return dut_group_add(dut_group, dut_nos, count);
}

/**
 * @brief  批量从指定分组删除 DUT（将对应 bit 清 0）
 *         删除不存在的 DUT 不报错（幂等）
 * @param  site_no   当前 site 编号（预留）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_nos   要删除的 DUT 编号数组（1-indexed）
 * @param  count     要删除的 DUT 个数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_DUT_NO 编号越界
 */
RES_API int32_t Res_DeleteDutFrmGroup(uint32_t site_no, uint32_t dut_group,
                                      const uint32_t *dut_nos, uint32_t count)
{
	(void)site_no;

	if (!dut_group_is_init()) {
		return DUT_GROUP_ERR_NOT_INIT;
	}
	if (dut_group >= DUT_GROUP_MAX) {
		return DUT_GROUP_ERR_GROUP;
	}
	if (dut_nos == NULL || count == 0) {
		return DUT_GROUP_ERR_PARAM;
	}

	return dut_group_remove(dut_group, dut_nos, count);
}

/**
 * @brief  清除整个 DUT 分组（所有 bit 置 0）
 * @param  site_no   当前 site 编号（预留）
 * @param  dut_group 需要操作的分组类型
 * @return DUT_GROUP_OK 成功
 */
RES_API int32_t Res_ClearDutGroup(uint32_t site_no, uint32_t dut_group)
{
	(void)site_no;

	if (!dut_group_is_init()) {
		return DUT_GROUP_ERR_NOT_INIT;
	}
	if (dut_group >= DUT_GROUP_MAX) {
		return DUT_GROUP_ERR_GROUP;
	}

	return dut_group_clear(dut_group);
}

