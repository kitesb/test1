#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>   /* offsetof */

#include "dealsoc_internal.h"
#include "soc_context.h"
#include "HashTable.h"
#include "cJSON.h"
#include "internal.h"
#include "log_mgmt.h"

RES_API void free_socket_data(SocContext *ctx)
{
	uint32_t i;

	if (ctx->dut_common_pins != NULL) {
		for (i = 0; i < ctx->meta.common_pin_num; i++) {
			if (ctx->dut_common_pins[i].pCommonDutPin != NULL) {
				free(ctx->dut_common_pins[i].pCommonDutPin);
				ctx->dut_common_pins[i].pCommonDutPin = NULL;
			}
		}
		free(ctx->dut_common_pins);
		ctx->dut_common_pins = NULL;
	}

	if (ctx->dut_power_pins != NULL) {
		for (i = 0; i < ctx->meta.power_pin_num; i++) {
			if (ctx->dut_power_pins[i].pPowerDutPin != NULL) {
				free(ctx->dut_power_pins[i].pPowerDutPin);
				ctx->dut_power_pins[i].pPowerDutPin = NULL;
			}
		}
		free(ctx->dut_power_pins);
		ctx->dut_power_pins = NULL;
	}

	hash_destroy(&ctx->hash_common);
	hash_destroy(&ctx->hash_power);

	/* reset pool counters */
	ctx->alloc_common_pin_num = 0;
	ctx->alloc_power_pin_num  = 0;
	ctx->alloc_dut_count      = 0;
}

/* 为 _dut_pin_t 内数组分配内存，失败打印错误并返回 SOC_ERR_MEMORY_ALLOC */
static int alloc_inner_pin_array(_dut_pin_t **inner_pin, uint32_t inner_capacity,
                                  const char *pool_name, uint32_t pin_idx)
{
    *inner_pin = (_dut_pin_t *)calloc(inner_capacity, sizeof(_dut_pin_t));
    if (*inner_pin == NULL) {
        DBG_ERR("parse_socket_json: calloc %s[%u] failed\n",
                pool_name, pin_idx);
        return SOC_ERR_MEMORY_ALLOC;
    }
    return SOC_OK;
}

/*
 * 扩容路径: 释放旧内外数组, calloc 新外数组(POOL_MARGIN)及活跃范围的内数组
 */
static int expand_outer_pool(const pin_pool_desc_t *desc,
                              uint32_t new_pin_count,
                              uint32_t inner_capacity)
{
    uint32_t new_alloc = POOL_MARGIN(new_pin_count);
    uint32_t pin_idx;

    /* 考虑dut_count没变或者变少，但pin_num变化的情况 */
    if (*desc->outer_array != NULL) {
        for (pin_idx = 0; pin_idx < *desc->alloc_count; pin_idx++) {
            void *outer_elem = (char *)(*desc->outer_array)
                       + pin_idx * desc->elem_size;
            free(*pin_pool_inner_ptr(desc, outer_elem));
        }
        free(*desc->outer_array);
        *desc->outer_array = NULL;
    }
    /* calloc是申请空间后直接初始化为0 */
    /* 先申请外数组 */
    *desc->outer_array = calloc(new_alloc, desc->elem_size);
    if (*desc->outer_array == NULL) {
        DBG_ERR("parse_socket_json: calloc %s failed\n",
                desc->pool_name);
        return SOC_ERR_MEMORY_ALLOC;
    }
    /* 申请每个外数组元素的内数组_dut_pin_t */
    *desc->alloc_count = new_alloc;
    for (pin_idx = 0; pin_idx < new_pin_count; pin_idx++) {
        void *outer_elem = (char *)(*desc->outer_array)
                   + pin_idx * desc->elem_size;
        _dut_pin_t **inner_pin = pin_pool_inner_ptr(desc, outer_elem);
        if (*inner_pin == NULL) {
            int ret = alloc_inner_pin_array(inner_pin, inner_capacity,
                                             desc->pool_name, pin_idx);
            if (ret != SOC_OK) {
                return ret;
            }
        }
    }
    return SOC_OK;
}

