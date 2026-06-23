/*===========================================================================
 * 测试入口 — 运行全部测试用例 (53 + 67 comprehensive = 120)
 *===========================================================================*/
#include "test_harness.h"

/* 全局计数器定义 */
int _tests_run       = 0;
int _tests_failed    = 0;
int _tests_ignored   = 0;
const char *_current_test = NULL;

/* --- 外部测试函数声明（签名: void test_xxx(SocContext *ctx) 或 void test_xxx(void)） --- */

/* A. scalars (8) */
void test_multiple_node_missing(SocContext *ctx);
void test_dutCount_missing(SocContext *ctx);
void test_commonPinCount_invalid(SocContext *ctx);
void test_drPinCount_missing(SocContext *ctx);
void test_stn_missing(SocContext *ctx);
void test_pps_missing(SocContext *ctx);
void test_full_json_scalars_normal(SocContext *ctx);
void test_minimal_json_scalars_normal(SocContext *ctx);

/* B. optional nodes (17) */
void test_pinNames_present(SocContext *ctx);
void test_pinNames_absent(SocContext *ctx);
void test_symbol_present(SocContext *ctx);
void test_symbol_absent(SocContext *ctx);
void test_vsDefinitions_present(SocContext *ctx);
void test_vsDefinitions_absent(SocContext *ctx);
void test_vsSiteDefinitions_present(SocContext *ctx);
void test_vsSiteDefinitions_absent(SocContext *ctx);
void test_hvppsDefinitions_present(SocContext *ctx);
void test_hvppsDefinitions_absent(SocContext *ctx);
void test_hvppsSiteDefinitions_present(SocContext *ctx);
void test_hvppsSiteDefinitions_absent(SocContext *ctx);
void test_separateEntities_present(SocContext *ctx);
void test_separateEntities_absent(SocContext *ctx);
void test_systemDut_present(SocContext *ctx);
void test_systemDut_absent(SocContext *ctx);
void test_vs_hvpps_dedup(SocContext *ctx);

/* C. pins (5) */
void test_powerPins_normal(SocContext *ctx);
void test_commonPins_normal(SocContext *ctx);
void test_powerPins_missing_dut_num(SocContext *ctx);
void test_commonPins_missing_ch_name(SocContext *ctx);
void test_dutSequence_normal(SocContext *ctx);

/* C. pins (new: ch_name array support) */
void test_powerPins_ch_name_array(SocContext *ctx);
void test_commonPins_ch_name_array(SocContext *ctx);
void test_powerPins_ch_name_string_backward_compat(SocContext *ctx);
void test_powerPins_ch_name_array_overflow(SocContext *ctx);
void test_commonPins_ch_name_array_invalid_element(SocContext *ctx);

/* D. config (6) */
void test_config_socket_not_parsed(SocContext *ctx);
void test_config_info_missing(SocContext *ctx);
void test_config_sem_num_missing(SocContext *ctx);
void test_config_ddut_lt_sdut(SocContext *ctx);
void test_config_normal_joint(SocContext *ctx);
void test_parallelVsMap_normal(SocContext *ctx);

/* E. lifecycle (5 + 1 special) */
void test_extra_not_null_after_create(SocContext *ctx);
void test_extra_zeroed_after_free(SocContext *ctx);
void test_double_parse_no_leak(SocContext *ctx);
void test_file_not_found(SocContext *ctx);
void test_invalid_json(SocContext *ctx);
void test_extra_null_triggers_internal_error(void);  /* no wrapper */

/* F. edge cases (4) */
void test_pool_expand_on_growth(SocContext *ctx);
void test_parallelVsMap_missing(SocContext *ctx);
void test_pins_absent_graceful(SocContext *ctx);
void test_pps_zero_guarded(SocContext *ctx);

/* G. threading & lifecycle (7) — no wrapper, self-managed ctx */
void test_context_lifecycle_no_thread(void);
void test_thread_start_stop(void);
void test_destroy_stops_thread(void);
void test_double_destroy_safety(void);
void test_wait_socket_parse_idle(void);
void test_concurrent_get_instance(void);
void test_singleton_cleared_after_destroy(void);

