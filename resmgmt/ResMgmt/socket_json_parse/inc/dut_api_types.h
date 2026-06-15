#ifndef _DUT_API_TYPES_H
#define _DUT_API_TYPES_H

#include "dealsoc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t dut_no;
	uint32_t dut_pin;
} dut_param_t;

typedef struct {
	dut_param_t dut_data;
	uint8_t     pin_type;
	uint32_t    pin_idx;
} io_logic_data_t;

typedef struct {
	dut_param_t dut_data;
	uint8_t     pin_type;
	uint32_t    pin_idx;
} vs_logic_data_t;

typedef struct {
	uint32_t sem_no;
	uint32_t asic_no;
	uint32_t channel_no;
} io_pin_t;

typedef struct {
	uint32_t psm_no;
	uint32_t channel_no;
} vs_channel_t;

typedef struct {
	uint32_t sem_no;
	uint32_t asic_no;
} sem_asic_t;

#ifdef __cplusplus
}
#endif

#endif /* _DUT_API_TYPES_H */