/*
 * 复用路径: 清零活跃范围元数据+内数组, 尾部超出部分清零元数据+内数组
 */
static int reuse_outer_pool(const pin_pool_desc_t *desc,
                             uint32_t new_pin_count,
                             uint32_t inner_capacity)
{
    uint32_t pin_idx;

    for (pin_idx = 0; pin_idx < new_pin_count; pin_idx++) {
        void *outer_elem = (char *)(*desc->outer_array)
                   + pin_idx * desc->elem_size;
        _dut_pin_t **inner_pin = pin_pool_inner_ptr(desc, outer_elem);
        /* 清零元数据 */
        memset(outer_elem, 0, desc->meta_size);
        /* 考虑inner可能会被dut那边释放了，所以要重新申请 */
        if (*inner_pin == NULL) {
            int ret = alloc_inner_pin_array(inner_pin, inner_capacity,
                                             desc->pool_name, pin_idx);
            if (ret != SOC_OK) {
                return ret;
            }
        } else {
            memset(*inner_pin, 0,
                   inner_capacity * sizeof(_dut_pin_t));
        }
    }
    for (pin_idx = new_pin_count; pin_idx < *desc->alloc_count; pin_idx++) {
        void *outer_elem = (char *)(*desc->outer_array)
                   + pin_idx * desc->elem_size;
        _dut_pin_t **inner_pin = pin_pool_inner_ptr(desc, outer_elem);
        if (*inner_pin != NULL) {
            memset(*inner_pin, 0,
                   inner_capacity * sizeof(_dut_pin_t));
        }
        memset(outer_elem, 0, desc->meta_size);
    }
    return SOC_OK;
}

/**
 * @brief  管理 pin pool 外数组生命周期
 *
 * 两种模式:
 *   EXPAND — new_pin_count > alloc_count, 释放旧内外数组, calloc 扩容
 *   REUSE  — new_pin_count <= alloc_count, 清零活跃范围+尾部
 *
 * @return SOC_OK 或 SOC_ERR_MEMORY_ALLOC
 */
/*
 * 管理单个 pin pool 的外数组生命周期:
 *   - 扩容: 释放旧内外数组, calloc 新外数组(POOL_MARGIN)及活跃范围的内数组
 *   - 复用: 清零活跃范围元数据+内数组, 尾部超出部分清零元数据+内数组
 */
static int realloc_pin_pool_outer(const pin_pool_desc_t *desc,
                                   uint32_t new_pin_count,
                                   uint32_t inner_capacity)
{
    if (new_pin_count > *desc->alloc_count) {
        return expand_outer_pool(desc, new_pin_count, inner_capacity);
    }
    /* 未超规格，清除原内存空间的所有内容 */
    else {
        return reuse_outer_pool(desc, new_pin_count, inner_capacity);
    }
}

/*
 * 管理 dut_common_pins / dut_power_pins 及其内嵌 _dut_pin_t 数组的全部生命周期:
 *   - 外数组: 按 common_pin_num / power_pin_num 扩容(POOL_MARGIN)或复用, 复用
 *     时保留仍在范围内的内数组指针并清零元数据, 尾部超出部分释放内数组后置零。
 *   - 内数组: 当 dut_count 超过上次分配的 ctx->alloc_dut_count 时扩容(POOL_MARGIN),
 *     统一释放所有旧内数组; 扩容后或复用时均保证活跃范围内的内数组已分配且清零,
 *     调用方(parse_*)可直接使用, 无需再关心内存分配或清零。
 */