/* H. code review verification (13) */
void test_1d_parse_socket_json_null_path(SocContext *ctx);
void test_1d_parse_config_json_null_path(SocContext *ctx);
void test_1e_dut_count_zero_rejected(SocContext *ctx);
void test_1f_strtol_overflow_power_pin_key(SocContext *ctx);
void test_1f_strtol_overflow_dut_key(SocContext *ctx);
void test_2_ch_idx_zero_rejected_common(SocContext *ctx);
void test_2_ch_idx_zero_rejected_power(SocContext *ctx);
void test_4a_pin_type_2_rejected(SocContext *ctx);
void test_4a_pin_type_255_rejected(SocContext *ctx);
void test_4b_query_io_pin_null_dut_sequence(SocContext *ctx);
void test_4c_query_vs_pin_null_dut_sequence(SocContext *ctx);
void test_1c_long_key_rejected(SocContext *ctx);
void test_set_socket_file_null_path(SocContext *ctx);

/* 1A 死锁验证 */
void test_1a_concurrent_query_and_signal(void);
void test_1a_query_no_wait_when_finished(SocContext *ctx);
void test_1a_both_no_wait_when_idle(SocContext *ctx);

/* I. Comprehensive — 补测所有未覆盖路径 (67) */

/*   I. Config 零值校验 (7) */
void test_config_sem_num_zero(SocContext *ctx);
void test_config_asic_num_zero(SocContext *ctx);
void test_config_psm_num_zero(SocContext *ctx);
void test_config_pps_num_zero(SocContext *ctx);
void test_config_dr_and_io_both_zero(SocContext *ctx);
void test_config_dr_zero_io_nonzero_ok(SocContext *ctx);
void test_config_io_zero_dr_nonzero_ok(SocContext *ctx);

/*   II. Config 结构缺失 (5) */
void test_config_missing_sem_node(SocContext *ctx);
void test_config_missing_asic_node(SocContext *ctx);
void test_config_missing_psm_node(SocContext *ctx);
void test_config_missing_pins_array(SocContext *ctx);
void test_config_missing_power_channels(SocContext *ctx);

/*   III. ch_name 不匹配 (2) */
void test_config_common_channel_not_found(SocContext *ctx);
void test_config_power_channel_not_found(SocContext *ctx);

/*   IV. Pool REUSE (4) */
void test_pool_reuse_same_size(SocContext *ctx);
void test_pool_reuse_large_then_small(SocContext *ctx);
void test_pool_reuse_dut_growth_only(SocContext *ctx);
void test_pool_reuse_triple_parse(SocContext *ctx);

/*   V. on_socket_board=0 (1) */
void test_config_on_socket_board_zero(SocContext *ctx);

/*   VI. pin_type 解析层校验 (3) */
void test_pin_type_invalid_255_rejected(SocContext *ctx);
void test_pin_type_invalid_2_rejected(SocContext *ctx);
void test_pin_type_valid_values_accepted(SocContext *ctx);

/*   VII. Pin/DUT 溢出守卫 (4) */
void test_power_pins_too_many_entries(SocContext *ctx);
void test_common_pins_too_many_entries(SocContext *ctx);
void test_power_pins_too_many_dut_entries(SocContext *ctx);
void test_common_pins_too_many_dut_entries(SocContext *ctx);

/*   VIII. HashTable (5) */
void test_hash_insert_find_normal(SocContext *ctx);
void test_hash_insert_duplicate_key(SocContext *ctx);
void test_hash_insert_null_params(SocContext *ctx);
void test_hash_find_missing_key(SocContext *ctx);
void test_hash_find_null_params(SocContext *ctx);

/*   IX. soc_wait 状态组合 (5) */
void test_wait_both_socket_finished_json_idle(SocContext *ctx);
void test_wait_both_both_finished(SocContext *ctx);
void test_wait_socket_finished_holds_lock(SocContext *ctx);
void test_wait_both_both_idle(SocContext *ctx);
void test_wait_socket_idle(SocContext *ctx);

