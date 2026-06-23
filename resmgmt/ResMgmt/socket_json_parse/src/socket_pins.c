/*--- 模块头文件 ---*/
#include "socket_internal.h"

/*--- 项目头文件 ---*/
#include "log_mgmt.h"

/*--- 第三方库头文件 ---*/
#include <cJSON.h>

/*--- 系统头文件 ---*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 填充单个 dut_entry: 解析 dut_no(逻辑标号), 校验 ch_name, 存入顺序槽位 dut_idx,
 * 将 dut_idx(非 dut_no) 插入哈希表供 config_parser 按位置索引。
 * dut_no 是逻辑标识符不可当数组下标; dut_idx 是调用方维护的顺序计数器。 */
static int fill_pin_dut_entry(SocContext *ctx, cJSON *dut_entry,
    _dut_pin_t *pin_array, uint32_t dut_idx,
    hash_table_t *hash, uint32_t outer_idx,
    const char *pin_type_name, uint32_t max_slots)
{
	cJSON *ch_name_item;
	int hr;
	uint32_t dut_no;

	{
		char *endptr;
		errno = 0;
		long val = strtol(dut_entry->string, &endptr, 10);
		if (dut_entry->string[0] == '\0' || *endptr != '\0'
		    || val < 0 || val > UINT32_MAX || errno == ERANGE) {
			DBG_ERR("fill_pin_dut_entry: %s dut_no key invalid\n", pin_type_name);
			return SOC_ERR_PARSE_FAILED;
		}
		dut_no = (uint32_t)val;
	}

	ch_name_item = cJSON_GetObjectItem(dut_entry, "ch_name");
	if (ch_name_item == NULL) {
		DBG_ERR("fill_pin_dut_entry: %s ch_name not found\n", pin_type_name);
		return 0;
	}

	if (cJSON_IsString(ch_name_item)) {
		/* 单字符串 ch_name（向后兼容） */
		if (dut_idx >= max_slots) {
			DBG_ERR("fill_pin_dut_entry: %s dut_idx %u >= max_slots %u\n",
			        pin_type_name, dut_idx, max_slots);
			return 0;
		}
		strncpy(pin_array[dut_idx].ch_name, ch_name_item->valuestring, MAX_CH_NAME_LEN - 1);
		pin_array[dut_idx].ch_name[MAX_CH_NAME_LEN - 1] = '\0';
		pin_array[dut_idx].dut_no = dut_no;

		/* 哈希表存顺序位置 dut_idx, 非逻辑标号 dut_no */
		hr = hash_insert(hash, pin_array[dut_idx].ch_name, outer_idx, dut_idx);
		if (hr != SOC_OK) {
			DBG_ERR("fill_pin_dut_entry: %s hash_insert failed for %s\n",
			        pin_type_name, pin_array[dut_idx].ch_name);
			return 0;
		}
		return 1;
	}

	if (cJSON_IsArray(ch_name_item)) {
		/* 数组 ch_name — 展开为多个连续槽位 */
		int arr_len = cJSON_GetArraySize(ch_name_item);
		if (arr_len <= 0) {
			DBG_ERR("fill_pin_dut_entry: %s ch_name array is empty\n", pin_type_name);
			return 0;
		}
		if (dut_idx + (uint32_t)arr_len > max_slots) {
			DBG_ERR("fill_pin_dut_entry: %s array overflow"
			        " (dut_idx=%u + arr_len=%d > max_slots=%u)\n",
			        pin_type_name, dut_idx, arr_len, max_slots);
			return 0;
		}
		for (int i = 0; i < arr_len; i++) {
			cJSON *elem = cJSON_GetArrayItem(ch_name_item, i);
			if (elem == NULL || !cJSON_IsString(elem)) {
				DBG_ERR("fill_pin_dut_entry: %s ch_name[%d] is not a string\n",
				        pin_type_name, i);
				return 0;
			}
			strncpy(pin_array[dut_idx + i].ch_name, elem->valuestring, MAX_CH_NAME_LEN - 1);
			pin_array[dut_idx + i].ch_name[MAX_CH_NAME_LEN - 1] = '\0';
			pin_array[dut_idx + i].dut_no = dut_no;

			/* 每个展开的 ch_name 独立插入哈希表 */
			hr = hash_insert(hash, pin_array[dut_idx + i].ch_name, outer_idx, dut_idx + i);
			if (hr != SOC_OK) {
				DBG_ERR("fill_pin_dut_entry: %s hash_insert failed for %s\n",
				        pin_type_name, pin_array[dut_idx + i].ch_name);
				return 0;
			}
		}
		return arr_len;
	}

	DBG_ERR("fill_pin_dut_entry: %s ch_name is neither string nor array\n", pin_type_name);
	return 0;
}

