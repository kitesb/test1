/*===========================================================================
 * A. 必填 scalar 字段测试（8 用例）
 *    签名: void test_xxx(SocContext *ctx)
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"

#define FULL_JSON    "test_fixtures/full.json"
#define MINIMAL_JSON "test_fixtures/minimal.json"

/* --- 错误路径 --- */

void test_multiple_node_missing(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    const char *json = "{ \"other\": {} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

void test_dutCount_missing(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    const char *json =
        "{ \"multiple\": {"
        "  \"commonPinCount\": 2, \"powerPinCount\": 2,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 2,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

void test_commonPinCount_invalid(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": \"not_a_number\", \"powerPinCount\": 2,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 2,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

void test_drPinCount_missing(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 2, \"powerPinCount\": 2,"
        "  \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 2,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

void test_stn_missing(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 2, \"powerPinCount\": 2,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"SITE\": 1, \"CHAIN\": 1, \"pps\": 2,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

void test_pps_missing(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    const char *json =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 2, \"powerPinCount\": 2,"
        "  \"drPinCount\": 2, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": {}, \"commonPins\": {}"
        "} }";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- 正常路径 --- */

void test_full_json_scalars_normal(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    int ret = parse_socket_json(ctx, FULL_JSON);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_EQUAL_UINT32(384, ctx->meta.dut_count);
    TEST_ASSERT_EQUAL_UINT32(34,  ctx->meta.common_pin_num);
    TEST_ASSERT_EQUAL_UINT32(5,   ctx->meta.power_pin_num);
    TEST_ASSERT_EQUAL_UINT32(28,  ctx->meta.dr_pin_count);
    TEST_ASSERT_EQUAL_UINT32(6,   ctx->meta.io_pin_count);
    TEST_ASSERT_EQUAL_UINT32(1,   ctx->meta.stn);
    TEST_ASSERT_EQUAL_UINT32(4,   ctx->meta.site);
    TEST_ASSERT_EQUAL_UINT32(1,   ctx->meta.chain);
    TEST_ASSERT_EQUAL_UINT32(5,   ctx->meta.parallel_vs_map_num);
}

void test_minimal_json_scalars_normal(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx);
    int ret = parse_socket_json(ctx, MINIMAL_JSON);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_EQUAL_UINT32(4, ctx->meta.dut_count);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.common_pin_num);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.power_pin_num);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.dr_pin_count);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->meta.io_pin_count);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->meta.stn);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->meta.site);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->meta.chain);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.parallel_vs_map_num);
}
