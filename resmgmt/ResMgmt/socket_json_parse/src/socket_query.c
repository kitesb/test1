/*--- 模块头文件 ---*/
#include "socket_query.h"

/*--- 项目头文件 ---*/
#include "socket_internal.h"   /* struct soc_context_impl 完整定义 */
#include "log_mgmt.h"

/*--- 系统头文件 ---*/
#include <stdio.h>
#include <string.h>

/* 判断 _dut_pin_t 是否有效 (ch_name 非空则已填充) */
#define DUT_PIN_IS_VALID(p)  ((p) != NULL && (p)->ch_name[0] != 0)

int32_t socket_set_file_name(SocContext *ctx, const char *path, uint32_t path_len)
{

	if (path == NULL || path_len == 0 || path_len >= MAX_FILE_PATH_LEN) {
		DBG_ERR("Res_SetSocketFileName: invalid param\n");
		return SOC_ERR_PARAM;
	}

	pthread_mutex_lock(ctx->parse_mutex);

	/* 相同文件则不操作 */
	size_t _fp_len = strnlen(ctx->meta.file_path, MAX_FILE_PATH_LEN);
	if (path_len == _fp_len && memcmp(path, ctx->meta.file_path, path_len) == 0) {
		pthread_mutex_unlock(ctx->parse_mutex);
		DBG_INFO("Res_SetSocketFileName: same file, no-op\n");
		return SOC_OK;
	}

	/* 存储新路径 */
	size_t _cp_len = (path_len < MAX_FILE_PATH_LEN - 1) ? path_len : (MAX_FILE_PATH_LEN - 1);
	memcpy(ctx->meta.file_path, path, _cp_len);
	ctx->meta.file_path[_cp_len] = '\0';

	/* 通知线程中断当前工作, 线程清理完后自动触发新解析 */
	ctx->restart_needed = 1;
	ctx->parse_pending  = 1;
	pthread_cond_signal(ctx->parse_cond);
	pthread_mutex_unlock(ctx->parse_mutex);

	/* 异步返回: 线程将在后台完成中断→清理→重新解析 */
	return SOC_OK;
}

int32_t socket_get_file_name(SocContext *ctx, char *socket_file_name, uint32_t mlen)
{

	size_t len;

	if (socket_file_name == NULL || mlen == 0) {
		return SOC_ERR_PARAM;
	}

	pthread_mutex_lock(ctx->parse_mutex);
	strncpy(socket_file_name, ctx->meta.file_path, mlen - 1);
	socket_file_name[mlen - 1] = '\0';
	len = strlen(socket_file_name);
	pthread_mutex_unlock(ctx->parse_mutex);
	return (int32_t)len;
}

