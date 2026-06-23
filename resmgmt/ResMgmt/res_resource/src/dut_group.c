/*--- 模块头文件 ---*/
#include "dut_group.h"

/*--- 项目头文件 ---*/
#include "log_mgmt.h"

/*--- 系统头文件 ---*/
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * 静态全局变量
 *===========================================================================*/

#ifdef DUT_GROUP_SELF_TEST
Res_Manager g_res_mgr;
#else
static Res_Manager g_res_mgr;
#endif

/*===========================================================================
 * 内部辅助函数（static，不对外暴露）
 *===========================================================================*/

/**
 * @brief  根据分组类型枚举返回对应的 bitmap 指针
 * @param  grp       指向 Res_Dut_group 的指针
 * @param  dut_group 分组类型枚举值
 * @return bitmap 指针，非法类型返回 NULL
 */
static uint8_t *get_group_ptr(Res_Dut_group *grp, uint32_t dut_group)
{
	switch (dut_group) {
	case DUT_GROUP_DDUT:  return grp->ddut;
	case DUT_GROUP_SDUT:  return grp->sdut;
	case DUT_GROUP_MDUT:  return grp->mdut;
	case DUT_GROUP_EDUT:  return grp->edut;
	case DUT_GROUP_CDUT:  return grp->cdut;
	case DUT_GROUP_PDUT:  return grp->pdut;
	case DUT_GROUP_FDUT:  return grp->fdut;
	case DUT_GROUP_GFDUT: return grp->gfdut;
	default:              return NULL;
	}
}

/**
 * @brief 释放所有 8 组 bitmap 内存，并将指针置 NULL
 * @param grp 指向 Res_Dut_group 的指针
 */
static void free_all_bitmaps(Res_Dut_group *grp)
{
	free(grp->ddut);   grp->ddut   = NULL;
	free(grp->sdut);   grp->sdut   = NULL;
	free(grp->mdut);   grp->mdut   = NULL;
	free(grp->edut);   grp->edut   = NULL;
	free(grp->cdut);   grp->cdut   = NULL;
	free(grp->pdut);   grp->pdut   = NULL;
	free(grp->fdut);   grp->fdut   = NULL;
	free(grp->gfdut);  grp->gfdut  = NULL;
}

/**
 * @brief  为 8 组 bitmap 分别 calloc 分配内存
 * @param  grp        指向 Res_Dut_group 的指针
 * @param  alloc_duts 分配容量（DUT 个数，已做 POOL_MARGIN）
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_ALLOC 任一分配失败
 */
static int32_t alloc_all_bitmaps(Res_Dut_group *grp, uint32_t alloc_duts)
{
	uint32_t alloc_bytes = RES_BITMAP_BYTES(alloc_duts);

	grp->ddut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->sdut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->mdut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->edut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->cdut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->pdut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->fdut  = (uint8_t *)calloc(alloc_bytes, 1);
	grp->gfdut = (uint8_t *)calloc(alloc_bytes, 1);

	/* 任一分配失败则全部释放 */
	if (!grp->ddut || !grp->sdut || !grp->mdut || !grp->edut
	    || !grp->cdut || !grp->pdut || !grp->fdut || !grp->gfdut) {
		free_all_bitmaps(grp);
		return DUT_GROUP_ERR_ALLOC;
	}

	grp->alloc_duts = alloc_duts;
	return DUT_GROUP_OK;
}

/*===========================================================================
 * 内部函数实现（无 RES_API，实现层可独立重构）
 *===========================================================================*/

/**
 * @brief  查询模块是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool dut_group_is_init(void)
{
	return g_res_mgr.initialized;
}

/**
 * @brief  DUT 分组管理子模块初始化入口
 *         按 max_duts + POOL_MARGIN 余量分配 8 组 bitmap
 *         支持重复调用，内部先释放旧内存再重新分配
 * @param  max_duts 最大 DUT 数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_ALLOC 内存分配失败
 */
