#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "ResAPI.h"
#include "dealsoc_types.h"
#include "dut_api_types.h"
#include "HashTable.h"
#include "dealsoc.h"
#include "internal.h"
#include "log_mgmt.h"

/* 判断 _dut_pin_t 是否有效 (ch_name 非空则已填充) */
#define DUT_PIN_IS_VALID(p)  ((p) != NULL && (p)->ch_name[0] != '\0')

/*===========================================================================
 * 对外接口
 *===========================================================================*/

/**
 * @brief 设置/更换 socket 文件名, 触发重新编译解析
 * @param[in] path      新的 socket 文件路径
 * @param[in] path_len  路径长度 (不含 '\0')
 * @return SOC_OK 成功, 负数 soc_error_t
 */
RES_API int32_t Res_SetSocketFileName(const char *path, uint32_t path_len)
{
	SocContext *ctx = soc_context_get_instance();
	if (path == NULL || path_len == 0 || path_len > MAX_FILE_PATH_LEN) {
		DBG_ERR("Res_SetSocketFileName: invalid param\n");
		return SOC_ERR_PARAM;
	}

	pthread_mutex_lock(ctx->parse_mutex);

	/* 相同文件则不操作 */
	if (strncmp(path, ctx->meta.file_path, MAX_FILE_PATH_LEN) == 0) {
		pthread_mutex_unlock(ctx->parse_mutex);
		DBG_INFO("Res_SetSocketFileName: same file, no-op\n");
		return SOC_OK;
	}

	/* 存储新路径 */
	strncpy(ctx->meta.file_path, path, MAX_FILE_PATH_LEN - 1);
	ctx->meta.file_path[MAX_FILE_PATH_LEN - 1] = '\0';

	/* 通知线程中断当前工作, 线程清理完后自动触发新解析 */
	ctx->restart_needed = 1;
	ctx->parse_pending  = 1;
	pthread_cond_signal(ctx->parse_cond);
	pthread_mutex_unlock(ctx->parse_mutex);

	/* 异步返回: 线程将在后台完成中断→清理→重新解析 */
	return SOC_OK;
}

/**
 * @brief 获取当前使用的 socket 文件名
 * @param[out] socket_file_name  输出缓冲区
 * @param[in]  mlen              缓冲区大小
 * @return 文件名字符串长度 (>=0), 负数 soc_error_t
 */
RES_API int32_t Res_GetSocketFileName(char *socket_file_name, uint32_t mlen)
{
	SocContext *ctx = soc_context_get_instance();
	size_t len;

	if (socket_file_name == NULL || mlen == 0) {
		return SOC_ERR_PARAM;
	}

	pthread_mutex_lock(ctx->parse_mutex);
	strncpy(socket_file_name, ctx->meta.file_path, mlen - 1);
	socket_file_name[mlen - 1] = '\0';
	len = strlen(ctx->meta.file_path);
	pthread_mutex_unlock(ctx->parse_mutex);

	return (int32_t)len;
}

/**
 * @brief 查询 pin 在 socket 中是否被定义
 * @param[in] dut_no    DUT 编号
 * @param[in] dut_pin   Pin 编号 (common_pin_num 或 vs_pin_num)
 * @param[in] pin_type  0=common pin, 1=power pin
 * @return 1=已定义, 0=未定义, 负数 soc_error_t
 */
RES_API int32_t Res_IsPinDefinedInSocket(uint32_t dut_no, uint32_t dut_pin, uint32_t pin_type)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t i;

	if (dut_no >= ctx->meta.dut_count) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_socket_parse(ctx); if (_r != SOC_OK) return _r; }

	if (pin_type == 0) {
		/* common pin */
		if (ctx->dut_common_pins == NULL) {
			return 0;
		}
		for (i = 0; i < ctx->meta.common_pin_num; i++) {
			if (ctx->dut_common_pins[i].common_pin_num == dut_pin) {
				if (ctx->dut_common_pins[i].pCommonDutPin != NULL
					&& DUT_PIN_IS_VALID(&ctx->dut_common_pins[i].pCommonDutPin[dut_no])) {
					return 1;
				}
				return 0;
			}
		}
	} else {
		/* power pin */
		if (ctx->dut_power_pins == NULL) {
			return 0;
		}
		for (i = 0; i < ctx->meta.power_pin_num; i++) {
			if (ctx->dut_power_pins[i].vs_pin_num == dut_pin) {
				if (ctx->dut_power_pins[i].pPowerDutPin != NULL
					&& DUT_PIN_IS_VALID(&ctx->dut_power_pins[i].pPowerDutPin[dut_no])) {
					return 1;
				}
				return 0;
			}
		}
	}

	return 0;
}

/**
 * @brief 获取指定 DUT 所有 IO pin 编号列表
 * @param[in]  dut_no      DUT 编号
 * @param[out] io_pin_num  输出 pin 编号数组 (调用方分配)
 * @param[in]  msize       io_pin_num 数组容量
 * @return 实际填充的 pin 数量, 负数 soc_error_t
 */
