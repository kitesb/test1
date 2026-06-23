/*--- 模块头文件 ---*/
#include "socket_internal.h"

/*--- 项目头文件 ---*/
#include "log_mgmt.h"
#include "soc_context.h"

/*--- 第三方库头文件 ---*/
#include <cJSON.h>

/*--- 系统头文件 ---*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int parse_optional_pin_names(SocContext *ctx, cJSON *root)
{
	cJSON *arr;
	cJSON *entry;
	cJSON *item;
	uint32_t idx;

	arr = cJSON_GetObjectItem(root, "pinNames");
	if (arr == NULL) {
		DBG_INFO("parse_optional_pin_names: pinNames not present, skip\n");
		return SOC_OK;
	}
	if (!cJSON_IsArray(arr)) {
		DBG_ERR("parse_optional_pin_names: pinNames is not an array\n");
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_optional_pin_names: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	ctx->extra->pin_name_count = (uint32_t)cJSON_GetArraySize(arr);
	if (ctx->extra->pin_name_count == 0) {
		DBG_INFO("parse_optional_pin_names: pinNames is empty array, skip\n");
		return SOC_OK;
	}

	ctx->extra->pin_names = (pin_name_t *)calloc(ctx->extra->pin_name_count,
	                                               sizeof(pin_name_t));
	if (ctx->extra->pin_names == NULL) {
		DBG_ERR("parse_optional_pin_names: calloc pin_names failed\n");
		return SOC_ERR_MEMORY_ALLOC;
	}

	idx = 0;
	cJSON_ArrayForEach(entry, arr) {

		/* 若发现错误的话直接return */
		if (!cJSON_IsObject(entry)) 
		{
			DBG_ERR("parse_optional_pin_names: entry in pinNames is not an object\n");
			return SOC_ERR_PARSE_FAILED;
		}

		item = cJSON_GetObjectItem(entry, "pinName");
		if (item != NULL) {
			if (cJSON_IsString(item)) {
				strncpy(ctx->extra->pin_names[idx].pin_name,
				        item->valuestring, 31);
				ctx->extra->pin_names[idx].pin_name[31] = '\0';
			} else {
				DBG_ERR("parse_optional_pin_names: pinName is not a string\n");
				return SOC_ERR_PARSE_FAILED;
			}
		}

		item = cJSON_GetObjectItem(entry, "pinNum");
		if (item != NULL) {
			if (cJSON_IsString(item)) {
				strncpy(ctx->extra->pin_names[idx].pin_num,
				        item->valuestring, 15);
				ctx->extra->pin_names[idx].pin_num[15] = '\0';
			} else {
				DBG_ERR("parse_optional_pin_names: pinNum is not a string\n");
				return SOC_ERR_PARSE_FAILED;
			}
		}

		item = cJSON_GetObjectItem(entry, "dutPinNum");
		if (item != NULL) {
			if (cJSON_IsNumber(item) && item->valueint >= 0) {
				ctx->extra->pin_names[idx].dut_pin_num = (uint32_t)item->valueint;
			} else {
				DBG_ERR("parse_optional_pin_names: dutPinNum is not a valid number\n");
				return SOC_ERR_PARSE_FAILED;
			}
		}

		if (idx >= ctx->extra->pin_name_count) break;
		idx++;
	}

	return SOC_OK;
}

