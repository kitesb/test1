#ifndef _SOCKET_TYPES_H
#define _SOCKET_TYPES_H

/*--- 项目头文件 ---*/
#include "general.h"

/*--- 第三方库头文件（无） ---*/

/*--- 系统头文件 ---*/
#include <pthread.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 常量
 *===========================================================================*/
#define MAX_CH_NAME_LEN      32
#define MAX_FILE_PATH_LEN    256

/*===========================================================================
 * 枚举
 *===========================================================================*/

typedef enum {
    PARSE_IDLE            = 0,  /* socket 与 config JSON 共用空闲态 */
    SOCKET_PARSE_NEED     = 1,
    SOCKET_PARSE_FINISHED = 2,
    JSON_PARSE_NEED       = 3,
    JSON_PARSE_FINISHED   = 4,
} parse_state_t;

typedef enum {
    SOC_OK                        =  0,
    SOC_ERR_CHANNEL_COMPILE_FAILED = -1,
    SOC_ERR_MEMORY_ALLOC          = -2,
    SOC_ERR_PARSE_FAILED          = -3,
    SOC_ERR_JSON_PARSE_FAILED     = -4,
    SOC_ERR_CHANNEL_NOT_FOUND     = -5,
    SOC_ERR_PARAM                 = -6,
    SOC_ERR_SOCKET_NOT_FOUND      = -7,
    SOC_ERR_JSON_NOT_FOUND        = -8,
    SOC_ERR_FILE_NOT_FOUND        = -9,
    SOC_ERR_QUERY_CHANNEL_NOT_FOUND = -10,
    SOC_ERR_BOARD_MISMATCH        = -11,
    SOC_ERR_INTERNAL              = -12
} soc_error_t;

#define SOC_ERR_INTERRUPTED  (-100)

typedef enum {
    PIN_TYPE_DR = 0,
    PIN_TYPE_IO = 1,
} pin_type_t;

/*===========================================================================
 * 结构体
 *===========================================================================*/

typedef struct {
    uint32_t dut_no;
    char     ch_name[MAX_CH_NAME_LEN];
    uint32_t whole_no;
} _dut_pin_t;

typedef struct {
    uint32_t      common_pin_num;
    uint8_t       pin_type;
    _dut_pin_t   *pCommonDutPin;
} dut_common_pin_t;

typedef struct {
    uint32_t    vs_pin_num;
    _dut_pin_t *pPowerDutPin;
} dut_power_pin_t;

typedef struct {
    uint32_t sem_num;
    uint32_t asic_num;
    uint32_t dr_num;
    uint32_t io_num;
    uint32_t psm_num;
    uint32_t pps_num;
    uint32_t dut_num;
} logic_config_t;

typedef struct {
    char         file_path[MAX_FILE_PATH_LEN];
    char         compiled_json_path[MAX_FILE_PATH_LEN];
    char         config_json_path[MAX_FILE_PATH_LEN];
    uint32_t     dut_count;
    uint32_t     common_pin_num;
    uint32_t     power_pin_num;
    uint32_t     on_socket_board;
    parse_state_t socket_parse_state;
    parse_state_t json_parse_state;
    uint32_t     dr_pin_count;
    uint32_t     io_pin_count;
    uint32_t     stn;
    uint32_t     site;
    uint32_t     chain;
    uint32_t     parallel_vs_map_num;
} soc_meta_t;

#ifdef __cplusplus
}
#endif

/*===========================================================================
 * whole_no 计算宏 (从1开始, 0=未初始化)
 *===========================================================================*/
#define COMMON_SERIAL(sem, asic, ch, asic_num, ch_total) \
    (((sem) - 1) * (asic_num) * (ch_total) + ((asic) - 1) * (ch_total) + (ch))

#define POWER_SERIAL(psm, ch, pps_num) \
    (((psm) - 1) * (pps_num) + (ch))

/*===========================================================================
 * whole_no 反解宏
 *===========================================================================*/
#define SERIAL_TO_SEM(s, asic_num, ch_total) \
    ((((s) - 1) / ((asic_num) * (ch_total))) + 1)
#define SERIAL_TO_ASIC(s, asic_num, ch_total) \
    (((((s) - 1) % ((asic_num) * (ch_total))) / (ch_total)) + 1)
#define SERIAL_TO_CH(s, ch_total) \
    ((((s) - 1) % (ch_total)) + 1)
#define SERIAL_TO_PSM(s, pps_num) \
    (((s) - 1) / (pps_num) + 1)
#define SERIAL_TO_PWR_CH(s, pps_num) \
    (((s) - 1) % (pps_num) + 1)

#endif /* _SOCKET_TYPES_H */
