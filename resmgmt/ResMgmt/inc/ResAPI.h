#ifndef _RESAPIS_H
#define _RESAPIS_H

#include "general.h"
#include "dut_api_types.h"

/******************************************************************************
* @file     ResAPI.h
* @brief    对外开放头文件: 提供给外部模块调用的接口
* @details  1. 外部接口用到的宏和结构体
*           2. 外部接口使用的API接口函数
* @author
* @version  V1.0.0
* @date     YYYY-MM-DD
* @copyright (C) 2026-2036, Tytantest. All rights reserved.
*****************************************************************************/

#ifdef __cplusplus
    extern "C" {
#endif

/*===========================================================================
 * 查询接口 API (RES_API 导出)
 *===========================================================================*/

/** @brief 设置/更换 socket 文件名, 触发重新编译解析 */
RES_API int32_t Res_SetSocketFileName(const char *path, uint32_t path_len);

/** @brief 获取当前使用的 socket 文件名 */
RES_API int32_t Res_GetSocketFileName(char *socket_file_name, uint32_t mlen);

/** @brief 查询 pin 在 socket 中是否被定义 */
RES_API int32_t Res_IsPinDefinedInSocket(uint32_t dut_no, uint32_t dut_pin, uint32_t pin_type);

/** @brief 获取指定 DUT 所有 IO pin 编号列表 */
RES_API int32_t Res_GetDutAllIoPin(uint32_t dut_no, uint32_t *io_pin_num, uint32_t msize);

/** @brief 根据 DUT 参数查询 common pin 的 whole_no 序号 */
RES_API int32_t Res_QueryIoPinSeqByDutParam(const dut_param_t *dut_param,
	io_logic_data_t *logic_data, uint32_t msize);

/** @brief 根据 DUT 参数查询 power pin 的 whole_no 序号 */
RES_API int32_t Res_QueryVsPinSeqByDutParam(const dut_param_t *dut_param,
	vs_logic_data_t *logic_data, uint32_t msize);

/** @brief 通过 SEM/ASIC/Channel 反查 common pin 的 DUT 参数和 whole_no */
RES_API int32_t Res_GetDutParamAndPinSeqFrmSemChannel(const io_pin_t *io_pin,
	dut_param_t *dut_param);

/** @brief 通过 PSM/Channel 反查 power pin 的 DUT 参数和 whole_no */
RES_API int32_t Res_QueryDutParamByPsmChannel(const vs_channel_t *vs_ch,
	dut_param_t *dut_param);

/*===========================================================================
 * 驱动接口 — 通道号反解（纯数学运算）
 *===========================================================================*/

/** @brief 根据 whole_no 反解 IO 通道号 (sem/asic/channel) */
RES_API int32_t Res_GetIoPinFrmPinSeq(uint32_t pin_idx, io_pin_t *pin_info);

/** @brief 根据 whole_no 反解电源通道号 (psm/channel) */
RES_API int32_t Res_GetVsChannelFrmPinSeq(uint32_t pin_idx, vs_channel_t *vs_channel);

/** @brief 获取所有 SEM×ASIC 组合列表 */
RES_API int32_t Res_GetAllSemAndAsic(sem_asic_t *logic_data, uint32_t size);


/*===========================================================================
 * 模块初始化
 *===========================================================================*/

int32_t res_Init(uint32_t argc, char *argv);

#if 1
void tst_send(char *pData);
void tst_recv();
#endif

#ifdef __cplusplus
    }
#endif

#endif