int parse_power_pins(SocContext *ctx, cJSON *power_pins, uint32_t power_pin_num, uint32_t dut_count)
{
	cJSON *pin_node;
	cJSON *dut_num_node;
	cJSON *dut_entry;

	uint32_t outer_idx = 0;
	uint32_t vs_pin_num;

	_dut_pin_t *p_pwr_dut_pin;


	for (pin_node = power_pins->child; pin_node != NULL; pin_node = pin_node->next) {
		/* 当 JSON 里的 pin 数量超过了数组容量，跳过后续多余的条目，/
		 避免写入dut_power_pins[outer_idx]时溢出 */

		/*直接break循环*/
		if (outer_idx >= power_pin_num) {
			DBG_WARN("parse_power_pins: outer_idx >= power_pin_num , skip\n");
			break;
		}
		{
			char *endptr;
			errno = 0;
			long val = strtol(pin_node->string, &endptr, 10);
			if (pin_node->string[0] == '\0' || *endptr != '\0'
			    || val <= 0 || val > UINT32_MAX || errno == ERANGE) {
				DBG_ERR("parse_power_pins: pin key '%s' is not a valid positive integer\n",
				        pin_node->string);
				return SOC_ERR_PARSE_FAILED;
			}
			vs_pin_num = (uint32_t)val;
		}

		ctx->dut_power_pins[outer_idx].vs_pin_num = vs_pin_num;
		p_pwr_dut_pin = ctx->dut_power_pins[outer_idx].pPowerDutPin;

		dut_num_node = cJSON_GetObjectItem(pin_node, "dut_num");

		/*当前无dut_num_node，直接异常退出*/
		if (dut_num_node == NULL || !cJSON_IsObject(dut_num_node)) {
			DBG_ERR("parse_socket_json: dut_num_node of power_pin \
				                         not found or invalid\n");
			return SOC_ERR_PARSE_FAILED;
		}
        /* cjson库在初始化的时候会为每个节点置一个空 */
		{
			uint32_t dut_idx = 0;
			for (dut_entry = dut_num_node->child; dut_entry != NULL;
				                       dut_entry = dut_entry->next) {
				int entry_count = fill_pin_dut_entry(ctx, dut_entry,
				    p_pwr_dut_pin, dut_idx, &ctx->hash_power,
				    outer_idx, "power_pin", dut_count);
				if (entry_count <= 0) return SOC_ERR_PARSE_FAILED;
				dut_idx += (uint32_t)entry_count;
			}
		}
		outer_idx++;
	}

	return SOC_OK;
}

int parse_common_pins(SocContext *ctx, cJSON *common_pins, uint32_t common_pin_num, uint32_t dut_count)
{
	cJSON *pin_node;
	cJSON *pin_type_item;
	cJSON *dut_num_node;
	cJSON *dut_entry;

	uint32_t outer_idx = 0;
	uint32_t pin_num_val;
	_dut_pin_t *p_comm_dut_pin;


	for (pin_node = common_pins->child; pin_node != NULL; pin_node = pin_node->next) {

		if (outer_idx >= common_pin_num) {
			DBG_WARN("parse_common_pins: outer_idx >= common_pin_num , skip\n");
			break;
		}
		{
			char *endptr;
			errno = 0;
			long val = strtol(pin_node->string, &endptr, 10);
			if (pin_node->string[0] == '\0' || *endptr != '\0'
			    || val <= 0 || val > UINT32_MAX || errno == ERANGE) {
				DBG_ERR("parse_common_pins: pin key '%s' is not a valid positive integer\n",
				        pin_node->string);
				return SOC_ERR_PARSE_FAILED;
			}
			pin_num_val = (uint32_t)val;
		}

		ctx->dut_common_pins[outer_idx].common_pin_num = pin_num_val;

		/* pin_type 在 pin 级别, 整数 0=DR 1=IO */
		pin_type_item = cJSON_GetObjectItem(pin_node, "pin_type");
		if (pin_type_item != NULL && cJSON_IsNumber(pin_type_item) && (pin_type_item->valueint == 0 || pin_type_item->valueint == 1)) {
			ctx->dut_common_pins[outer_idx].pin_type = (uint8_t)pin_type_item->valueint;
		}

		p_comm_dut_pin = ctx->dut_common_pins[outer_idx].pCommonDutPin;

		dut_num_node = cJSON_GetObjectItem(pin_node, "dut_num");
		/*当前无dut_num_node，直接异常退出*/
		if (dut_num_node == NULL || !cJSON_IsObject(dut_num_node)) {
	        DBG_ERR("parse_socket_json: dut_num_node of common_pin \
					                       not found or invalid\n");
			return SOC_ERR_PARSE_FAILED;
		}

		{
			uint32_t dut_idx = 0;
			for (dut_entry = dut_num_node->child; dut_entry != NULL;
			                       dut_entry = dut_entry->next) {
				int entry_count = fill_pin_dut_entry(ctx, dut_entry,
				    p_comm_dut_pin, dut_idx, &ctx->hash_common,
				    outer_idx, "common_pin", dut_count);
				if (entry_count <= 0) return SOC_ERR_PARSE_FAILED;
				dut_idx += (uint32_t)entry_count;
			}
		}
		outer_idx++;
	}

	return SOC_OK;
}