int32_t dut_group_init(uint32_t max_duts)
{
	int32_t ret;

	/* 支持重复调用：释放已有内存后再分配 */
	free_all_bitmaps(&g_res_mgr.dut_group);
	memset(&g_res_mgr, 0, sizeof(g_res_mgr));

	/* 按 POOL_MARGIN 余量分配 bitmap */
	ret = alloc_all_bitmaps(&g_res_mgr.dut_group,
	                        (uint32_t)POOL_MARGIN(max_duts));
	if (ret != DUT_GROUP_OK) {
		DBG_ERR("dut_group_init: alloc failed\n");
		return ret;
	}

	/* 初始化各字段 */
	g_res_mgr.dut_group.num_duts     = max_duts;
	g_res_mgr.dut_group.bitmap_bytes = RES_BITMAP_BYTES(max_duts);
	g_res_mgr.site.num_sites         = 1;
	g_res_mgr.site.sites[0]          = 0;
	g_res_mgr.current_site           = 0;
	g_res_mgr.initialized            = true;

	pthread_mutex_init(&g_res_mgr.mutex, NULL);

	DBG_INFO("dut_group_init: done, num_duts=%u, alloc_duts=%u\n",
	         g_res_mgr.dut_group.num_duts, g_res_mgr.dut_group.alloc_duts);

	return DUT_GROUP_OK;
}

/**
 * @brief  查询指定 DUT 分组中置位的 DUT 编号
 *         采用 bitmap 按 bit 位存储，天然升序遍历
 * @param  dut_group 需要操作的分组类型（dut_group_type_t）
 * @param  dut_list  输出 DUT 编号数组地址（msize == 0 时可传 NULL）
 * @param  msize     传入的 dut_list 数组大小（0 = 仅返回计数）
 * @return > 0 实际的 DUT 个数，< 0 dut_group_error_t 错误码
 */
int32_t dut_group_get(uint32_t dut_group, uint32_t *dut_list, uint32_t msize)
{
	Res_Dut_group *grp;
	uint8_t *group;
	uint32_t dut_no;
	uint32_t byte_idx, bit_idx;
	uint32_t list_idx, total_count;

	grp   = &g_res_mgr.dut_group;
	group = get_group_ptr(grp, dut_group);

	pthread_mutex_lock(&g_res_mgr.mutex);

	/* 第一遍遍历：统计置位 bit 总数 */
	total_count = 0;
	for (dut_no = 1; dut_no <= grp->num_duts; dut_no++) {
		byte_idx = (dut_no - 1) / 8;
		bit_idx  = (dut_no - 1) % 8;
		if (group[byte_idx] & (1 << bit_idx)) {
			total_count++;
		}
	}

	/* msize == 0 仅返回计数，不填充数组 */
	if (msize == 0) {
		pthread_mutex_unlock(&g_res_mgr.mutex);
		return (int32_t)total_count;
	}

	/* 第二遍遍历：填充 dut_list（bitmap 按 bit 位存储，天然升序） */
	list_idx = 0;
	for (dut_no = 1; dut_no <= grp->num_duts && list_idx < msize; dut_no++) {
		byte_idx = (dut_no - 1) / 8;
		bit_idx  = (dut_no - 1) % 8;
		if (group[byte_idx] & (1 << bit_idx)) {
			dut_list[list_idx++] = dut_no;
		}
	}

	pthread_mutex_unlock(&g_res_mgr.mutex);
	return (int32_t)list_idx;
}

/**
 * @brief  批量增加 DUT 到指定分组（将对应 bit 置 1）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_nos   要增加的 DUT 编号数组（1-indexed）
 * @param  count     要增加的 DUT 个数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_DUT_NO 编号越界
 */
int32_t dut_group_add(uint32_t dut_group,
                      const uint32_t *dut_nos, uint32_t count)
{
	Res_Dut_group *grp;
	uint8_t *group;
	uint32_t arr_idx, dut_no;
	uint32_t byte_idx, bit_idx;

	grp   = &g_res_mgr.dut_group;
	group = get_group_ptr(grp, dut_group);

	pthread_mutex_lock(&g_res_mgr.mutex);

	/* 遍历待增加的 DUT 编号，逐位置位 */
	for (arr_idx = 0; arr_idx < count; arr_idx++) {
		dut_no = dut_nos[arr_idx];
		if (dut_no < 1 || dut_no > grp->num_duts) {
			pthread_mutex_unlock(&g_res_mgr.mutex);
			return DUT_GROUP_ERR_DUT_NO;
		}
		byte_idx = (dut_no - 1) / 8;
		bit_idx  = (dut_no - 1) % 8;
		group[byte_idx] |= (uint8_t)(1 << bit_idx);
	}

	pthread_mutex_unlock(&g_res_mgr.mutex);
	return DUT_GROUP_OK;
}

/**
 * @brief  批量从指定分组删除 DUT（将对应 bit 清 0）
 *         删除不存在的 DUT 不报错（幂等）
 * @param  dut_group 需要操作的分组类型
 * @param  dut_nos   要删除的 DUT 编号数组（1-indexed）
 * @param  count     要删除的 DUT 个数
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_DUT_NO 编号越界
 */
