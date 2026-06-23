/*===========================================================================
 * F. 边界条件 / 工业级边缘场景测试（4 用例）
 *    签名: void test_xxx(SocContext *ctx)
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"

#define MINIMAL_JSON "test_fixtures/minimal.json"
#define FULL_JSON    "test_fixtures/full.json"

/* --- F1. 先解析 small pool 再解析 large pool（触发 EXPAND 扩容路径）--- */

void test_pool_expand_on_growth(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_EQUAL_UINT32(4, ctx->meta.dut_count);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.common_pin_num);

    free_socket_data(ctx);
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_EQUAL_UINT32(384, ctx->meta.dut_count);
    TEST_ASSERT_EQUAL_UINT32(34,  ctx->meta.common_pin_num);

    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);
    TEST_ASSERT_MESSAGE(strlen(ctx->dut_common_pins[0].pCommonDutPin[1].ch_name) > 0,
        "common pin ch_name should be filled after expand");
}

/* --- F2. parallelVsMap 缺失（multiple 节点存在但无 parallelVsMap）--- */

void test_parallelVsMap_missing(SocContext *ctx)
{
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- F3. powerPins / commonPins 完全缺失（合法的无 pin 数据场景）--- */

void test_pins_absent_graceful(SocContext *ctx)
{
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2]"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins);
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);
    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins[0].pPowerDutPin);
    TEST_ASSERT_EQUAL_UINT32(0, strlen(ctx->dut_common_pins[0].pCommonDutPin[0].ch_name));
}

/* --- F4. pps=0 触发 parallel_vs_map 零值守卫 --- */

void test_pps_zero_guarded(SocContext *ctx)
{
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 0,"
        "  \"parallelVsMap\": {},"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_NULL(ctx->extra->vs_map);
}
