/*===========================================================================
 * C. powerPins / commonPins 解析测试（5 用例）
 *    签名: void test_xxx(SocContext *ctx)
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"

#define FULL_JSON    "test_fixtures/full.json"

/* --- C1. powerPins 正常解析。full.json 的 dut_num key 从 "1" 开始 --- */

void test_powerPins_normal(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));

    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].vs_pin_num);
    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins[0].pPowerDutPin);
    /* DUT 编号从 1 开始，pPowerDutPin[1] 是第一个被填充的 */
    TEST_ASSERT_MESSAGE(strlen(ctx->dut_power_pins[0].pPowerDutPin[1].ch_name) > 0,
        "power pin ch_name should not be empty");
}

/* --- C2. commonPins 正常解析 --- */

void test_commonPins_normal(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));

    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].common_pin_num);
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);
    TEST_ASSERT_MESSAGE(ctx->dut_common_pins[0].pin_type <= 1,
        "pin_type should be 0 or 1");
    /* DUT 编号从 1 开始 */
    TEST_ASSERT_MESSAGE(strlen(ctx->dut_common_pins[0].pCommonDutPin[1].ch_name) > 0,
        "common pin ch_name should not be empty");
}

/* --- C3. powerPins 缺少 dut_num --- */

void test_powerPins_missing_dut_num(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2]"
        "  },"
        "  \"powerPins\": { \"1\": {} },"
        "  \"commonPins\": { \"1\": { \"pin_type\": 0, \"dut_num\": {} } }"
        "}";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- C4. commonPins 缺少 ch_name --- */

void test_commonPins_missing_ch_name(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2]"
        "  },"
        "  \"powerPins\": { \"1\": { \"dut_num\": { \"0\": { \"ch_name\": \"V1\" } } } },"
        "  \"commonPins\": { \"1\": { \"pin_type\": 0, \"dut_num\": { \"0\": {} } } }"
        "}";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- C5. dutSequence 正常解析 --- */

void test_dutSequence_normal(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));

    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->dut_sequence);
    TEST_ASSERT_EQUAL_UINT32(384, ctx->extra->dut_seq_len);
    TEST_ASSERT_EQUAL_UINT32(1,   ctx->extra->dut_sequence[0]);
    TEST_ASSERT_EQUAL_UINT32(384, ctx->extra->dut_sequence[383]);
}

/* --- C6. powerPins ch_name 为数组格式 --- */

void test_powerPins_ch_name_array(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 4, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2, 3, 4]"
        "  },"
        "  \"powerPins\": {"
        "    \"1\": {"
        "      \"dut_num\": {"
        "        \"1\": { \"ch_name\": [\"PPS003\", \"PPS004\"] },"
        "        \"2\": { \"ch_name\": [\"PPS006\", \"PPS007\"] }"
        "      }"
        "    }"
        "  },"
        "  \"commonPins\": {"
        "    \"1\": {"
        "      \"pin_type\": 0,"
        "      \"dut_num\": {"
        "        \"0\": { \"ch_name\": \"DUT0\" }"
        "      }"
        "    }"
        "  }"
        "}";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].vs_pin_num);
    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins[0].pPowerDutPin);

    /* DUT "1" 展开为 2 个条目: dut_idx 0=PPS003, dut_idx 1=PPS004 */
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].pPowerDutPin[0].dut_no);
    TEST_ASSERT_EQUAL_STRING("PPS003", ctx->dut_power_pins[0].pPowerDutPin[0].ch_name);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].pPowerDutPin[1].dut_no);
    TEST_ASSERT_EQUAL_STRING("PPS004", ctx->dut_power_pins[0].pPowerDutPin[1].ch_name);

    /* DUT "2" 展开为 2 个条目: dut_idx 2=PPS006, dut_idx 3=PPS007 */
    TEST_ASSERT_EQUAL_UINT32(2, ctx->dut_power_pins[0].pPowerDutPin[2].dut_no);
    TEST_ASSERT_EQUAL_STRING("PPS006", ctx->dut_power_pins[0].pPowerDutPin[2].ch_name);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->dut_power_pins[0].pPowerDutPin[3].dut_no);
    TEST_ASSERT_EQUAL_STRING("PPS007", ctx->dut_power_pins[0].pPowerDutPin[3].ch_name);
}

/* --- C7. commonPins ch_name 为数组格式 --- */