/*   X. API 驱动接口 (19) */
void test_api_get_dut_all_io_pin_normal(SocContext *ctx);
void test_api_get_dut_all_io_pin_null_params(SocContext *ctx);
void test_api_get_io_pin_frm_pin_seq_normal(SocContext *ctx);
void test_api_get_io_pin_frm_pin_seq_zero_idx(SocContext *ctx);
void test_api_get_io_pin_frm_pin_seq_overflow_idx(SocContext *ctx);
void test_api_get_io_pin_frm_pin_seq_null_param(SocContext *ctx);
void test_api_get_vs_channel_frm_pin_seq_normal(SocContext *ctx);
void test_api_get_vs_channel_frm_pin_seq_zero_idx(SocContext *ctx);
void test_api_get_vs_channel_frm_pin_seq_null_param(SocContext *ctx);
void test_api_get_all_sem_and_asic_normal(SocContext *ctx);
void test_api_get_all_sem_and_asic_small_buffer(SocContext *ctx);
void test_api_get_all_sem_and_asic_null_params(SocContext *ctx);
void test_api_get_dut_param_frm_sem_channel_normal(SocContext *ctx);
void test_api_get_dut_param_frm_sem_channel_zero_input(SocContext *ctx);
void test_api_get_dut_param_frm_sem_channel_null_params(SocContext *ctx);
void test_api_query_dut_param_by_psm_channel_normal(SocContext *ctx);
void test_api_query_dut_param_by_psm_channel_zero_input(SocContext *ctx);
void test_api_query_dut_param_by_psm_channel_null_params(SocContext *ctx);
void test_api_get_socket_file_name_normal(SocContext *ctx);
void test_api_get_socket_file_name_null_params(SocContext *ctx);
void test_api_set_socket_file_same_file_noop(SocContext *ctx);
void test_api_is_pin_defined_valid_common_pin(SocContext *ctx);
void test_api_is_pin_defined_valid_power_pin(SocContext *ctx);
void test_api_is_pin_defined_not_found(SocContext *ctx);
void test_api_is_pin_defined_invalid_dut_no(SocContext *ctx);
void test_api_query_io_pin_seq_null_params(SocContext *ctx);
void test_api_query_vs_pin_seq_null_params(SocContext *ctx);

/* K. Exhaustive review — 穷举法审查新增 Bug 验证 (14 tests) */
void test_ex_b1_path_len_max_boundary(SocContext *ctx);
void test_ex_b1_path_len_max_minus_one_ok(SocContext *ctx);
void test_ex_b1_set_config_path_len_max_boundary(SocContext *ctx);
void test_ex_b2_pin_type_3_rejected(SocContext *ctx);
void test_ex_b2_pin_type_100_rejected(SocContext *ctx);
void test_ex_b2_pin_type_missing_defaults_to_dr(SocContext *ctx);
void test_ex_b3_empty_filename_returns_zero(SocContext *ctx);
void test_ex_b5_dut_sequence_shorter_than_dut_count(SocContext *ctx);
void test_ex_b5_dut_sequence_zero_element_accepted(SocContext *ctx);
void test_ex_b6_strtol_errno_cleared_before_use(SocContext *ctx);
void test_ex_b6_strtol_negative_pin_key_rejected(SocContext *ctx);
void test_ex_b6_strtol_non_numeric_pin_key_rejected(SocContext *ctx);
void test_ex_b8_hash_insert_long_key_returns_error(SocContext *ctx);
void test_ex_b8_hash_insert_exact_max_minus_one_ok(SocContext *ctx);
void test_ex_b9_init_sets_need_before_register(void);

