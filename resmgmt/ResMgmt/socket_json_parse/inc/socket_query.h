#ifndef _SOCKET_QUERY_H
#define _SOCKET_QUERY_H

/*--- 项目头文件 ---*/
#include "socket_types.h"
#include "dut_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明 context (完整定义见 socket_internal.h) */
typedef struct soc_context_impl SocContext;

/*===========================================================================
 * 查询 API — ResSocket.c Facade 的唯一业务入口
 * 封装所有 for 循环遍历、pin 匹配、whole_no 查找、mutex 管理
 *===========================================================================*/

int32_t socket_query_is_pin_defined(SocContext *ctx, uint32_t dut_no,
                                     uint32_t dut_pin, uint32_t pin_type);

int32_t socket_query_get_dut_io_pins(SocContext *ctx, uint32_t dut_no,
                                      uint32_t *io_pin_num, uint32_t msize);

int32_t socket_query_io_pin_seq_by_dut_param(SocContext *ctx,
                                              const dut_param_t *dut_param,
                                              io_logic_data_t *logic_data,
                                              uint32_t msize);

int32_t socket_query_vs_pin_seq_by_dut_param(SocContext *ctx,
                                              const dut_param_t *dut_param,
                                              vs_logic_data_t *logic_data,
                                              uint32_t msize);

int32_t socket_query_dut_param_from_sem_ch(SocContext *ctx,
                                            const io_pin_t *io_pin,
                                            dut_param_t *dut_param);

int32_t socket_query_dut_param_from_psm_ch(SocContext *ctx,
                                            const vs_channel_t *vs_ch,
                                            dut_param_t *dut_param);

int32_t socket_query_get_io_pin_from_seq(SocContext *ctx,
                                          uint32_t pin_idx,
                                          io_pin_t *pin_info);

int32_t socket_query_get_vs_ch_from_seq(SocContext *ctx,
                                         uint32_t pin_idx,
                                         vs_channel_t *vs_channel);

int32_t socket_query_get_all_sem_asic(SocContext *ctx,
                                       sem_asic_t *logic_data,
                                       uint32_t size);

/* --- 文件管理 (Res_SetSocketFileName / Res_GetSocketFileName 的业务逻辑) --- */

int32_t socket_set_file_name(SocContext *ctx, const char *path, uint32_t path_len);

int32_t socket_get_file_name(SocContext *ctx, char *socket_file_name, uint32_t mlen);

#ifdef __cplusplus
}
#endif

#endif /* _SOCKET_QUERY_H */
