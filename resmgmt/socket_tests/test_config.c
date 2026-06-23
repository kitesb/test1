/*===========================================================================
 * D. Config JSON 联合解析测试（6 用例）
 *    签名: void test_xxx(SocContext *ctx)
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"

#define FULL_JSON       "test_fixtures/full.json"
#define CONFIG_JSON     "config-local.json"

/* --- D1. socket 未解析，直接调 config --- */

void test_config_socket_not_parsed(SocContext *ctx)
{
    int ret = parse_config_json(ctx, CONFIG_JSON);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* --- D2. INFO 节点缺失 --- */

void test_config_info_missing(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    const char *json = "{ \"OTHER\": {} }";
    int ret = parse_config_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- D3. SEM_NUM 字段缺失 --- */

void test_config_sem_num_missing(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    const char *json =
        "{ \"INFO\": {"
        "  \"ASIC_NUM\": 4, \"DR_NUM\": 14, \"IO_NUM\": 37,"
        "  \"PSM_NUM\": 4, \"PPS_NUM\": 256, \"DUT_NUM\": 512"
        "} }";
    int ret = parse_config_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* --- D4. ddut < sdut → BOARD_MISMATCH --- */

void test_config_ddut_lt_sdut(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    const char *json =
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 1, \"ASIC_NUM\": 1, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 1, \"PPS_NUM\": 1, \"DUT_NUM\": 1"
        "} }";
    int ret = parse_config_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_BOARD_MISMATCH, ret);
}

/* --- D5. 正常联合解析 --- */

void test_config_normal_joint(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    int ret = parse_config_json(ctx, CONFIG_JSON);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_EQUAL_UINT32(32,  ctx->logic_config.sem_num);
    TEST_ASSERT_EQUAL_UINT32(4,   ctx->logic_config.asic_num);
    TEST_ASSERT_EQUAL_UINT32(14,  ctx->logic_config.dr_num);
    TEST_ASSERT_EQUAL_UINT32(37,  ctx->logic_config.io_num);
    TEST_ASSERT_EQUAL_UINT32(4,   ctx->logic_config.psm_num);
    TEST_ASSERT_EQUAL_UINT32(256, ctx->logic_config.pps_num);
    TEST_ASSERT_EQUAL_UINT32(512, ctx->logic_config.dut_num);

    /* 验证部分 pin 的 whole_no 被赋值 */
    int filled = 0;
    uint32_t i, j;
    for (i = 0; i < ctx->meta.common_pin_num; i++) {
        for (j = 0; j < ctx->meta.dut_count; j++) {
            if (ctx->dut_common_pins[i].pCommonDutPin[j].whole_no != 0) {
                filled++;
            }
        }
    }
    TEST_ASSERT_MESSAGE(filled > 0,
        "at least some common pins should have whole_no assigned");
}

/* --- D6. parallelVsMap 正常解析 --- */

void test_parallelVsMap_normal(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));

    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->vs_map);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->extra->vs_map[0]);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->extra->vs_map[4]);
}