int32_t socket_query_is_pin_defined(SocContext *ctx, uint32_t dut_no, uint32_t dut_pin, uint32_t pin_type)
{

	uint32_t i;

	int wret = soc_wait_socket_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}
	/* soc_wait_socket_parse 成功后持有 parse_mutex, 读取后各路径负责 unlock */

	/* 校验 dut_no 是否是 socket.json dutSequence 中定义的逻辑编号 */
	if (ctx->extra == NULL || ctx->extra->dut_sequence == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARSE_FAILED;
	}
	{
		int _dut_found = 0;
		for (i = 0; i < ctx->extra->dut_seq_len; i++) {
			if (ctx->extra->dut_sequence[i] == dut_no) {
				_dut_found = 1;
				break;
			}
		}
		if (!_dut_found) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARAM;
		}
	}

	/* 校验 pin_type: 仅允许 0 (common) 或 1 (power) */
	if (pin_type > 1) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARAM;
	}

	if (pin_type == 0) {
		/* common pin */
		if (ctx->dut_common_pins == NULL) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
		for (i = 0; i < ctx->meta.common_pin_num; i++) {
			if (ctx->dut_common_pins[i].common_pin_num == dut_pin) {
				if (ctx->dut_common_pins[i].pCommonDutPin != NULL) {
					/* dut_no 是逻辑标号非数组索引, 遍历内数组匹配 */
					uint32_t dut;
					for (dut = 0; dut < ctx->meta.dut_count; dut++) {
						_dut_pin_t *candidate = &ctx->dut_common_pins[i].pCommonDutPin[dut];
						if (DUT_PIN_IS_VALID(candidate) && candidate->dut_no == dut_no) {
							pthread_mutex_unlock(ctx->parse_mutex);
							return 1;
						}
					}
				}
				pthread_mutex_unlock(ctx->parse_mutex);
				return 0;
			}
		}
	} else {
		/* power pin */
		if (ctx->dut_power_pins == NULL) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
		for (i = 0; i < ctx->meta.power_pin_num; i++) {
			if (ctx->dut_power_pins[i].vs_pin_num == dut_pin) {
				if (ctx->dut_power_pins[i].pPowerDutPin != NULL) {
					/* dut_no 是逻辑标号非数组索引, 遍历内数组匹配 */
					uint32_t dut;
					for (dut = 0; dut < ctx->meta.dut_count; dut++) {
						_dut_pin_t *candidate = &ctx->dut_power_pins[i].pPowerDutPin[dut];
						if (DUT_PIN_IS_VALID(candidate) && candidate->dut_no == dut_no) {
							pthread_mutex_unlock(ctx->parse_mutex);
							return 1;
						}
					}
				}
				pthread_mutex_unlock(ctx->parse_mutex);
				return 0;
			}
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return 0;
}

int32_t socket_query_get_dut_io_pins(SocContext *ctx, uint32_t dut_no, uint32_t *io_pin_num, uint32_t msize)
{

	uint32_t i;
	uint32_t count = 0;

	if (io_pin_num == NULL || msize == 0) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_socket_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}

	if (ctx->dut_common_pins == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARSE_FAILED;
	}

	/* 校验 dut_no 是否是 socket.json dutSequence 中定义的逻辑编号 */
	if (ctx->extra == NULL || ctx->extra->dut_sequence == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARSE_FAILED;
	}
	{
		int _dut_found = 0;
		for (i = 0; i < ctx->extra->dut_seq_len; i++) {
			if (ctx->extra->dut_sequence[i] == dut_no) {
				_dut_found = 1;
				break;
			}
		}
		if (!_dut_found) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARAM;
		}
	}

	for (i = 0; i < ctx->meta.common_pin_num; i++) {
		if (ctx->dut_common_pins[i].pin_type == 1
			&& ctx->dut_common_pins[i].pCommonDutPin != NULL) {
			/* dut_no 是逻辑标号非数组索引, 遍历内数组匹配 */
			uint32_t dut;
			for (dut = 0; dut < ctx->meta.dut_count; dut++) {
				_dut_pin_t *candidate = &ctx->dut_common_pins[i].pCommonDutPin[dut];
				if (DUT_PIN_IS_VALID(candidate) && candidate->dut_no == dut_no) {
					io_pin_num[count] = ctx->dut_common_pins[i].common_pin_num;
					count++;
					break;
				}
			}
			if (count >= msize) {
				break;
			}
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return (int32_t)count;
}

int32_t socket_query_io_pin_seq_by_dut_param(SocContext *ctx, const dut_param_t *dut_param, io_logic_data_t *logic_data, uint32_t msize)
{

	uint32_t i;
	uint32_t count = 0;

	if (dut_param == NULL || logic_data == NULL || msize == 0) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}

	if (ctx->dut_common_pins == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARSE_FAILED ;
	}

	for (i = 0; i < ctx->meta.common_pin_num; i++) {
		if (ctx->dut_common_pins[i].common_pin_num == dut_param->dut_pin) {
			uint32_t dut;
			_dut_pin_t *p = NULL;

			/* dut_no 是逻辑标识符而非数组索引，遍历内数组匹配 */
			if (ctx->dut_common_pins[i].pCommonDutPin == NULL) {
				pthread_mutex_unlock(ctx->parse_mutex);
				return SOC_ERR_PARSE_FAILED;
			}
			for (dut = 0; dut < ctx->meta.dut_count; dut++) {
				_dut_pin_t *candidate = &ctx->dut_common_pins[i].pCommonDutPin[dut];
				if (DUT_PIN_IS_VALID(candidate)
				    && candidate->dut_no == dut_param->dut_no) {
					p = candidate;
					break;
				}
			}

			if (p != NULL && p->whole_no > 0) {
				logic_data[count].dut_data.dut_no  = p->dut_no;
				logic_data[count].dut_data.dut_pin = ctx->dut_common_pins[i].common_pin_num;
				logic_data[count].pin_type          = ctx->dut_common_pins[i].pin_type;
				logic_data[count].pin_idx           = p->whole_no;
				count++;
				if (count >= msize) {
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return (int32_t)count;
}

int32_t socket_query_vs_pin_seq_by_dut_param(SocContext *ctx, const dut_param_t *dut_param, vs_logic_data_t *logic_data, uint32_t msize)
{

	uint32_t i;
	uint32_t count = 0;

	if (dut_param == NULL || logic_data == NULL || msize == 0) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}

	if (ctx->dut_power_pins == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARSE_FAILED ;
	}

	for (i = 0; i < ctx->meta.power_pin_num; i++) {
		if (ctx->dut_power_pins[i].vs_pin_num == dut_param->dut_pin) {
			uint32_t dut;
			_dut_pin_t *p = NULL;

			/* dut_no 是逻辑标识符而非数组索引，遍历内数组匹配 */
			if (ctx->dut_power_pins[i].pPowerDutPin == NULL) {
				pthread_mutex_unlock(ctx->parse_mutex);
				return SOC_ERR_PARSE_FAILED;
			}
			for (dut = 0; dut < ctx->meta.dut_count; dut++) {
				_dut_pin_t *candidate = &ctx->dut_power_pins[i].pPowerDutPin[dut];
				if (DUT_PIN_IS_VALID(candidate)
				    && candidate->dut_no == dut_param->dut_no) {
					p = candidate;
					break;
				}
			}

			if (p != NULL && p->whole_no > 0) {
				logic_data[count].dut_data.dut_no  = p->dut_no;
				logic_data[count].dut_data.dut_pin = ctx->dut_power_pins[i].vs_pin_num;
				logic_data[count].pin_idx           = p->whole_no;
				count++;
				if (count >= msize) {
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return (int32_t)count;
}

int32_t socket_query_dut_param_from_sem_ch(SocContext *ctx, const io_pin_t *io_pin, dut_param_t *dut_param)
{
	uint32_t ch_total;
	uint32_t calced_whole_no;
	uint32_t i;
	uint32_t dut;


	if (io_pin == NULL || dut_param == NULL) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}

	/* 防御: 所有编号为 1-indexed, 0 会导致 COMMON_SERIAL 宏回绕 */
	if (io_pin->sem_no == 0 || io_pin->asic_no == 0 || io_pin->channel_no == 0) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARAM;
	}

	ch_total = ctx->logic_config.dr_num + ctx->logic_config.io_num;

	calced_whole_no = COMMON_SERIAL(io_pin->sem_no, io_pin->asic_no,
			io_pin->channel_no, ctx->logic_config.asic_num, ch_total);

	if (ctx->dut_common_pins == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	for (i = 0; i < ctx->meta.common_pin_num; i++) {
		if (ctx->dut_common_pins[i].pCommonDutPin == NULL) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
		for (dut = 0; dut < ctx->meta.dut_count; dut++) {
			_dut_pin_t *p = &ctx->dut_common_pins[i].pCommonDutPin[dut];

			if (DUT_PIN_IS_VALID(p) && p->whole_no == calced_whole_no) {
				dut_param->dut_no  = p->dut_no;
				dut_param->dut_pin = ctx->dut_common_pins[i].common_pin_num;
				pthread_mutex_unlock(ctx->parse_mutex);
				return (int32_t)calced_whole_no;
			}
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
}

int32_t socket_query_dut_param_from_psm_ch(SocContext *ctx, const vs_channel_t *vs_ch, dut_param_t *dut_param)
{

	uint32_t calced_whole_no;
	uint32_t i;
	uint32_t dut;

	if (vs_ch == NULL || dut_param == NULL) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}

	/* 防御: 所有编号为 1-indexed, 0 会导致 POWER_SERIAL 宏回绕 */
	if (vs_ch->psm_no == 0 || vs_ch->channel_no == 0) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_PARAM;
	}

	calced_whole_no = POWER_SERIAL(vs_ch->psm_no, vs_ch->channel_no,
			ctx->logic_config.pps_num);

	if (ctx->dut_power_pins == NULL) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	for (i = 0; i < ctx->meta.power_pin_num; i++) {
		if (ctx->dut_power_pins[i].pPowerDutPin == NULL) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
		for (dut = 0; dut < ctx->meta.dut_count; dut++) {
			_dut_pin_t *p = &ctx->dut_power_pins[i].pPowerDutPin[dut];

			if (DUT_PIN_IS_VALID(p) && p->whole_no == calced_whole_no) {
				dut_param->dut_no  = p->dut_no;
				dut_param->dut_pin = ctx->dut_power_pins[i].vs_pin_num;
				pthread_mutex_unlock(ctx->parse_mutex);
				return (int32_t)calced_whole_no;
			}
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
}

int32_t socket_query_get_io_pin_from_seq(SocContext *ctx, uint32_t pin_idx, io_pin_t *pin_info)
{

	uint32_t ch_total;
	uint32_t actul_pin_idx;
	uint32_t asic_num;

	if (pin_info == NULL) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}

	ch_total      = ctx->logic_config.dr_num + ctx->logic_config.io_num;
	asic_num      = ctx->logic_config.asic_num;
	actul_pin_idx = ctx->logic_config.sem_num * asic_num * ch_total;
	pthread_mutex_unlock(ctx->parse_mutex);

	if (pin_idx == 0
		|| pin_idx > actul_pin_idx) {
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	pin_info->sem_no     = SERIAL_TO_SEM(pin_idx, asic_num, ch_total);
	pin_info->asic_no    = SERIAL_TO_ASIC(pin_idx, asic_num, ch_total);
	pin_info->channel_no = SERIAL_TO_CH(pin_idx, ch_total);

	return SOC_OK;
}

int32_t socket_query_get_vs_ch_from_seq(SocContext *ctx, uint32_t pin_idx, vs_channel_t *vs_channel)
{
	uint32_t actul_pin_idx;
	uint32_t pps_num;

	if (vs_channel == NULL) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}
	pps_num       = ctx->logic_config.pps_num;
	actul_pin_idx = ctx->logic_config.psm_num * pps_num;
	pthread_mutex_unlock(ctx->parse_mutex);

	if (pin_idx == 0
		|| pin_idx > actul_pin_idx) {
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	vs_channel->psm_no     = SERIAL_TO_PSM(pin_idx, pps_num);
	vs_channel->channel_no = SERIAL_TO_PWR_CH(pin_idx, pps_num);

	return SOC_OK;
}

int32_t socket_query_get_all_sem_asic(SocContext *ctx, sem_asic_t *logic_data, uint32_t size)
{
	uint32_t sem_idx;
	uint32_t asic_idx;
	uint32_t count = 0;

	if (logic_data == NULL || size == 0) {
		return SOC_ERR_PARAM;
	}

	int wret = soc_wait_both_parse(ctx);
	if (wret != SOC_OK) {
		return wret;
	}
	uint32_t sem_num = ctx->logic_config.sem_num;
	uint32_t asic_num = ctx->logic_config.asic_num;
	pthread_mutex_unlock(ctx->parse_mutex);

	for (sem_idx = 1; sem_idx <= sem_num; sem_idx++) {
		for (asic_idx = 1; asic_idx <= asic_num; asic_idx++) {
			if (count >= size) {
				return (int32_t)count;
			}
			logic_data[count].sem_no  = sem_idx;
			logic_data[count].asic_no = asic_idx;
			count++;
		}
	}

	return (int32_t)count;
}

