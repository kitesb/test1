#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dealsoc_internal.h"
#include "soc_context.h"
#include "HashTable.h"
#include "cJSON.h"
#include "internal.h"
#include "log_mgmt.h"

static soc_error_t parse_info_node(SocContext *ctx, cJSON *root)
{
	cJSON *info;
	cJSON *item;

	/* 3. 解析 INFO 节点 */
	info = cJSON_GetObjectItem(root, "INFO");
	if (info == NULL) {
		DBG_ERR("parse_config_json: INFO node not found\n");
		return SOC_ERR_PARSE_FAILED;
	}

	item = cJSON_GetObjectItem(info, "SEM_NUM");
	if (item == NULL || !cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: SEM_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.sem_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(info, "ASIC_NUM");
	if (item == NULL || !cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: ASIC_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.asic_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(info, "DR_NUM");
	if (item == NULL || !cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: DR_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.dr_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(info, "IO_NUM");
	if (item == NULL || !cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: IO_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.io_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(info, "PSM_NUM");
	if (item == NULL ||!cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: PSM_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.psm_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(info, "PPS_NUM");
	if (item == NULL || !cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: DUT_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.pps_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(info, "DUT_NUM");
	if (item == NULL ||! cJSON_IsNumber(item)){
		DBG_ERR("parse_config_json: PPS_NUM not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	ctx->logic_config.dut_num = (uint32_t)item->valueint;

	if (ctx->restart_needed) {
		return SOC_ERR_INTERRUPTED;
	}

	DBG_INFO("parse_config_json: sem=%u asic=%u dr=%u io=%u psm=%u pps=%u dut=%u\n",
		ctx->logic_config.sem_num, ctx->logic_config.asic_num,
		ctx->logic_config.dr_num, ctx->logic_config.io_num,
		ctx->logic_config.psm_num, ctx->logic_config.pps_num,
		ctx->logic_config.dut_num);

	return SOC_OK;
}

static soc_error_t fill_single_common_pin(SocContext *ctx, cJSON *pin_obj_item,
    uint32_t sem_idx, uint32_t asic_idx, uint32_t asic_num, uint32_t ch_total)
{
	const char *ch_name;
	uint32_t ch_idx;
	uint32_t outer_idx;
	uint32_t dut_no;
	uint32_t whole_no;
	cJSON *ch_node;

	/* 没找到ch_name,异常退出 */
	if (pin_obj_item == NULL || pin_obj_item->child == NULL) {
		DBG_ERR("parse_config_json: pin_obj_item of common_pin \
						                           not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}

	/* 单键对象 {"PD0161B1": 52} */
	ch_node = pin_obj_item->child;
	ch_name = ch_node->string;
	ch_idx  = (uint32_t)ch_node->valueint;

	if (hash_find(&ctx->hash_common, ch_name, &outer_idx, &dut_no)) {
		whole_no = COMMON_SERIAL(sem_idx, asic_idx, ch_idx,
			asic_num, ch_total);
		ctx->dut_common_pins[outer_idx].pCommonDutPin[dut_no].whole_no = whole_no;
	} else {
		DBG_ERR("fill_common_pin_whole_no: ch_name '%s' not found in hash_common\n", ch_name);
	}

	return SOC_OK;
}

static soc_error_t fill_common_asic_pins(SocContext *ctx, cJSON *asic_node,
    uint32_t sem_idx, uint32_t asic_idx, uint32_t asic_num, uint32_t ch_total)
{
	cJSON *pins_arr;
	cJSON *pin_obj_item;
	int pin_count;
	int i;
	soc_error_t ret;

	pins_arr = cJSON_GetObjectItem(asic_node, "pins");
	/* 若没有pin_arr节点，证明json文件有问题，直接异常退出 */
	if (pins_arr == NULL || !cJSON_IsArray(pins_arr)) {
		DBG_ERR("parse_config_json: pins_arr_node not found or invalid\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}

	pin_count = cJSON_GetArraySize(pins_arr);
	for (i = 0; i < pin_count; i++) {
		pin_obj_item = cJSON_GetArrayItem(pins_arr, i);
		ret = fill_single_common_pin(ctx, pin_obj_item, sem_idx, asic_idx,
			asic_num, ch_total);
		if (ret != SOC_OK) {
			return ret;
		}
	}

	return SOC_OK;
}

static soc_error_t fill_common_pin_whole_no(SocContext *ctx, cJSON *root)
{
	uint32_t sem_idx;
	uint32_t asic_idx;
	uint32_t ch_total;
	char sem_key[16];
	char asic_key[16];
	cJSON *sem_node;
	cJSON *asic_node;
	soc_error_t ret;

	ch_total = ctx->logic_config.dr_num + ctx->logic_config.io_num;

	/* 6. 遍历 SEM → ASIC → pins, 填充普通 Pin 的 whole_no */
	for (sem_idx = 1; sem_idx <= ctx->logic_config.sem_num; sem_idx++) {
		snprintf(sem_key, sizeof(sem_key), "SEM%u", sem_idx);
		sem_node = cJSON_GetObjectItem(root, sem_key);
		/* 若没有sem_node，证明json文件有问题，直接异常退出 */
		if (sem_node == NULL) {
			DBG_ERR("parse_config_json: sem_node not found or invalid\n");
			return SOC_ERR_JSON_PARSE_FAILED;
		}
		if (ctx->restart_needed) {
			return SOC_ERR_INTERRUPTED;
		}

		for (asic_idx = 1; asic_idx <= ctx->logic_config.asic_num; asic_idx++) {
			snprintf(asic_key, sizeof(asic_key), "ASIC%u", asic_idx);
			asic_node = cJSON_GetObjectItem(sem_node, asic_key);
			/* 若没有asic_node，证明json文件有问题，直接异常退出 */
			if (asic_node == NULL) {
				DBG_ERR("parse_config_json: asic_node not found or invalid\n");
				return SOC_ERR_JSON_PARSE_FAILED;
			}

			ret = fill_common_asic_pins(ctx, asic_node, sem_idx, asic_idx,
				ctx->logic_config.asic_num, ch_total);
			if (ret != SOC_OK) {
				return ret;
			}
		}
	}

	return SOC_OK;
}

static soc_error_t fill_power_pin_whole_no(SocContext *ctx, cJSON *root)
{
	uint32_t psm_idx;
	const char *ch_name;
	uint32_t ch_idx;
	uint32_t outer_idx;
	uint32_t dut_no;
	uint32_t whole_no;
	char psm_key[16];
	int pwr_count;
	int i;

	cJSON *pin_obj_item;
	cJSON *ch_node;
	cJSON *psm_node;
	cJSON *pwr_arr;

	/* 7. 遍历 PSM → power_channels, 填充电源 Pin 的 whole_no */
	for (psm_idx = 1; psm_idx <= ctx->logic_config.psm_num; psm_idx++) {
		snprintf(psm_key, sizeof(psm_key), "PSM%u", psm_idx);
		psm_node = cJSON_GetObjectItem(root, psm_key);
		if (psm_node == NULL) {
		        DBG_ERR("parse_config_json: psm_node not found or invalid\n");
			return SOC_ERR_JSON_PARSE_FAILED;
		}
		if (ctx->restart_needed) {
			return SOC_ERR_INTERRUPTED;
		}

		pwr_arr = cJSON_GetObjectItem(psm_node, "power_channels");
		if (pwr_arr == NULL || !cJSON_IsArray(pwr_arr)) {
		        DBG_ERR("parse_config_json: pwr_arr not found or invalid\n");
			return SOC_ERR_JSON_PARSE_FAILED;
		}

		pwr_count = cJSON_GetArraySize(pwr_arr);
		for (i = 0; i < pwr_count; i++) {
			pin_obj_item = cJSON_GetArrayItem(pwr_arr, i);
			if (pin_obj_item == NULL || pin_obj_item->child == NULL) {
		        	DBG_ERR("parse_config_json: pin_obj_item of power_pin \
						                          not found or invalid\n");
				return SOC_ERR_JSON_PARSE_FAILED;
			}

			ch_node = pin_obj_item->child;
			ch_name = ch_node->string;
			ch_idx  = (uint32_t)ch_node->valueint;

			if (hash_find(&ctx->hash_power, ch_name, &outer_idx, &dut_no)) {
				whole_no = POWER_SERIAL(psm_idx, ch_idx,
					ctx->logic_config.pps_num);
				ctx->dut_power_pins[outer_idx].pPowerDutPin[dut_no].whole_no = whole_no;
			} else {
				DBG_ERR("fill_power_pin_whole_no: ch_name '%s' not found in hash_power\n", ch_name);
			}
		}
	}

	return SOC_OK;
}

int parse_config_json(SocContext *ctx, const char *json_path)
{
	char *file_buf;
	cJSON *root;
	uint32_t ddut;
	uint32_t sdut;
	soc_error_t ret;

	DBG_INFO("parse_config_json: start, path=%s\n", json_path);

	/* 前置条件: socket 必须先解析完成才能填 whole_no */
	if (ctx->meta.socket_parse_state != SOCKET_PARSE_FINISHED) {
		DBG_ERR("parse_config_json: socket not parsed yet\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}

	/* on_socket_board 为 0 则跳过, 不报错 */
	if (ctx->meta.on_socket_board == 0) {
		DBG_INFO("parse_config_json: on_socket_board=0, skip\n");
		return SOC_OK;
	}

	/* 1. 读取文件 */
	file_buf = read_file_to_string(json_path);
	if (file_buf == NULL) {
		return SOC_ERR_JSON_NOT_FOUND;
	}
	if (ctx->restart_needed) {
		free(file_buf);
		return SOC_ERR_INTERRUPTED;
	}

	/* 2. 解析 JSON */
	root = cJSON_Parse(file_buf);
	free(file_buf);
	file_buf = NULL;

	if (root == NULL) {
		DBG_ERR("parse_config_json: cJSON_Parse failed\n");
		return SOC_ERR_JSON_PARSE_FAILED;
	}
	if (ctx->restart_needed) {
		RET_WITH_DEL_JSON(root,SOC_ERR_INTERRUPTED);
	}

	/* 3. 解析 INFO 节点 */
	ret = parse_info_node(ctx, root);
	if (ret != SOC_OK) {
		RET_WITH_DEL_JSON(root,ret);
	}

	/* 校验: config.dut_num >= socket.dut_count */
	ddut = ctx->logic_config.dut_num;
	sdut = ctx->meta.dut_count;
	if (ddut < sdut) {
		DBG_ERR("parse_config_json: ddut %u < sdut %u, board mismatch\n", ddut, sdut);
		RET_WITH_DEL_JSON(root,SOC_ERR_BOARD_MISMATCH);
	}


	/* 4. 遍历 SEM → ASIC → pins, 填充普通 Pin 的 whole_no */
	ret = fill_common_pin_whole_no(ctx, root);
	if (ret != SOC_OK) {
	    	RET_WITH_DEL_JSON(root,ret);
	}

	/* 5. 遍历 PSM → power_channels, 填充电源 Pin 的 whole_no */
	ret = fill_power_pin_whole_no(ctx, root);
	if (ret != SOC_OK) {
		RET_WITH_DEL_JSON(root,ret);
	}

	/* 6. 解析完成 — 状态设置和广播由 pipeline wrapper 负责 */

	DBG_INFO("parse_config_json: finished, JSON_PARSE_FINISHED\n");

	cJSON_Delete(root);
	return SOC_OK;
}