static int realloc_pin_pools(SocContext *ctx, uint32_t common_pin_num, uint32_t power_pin_num, uint32_t dut_count)
{
	uint32_t pi;
	int ret;

	/* ---- inner _dut_pin_t arrays ---- */
	/* dut_count超规格，释放内数组pCommonDutPin和pPowerDutpin */
	if (dut_count > ctx->alloc_dut_count) {
		ctx->alloc_dut_count = POOL_MARGIN(dut_count);
		for (pi = 0; pi < ctx->alloc_common_pin_num; pi++) {
			free(ctx->dut_common_pins[pi].pCommonDutPin);
			ctx->dut_common_pins[pi].pCommonDutPin = NULL;
		}
		for (pi = 0; pi < ctx->alloc_power_pin_num; pi++) {
			free(ctx->dut_power_pins[pi].pPowerDutPin);
			ctx->dut_power_pins[pi].pPowerDutPin = NULL;
		}
	}

	/* ---- dut_common_pins outer ---- */
	/* common_pin_num超规格，先释放内数组pCommonDutPin，再释放外数组dut_common_pins */
	{
		const pin_pool_desc_t common_desc = {
			.inner_ptr_offset = offsetof(dut_common_pin_t, pCommonDutPin),
			.meta_size        = offsetof(dut_common_pin_t, pCommonDutPin),
			.outer_array      = (void **)&ctx->dut_common_pins,
			.alloc_count      = &ctx->alloc_common_pin_num,
			.elem_size        = sizeof(dut_common_pin_t),
			.pool_name        = "dut_common_pins",
		};
		ret = realloc_pin_pool_outer(&common_desc, common_pin_num,
		                              ctx->alloc_dut_count);
		if (ret != SOC_OK) return ret;
	}

	/* ---- dut_power_pins outer ---- */
	/* power_pin_num超规格，先释放内数组pPowerDutPin，再释放外数组dut_power_pins */
	{
		const pin_pool_desc_t power_desc = {
			.inner_ptr_offset = offsetof(dut_power_pin_t, pPowerDutPin),
			.meta_size        = offsetof(dut_power_pin_t, pPowerDutPin),
			.outer_array      = (void **)&ctx->dut_power_pins,
			.alloc_count      = &ctx->alloc_power_pin_num,
			.elem_size        = sizeof(dut_power_pin_t),
			.pool_name        = "dut_power_pins",
		};
		ret = realloc_pin_pool_outer(&power_desc, power_pin_num,
		                              ctx->alloc_dut_count);
		if (ret != SOC_OK) return ret;
	}

	return SOC_OK;
}

static int parse_power_pins(SocContext *ctx, cJSON *power_pins, uint32_t power_pin_num, uint32_t dut_count)
{
	cJSON *pin_node;
	cJSON *dut_num_node;
	cJSON *dut_entry;
	cJSON *ch_name_item;

	uint32_t outer_idx = 0;
	uint32_t vs_pin_num;
	uint32_t dut_no;

	_dut_pin_t *p_pwr_dut_pin;


	for (pin_node = power_pins->child; pin_node != NULL; pin_node = pin_node->next) {
		/* 当 JSON 里的 pin 数量超过了数组容量，跳过后续多余的条目，/
		 避免写入dut_power_pins[outer_idx]时溢出 */

		/*直接break循环*/
		if (outer_idx >= power_pin_num) {
			DBG_WARN("parse_power_pins: outer_idx > power_pin_num , skip\n");
			break;
		}
		vs_pin_num = (uint32_t)atoi(pin_node->string);

		ctx->dut_power_pins[outer_idx].vs_pin_num = vs_pin_num;
		p_pwr_dut_pin = ctx->dut_power_pins[outer_idx].pPowerDutPin;

		dut_num_node = cJSON_GetObjectItem(pin_node, "dut_num");

		/*当前无dut_num_node，直接异常退出*/
		if (dut_num_node == NULL) {
			DBG_ERR("parse_socket_json: dut_num_node of power_pin \
				                         not found or invalid\n");
			return SOC_ERR_PARSE_FAILED;
		}
        /* cjson库在初始化的时候会为每个节点置一个空 */
		for (dut_entry = dut_num_node->child; dut_entry != NULL; \
			                       dut_entry = dut_entry->next) 
			{

			dut_no = (uint32_t)atoi(dut_entry->string);

			/* 若dut_no超过dut_count直接退出此次遍历的dut_num_node节点，直接break退出*/
			if (dut_no > dut_count) {
				DBG_WARN("parse_power_pins: dut_no >  dut_count, skip\n");			
				break;
			}

			ch_name_item = cJSON_GetObjectItem(dut_entry, "ch_name");

			/* 若某个dut_num_node的子节点无ch_name，直接异常退出 */
			if (ch_name_item == NULL 
				|| !cJSON_IsString(ch_name_item)) 
			{
	            DBG_ERR("parse_socket_json: ch_name_node of power_pin \
					                        not found or invalid\n");				
				return SOC_ERR_PARSE_FAILED;
			}

			strncpy(p_pwr_dut_pin[dut_no].ch_name, 
				        ch_name_item->valuestring,
				             MAX_CH_NAME_LEN - 1);

			p_pwr_dut_pin[dut_no].ch_name[MAX_CH_NAME_LEN - 1] = '\0';
			p_pwr_dut_pin[dut_no].dut_no = dut_no;

			hash_insert(&ctx->hash_power, p_pwr_dut_pin[dut_no].ch_name,
				outer_idx, dut_no);
		}
		outer_idx++;
	}

	return SOC_OK;
}

