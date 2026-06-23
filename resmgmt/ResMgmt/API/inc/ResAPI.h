#ifndef _RESAPIS_H
#define _RESAPIS_H

/*--- 项目头文件 ---*/
#include "general.h"
#include "dut_api_types.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件（无） ---*/

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
 * DUT 分组管理 API
 *===========================================================================*/

/** @brief DUT 分组管理子模块初始化入口 */
RES_API int32_t Res_DutGroupInit(void);

/** @brief 查询指定 DUT 分组中置位的 DUT 编号 */
RES_API int32_t Res_GetDutGroup(uint32_t site_no, uint32_t dut_group,
                                 uint32_t *dut_list, uint32_t msize);

/** @brief 批量增加 DUT 到指定分组 */
RES_API int32_t Res_AddDutToGroup(uint32_t site_no, uint32_t dut_group,
                                   const uint32_t *dut_nos, uint32_t count);

/** @brief 批量从指定分组删除 DUT */
RES_API int32_t Res_DeleteDutFrmGroup(uint32_t site_no, uint32_t dut_group,
                                       const uint32_t *dut_nos, uint32_t count);

/** @brief 清除整个 DUT 分组 */
RES_API int32_t Res_ClearDutGroup(uint32_t site_no, uint32_t dut_group);

/*===========================================================================
 * 会话信息查询 API
 *===========================================================================*/

/** @brief 获取所有 ON 的 function key 数组 */
RES_API int32_t Res_GetFuncKey(uint32_t *func_key, uint32_t msize);

/** @brief 获取测试程序名 */
RES_API int32_t Res_GetProgramName(char *program_name, uint32_t max_len);

/** @brief 获取测试项名称 */
RES_API int32_t Res_GetTestName(char *test_name, uint32_t max_len);

/** @brief 查询 site 编号数组 */
RES_API int32_t Res_QuerySiteNos(uint32_t *site_nos, uint32_t msize);

/** @brief 查询当前 site 编号 */
RES_API int32_t Res_QuerySiteNo(void);

/*===========================================================================
 * 模块初始化
 *===========================================================================*/

RES_API int32_t res_Init(int argc, char **argv);

#ifdef __cplusplus
    }
#endif

#endif