int parse_optional_symbol(SocContext *ctx, cJSON *root)
{
	cJSON *sym;
	cJSON *child;
	uint32_t idx;

	sym = cJSON_GetObjectItem(root, "symbol");
	if (sym == NULL) {
		DBG_INFO("parse_optional_symbol: symbol not present, skip\n");
		return SOC_OK;
	}
	if (!cJSON_IsObject(sym)) {
		DBG_ERR("parse_optional_symbol: symbol is not an object\n");
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_optional_symbol: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	ctx->extra->symbol_count = 0;
	for (child = sym->child; child != NULL; child = child->next)
		ctx->extra->symbol_count++;

	if (ctx->extra->symbol_count == 0) {
		return SOC_OK;
	}

	ctx->extra->symbols = (symbol_entry_t *)calloc(ctx->extra->symbol_count,
	                                                 sizeof(symbol_entry_t));
	if (ctx->extra->symbols == NULL) {
		DBG_ERR("parse_optional_symbol: calloc symbols failed\n");
		return SOC_ERR_MEMORY_ALLOC;
	}

	idx = 0;
	for (child = sym->child; child != NULL; child = child->next) {
		if (cJSON_IsString(child)) {
			strncpy(ctx->extra->symbols[idx].pin_name, child->string, 31);
			ctx->extra->symbols[idx].pin_name[31] = '\0';
			strncpy(ctx->extra->symbols[idx].vs_name, child->valuestring, 15);
			ctx->extra->symbols[idx].vs_name[15] = '\0';
		} else {
			DBG_ERR("parse_optional_symbol: symbol value for \"%s\" is not a string\n",
			        child->string);
			return SOC_ERR_PARSE_FAILED;
		}
		idx++;
	}

	return SOC_OK;
}

static int parse_optional_vs_defs_internal(SocContext *ctx, cJSON *parent,
                                             const char *node_name,
                                             vs_def_t **out_defs,
                                             uint32_t *out_count)
{
	cJSON *node;
	cJSON *vs_entry;
	cJSON *ch_item;
	cJSON *child;
	uint32_t vs_idx;
	uint32_t ch_idx;

	node = cJSON_GetObjectItem(parent, node_name);
	if (node == NULL) {
		DBG_INFO("parse_optional_vs_defs: %s not present, skip\n", node_name);
		return SOC_OK;
	}
	if (!cJSON_IsObject(node)) {
		DBG_ERR("parse_optional_vs_defs: %s is not an object\n", node_name);
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_optional_vs_defs: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	*out_count = 0;
	for (vs_entry = node->child; vs_entry != NULL; vs_entry = vs_entry->next) {
		if (!cJSON_IsObject(vs_entry)) {
			DBG_ERR("parse_optional_vs_defs: entry in %s is not an object\n", node_name);
			return SOC_ERR_PARSE_FAILED;
		}
		(*out_count)++;
	}

	if (*out_count == 0) {
		return SOC_OK;
	}

	*out_defs = (vs_def_t *)calloc(*out_count, sizeof(vs_def_t));
	if (*out_defs == NULL) {
		DBG_ERR("parse_optional_vs_defs: calloc %s failed\n", node_name);
		return SOC_ERR_MEMORY_ALLOC;
	}

	vs_idx = 0;
	for (vs_entry = node->child; vs_entry != NULL; vs_entry = vs_entry->next) {
		if (!cJSON_IsObject(vs_entry)) {
			DBG_ERR("parse_optional_vs_defs: entry in %s is not an object\n", node_name);
			return SOC_ERR_PARSE_FAILED;
		}
		strncpy((*out_defs)[vs_idx].name, vs_entry->string, 15);
		(*out_defs)[vs_idx].name[15] = '\0';

		ch_item = cJSON_GetObjectItem(vs_entry, "ppsType");
		if (ch_item != NULL) {
			if (cJSON_IsString(ch_item)) {
				strncpy((*out_defs)[vs_idx].pps_type, ch_item->valuestring, 15);
				(*out_defs)[vs_idx].pps_type[15] = '\0';
			} else {
				DBG_ERR("parse_optional_vs_defs: ppsType in %s is not a string\n", node_name);
				return SOC_ERR_PARSE_FAILED;
			}
		}

		ch_item = cJSON_GetObjectItem(vs_entry, "channels");
		if (ch_item != NULL) {
			if (cJSON_IsArray(ch_item)) {
				(*out_defs)[vs_idx].channel_count = (uint32_t)cJSON_GetArraySize(ch_item);
				if ((*out_defs)[vs_idx].channel_count > 0) {
					(*out_defs)[vs_idx].channels = (uint32_t *)calloc(
					    (*out_defs)[vs_idx].channel_count, sizeof(uint32_t));
					if ((*out_defs)[vs_idx].channels == NULL) {
						DBG_ERR("parse_optional_vs_defs: calloc channels failed\n");
						return SOC_ERR_MEMORY_ALLOC;
					}
					ch_idx = 0;
					cJSON_ArrayForEach(child, ch_item) {
						if (cJSON_IsNumber(child) && child->valueint >= 0) {
							(*out_defs)[vs_idx].channels[ch_idx] = (uint32_t)child->valueint;
						} else {
							DBG_ERR("parse_optional_vs_defs: channel[%u] in %s is not a valid number\n",
							        ch_idx, node_name);
							return SOC_ERR_PARSE_FAILED;
						}
						ch_idx++;
					}
				}
			} else {
				DBG_ERR("parse_optional_vs_defs: channels in %s is not an array\n", node_name);
				return SOC_ERR_PARSE_FAILED;
			}
		}
		vs_idx++;
	}

	return SOC_OK;
}

int parse_optional_vs_definitions(SocContext *ctx, cJSON *root)
{
	return parse_optional_vs_defs_internal(ctx, root, "vsDefinitions",
	                                         &ctx->extra->vs_defs,
	                                         &ctx->extra->vs_def_count);
}

static int parse_optional_vs_site_defs_internal(SocContext *ctx, cJSON *parent,
                                                   const char *node_name,
                                                   vs_site_entry_t **out_defs,
                                                   uint32_t *out_count)
{
	cJSON *node;
	cJSON *site_entry;
	cJSON *vs_entry;
	cJSON *ch_item;
	cJSON *child;
	uint32_t entry_idx;
	uint32_t ch_idx;

	node = cJSON_GetObjectItem(parent, node_name);
	if (node == NULL) {
		DBG_INFO("parse_optional_vs_site_defs: %s not present, skip\n", node_name);
		return SOC_OK;
	}
	if (!cJSON_IsObject(node)) {
		DBG_ERR("parse_optional_vs_site_defs: %s is not an object\n", node_name);
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_optional_vs_site_defs: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	*out_count = 0;
	for (site_entry = node->child; site_entry != NULL; site_entry = site_entry->next) {
		if (!cJSON_IsObject(site_entry)) {
			DBG_ERR("parse_optional_vs_site_defs: site entry in %s is not an object\n", node_name);
			return SOC_ERR_PARSE_FAILED;
		}
		for (vs_entry = site_entry->child; vs_entry != NULL; vs_entry = vs_entry->next) {
			if (!cJSON_IsObject(vs_entry)) {
				DBG_ERR("parse_optional_vs_site_defs: vs entry in %s is not an object\n", node_name);
				return SOC_ERR_PARSE_FAILED;
			}
			(*out_count)++;
		}
	}

	if (*out_count == 0) {
		return SOC_OK;
	}

	*out_defs = (vs_site_entry_t *)calloc(*out_count, sizeof(vs_site_entry_t));
	if (*out_defs == NULL) {
		DBG_ERR("parse_optional_vs_site_defs: calloc %s failed\n", node_name);
		return SOC_ERR_MEMORY_ALLOC;
	}

	entry_idx = 0;
	for (site_entry = node->child; site_entry != NULL; site_entry = site_entry->next) {
		if (!cJSON_IsObject(site_entry)) {
			DBG_ERR("parse_optional_vs_site_defs: site entry in %s is not an object\n", node_name);
			return SOC_ERR_PARSE_FAILED;
		}
		for (vs_entry = site_entry->child; vs_entry != NULL; vs_entry = vs_entry->next) {
			if (!cJSON_IsObject(vs_entry)) {
				DBG_ERR("parse_optional_vs_site_defs: vs entry in %s is not an object\n", node_name);
				return SOC_ERR_PARSE_FAILED;
			}
			strncpy((*out_defs)[entry_idx].site_name, site_entry->string, 15);
			(*out_defs)[entry_idx].site_name[15] = '\0';
			strncpy((*out_defs)[entry_idx].vs_name, vs_entry->string, 15);
			(*out_defs)[entry_idx].vs_name[15] = '\0';

			ch_item = cJSON_GetObjectItem(vs_entry, "ppsType");
			if (ch_item != NULL) {
				if (cJSON_IsString(ch_item)) {
					strncpy((*out_defs)[entry_idx].pps_type, ch_item->valuestring, 15);
					(*out_defs)[entry_idx].pps_type[15] = '\0';
				} else {
					DBG_ERR("parse_optional_vs_site_defs: ppsType in %s is not a string\n", node_name);
					return SOC_ERR_PARSE_FAILED;
				}
			}

			ch_item = cJSON_GetObjectItem(vs_entry, "channels");
			if (ch_item != NULL) {
				if (cJSON_IsArray(ch_item)) {
					(*out_defs)[entry_idx].channel_count = (uint32_t)cJSON_GetArraySize(ch_item);
					if ((*out_defs)[entry_idx].channel_count > 0) {
						(*out_defs)[entry_idx].channels = (uint32_t *)calloc(
						    (*out_defs)[entry_idx].channel_count, sizeof(uint32_t));
						if ((*out_defs)[entry_idx].channels == NULL) {
							DBG_ERR("parse_optional_vs_site_defs: calloc channels failed\n");
							return SOC_ERR_MEMORY_ALLOC;
						}
						ch_idx = 0;
						cJSON_ArrayForEach(child, ch_item) {
							if (cJSON_IsNumber(child) && child->valueint >= 0) {
								(*out_defs)[entry_idx].channels[ch_idx] = (uint32_t)child->valueint;
							} else {
								DBG_ERR("parse_optional_vs_site_defs: channel[%u] in %s is not a valid number\n",
								        ch_idx, node_name);
								return SOC_ERR_PARSE_FAILED;
							}
							ch_idx++;
						}
					}
				} else {
					DBG_ERR("parse_optional_vs_site_defs: channels in %s is not an array\n", node_name);
					return SOC_ERR_PARSE_FAILED;
				}
			}
			entry_idx++;
		}
	}

	return SOC_OK;
}

int parse_optional_vs_site_definitions(SocContext *ctx, cJSON *root)
{
	return parse_optional_vs_site_defs_internal(ctx, root, "vsSiteDefinitions",
	                                               &ctx->extra->vs_site_defs,
	                                               &ctx->extra->vs_site_def_count);
}

int parse_optional_hvpps_definitions(SocContext *ctx, cJSON *multiple)
{
	return parse_optional_vs_defs_internal(ctx, multiple, "hvppsDefinitions",
	                                         &ctx->extra->hvpps_defs,
	                                         &ctx->extra->hvpps_def_count);
}

int parse_optional_hvpps_site_definitions(SocContext *ctx, cJSON *multiple)
{
	return parse_optional_vs_site_defs_internal(ctx, multiple, "hvppsSiteDefinitions",
	                                               &ctx->extra->hvpps_site_defs,
	                                               &ctx->extra->hvpps_site_def_count);
}

int parse_optional_system_dut(SocContext *ctx, cJSON *multiple)
{
	cJSON *arr;
	cJSON *entry;
	cJSON *item;
	uint32_t idx;

	arr = cJSON_GetObjectItem(multiple, "systemDut");
	if (arr == NULL) {
		DBG_INFO("parse_optional_system_dut: systemDut not present, skip\n");
		return SOC_OK;
	}
	if (!cJSON_IsArray(arr)) {
		DBG_ERR("parse_optional_system_dut: systemDut is not an array\n");
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_optional_system_dut: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	ctx->extra->system_dut_count = (uint32_t)cJSON_GetArraySize(arr);
	if (ctx->extra->system_dut_count == 0) {
		DBG_INFO("parse_optional_system_dut: systemDut is empty array, skip\n");
		return SOC_OK;
	}

	ctx->extra->system_duts = (system_dut_t *)calloc(ctx->extra->system_dut_count,
	                                                   sizeof(system_dut_t));
	if (ctx->extra->system_duts == NULL) {
		DBG_ERR("parse_optional_system_dut: calloc system_duts failed\n");
		return SOC_ERR_MEMORY_ALLOC;
	}

	idx = 0;
	cJSON_ArrayForEach(entry, arr) {
		if (!cJSON_IsObject(entry)) {
			DBG_ERR("parse_optional_system_dut: entry in systemDut is not an object\n");
			return SOC_ERR_PARSE_FAILED;
		}
		item = cJSON_GetObjectItem(entry, "systemDutNum");
		if (item != NULL) {
			if (cJSON_IsNumber(item) && item->valueint >= 0) {
				ctx->extra->system_duts[idx].system_dut_num = (uint32_t)item->valueint;
			} else {
				DBG_ERR("parse_optional_system_dut: systemDutNum is not a valid number\n");
				return SOC_ERR_PARSE_FAILED;
			}
		}
		item = cJSON_GetObjectItem(entry, "siteNum");
		if (item != NULL) {
			if (cJSON_IsNumber(item) && item->valueint >= 0) {
				ctx->extra->system_duts[idx].site_num = (uint32_t)item->valueint;
			} else {
				DBG_ERR("parse_optional_system_dut: siteNum is not a valid number\n");
				return SOC_ERR_PARSE_FAILED;
			}
		}
		item = cJSON_GetObjectItem(entry, "dutNum");
		if (item != NULL) {
			if (cJSON_IsNumber(item) && item->valueint >= 0) {
				ctx->extra->system_duts[idx].dut_num = (uint32_t)item->valueint;
			} else {
				DBG_ERR("parse_optional_system_dut: dutNum is not a valid number\n");
				return SOC_ERR_PARSE_FAILED;
			}
		}
		if (idx >= ctx->extra->system_dut_count) break;
		idx++;
	}

	return SOC_OK;
}

int parse_multiple_scalars(SocContext *ctx, cJSON *multiple)
{
	cJSON *item;

	item = cJSON_GetObjectItem(multiple, "dutCount");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint <= 0) {
		DBG_ERR("parse_multiple_scalars: dutCount not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.dut_count = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "commonPinCount");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: commonPinCount not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.common_pin_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "powerPinCount");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: powerPinCount not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.power_pin_num = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "drPinCount");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: drPinCount not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.dr_pin_count = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "ioPinCount");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: ioPinCount not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.io_pin_count = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "STN");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: STN not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.stn = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "SITE");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: SITE not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.site = (uint32_t)item->valueint;

	item = cJSON_GetObjectItem(multiple, "CHAIN");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: CHAIN not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.chain = (uint32_t)item->valueint;
    

	item = cJSON_GetObjectItem(multiple, "pps");
	if (item == NULL || !cJSON_IsNumber(item) || item->valueint < 0) {
		DBG_ERR("parse_multiple_scalars: pps not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}
	ctx->meta.parallel_vs_map_num = (uint32_t)item->valueint;

	DBG_INFO("parse_multiple_scalars: dut=%u, common=%u, power=%u, dr=%u, io=%u, STN=%u, SITE=%u, CHAIN=%u, pps=%u\n",
		ctx->meta.dut_count, ctx->meta.common_pin_num, ctx->meta.power_pin_num,
		ctx->meta.dr_pin_count, ctx->meta.io_pin_count,
		ctx->meta.stn, ctx->meta.site, ctx->meta.chain, ctx->meta.parallel_vs_map_num);

	return SOC_OK;
}

int parse_parallel_vs_map(SocContext *ctx, cJSON *multiple)
{
	cJSON *vs_map_json;
	cJSON *vs_item;
	uint32_t vs_idx;
	char vs_key[16];

	vs_map_json = cJSON_GetObjectItem(multiple, "parallelVsMap");
	if (vs_map_json == NULL) {
		DBG_ERR("parse_parallel_vs_map: parallelVsMap not found\n");
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->meta.parallel_vs_map_num == 0) {
		DBG_INFO("parse_parallel_vs_map: parallel_vs_map_num is 0, skip\n");
		return SOC_OK;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_parallel_vs_map: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	ctx->extra->vs_map = (uint32_t *)calloc(ctx->meta.parallel_vs_map_num, sizeof(uint32_t));
	if (ctx->extra->vs_map == NULL) {
		DBG_ERR("parse_parallel_vs_map: calloc vs_map failed\n");
		return SOC_ERR_MEMORY_ALLOC;
	}

	for (vs_idx = 0; vs_idx < ctx->meta.parallel_vs_map_num; vs_idx++) {
		snprintf(vs_key, sizeof(vs_key), "VS%u", vs_idx + 1);
		vs_item = cJSON_GetObjectItem(vs_map_json, vs_key);
		if (vs_item == NULL || !cJSON_IsNumber(vs_item) || vs_item->valueint < 0) {
			DBG_ERR("parse_parallel_vs_map: %s not found or invalid\n", vs_key);
			return SOC_ERR_PARSE_FAILED;
		}
		ctx->extra->vs_map[vs_idx] = (uint32_t)vs_item->valueint;
	}

	return SOC_OK;
}

int parse_separate_entities(SocContext *ctx, cJSON *multiple)
{
	cJSON *entities;
	cJSON *entity;
	cJSON *type_item;
	cJSON *number_item;
	cJSON *mode_item;
	uint32_t idx;
	uint32_t count;

	entities = cJSON_GetObjectItem(multiple, "separateEntities");
	if (entities == NULL) {
		DBG_INFO("parse_separate_entities: separateEntities not present, skip\n");
		return SOC_OK;
	}
	if (!cJSON_IsArray(entities)) {
		DBG_ERR("parse_separate_entities: separateEntities is not an array\n");
		return SOC_ERR_PARSE_FAILED;
	}

	count = (uint32_t)cJSON_GetArraySize(entities);
	if (count == 0) {
		return SOC_OK;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_separate_entities: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	ctx->extra->entities = (separate_entity_t *)calloc(count, sizeof(separate_entity_t));
	if (ctx->extra->entities == NULL) {
		DBG_ERR("parse_separate_entities: calloc entities failed\n");
		return SOC_ERR_MEMORY_ALLOC;
	}
	ctx->extra->entity_count = count;

	idx = 0;
	cJSON_ArrayForEach(entity, entities) {
		type_item = cJSON_GetObjectItem(entity, "type");
		number_item = cJSON_GetObjectItem(entity, "number");
		mode_item = cJSON_GetObjectItem(entity, "mode");
		if (type_item == NULL || !cJSON_IsString(type_item) ||
			number_item == NULL || !cJSON_IsNumber(number_item) || number_item->valueint < 0 ||
			mode_item == NULL || !cJSON_IsNumber(mode_item) || mode_item->valueint < 0) {
			DBG_ERR("parse_separate_entities: entity[%u] invalid\n", idx);
			return SOC_ERR_PARSE_FAILED;
		}
		strncpy(ctx->extra->entities[idx].type, type_item->valuestring, 15);
		ctx->extra->entities[idx].type[15] = '\0';
		ctx->extra->entities[idx].number = (uint32_t)number_item->valueint;
		ctx->extra->entities[idx].mode = (uint32_t)mode_item->valueint;
		idx++;
	}

	return SOC_OK;
}

int parse_dut_sequence(SocContext *ctx, cJSON *multiple)
{
	cJSON *dut_seq;
	cJSON *dut_item;
	uint32_t idx;

	dut_seq = cJSON_GetObjectItem(multiple, "dutSequence");
	if (dut_seq == NULL || !cJSON_IsArray(dut_seq)) {
		DBG_ERR("parse_dut_sequence: dutSequence not found or invalid\n");
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->meta.dut_count == 0) {
		DBG_ERR("parse_dut_sequence: dut_count is 0, cannot allocate\n");
		return SOC_ERR_PARSE_FAILED;
	}

	if (ctx->extra == NULL) {
		DBG_ERR("parse_dut_sequence: ctx->extra is NULL\n");
		return SOC_ERR_INTERNAL;
	}

	ctx->extra->dut_seq_len = ctx->meta.dut_count;
	ctx->extra->dut_sequence = (uint32_t *)calloc(ctx->extra->dut_seq_len, sizeof(uint32_t));
	if (ctx->extra->dut_sequence == NULL) {
		DBG_ERR("parse_dut_sequence: calloc dut_sequence failed\n");
		return SOC_ERR_MEMORY_ALLOC;
	}

	idx = 0;
	cJSON_ArrayForEach(dut_item, dut_seq) {
		if (idx >= ctx->extra->dut_seq_len) break;
		if (!cJSON_IsNumber(dut_item) || dut_item->valueint <= 0) {
			DBG_ERR("parse_dut_sequence: dut_seq[%u] not a number\n", idx);
			return SOC_ERR_PARSE_FAILED;
		}
		ctx->extra->dut_sequence[idx] = (uint32_t)dut_item->valueint;
		idx++;
	}

	if (idx < (uint32_t)cJSON_GetArraySize(dut_seq)) {
		DBG_WARN("parse_dut_sequence: JSON array longer than dut_count"
		         " (%d > %u), tail truncated\n",
		         cJSON_GetArraySize(dut_seq), ctx->meta.dut_count);
	}

	return SOC_OK;
}