static int parse_common_pins(SocContext *ctx, cJSON *common_pins, uint32_t common_pin_num, uint32_t dut_count)
{
	cJSON *pin_node;
	cJSON *pin_type_item;
	cJSON *dut_num_node;
	cJSON *dut_entry;
    cJSON *ch_name_item;

	uint32_t outer_idx = 0;
	uint32_t pin_num_val;
	uint32_t dut_no;
	_dut_pin_t *p_comm_dut_pin;


	for (pin_node = common_pins->child; pin_node != NULL; pin_node = pin_node->next) {

		if (outer_idx >= common_pin_num) {
			DBG_WARN("parse_common_pins: outer_idx > common_pin_num , skip\n");
			break;
		}
		pin_num_val = (uint32_t)atoi(pin_node->string);

		ctx->dut_common_pins[outer_idx].common_pin_num = pin_num_val;

		/* pin_type 在 pin 级别, 整数 0=DR 1=IO */
		pin_type_item = cJSON_GetObjectItem(pin_node, "pin_type");
		if (pin_type_item != NULL && cJSON_IsNumber(pin_type_item)) {
			ctx->dut_common_pins[outer_idx].pin_type = (uint8_t)pin_type_item->valueint;
		}

		p_comm_dut_pin = ctx->dut_common_pins[outer_idx].pCommonDutPin;

		dut_num_node = cJSON_GetObjectItem(pin_node, "dut_num");
		/*当前无dut_num_node，直接异常退出*/		
		if (dut_num_node == NULL) {
	        DBG_ERR("parse_socket_json: dut_num_node of common_pin \
					                       not found or invalid\n");				
			return SOC_ERR_PARSE_FAILED;
		}

		for (dut_entry = dut_num_node->child; dut_entry != NULL; \
			                          dut_entry = dut_entry->next) 
		{

			dut_no = (uint32_t)atoi(dut_entry->string);

			/* 若dut_no超过dut_count直接退出此次遍历的dut_num_node节点，直接break退出*/
			if (dut_no > dut_count) {
				DBG_WARN("parse_common_pins: dut_no > dut_count , skip\n");
				break;;
			}

			ch_name_item = cJSON_GetObjectItem(dut_entry, "ch_name");

			/* 若某个dut_num_node的子节点无ch_name，直接异常退出 */
			if (ch_name_item == NULL || !cJSON_IsString(ch_name_item)) {
	            DBG_ERR("parse_socket_json: ch_name_node of common_pin \
					                           not found or invalid\n");	
				return SOC_ERR_PARSE_FAILED;
			}

			strncpy(p_comm_dut_pin[dut_no].ch_name, ch_name_item->valuestring,
				MAX_CH_NAME_LEN - 1);
			p_comm_dut_pin[dut_no].ch_name[MAX_CH_NAME_LEN - 1] = '\0';
			p_comm_dut_pin[dut_no].dut_no = dut_no;

			hash_insert(&ctx->hash_common, p_comm_dut_pin[dut_no].ch_name,
				outer_idx, dut_no);
		}
		outer_idx++;
	}

	return SOC_OK;
}