void test_commonPins_ch_name_array(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 3, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2, 3]"
        "  },"
        "  \"powerPins\": {"
        "    \"1\": {"
        "      \"dut_num\": {"
        "        \"0\": { \"ch_name\": \"PWR\" }"
        "      }"
        "    }"
        "  },"
        "  \"commonPins\": {"
        "    \"1\": {"
        "      \"pin_type\": 1,"
        "      \"dut_num\": {"
        "        \"1\": { \"ch_name\": [\"IO_A\", \"IO_B\"] },"
        "        \"2\": { \"ch_name\": [\"IO_C\"] }"
        "      }"
        "    }"
        "  }"
        "}";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].common_pin_num);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].pin_type);
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);

    /* DUT "1" 展开为 2 个条目 */
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].pCommonDutPin[0].dut_no);
    TEST_ASSERT_EQUAL_STRING("IO_A", ctx->dut_common_pins[0].pCommonDutPin[0].ch_name);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].pCommonDutPin[1].dut_no);
    TEST_ASSERT_EQUAL_STRING("IO_B", ctx->dut_common_pins[0].pCommonDutPin[1].ch_name);

    /* DUT "2" 展开为 1 个条目（数组只有一个元素 = 向后兼容） */
    TEST_ASSERT_EQUAL_UINT32(2, ctx->dut_common_pins[0].pCommonDutPin[2].dut_no);
    TEST_ASSERT_EQUAL_STRING("IO_C", ctx->dut_common_pins[0].pCommonDutPin[2].ch_name);
}

/* --- C8. ch_name 单字符串格式（向后兼容） --- */

void test_powerPins_ch_name_string_backward_compat(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2]"
        "  },"
        "  \"powerPins\": {"
        "    \"1\": {"
        "      \"dut_num\": {"
        "        \"1\": { \"ch_name\": \"HCPPS001\" },"
        "        \"2\": { \"ch_name\": \"HCPPS002\" }"
        "      }"
        "    }"
        "  },"
        "  \"commonPins\": {"
        "    \"1\": {"
        "      \"pin_type\": 0,"
        "      \"dut_num\": {"
        "        \"0\": { \"ch_name\": \"DUT0\" }"
        "      }"
        "    }"
        "  }"
        "}";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    TEST_ASSERT_NOT_NULL(ctx->dut_power_pins);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].vs_pin_num);

    /* 单字符串 ch_name 行为不变 */
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].pPowerDutPin[0].dut_no);
    TEST_ASSERT_EQUAL_STRING("HCPPS001", ctx->dut_power_pins[0].pPowerDutPin[0].ch_name);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->dut_power_pins[0].pPowerDutPin[1].dut_no);
    TEST_ASSERT_EQUAL_STRING("HCPPS002", ctx->dut_power_pins[0].pPowerDutPin[1].ch_name);
}

/* --- C9. ch_name 数组元素溢出检查 --- */

void test_powerPins_ch_name_array_overflow(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2]"
        "  },"
        "  \"powerPins\": {"
        "    \"1\": {"
        "      \"dut_num\": {"
        "        \"1\": { \"ch_name\": [\"A\", \"B\", \"C\"] }"
        "      }"
        "    }"
        "  },"
        "  \"commonPins\": {"
        "    \"1\": {"
        "      \"pin_type\": 0,"
        "      \"dut_num\": {"
        "        \"0\": { \"ch_name\": \"DUT0\" }"
        "      }"
        "    }"
        "  }"
        "}";
    /* dutCount=2, 但 DUT "1" 需要 3 个槽位 → 溢出 */
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- C10. ch_name 数组内包含非字符串元素 --- */

void test_commonPins_ch_name_array_invalid_element(SocContext *ctx)
{
    const char *json =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 2, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1, 2]"
        "  },"
        "  \"powerPins\": {"
        "    \"1\": {"
        "      \"dut_num\": {"
        "        \"0\": { \"ch_name\": \"PWR\" }"
        "      }"
        "    }"
        "  },"
        "  \"commonPins\": {"
        "    \"1\": {"
        "      \"pin_type\": 0,"
        "      \"dut_num\": {"
        "        \"1\": { \"ch_name\": [\"OK\", 123] }"
        "      }"
        "    }"
        "  }"
        "}";
    /* 数组第 2 个元素 123 不是字符串 */
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}