int main(void)
{
    UNITY_BEGIN();

    /* A. scalars */
    RUN_TEST_WITH_CTX(test_multiple_node_missing);
    RUN_TEST_WITH_CTX(test_dutCount_missing);
    RUN_TEST_WITH_CTX(test_commonPinCount_invalid);
    RUN_TEST_WITH_CTX(test_drPinCount_missing);
    RUN_TEST_WITH_CTX(test_stn_missing);
    RUN_TEST_WITH_CTX(test_pps_missing);
    RUN_TEST_WITH_CTX(test_full_json_scalars_normal);
    RUN_TEST_WITH_CTX(test_minimal_json_scalars_normal);

    /* B. optional nodes */
    RUN_TEST_WITH_CTX(test_pinNames_present);
    RUN_TEST_WITH_CTX(test_pinNames_absent);
    RUN_TEST_WITH_CTX(test_symbol_present);
    RUN_TEST_WITH_CTX(test_symbol_absent);
    RUN_TEST_WITH_CTX(test_vsDefinitions_present);
    RUN_TEST_WITH_CTX(test_vsDefinitions_absent);
    RUN_TEST_WITH_CTX(test_vsSiteDefinitions_present);
    RUN_TEST_WITH_CTX(test_vsSiteDefinitions_absent);
    RUN_TEST_WITH_CTX(test_hvppsDefinitions_present);
    RUN_TEST_WITH_CTX(test_hvppsDefinitions_absent);
    RUN_TEST_WITH_CTX(test_hvppsSiteDefinitions_present);
    RUN_TEST_WITH_CTX(test_hvppsSiteDefinitions_absent);
    RUN_TEST_WITH_CTX(test_separateEntities_present);
    RUN_TEST_WITH_CTX(test_separateEntities_absent);
    RUN_TEST_WITH_CTX(test_systemDut_present);
    RUN_TEST_WITH_CTX(test_systemDut_absent);
    RUN_TEST_WITH_CTX(test_vs_hvpps_dedup);

    /* C. pins */
    RUN_TEST_WITH_CTX(test_powerPins_normal);
    RUN_TEST_WITH_CTX(test_commonPins_normal);
    RUN_TEST_WITH_CTX(test_powerPins_missing_dut_num);
    RUN_TEST_WITH_CTX(test_commonPins_missing_ch_name);
    RUN_TEST_WITH_CTX(test_dutSequence_normal);

    /* C6-C10: ch_name array support */
    RUN_TEST_WITH_CTX(test_powerPins_ch_name_array);
    RUN_TEST_WITH_CTX(test_commonPins_ch_name_array);
    RUN_TEST_WITH_CTX(test_powerPins_ch_name_string_backward_compat);
    RUN_TEST_WITH_CTX(test_powerPins_ch_name_array_overflow);
    RUN_TEST_WITH_CTX(test_commonPins_ch_name_array_invalid_element);

    /* D. config */
    RUN_TEST_WITH_CTX(test_config_socket_not_parsed);
    RUN_TEST_WITH_CTX(test_config_info_missing);
    RUN_TEST_WITH_CTX(test_config_sem_num_missing);
    RUN_TEST_WITH_CTX(test_config_ddut_lt_sdut);
    RUN_TEST_WITH_CTX(test_config_normal_joint);
    RUN_TEST_WITH_CTX(test_parallelVsMap_normal);

    /* E. lifecycle */
    RUN_TEST_WITH_CTX(test_extra_not_null_after_create);
    RUN_TEST_WITH_CTX(test_extra_zeroed_after_free);
    RUN_TEST_WITH_CTX(test_double_parse_no_leak);
    RUN_TEST_WITH_CTX(test_file_not_found);
    RUN_TEST_WITH_CTX(test_invalid_json);
    RUN_TEST(test_extra_null_triggers_internal_error);  /* 手动管理 ctx */

    /* F. edge cases */
    RUN_TEST_WITH_CTX(test_pool_expand_on_growth);
    RUN_TEST_WITH_CTX(test_parallelVsMap_missing);
    RUN_TEST_WITH_CTX(test_pins_absent_graceful);
    RUN_TEST_WITH_CTX(test_pps_zero_guarded);

    /* G. threading & lifecycle (self-managed context, no wrapper) */
    RUN_TEST(test_context_lifecycle_no_thread);
    RUN_TEST(test_thread_start_stop);
    RUN_TEST(test_destroy_stops_thread);
    RUN_TEST(test_double_destroy_safety);
    RUN_TEST(test_wait_socket_parse_idle);
    RUN_TEST(test_concurrent_get_instance);
    RUN_TEST(test_singleton_cleared_after_destroy);

    /* H. code review verification */
    RUN_TEST_WITH_CTX(test_1d_parse_socket_json_null_path);
    RUN_TEST_WITH_CTX(test_1d_parse_config_json_null_path);
    RUN_TEST_WITH_CTX(test_1e_dut_count_zero_rejected);
    RUN_TEST_WITH_CTX(test_1f_strtol_overflow_power_pin_key);
    RUN_TEST_WITH_CTX(test_1f_strtol_overflow_dut_key);
    RUN_TEST_WITH_CTX(test_2_ch_idx_zero_rejected_common);
    RUN_TEST_WITH_CTX(test_2_ch_idx_zero_rejected_power);
    RUN_TEST_WITH_CTX(test_4a_pin_type_2_rejected);
    RUN_TEST_WITH_CTX(test_4a_pin_type_255_rejected);
    RUN_TEST_WITH_CTX(test_4b_query_io_pin_null_dut_sequence);
    RUN_TEST_WITH_CTX(test_4c_query_vs_pin_null_dut_sequence);
    RUN_TEST_WITH_CTX(test_1c_long_key_rejected);
    RUN_TEST_WITH_CTX(test_set_socket_file_null_path);

    /* I. 1A 死锁验证 */
    RUN_TEST(test_1a_concurrent_query_and_signal);
    RUN_TEST_WITH_CTX(test_1a_query_no_wait_when_finished);
    RUN_TEST_WITH_CTX(test_1a_both_no_wait_when_idle);

    /* ================================================================
     * J. Comprehensive — 补测所有未覆盖路径 (67 tests)
     * ================================================================ */

    /* J-I. Config 零值校验 (7) */
    RUN_TEST_WITH_CTX(test_config_sem_num_zero);
    RUN_TEST_WITH_CTX(test_config_asic_num_zero);
    RUN_TEST_WITH_CTX(test_config_psm_num_zero);
    RUN_TEST_WITH_CTX(test_config_pps_num_zero);
    RUN_TEST_WITH_CTX(test_config_dr_and_io_both_zero);
    RUN_TEST_WITH_CTX(test_config_dr_zero_io_nonzero_ok);
    RUN_TEST_WITH_CTX(test_config_io_zero_dr_nonzero_ok);

    /* J-II. Config 结构缺失 (5) */
    RUN_TEST_WITH_CTX(test_config_missing_sem_node);
    RUN_TEST_WITH_CTX(test_config_missing_asic_node);
    RUN_TEST_WITH_CTX(test_config_missing_psm_node);
    RUN_TEST_WITH_CTX(test_config_missing_pins_array);
    RUN_TEST_WITH_CTX(test_config_missing_power_channels);

    /* J-III. ch_name 不匹配 (2) */
    RUN_TEST_WITH_CTX(test_config_common_channel_not_found);
    RUN_TEST_WITH_CTX(test_config_power_channel_not_found);

    /* J-IV. Pool REUSE (4) */
    RUN_TEST_WITH_CTX(test_pool_reuse_same_size);
    RUN_TEST_WITH_CTX(test_pool_reuse_large_then_small);
    RUN_TEST_WITH_CTX(test_pool_reuse_dut_growth_only);
    RUN_TEST_WITH_CTX(test_pool_reuse_triple_parse);

    /* J-V. on_socket_board=0 (1) */
    RUN_TEST_WITH_CTX(test_config_on_socket_board_zero);

    /* J-VI. pin_type 解析层校验 (3) */
    RUN_TEST_WITH_CTX(test_pin_type_invalid_255_rejected);
    RUN_TEST_WITH_CTX(test_pin_type_invalid_2_rejected);
    RUN_TEST_WITH_CTX(test_pin_type_valid_values_accepted);

    /* J-VII. Pin/DUT 溢出守卫 (4) */
    RUN_TEST_WITH_CTX(test_power_pins_too_many_entries);
    RUN_TEST_WITH_CTX(test_common_pins_too_many_entries);
    RUN_TEST_WITH_CTX(test_power_pins_too_many_dut_entries);
    RUN_TEST_WITH_CTX(test_common_pins_too_many_dut_entries);

    /* J-VIII. HashTable (5) */
    RUN_TEST_WITH_CTX(test_hash_insert_find_normal);
    RUN_TEST_WITH_CTX(test_hash_insert_duplicate_key);
    RUN_TEST_WITH_CTX(test_hash_insert_null_params);
    RUN_TEST_WITH_CTX(test_hash_find_missing_key);
    RUN_TEST_WITH_CTX(test_hash_find_null_params);

    /* J-IX. soc_wait 状态组合 (5) */
    RUN_TEST_WITH_CTX(test_wait_both_socket_finished_json_idle);
    RUN_TEST_WITH_CTX(test_wait_both_both_finished);
    RUN_TEST_WITH_CTX(test_wait_socket_finished_holds_lock);
    RUN_TEST_WITH_CTX(test_wait_both_both_idle);
    RUN_TEST_WITH_CTX(test_wait_socket_idle);

    /* J-X. API 驱动接口 (27) */
    RUN_TEST_WITH_CTX(test_api_get_dut_all_io_pin_normal);
    RUN_TEST_WITH_CTX(test_api_get_dut_all_io_pin_null_params);
    RUN_TEST_WITH_CTX(test_api_get_io_pin_frm_pin_seq_normal);
    RUN_TEST_WITH_CTX(test_api_get_io_pin_frm_pin_seq_zero_idx);
    RUN_TEST_WITH_CTX(test_api_get_io_pin_frm_pin_seq_overflow_idx);
    RUN_TEST_WITH_CTX(test_api_get_io_pin_frm_pin_seq_null_param);
    RUN_TEST_WITH_CTX(test_api_get_vs_channel_frm_pin_seq_normal);
    RUN_TEST_WITH_CTX(test_api_get_vs_channel_frm_pin_seq_zero_idx);
    RUN_TEST_WITH_CTX(test_api_get_vs_channel_frm_pin_seq_null_param);
    RUN_TEST_WITH_CTX(test_api_get_all_sem_and_asic_normal);
    RUN_TEST_WITH_CTX(test_api_get_all_sem_and_asic_small_buffer);
    RUN_TEST_WITH_CTX(test_api_get_all_sem_and_asic_null_params);
    RUN_TEST_WITH_CTX(test_api_get_dut_param_frm_sem_channel_normal);
    RUN_TEST_WITH_CTX(test_api_get_dut_param_frm_sem_channel_zero_input);
    RUN_TEST_WITH_CTX(test_api_get_dut_param_frm_sem_channel_null_params);
    RUN_TEST_WITH_CTX(test_api_query_dut_param_by_psm_channel_normal);
    RUN_TEST_WITH_CTX(test_api_query_dut_param_by_psm_channel_zero_input);
    RUN_TEST_WITH_CTX(test_api_query_dut_param_by_psm_channel_null_params);
    RUN_TEST_WITH_CTX(test_api_get_socket_file_name_normal);
    RUN_TEST_WITH_CTX(test_api_get_socket_file_name_null_params);
    RUN_TEST_WITH_CTX(test_api_set_socket_file_same_file_noop);
    RUN_TEST_WITH_CTX(test_api_is_pin_defined_valid_common_pin);
    RUN_TEST_WITH_CTX(test_api_is_pin_defined_valid_power_pin);
    RUN_TEST_WITH_CTX(test_api_is_pin_defined_not_found);
    RUN_TEST_WITH_CTX(test_api_is_pin_defined_invalid_dut_no);
    RUN_TEST_WITH_CTX(test_api_query_io_pin_seq_null_params);
    RUN_TEST_WITH_CTX(test_api_query_vs_pin_seq_null_params);

    /* ================================================================
     * K. Exhaustive Review — 穷举法审查验证 (14 tests)
     * ================================================================ */

    /* K-1. EX-B1: Res_SetSocketFileName path_len 边界 */
    RUN_TEST_WITH_CTX(test_ex_b1_path_len_max_boundary);
    RUN_TEST_WITH_CTX(test_ex_b1_path_len_max_minus_one_ok);
    RUN_TEST_WITH_CTX(test_ex_b1_set_config_path_len_max_boundary);

    /* K-2. EX-B2: parse_common_pins pin_type 非法值 */
    RUN_TEST_WITH_CTX(test_ex_b2_pin_type_3_rejected);
    RUN_TEST_WITH_CTX(test_ex_b2_pin_type_100_rejected);
    RUN_TEST_WITH_CTX(test_ex_b2_pin_type_missing_defaults_to_dr);

    /* K-3. EX-B3: Res_GetSocketFileName 空文件名歧义 */
    RUN_TEST_WITH_CTX(test_ex_b3_empty_filename_returns_zero);

    /* K-4. EX-B5: parse_dut_sequence 尾部零值 */
    RUN_TEST_WITH_CTX(test_ex_b5_dut_sequence_shorter_than_dut_count);
    RUN_TEST_WITH_CTX(test_ex_b5_dut_sequence_zero_element_accepted);

    /* K-5. EX-B6: strtol errno 回归验证 */
    RUN_TEST_WITH_CTX(test_ex_b6_strtol_errno_cleared_before_use);
    RUN_TEST_WITH_CTX(test_ex_b6_strtol_negative_pin_key_rejected);
    RUN_TEST_WITH_CTX(test_ex_b6_strtol_non_numeric_pin_key_rejected);

    /* K-6. EX-B8: HashTable 超长 key 边界 */
    RUN_TEST_WITH_CTX(test_ex_b8_hash_insert_long_key_returns_error);
    RUN_TEST_WITH_CTX(test_ex_b8_hash_insert_exact_max_minus_one_ok);

    /* K-7. EX-B9: socket_json_parse_init 注册竞态验证 (自管理 ctx, 不用 wrapper) */
    RUN_TEST(test_ex_b9_init_sets_need_before_register);

    UNITY_END();
    return unity_result();
}
