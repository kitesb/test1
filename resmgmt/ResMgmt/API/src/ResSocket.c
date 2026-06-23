/*--- 模块头文件（无） ---*/

/*--- 项目头文件 ---*/
#include "ResAPI.h"
#include "dut_api_types.h"
#include "socket_query.h"
#include "soc_context.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件（无） ---*/

/*===========================================================================
 * 对外接口 — 纯薄 Facade
 * 全部 10 个 RES_API 函数为纯委托: ctx NULL check → delegate → return
 * 零 ctx 字段访问, 零业务逻辑, 零 mutex 操作
 *===========================================================================*/

RES_API int32_t Res_SetSocketFileName(const char *path, uint32_t path_len)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    return socket_set_file_name(ctx, path, path_len);
}

RES_API int32_t Res_GetSocketFileName(char *socket_file_name, uint32_t mlen)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    return socket_get_file_name(ctx, socket_file_name, mlen);
}

RES_API int32_t Res_IsPinDefinedInSocket(uint32_t dut_no, uint32_t dut_pin, uint32_t pin_type)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    return socket_query_is_pin_defined(ctx, dut_no, dut_pin, pin_type);
}

RES_API int32_t Res_GetDutAllIoPin(uint32_t dut_no, uint32_t *io_pin_num, uint32_t msize)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (io_pin_num == NULL || msize == 0) return SOC_ERR_PARAM;
    return socket_query_get_dut_io_pins(ctx, dut_no, io_pin_num, msize);
}

RES_API int32_t Res_QueryIoPinSeqByDutParam(const dut_param_t *dut_param,
    io_logic_data_t *logic_data, uint32_t msize)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (dut_param == NULL || logic_data == NULL || msize == 0)
        return SOC_ERR_PARAM;
    return socket_query_io_pin_seq_by_dut_param(ctx, dut_param, logic_data, msize);
}

RES_API int32_t Res_QueryVsPinSeqByDutParam(const dut_param_t *dut_param,
    vs_logic_data_t *logic_data, uint32_t msize)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (dut_param == NULL || logic_data == NULL || msize == 0)
        return SOC_ERR_PARAM;
    return socket_query_vs_pin_seq_by_dut_param(ctx, dut_param, logic_data, msize);
}

RES_API int32_t Res_GetDutParamAndPinSeqFrmSemChannel(const io_pin_t *io_pin,
    dut_param_t *dut_param)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (io_pin == NULL || dut_param == NULL) return SOC_ERR_PARAM;
    return socket_query_dut_param_from_sem_ch(ctx, io_pin, dut_param);
}

RES_API int32_t Res_QueryDutParamByPsmChannel(const vs_channel_t *vs_ch,
    dut_param_t *dut_param)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (vs_ch == NULL || dut_param == NULL) return SOC_ERR_PARAM;
    return socket_query_dut_param_from_psm_ch(ctx, vs_ch, dut_param);
}

RES_API int32_t Res_GetIoPinFrmPinSeq(uint32_t pin_idx, io_pin_t *pin_info)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (pin_info == NULL) return SOC_ERR_PARAM;
    return socket_query_get_io_pin_from_seq(ctx, pin_idx, pin_info);
}

RES_API int32_t Res_GetVsChannelFrmPinSeq(uint32_t pin_idx, vs_channel_t *vs_channel)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (vs_channel == NULL) return SOC_ERR_PARAM;
    return socket_query_get_vs_ch_from_seq(ctx, pin_idx, vs_channel);
}

RES_API int32_t Res_GetAllSemAndAsic(sem_asic_t *logic_data, uint32_t size)
{
    SocContext *ctx = soc_context_get_instance();
    if (ctx == NULL) return SOC_ERR_INTERNAL;
    if (logic_data == NULL || size == 0) return SOC_ERR_PARAM;
    return socket_query_get_all_sem_asic(ctx, logic_data, size);
}