RES_API int parse_socket_json(SocContext *ctx, const char *json_path)
{
	char *file_buf;
	cJSON *root;
	cJSON *item;
	uint32_t dut_count;
	uint32_t power_pin_num;
	uint32_t common_pin_num;
	cJSON *power_pins;
	cJSON *common_pins;
	int ret;

	DBG_INFO("parse_socket_json: start, path=%s\n", json_path);

	/* 1. 读取文件 */
	file_buf = read_file_to_string(json_path);
	if (file_buf == NULL) {
		return SOC_ERR_FILE_NOT_FOUND;
	}
	//插入哨兵，检查是否有重置需求
	if (ctx->restart_needed) {
		free(file_buf);
		return SOC_ERR_INTERRUPTED;
	}

	/* 2. 解析 JSON */
	root = cJSON_Parse(file_buf);
	free(file_buf);
	file_buf = NULL;

	if (root == NULL) {
		DBG_ERR("parse_socket_json: cJSON_Parse failed\n");
		return SOC_ERR_PARSE_FAILED;
	}
	if (ctx->restart_needed) { 
		RET_WITH_DEL_JSON(root,SOC_ERR_INTERRUPTED);
	}

	/* 3. 读取顶层计数字段 */
	item = cJSON_GetObjectItem(root, "dutCount");
	if (item == NULL || !cJSON_IsNumber(item)) {
		DBG_ERR("parse_socket_json: dutCount not found or invalid\n");
		RET_WITH_DEL_JSON(root,SOC_ERR_PARSE_FAILED);
	}
	dut_count = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(root, "commonPinCount");
	if (item == NULL || !cJSON_IsNumber(item)) {
		DBG_ERR("parse_socket_json: commonPinCount not found or invalid\n");
		RET_WITH_DEL_JSON(root,SOC_ERR_PARSE_FAILED);		
	}
	common_pin_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(root, "powerPinCount");
	if (item == NULL || !cJSON_IsNumber(item)) {
		DBG_ERR("parse_socket_json: powerPinCount not found or invalid\n");
		RET_WITH_DEL_JSON(root,SOC_ERR_PARSE_FAILED);
	}
	power_pin_num = (uint32_t)item->valueint;

	DBG_INFO("parse_socket_json: dut=%u, common_pin=%u, power_pin=%u\n",
		dut_count, common_pin_num, power_pin_num);

	ctx->meta.dut_count      = dut_count;
	ctx->meta.common_pin_num = common_pin_num;
	ctx->meta.power_pin_num  = power_pin_num;

	if (ctx->restart_needed) {
		RET_WITH_DEL_JSON(root,SOC_ERR_INTERRUPTED);
	}
	/* 5. pool: reuse or realloc outer & inner arrays */
	ret = realloc_pin_pools(ctx, common_pin_num, power_pin_num, dut_count);
	if (ret != SOC_OK) {
		cJSON_Delete(root);
		free_socket_data(ctx);
		return ret;
	}

	/* 6. 初始化哈希表 */
	hash_init(&ctx->hash_common);
	hash_init(&ctx->hash_power);

	if (ctx->restart_needed) { 
		RET_WITH_DEL_JSON(root,SOC_ERR_INTERRUPTED);
	}

	/* 7. 解析 powerPins */
	power_pins = cJSON_GetObjectItem(root, "powerPins");
	if (power_pins != NULL) {
		ret = parse_power_pins(ctx, power_pins, power_pin_num, dut_count);
		if (ret != SOC_OK) {
			cJSON_Delete(root);
			free_socket_data(ctx);
			return ret;
		}
	}

	if (ctx->restart_needed) {
		RET_WITH_DEL_JSON(root,SOC_ERR_INTERRUPTED);
	}

	/* 9. 解析 commonPins */
	common_pins = cJSON_GetObjectItem(root, "commonPins");
	if (common_pins != NULL) {
		ret = parse_common_pins(ctx, common_pins, common_pin_num, dut_count);
		if (ret != SOC_OK) {
			cJSON_Delete(root);
			free_socket_data(ctx);
			return ret;
		}
	}
	cJSON_Delete(root);
	return SOC_OK;
}