int32_t dut_group_remove(uint32_t dut_group,
                         const uint32_t *dut_nos, uint32_t count)
{
	Res_Dut_group *grp;
	uint8_t *group;
	uint32_t arr_idx, dut_no;
	uint32_t byte_idx, bit_idx;

	grp   = &g_res_mgr.dut_group;
	group = get_group_ptr(grp, dut_group);

	pthread_mutex_lock(&g_res_mgr.mutex);

	/* 遍历待删除的 DUT 编号，逐位清零（不存在的 bit 幂等） */
	for (arr_idx = 0; arr_idx < count; arr_idx++) {
		dut_no = dut_nos[arr_idx];
		if (dut_no < 1 || dut_no > grp->num_duts) {
			pthread_mutex_unlock(&g_res_mgr.mutex);
			return DUT_GROUP_ERR_DUT_NO;
		}
		byte_idx = (dut_no - 1) / 8;
		bit_idx  = (dut_no - 1) % 8;
		group[byte_idx] &= (uint8_t)(~(1 << bit_idx));
	}

	pthread_mutex_unlock(&g_res_mgr.mutex);
	return DUT_GROUP_OK;
}

/**
 * @brief  清除整个 DUT 分组（所有 bit 置 0）
 * @param  dut_group 需要操作的分组类型
 * @return DUT_GROUP_OK 成功
 */
int32_t dut_group_clear(uint32_t dut_group)
{
	Res_Dut_group *grp;
	uint8_t *group;

	grp   = &g_res_mgr.dut_group;
	group = get_group_ptr(grp, dut_group);

	pthread_mutex_lock(&g_res_mgr.mutex);
	memset(group, 0, grp->bitmap_bytes);
	pthread_mutex_unlock(&g_res_mgr.mutex);

	return DUT_GROUP_OK;
}

/**
 * @brief  dut_count 变化通知（由 socket_json 子模块在解析 JSON 后调用）
 *         若 new_dut_count ≤ alloc_duts：清零各 bitmap 后复用
 *         若 new_dut_count > alloc_duts：释放旧内存，按 POOL_MARGIN 重新分配
 * @param  new_dut_count 新的 DUT 数量（>= 1）
 * @return DUT_GROUP_OK 成功，DUT_GROUP_ERR_ALLOC 重新分配失败
 */
int32_t dut_group_resize(uint32_t new_dut_count)
{
	Res_Dut_group *grp;
	uint32_t new_alloc;
	uint32_t new_bytes;
	int32_t ret;

	grp = &g_res_mgr.dut_group;

	pthread_mutex_lock(&g_res_mgr.mutex);

	if (new_dut_count <= grp->alloc_duts) {
		/* 未超出现有分配容量：memset 清零后复用，不重新分配 */
		new_bytes = RES_BITMAP_BYTES(new_dut_count);
		memset(grp->ddut,  0, new_bytes);
		memset(grp->sdut,  0, new_bytes);
		memset(grp->mdut,  0, new_bytes);
		memset(grp->edut,  0, new_bytes);
		memset(grp->cdut,  0, new_bytes);
		memset(grp->pdut,  0, new_bytes);
		memset(grp->fdut,  0, new_bytes);
		memset(grp->gfdut, 0, new_bytes);

		grp->num_duts     = new_dut_count;
		grp->bitmap_bytes = new_bytes;

		DBG_INFO("dut_group_resize: zeroed, num_duts=%u "
		         "(alloc=%u unchanged)\n",
		         new_dut_count, grp->alloc_duts);
	} else {
		/* 超出分配容量：释放旧内存，按 POOL_MARGIN 余量重新分配 */
		new_alloc = (uint32_t)POOL_MARGIN(new_dut_count);

		free_all_bitmaps(grp);

		ret = alloc_all_bitmaps(grp, new_alloc);
		if (ret != DUT_GROUP_OK) {
			pthread_mutex_unlock(&g_res_mgr.mutex);
			DBG_ERR("dut_group_resize: realloc failed\n");
			return ret;
		}

		grp->num_duts     = new_dut_count;
		grp->bitmap_bytes = RES_BITMAP_BYTES(new_dut_count);

		DBG_INFO("dut_group_resize: realloc'd, num_duts=%u, "
		         "alloc_duts=%u\n",
		         new_dut_count, new_alloc);
	}

	pthread_mutex_unlock(&g_res_mgr.mutex);
	return DUT_GROUP_OK;
}