RES_API int32_t Res_GetDutAllIoPin(uint32_t dut_no, uint32_t *io_pin_num, uint32_t msize)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t i;
	uint32_t count = 0;

	if (dut_no >= ctx->meta.dut_count) {
		return SOC_ERR_PARAM;
	}

	if (io_pin_num == NULL || msize == 0) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_socket_parse(ctx); if (_r != SOC_OK) return _r; }

	if (ctx->dut_common_pins == NULL) {
		return 0;
	}

	for (i = 0; i < ctx->meta.common_pin_num; i++) {
		if (ctx->dut_common_pins[i].pin_type == 1
			&& ctx->dut_common_pins[i].pCommonDutPin != NULL
			&& DUT_PIN_IS_VALID(&ctx->dut_common_pins[i].pCommonDutPin[dut_no])) {
			io_pin_num[count] = ctx->dut_common_pins[i].common_pin_num;
			count++;
			if (count >= msize) {
				break;
			}
		}
	}

	return (int32_t)count;
}

/**
 * @brief 根据 DUT 参数查询 common pin 的 whole_no 序号
 * @param[in]  dut_param   输入: DUT 编号 + Pin 编号
 * @param[out] logic_data  输出: 完整的 IO 逻辑通道参数数组
 * @param[in]  msize       logic_data 数组容量
 * @return 实际填充数量, 负数 soc_error_t
 */
RES_API int32_t Res_QueryIoPinSeqByDutParam(const dut_param_t *dut_param,
	io_logic_data_t *logic_data, uint32_t msize)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t i;
	uint32_t count = 0;

	if (dut_param == NULL || logic_data == NULL || msize == 0) {
		return SOC_ERR_PARAM;
	}

	if (dut_param->dut_no >= ctx->meta.dut_count) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	if (ctx->dut_common_pins == NULL) {
		return 0;
	}

	for (i = 0; i < ctx->meta.common_pin_num; i++) {
		if (ctx->dut_common_pins[i].common_pin_num == dut_param->dut_pin) {
			_dut_pin_t *p = &ctx->dut_common_pins[i].pCommonDutPin[dut_param->dut_no];

			if (DUT_PIN_IS_VALID(p) && p->whole_no > 0) {
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

	return (int32_t)count;
}

/**
 * @brief 根据 DUT 参数查询 power pin 的 whole_no 序号
 * @param[in]  dut_param   输入: DUT 编号 + Pin 编号
 * @param[out] logic_data  输出: 完整的 VS 逻辑通道参数数组
 * @param[in]  msize       logic_data 数组容量
 * @return 实际填充数量, 负数 soc_error_t
 */
RES_API int32_t Res_QueryVsPinSeqByDutParam(const dut_param_t *dut_param,
	vs_logic_data_t *logic_data, uint32_t msize)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t i;
	uint32_t count = 0;

	if (dut_param == NULL || logic_data == NULL || msize == 0) {
		return SOC_ERR_PARAM;
	}

	if (dut_param->dut_no >= ctx->meta.dut_count) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	if (ctx->dut_power_pins == NULL) {
		return 0;
	}

	for (i = 0; i < ctx->meta.power_pin_num; i++) {
		if (ctx->dut_power_pins[i].vs_pin_num == dut_param->dut_pin) {
			_dut_pin_t *p = &ctx->dut_power_pins[i].pPowerDutPin[dut_param->dut_no];

			if (DUT_PIN_IS_VALID(p) && p->whole_no > 0) {
				logic_data[count].dut_data.dut_no  = p->dut_no;
				logic_data[count].dut_data.dut_pin = ctx->dut_power_pins[i].vs_pin_num;
				logic_data[count].pin_type          = 0;
				logic_data[count].pin_idx           = p->whole_no;
				count++;
				if (count >= msize) {
					break;
				}
			}
		}
	}

	return (int32_t)count;
}

/**
 * @brief 通过 SEM/ASIC/Channel 反查 common pin 的 DUT 参数和 whole_no
 * @param[in]  io_pin     输入: SEM/ASIC/Channel 编号 (1-indexed)
 * @param[out] dut_param  输出: 查到的 DUT 编号 + Pin 编号
 * @return >0: whole_no, SOC_ERR_QUERY_CHANNEL_NOT_FOUND: 未找到
 */
RES_API int32_t Res_GetDutParamAndPinSeqFrmSemChannel(const io_pin_t *io_pin,
	dut_param_t *dut_param)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t ch_total;
	uint32_t calced_whole_no;
	uint32_t i;
	uint32_t dut;

	if (io_pin == NULL || dut_param == NULL) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	ch_total = ctx->logic_config.dr_num + ctx->logic_config.io_num;

	calced_whole_no = COMMON_SERIAL(io_pin->sem_no, io_pin->asic_no,
		io_pin->channel_no, ctx->logic_config.asic_num, ch_total);

	if (ctx->dut_common_pins == NULL) {
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	for (i = 0; i < ctx->meta.common_pin_num; i++) {
		if (ctx->dut_common_pins[i].pCommonDutPin == NULL) {
			continue;
		}
		for (dut = 0; dut < ctx->meta.dut_count; dut++) {
			_dut_pin_t *p = &ctx->dut_common_pins[i].pCommonDutPin[dut];

			if (DUT_PIN_IS_VALID(p) && p->whole_no == calced_whole_no) {
				dut_param->dut_no  = p->dut_no;
				dut_param->dut_pin = ctx->dut_common_pins[i].common_pin_num;
				return (int32_t)calced_whole_no;
			}
		}
	}

	return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
}

/**
 * @brief 通过 PSM/Channel 反查 power pin 的 DUT 参数和 whole_no
 * @param[in]  vs_ch      输入: PSM/Channel 编号 (1-indexed)
 * @param[out] dut_param  输出: 查到的 DUT 编号 + Pin 编号
 * @return >0: whole_no, SOC_ERR_QUERY_CHANNEL_NOT_FOUND: 未找到
 */
RES_API int32_t Res_QueryDutParamByPsmChannel(const vs_channel_t *vs_ch,
	dut_param_t *dut_param)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t calced_whole_no;
	uint32_t i;
	uint32_t dut;

	if (vs_ch == NULL || dut_param == NULL) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	calced_whole_no = POWER_SERIAL(vs_ch->psm_no, vs_ch->channel_no,
		ctx->logic_config.pps_num);

	if (ctx->dut_power_pins == NULL) {
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	for (i = 0; i < ctx->meta.power_pin_num; i++) {
		if (ctx->dut_power_pins[i].pPowerDutPin == NULL) {
			continue;
		}
		for (dut = 0; dut < ctx->meta.dut_count; dut++) {
			_dut_pin_t *p = &ctx->dut_power_pins[i].pPowerDutPin[dut];

			if (DUT_PIN_IS_VALID(p) && p->whole_no == calced_whole_no) {
				dut_param->dut_no  = p->dut_no;
				dut_param->dut_pin = ctx->dut_power_pins[i].vs_pin_num;
				return (int32_t)calced_whole_no;
			}
		}
	}

	return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
}
/*===========================================================================
 * 驱动接口 — 通道号反解 (纯数学运算, 不依赖哈希表)
 *===========================================================================*/

/**
 * @brief 根据 whole_no 反解出 sem/asic/channel 编号
 * @param[in]  pin_idx   whole_no (1-indexed, 来自 Res_QueryIoPinSeqByDutParam)
 * @param[out] pin_info  输出的 sem_no / asic_no / channel_no (1-indexed)
 * @return SOC_OK=成功, 负数 soc_error_t
 */
RES_API int32_t Res_GetIoPinFrmPinSeq(uint32_t pin_idx, io_pin_t *pin_info)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t ch_total;

	if (pin_info == NULL) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	ch_total = ctx->logic_config.dr_num + ctx->logic_config.io_num;

	if (pin_idx == 0
		|| pin_idx > ctx->logic_config.sem_num * ctx->logic_config.asic_num * ch_total) {
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	pin_info->sem_no     = SERIAL_TO_SEM(pin_idx, ctx->logic_config.asic_num, ch_total);
	pin_info->asic_no    = SERIAL_TO_ASIC(pin_idx, ctx->logic_config.asic_num, ch_total);
	pin_info->channel_no = SERIAL_TO_CH(pin_idx, ch_total);

	return SOC_OK;
}

/**
 * @brief 根据 whole_no 反解出 psm/channel 编号
 * @param[in]  pin_idx     whole_no (1-indexed, 来自 Res_QueryVsPinSeqByDutParam)
 * @param[out] vs_channel  输出的 psm_no / channel_no (1-indexed)
 * @return SOC_OK=成功, 负数 soc_error_t
 */
RES_API int32_t Res_GetVsChannelFrmPinSeq(uint32_t pin_idx, vs_channel_t *vs_channel)
{
	SocContext *ctx = soc_context_get_instance();
	if (vs_channel == NULL) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	if (pin_idx == 0
		|| pin_idx > ctx->logic_config.psm_num * ctx->logic_config.pps_num) {
		return SOC_ERR_QUERY_CHANNEL_NOT_FOUND;
	}

	vs_channel->psm_no     = SERIAL_TO_PSM(pin_idx, ctx->logic_config.pps_num);
	vs_channel->channel_no = SERIAL_TO_PWR_CH(pin_idx, ctx->logic_config.pps_num);

	return SOC_OK;
}

/**
 * @brief 获取所有 SEM×ASIC 组合列表
 * @param[out] logic_data  sem_asic_t 输出数组 (调用方分配)
 * @param[in]  size       数组容量
 * @return 实际填充的数量, 负数 soc_error_t
 */
RES_API int32_t Res_GetAllSemAndAsic(sem_asic_t *logic_data, uint32_t size)
{
	SocContext *ctx = soc_context_get_instance();
	uint32_t sem_idx;
	uint32_t asic_idx;
	uint32_t count = 0;

	if (logic_data == NULL || size == 0) {
		return SOC_ERR_PARAM;
	}

	{ int _r = soc_wait_both_parse(ctx); if (_r != SOC_OK) return _r; }

	for (sem_idx = 1; sem_idx <= ctx->logic_config.sem_num; sem_idx++) {
		for (asic_idx = 1; asic_idx <= ctx->logic_config.asic_num; asic_idx++) {
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

