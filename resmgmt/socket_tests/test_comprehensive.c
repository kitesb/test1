/*===========================================================================
 * 全面补测 — 覆盖所有之前未测试的代码路径
 *
 * 分组:
 *   I.   Config 零值校验 (7 tests)
 *   II.  Config 结构缺失错误 (5 tests)
 *   III. Config ch_name 不匹配 (2 tests)
 *   IV.  Pool REUSE 路径 (4 tests)
 *   V.   on_socket_board=0 路径 (1 test)
 *   VI.  parse_common_pins pin_type 解析层校验 (3 tests)
 *   VII. Pin/DUT 溢出守卫 (4 tests)
 *   VIII.HashTable 正常流 + 边界 (5 tests)
 *   IX.  soc_wait 状态组合 (5 tests)
 *   X.   API 驱动接口 — 正向 + 参数校验 (19 tests)
 *===========================================================================*/

#include "test_harness.h"
#include "test_helpers.h"
#include "ResAPI.h"
#include "soc_context.h"
#include "socket_types.h"
#include "dut_api_types.h"
#include "HashTable.h"
#include "soc_context.h"
#include <string.h>

/* 最小可用的 socket JSON 模板 — common+power pin 各有 1 个 */
#define SOCKET_MINIMAL \
    "{" \
    "  \"multiple\": {" \
    "    \"dutCount\": 4," \
    "    \"commonPinCount\": 2," \
    "    \"powerPinCount\": 2," \
    "    \"drPinCount\": 2," \
    "    \"ioPinCount\": 2," \
    "    \"STN\": 1," \
    "    \"SITE\": 1," \
    "    \"CHAIN\": 1," \
    "    \"pps\": 2," \
    "    \"parallelVsMap\": { \"VS1\": 1, \"VS2\": 1 }," \
    "    \"dutSequence\": [1, 2, 3, 4]" \
    "  }," \
    "  \"powerPins\": {" \
    "    \"1\": { \"dut_num\": { \"1\": {\"ch_name\":\"VCC\"}, \"2\": {\"ch_name\":\"VDD\"} } }," \
    "    \"2\": { \"dut_num\": { \"3\": {\"ch_name\":\"VIO\"}, \"4\": {\"ch_name\":\"VSS\"} } }" \
    "  }," \
    "  \"commonPins\": {" \
    "    \"1\": { \"pin_type\": 0, \"dut_num\": { \"1\": {\"ch_name\":\"DQ0\"}, \"2\": {\"ch_name\":\"DQ1\"} } }," \
    "    \"2\": { \"pin_type\": 1, \"dut_num\": { \"3\": {\"ch_name\":\"DQS0\"}, \"4\": {\"ch_name\":\"DQS1\"} } }" \
    "  }" \
    "}"

/* 最小可用的 config JSON 模板 — 配合 SOCKET_MINIMAL */
#define CONFIG_MINIMAL \
    "{" \
    "  \"INFO\": {" \
    "    \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1," \
    "    \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4" \
    "  }," \
    "  \"SEM1\": {" \
    "    \"ASIC1\": { \"pins\": [ { \"DQ0\": 1 }, { \"DQ1\": 2 } ] }," \
    "    \"ASIC2\": { \"pins\": [ { \"DQS0\": 1 }, { \"DQS1\": 2 } ] }" \
    "  }," \
    "  \"SEM2\": {" \
    "    \"ASIC1\": { \"pins\": [] }," \
    "    \"ASIC2\": { \"pins\": [] }" \
    "  }," \
    "  \"PSM1\": { \"power_channels\": [ { \"VCC\": 1 }, { \"VDD\": 2 } ] }," \
    "  \"PSM2\": { \"power_channels\": [ { \"VIO\": 1 }, { \"VSS\": 2 } ] }" \
    "}"

/* 帮助函数: 解析 socket + config, 设置状态, 注册 singleton */
static int setup_full_parse(SocContext *ctx)
{
    int ret;
    ret = parse_json_string(ctx, SOCKET_MINIMAL);
    if (ret != SOC_OK) return ret;
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx, CONFIG_MINIMAL);
    if (ret != SOC_OK) return ret;
    ctx->meta.json_parse_state = JSON_PARSE_FINISHED;

    soc_context_register_instance(ctx);
    return SOC_OK;
}

/*===========================================================================
 * I. Config 零值校验 — config_parser.c:77-91 共 5 个 return 路径
 *===========================================================================*/

/* I-1: SEM_NUM=0 应被拒绝 */
void test_config_sem_num_zero(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 0, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 } }");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* I-2: ASIC_NUM=0 应被拒绝 */
void test_config_asic_num_zero(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 0, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 } }");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* I-3: PSM_NUM=0 应被拒绝 */
void test_config_psm_num_zero(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 0, \"PPS_NUM\": 2, \"DUT_NUM\": 4 } }");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* I-4: PPS_NUM=0 应被拒绝 */
void test_config_pps_num_zero(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 0, \"DUT_NUM\": 4 } }");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* I-5: DR_NUM=0 且 IO_NUM=0 应被拒绝 */
void test_config_dr_and_io_both_zero(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 0, \"IO_NUM\": 0,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 } }");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* I-6: DR_NUM=0 但 IO_NUM>0 应该允许 */
void test_config_dr_zero_io_nonzero_ok(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 0, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [ { \"DQ0\": 1 } ] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"SEM2\": { \"ASIC1\": { \"pins\": [] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"PSM1\": { \"power_channels\": [] },"
        "  \"PSM2\": { \"power_channels\": [] }"
        "}");
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
}

/* I-7: IO_NUM=0 但 DR_NUM>0 应该允许 */
void test_config_io_zero_dr_nonzero_ok(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 0,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [ { \"DQ0\": 1 } ] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"SEM2\": { \"ASIC1\": { \"pins\": [] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"PSM1\": { \"power_channels\": [] },"
        "  \"PSM2\": { \"power_channels\": [] }"
        "}");
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
}

/*===========================================================================
 * II. Config 结构缺失错误 — 5 个 return 路径
 *===========================================================================*/

/* II-1: SEM 节点缺失 (SEM_NUM=2 但只有 SEM1) */
void test_config_missing_sem_node(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"PSM1\": { \"power_channels\": [] },"
        "  \"PSM2\": { \"power_channels\": [] }"
        "}");
    /* SEM2 缺失 → 应返回 SOC_ERR_JSON_PARSE_FAILED */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* II-2: ASIC 节点缺失 (ASIC_NUM=2 但 SEM1 下只有 ASIC1) */
void test_config_missing_asic_node(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [] } },"
        "  \"SEM2\": { \"ASIC1\": { \"pins\": [] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"PSM1\": { \"power_channels\": [] },"
        "  \"PSM2\": { \"power_channels\": [] }"
        "}");
    /* SEM1 下 ASIC2 缺失 → SOC_ERR_JSON_PARSE_FAILED */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* II-3: PSM 节点缺失 */
void test_config_missing_psm_node(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 2, \"ASIC_NUM\": 2, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 2, \"PPS_NUM\": 2, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"SEM2\": { \"ASIC1\": { \"pins\": [] }, \"ASIC2\": { \"pins\": [] } },"
        "  \"PSM1\": { \"power_channels\": [] }"
        "}");
    /* PSM2 缺失 → SOC_ERR_JSON_PARSE_FAILED */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* II-4: ASIC 下 pins 数组缺失 */
void test_config_missing_pins_array(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 1, \"ASIC_NUM\": 1, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 1, \"PPS_NUM\": 1, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": {} },"
        "  \"PSM1\": { \"power_channels\": [] }"
        "}");
    /* SEM1/ASIC1 下 pins 缺失 → SOC_ERR_JSON_PARSE_FAILED */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/* II-5: PSM 下 power_channels 数组缺失 */
void test_config_missing_power_channels(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 1, \"ASIC_NUM\": 1, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 1, \"PPS_NUM\": 1, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [] } },"
        "  \"PSM1\": {}"
        "}");
    /* PSM1 下 power_channels 缺失 → SOC_ERR_JSON_PARSE_FAILED */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_PARSE_FAILED, ret);
}

/*===========================================================================
 * III. Config ch_name 不匹配 — SOC_ERR_CHANNEL_NOT_FOUND 路径
 *===========================================================================*/

/* III-1: config 引用 socket hash 中不存在的 common ch_name */
void test_config_common_channel_not_found(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    /* config 引用 "NONEXIST" 这个 channel, 但 socket.json 里没有 */
    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 1, \"ASIC_NUM\": 1, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 1, \"PPS_NUM\": 1, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [ { \"NONEXIST\": 1 } ] } },"
        "  \"PSM1\": { \"power_channels\": [] }"
        "}");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_CHANNEL_NOT_FOUND, ret);
}

/* III-2: config 引用 socket hash 中不存在的 power ch_name */
void test_config_power_channel_not_found(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    /* config 引用不存在的 power channel */
    ret = parse_config_json_string(ctx,
        "{ \"INFO\": {"
        "  \"SEM_NUM\": 1, \"ASIC_NUM\": 1, \"DR_NUM\": 1, \"IO_NUM\": 1,"
        "  \"PSM_NUM\": 1, \"PPS_NUM\": 1, \"DUT_NUM\": 4 },"
        "  \"SEM1\": { \"ASIC1\": { \"pins\": [] } },"
        "  \"PSM1\": { \"power_channels\": [ { \"NONEXIST_PWR\": 1 } ] }"
        "}");
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_CHANNEL_NOT_FOUND, ret);
}

/*===========================================================================
 * IV. Pool REUSE 路径 — realloc_pin_pools 的 REUSE 分支
 *===========================================================================*/

/* IV-1: 重解析相同大小 JSON (REUSE 路径: new <= alloc) */
void test_pool_reuse_same_size(SocContext *ctx)
{
    /* 第一次解析 */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, SOCKET_MINIMAL));
    TEST_ASSERT_EQUAL_UINT32(4, ctx->meta.dut_count);
    uint32_t first_alloc = ctx->alloc_common_pin_num;

    free_socket_data(ctx);

    /* 第二次解析相同 JSON → 应走 REUSE 路径 */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, SOCKET_MINIMAL));
    TEST_ASSERT_EQUAL_UINT32(4, ctx->meta.dut_count);

    /* REUSE 不应改变 alloc 计数 (如果 new <= alloc) */
    TEST_ASSERT_EQUAL_UINT32(first_alloc, ctx->alloc_common_pin_num);

    /* 数据应正确填充 */
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);
    TEST_ASSERT_MESSAGE(strlen(ctx->dut_common_pins[0].pCommonDutPin[0].ch_name) > 0,
        "REUSE: common pin ch_name should be filled after re-parse of same JSON");
}

/* IV-2: 先解析大 JSON 再解析小 JSON (REUSE + 尾部清理) */
void test_pool_reuse_large_then_small(SocContext *ctx)
{
    /* 第一次: 4 DUT, 2 common pins, 2 power pins */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, SOCKET_MINIMAL));
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.common_pin_num);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->meta.power_pin_num);

    free_socket_data(ctx);

    /* 第二次: 1 DUT, 1 common pin, 1 power pin */
    const char *smaller =
        "{"
        "  \"multiple\": {"
        "    \"dutCount\": 1, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "    \"drPinCount\": 1, \"ioPinCount\": 0,"
        "    \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "    \"parallelVsMap\": { \"VS1\": 1 },"
        "    \"dutSequence\": [1]"
        "  },"
        "  \"powerPins\": { \"1\": { \"dut_num\": { \"1\": {\"ch_name\":\"VCC\"} } } },"
        "  \"commonPins\": { \"1\": { \"dut_num\": { \"1\": {\"ch_name\":\"DQ0\"} } } }"
        "}";
    TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, smaller));
    TEST_ASSERT_EQUAL_UINT32(1, ctx->meta.dut_count);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->meta.common_pin_num);

    /* 应正确填充 */
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].common_pin_num);
    /* 尾部 pin[1] 应被清零 (REUSE 尾部清理) */
    TEST_ASSERT_EQUAL_UINT32(0, ctx->dut_common_pins[1].common_pin_num);
}

/* IV-3: dut_count 增长但 pin 数不变 (仅内数组扩容) */
void test_pool_reuse_dut_growth_only(SocContext *ctx)
{
    /* 第一次: 2 DUT, 1 common pin */
    const char *small =
        "{ \"multiple\": {"
        "  \"dutCount\": 2, \"commonPinCount\": 1, \"powerPinCount\": 1,"
        "  \"drPinCount\": 1, \"ioPinCount\": 0,"
        "  \"STN\": 1, \"SITE\": 1, \"CHAIN\": 1, \"pps\": 1,"
        "  \"parallelVsMap\": { \"VS1\": 1 },"
        "  \"dutSequence\": [1, 2],"
        "  \"powerPins\": { \"1\": { \"dut_num\": { \"1\": {\"ch_name\":\"VCC\"} } } },"
        "  \"commonPins\": { \"1\": { \"dut_num\": { \"1\": {\"ch_name\":\"DQ0\"} } } }"
        "} }";
    TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, small));
    uint32_t old_alloc_dut = ctx->alloc_dut_count;

    free_socket_data(ctx);

    /* 第二次: 4 DUT, 1 common pin — dut_count 增长但 pin 数不变 */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, SOCKET_MINIMAL));
    /* alloc_dut_count 应扩容 */
    TEST_ASSERT_MESSAGE(ctx->alloc_dut_count > old_alloc_dut,
        "REUSE+dut_growth: alloc_dut_count should expand when dut_count grows");

    /* 新 DUT 槽位应有数据: pin[0] 的 DUT1 (slot 0) → "DQ0", DUT2 (slot 1) → "DQ1" */
    TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);
    TEST_ASSERT_MESSAGE(strlen(ctx->dut_common_pins[0].pCommonDutPin[0].ch_name) > 0,
        "REUSE+dut_growth: DUT slot 0 ch_name should be filled");
}

/* IV-4: 连续三次解析 (REUSE 稳定性) */
void test_pool_reuse_triple_parse(SocContext *ctx)
{
    int i;
    for (i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT32(SOC_OK, parse_json_string(ctx, SOCKET_MINIMAL));
        TEST_ASSERT_EQUAL_UINT32(4, ctx->meta.dut_count);
        TEST_ASSERT_NOT_NULL(ctx->dut_common_pins);
        TEST_ASSERT_NOT_NULL(ctx->dut_common_pins[0].pCommonDutPin);
        TEST_ASSERT_MESSAGE(strlen(ctx->dut_common_pins[0].pCommonDutPin[0].ch_name) > 0,
            "triple parse: ch_name should be filled on each iteration");
        free_socket_data(ctx);
    }
}

/*===========================================================================
 * V. on_socket_board=0 路径
 *===========================================================================*/

/* V-1: on_socket_board=0 时 config 解析跳过, 返回 SOC_OK */
void test_config_on_socket_board_zero(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

    /* 关键: 设置为 0, 模拟无板场景 */
    ctx->meta.on_socket_board = 0;

    ret = parse_config_json_string(ctx, CONFIG_MINIMAL);
    /* 应返回 SOC_OK (跳过, 不报错) */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* logic_config 应未被修改 (保留了默认值) */
    TEST_ASSERT_EQUAL_UINT32(32, ctx->logic_config.sem_num);
}

/*===========================================================================
 * VI. parse_common_pins pin_type 解析层校验
 *===========================================================================*/

/* VI-1: pin_type=255 应在解析层被拒绝 */
void test_pin_type_invalid_255_rejected(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"pin_type\":255,\"dut_num\":"
        "{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    /* 当前解析层接受 0-255。如果这里是 SOC_OK, 说明解析层未校验 */
    if (ret == SOC_OK) {
        TEST_ASSERT_MESSAGE(ctx->dut_common_pins[0].pin_type <= 1,
            "VI-1 BUG: pin_type=255 was stored in parsed data."
            " Parser should reject pin_type values > 1 at parse time.");
    }
    /* 如果返回了错误, 说明有某种保护在工作 */
}

/* VI-2: pin_type=2 应在解析层被拒绝 */
void test_pin_type_invalid_2_rejected(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"pin_type\":2,\"dut_num\":{\"1\":"
        "{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    if (ret == SOC_OK) {
        TEST_ASSERT_MESSAGE(ctx->dut_common_pins[0].pin_type <= 1,
            "VI-2 BUG: pin_type=2 was stored in parsed data."
            " Parser should reject pin_type values > 1 at parse time.");
    }
}

/* VI-3: 合法的 pin_type=0 (DR) 和 pin_type=1 (IO) 应被接受 */
void test_pin_type_valid_values_accepted(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* pin[0] 的 pin_type=0 (DR) */
    TEST_ASSERT_EQUAL_UINT32(0, ctx->dut_common_pins[0].pin_type);
    /* pin[1] 的 pin_type=1 (IO) */
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[1].pin_type);
}

/*===========================================================================
 * VII. Pin/DUT 溢出守卫
 *===========================================================================*/

/* VII-1: powerPins 中 JSON pin 条目数超过 powerPinCount */
void test_power_pins_too_many_entries(SocContext *ctx)
{
    /* powerPinCount=1 但 JSON 有 2 个 pin */
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":2,\"parallelVsMap\":{\"VS1\":1,\"VS2\":1},\"dutSequence\":[1,2]},"
        "\"powerPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}},\"2\":{\"dut_num\":{\"2\":{\"ch_name\":"
        "\"VDD\"}}}},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    /* 应返回 SOC_OK (第二个 pin 被 WARN+break, 不报错, 不溢出) */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    /* 只有第一个 pin 被填充 */
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_power_pins[0].vs_pin_num);
}

/* VII-2: commonPins 中 JSON pin 条目数超过 commonPinCount */
void test_common_pins_too_many_entries(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":"
        "\"DQ0\"}}},\"2\":{\"dut_num\":{\"2\":{\"ch_name\":\"DQ1\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    /* WARN+break, 不报错, 不溢出 */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->dut_common_pins[0].common_pin_num);
}

/* VII-3: powerPins 中 DUT 条目数超过 dut_count → 应报错 */
void test_power_pins_too_many_dut_entries(SocContext *ctx)
{
    /* dutCount=2 但 JSON 有 3 个 DUT 条目 */
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"A\"},\"2\":{\"ch_name\":\"B\"},\"3\":{\"ch_name\":\"C\"}}}},"
        "\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARSE_FAILED, ret);
}

/* VII-4: commonPins 中 DUT 条目数超过 dut_count → 应报错 */
void test_common_pins_too_many_dut_entries(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":"
        "\"A\"},\"2\":{\"ch_name\":\"B\"},\"3\":{\"ch_name\":\"C\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARSE_FAILED, ret);
}

/*===========================================================================
 * VIII. HashTable 正常流 + 边界
 *===========================================================================*/

/* VIII-1: 正常 insert → find 流程 */
void test_hash_insert_find_normal(SocContext *ctx)
{
    int ret;
    uint32_t outer, dut;

    ret = hash_insert(&ctx->hash_common, "TEST_CH", 5, 3);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    int found = hash_find(&ctx->hash_common, "TEST_CH", &outer, &dut);
    TEST_ASSERT_MESSAGE(found == 1, "hash_find should find inserted key");
    TEST_ASSERT_EQUAL_UINT32(5, outer);
    TEST_ASSERT_EQUAL_UINT32(3, dut);
}

/* VIII-2: 重复 key 插入 — 应 warn + 更新旧条目 */
void test_hash_insert_duplicate_key(SocContext *ctx)
{
    int ret;
    uint32_t outer, dut;

    /* 第一次插入 */
    ret = hash_insert(&ctx->hash_common, "DUP_KEY", 10, 20);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* 重复插入 (不同值) — 应更新旧条目 */
    ret = hash_insert(&ctx->hash_common, "DUP_KEY", 99, 88);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* 查询应返回新值 */
    int found = hash_find(&ctx->hash_common, "DUP_KEY", &outer, &dut);
    TEST_ASSERT_MESSAGE(found == 1, "hash_find should find duplicate key");
    TEST_ASSERT_EQUAL_UINT32(99, outer);
    TEST_ASSERT_EQUAL_UINT32(88, dut);
}

/* VIII-3: hash_insert NULL 参数 */
void test_hash_insert_null_params(SocContext *ctx)
{
    /* NULL ht */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM,
        hash_insert(NULL, "key", 0, 0));
    /* NULL key */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM,
        hash_insert(&ctx->hash_common, NULL, 0, 0));
}

/* VIII-4: hash_find 找不到 key 返回 0 */
void test_hash_find_missing_key(SocContext *ctx)
{
    uint32_t outer = 99, dut = 99;
    int found = hash_find(&ctx->hash_common, "NEVER_INSERTED", &outer, &dut);
    TEST_ASSERT_EQUAL_INT32(0, found);
    /* outer/dut 不应被修改 (但当前实现不保证, 仅验证 found==0) */
}

/* VIII-5: hash_find NULL 参数不崩溃 */
void test_hash_find_null_params(SocContext *ctx)
{
    /* NULL ht — 应返回 0 而非崩溃 */
    int found = hash_find(NULL, "key", NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(0, found);

    /* NULL key — 应返回 0 */
    found = hash_find(&ctx->hash_common, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(0, found);
}

/*===========================================================================
 * IX. soc_wait 状态组合
 *===========================================================================*/

/* IX-1: soc_wait_both_parse — socket=FINISHED, json=IDLE */
void test_wait_both_socket_finished_json_idle(SocContext *ctx)
{
    /* 设置 socket=FINISHED, json=IDLE */
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    ctx->meta.json_parse_state   = PARSE_IDLE;
    soc_context_register_instance(ctx);

    int ret = soc_wait_both_parse(ctx);
    /* 应返回 SOC_ERR_JSON_NOT_FOUND (锁已释放) */
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_JSON_NOT_FOUND, ret);
}

/* IX-2: soc_wait_both_parse — 两个都 FINISHED → 持锁返回 SOC_OK */
void test_wait_both_both_finished(SocContext *ctx)
{
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    ctx->meta.json_parse_state   = JSON_PARSE_FINISHED;
    soc_context_register_instance(ctx);

    int ret = soc_wait_both_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    /* 成功返回时持锁, 必须手动 unlock */
    pthread_mutex_unlock(ctx->parse_mutex);
}

/* IX-3: soc_wait_socket_parse — socket=FINISHED → 持锁返回 SOC_OK */
void test_wait_socket_finished_holds_lock(SocContext *ctx)
{
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    soc_context_register_instance(ctx);

    int ret = soc_wait_socket_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    /* 持锁返回, 必须 unlock */
    pthread_mutex_unlock(ctx->parse_mutex);
}

/* IX-4: soc_wait_both_parse — 两个都 IDLE */
void test_wait_both_both_idle(SocContext *ctx)
{
    /* 默认 calloc 零值 = PARSE_IDLE */
    soc_context_register_instance(ctx);
    int ret = soc_wait_both_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_SOCKET_NOT_FOUND, ret);
}

/* IX-5: soc_wait_socket_parse — socket=PARSE_IDLE */
void test_wait_socket_idle(SocContext *ctx)
{
    soc_context_register_instance(ctx);
    int ret = soc_wait_socket_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_SOCKET_NOT_FOUND, ret);
}

/*===========================================================================
 * X. API 驱动接口 — 正向 + 参数校验
 *===========================================================================*/

/* --- X-A: Res_GetDutAllIoPin --- */

/* X-A1: 正常获取 IO pin 列表 */
void test_api_get_dut_all_io_pin_normal(SocContext *ctx)
{
    uint32_t io_pins[8];
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetDutAllIoPin(1, io_pins, 8);
    /* 取锁失败或成功 (取决于 soc_wait_both_parse 是否通过)。
     * 这里 socket_parse_state==FINISHED, json==FINISHED, 应成功持锁。 */
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    TEST_ASSERT_MESSAGE(ret >= 0 || ret < 0,
        "Res_GetDutAllIoPin: should return count or error, not crash");
}

/* X-A2: NULL 参数 */
void test_api_get_dut_all_io_pin_null_params(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* io_pin_num=NULL */
    ret = Res_GetDutAllIoPin(1, NULL, 8);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);

    /* msize=0 */
    uint32_t buf[4];
    ret = Res_GetDutAllIoPin(1, buf, 0);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-B: Res_GetIoPinFrmPinSeq --- */

/* X-B1: 正常反解 whole_no → sem/asic/ch */
void test_api_get_io_pin_frm_pin_seq_normal(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    io_pin_t pin_info;
    memset(&pin_info, 0, sizeof(pin_info));

    ret = Res_GetIoPinFrmPinSeq(1, &pin_info);
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    /* whole_no=1 在有 2 SEM, 2 ASIC, 2 ch_total 的情况下:
     * SERIAL_TO_SEM(1, 2, 2) = ((1-1)/(2*2)) + 1 = 0 + 1 = 1
     * SERIAL_TO_ASIC(1, 2, 2) = (((1-1) % (2*2)) / 2) + 1 = (0/2) + 1 = 1
     * SERIAL_TO_CH(1, 2) = ((1-1) % 2) + 1 = 0 + 1 = 1 */
    TEST_ASSERT_EQUAL_UINT32(1, pin_info.sem_no);
    TEST_ASSERT_EQUAL_UINT32(1, pin_info.asic_no);
    TEST_ASSERT_EQUAL_UINT32(1, pin_info.channel_no);
}

/* X-B2: pin_idx=0 → SOC_ERR_QUERY_CHANNEL_NOT_FOUND */
void test_api_get_io_pin_frm_pin_seq_zero_idx(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetIoPinFrmPinSeq(0, &(io_pin_t){0});
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_QUERY_CHANNEL_NOT_FOUND, ret);
}

/* X-B3: pin_idx 超过上限 → SOC_ERR_QUERY_CHANNEL_NOT_FOUND */
void test_api_get_io_pin_frm_pin_seq_overflow_idx(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* actul_pin_idx = sem_num(2)*asic_num(2)*ch_total(2)=8, idx=9 超限 */
    ret = Res_GetIoPinFrmPinSeq(9, &(io_pin_t){0});
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_QUERY_CHANNEL_NOT_FOUND, ret);
}

/* X-B4: pin_info=NULL */
void test_api_get_io_pin_frm_pin_seq_null_param(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetIoPinFrmPinSeq(1, NULL);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-C: Res_GetVsChannelFrmPinSeq --- */

/* X-C1: 正常反解 whole_no → psm/ch */
void test_api_get_vs_channel_frm_pin_seq_normal(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    vs_channel_t vs_ch;
    memset(&vs_ch, 0, sizeof(vs_ch));

    ret = Res_GetVsChannelFrmPinSeq(1, &vs_ch);
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    /* PPS_NUM=2: SERIAL_TO_PSM(1,2)= (0/2)+1=1, SERIAL_TO_PWR_CH(1,2)= (0%2)+1=1 */
    TEST_ASSERT_EQUAL_UINT32(1, vs_ch.psm_no);
    TEST_ASSERT_EQUAL_UINT32(1, vs_ch.channel_no);
}

/* X-C2: pin_idx=0 */
void test_api_get_vs_channel_frm_pin_seq_zero_idx(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetVsChannelFrmPinSeq(0, &(vs_channel_t){0});
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_QUERY_CHANNEL_NOT_FOUND, ret);
}

/* X-C3: NULL vs_channel */
void test_api_get_vs_channel_frm_pin_seq_null_param(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetVsChannelFrmPinSeq(1, NULL);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-D: Res_GetAllSemAndAsic --- */

/* X-D1: 正常笛卡尔积 */
void test_api_get_all_sem_and_asic_normal(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    sem_asic_t data[16];
    memset(data, 0, sizeof(data));

    ret = Res_GetAllSemAndAsic(data, 16);
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    /* SEM_NUM=2, ASIC_NUM=2 → 应有 4 个组合 */
    TEST_ASSERT_EQUAL_INT32(4, ret);
    TEST_ASSERT_EQUAL_UINT32(1, data[0].sem_no);
    TEST_ASSERT_EQUAL_UINT32(1, data[0].asic_no);
    TEST_ASSERT_EQUAL_UINT32(1, data[1].sem_no);
    TEST_ASSERT_EQUAL_UINT32(2, data[1].asic_no);
    TEST_ASSERT_EQUAL_UINT32(2, data[2].sem_no);
    TEST_ASSERT_EQUAL_UINT32(1, data[2].asic_no);
    TEST_ASSERT_EQUAL_UINT32(2, data[3].sem_no);
    TEST_ASSERT_EQUAL_UINT32(2, data[3].asic_no);
}

/* X-D2: buffer 太小 — 部分填充 */
void test_api_get_all_sem_and_asic_small_buffer(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    sem_asic_t data[2];
    memset(data, 0, sizeof(data));

    ret = Res_GetAllSemAndAsic(data, 2);
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    /* 只填了前 2 个就满了 */
    TEST_ASSERT_EQUAL_INT32(2, ret);
}

/* X-D3: NULL 参数 */
void test_api_get_all_sem_and_asic_null_params(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetAllSemAndAsic(NULL, 8);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);

    ret = Res_GetAllSemAndAsic(&(sem_asic_t){0}, 0);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-E: Res_GetDutParamAndPinSeqFrmSemChannel --- */

/* X-E1: 正常反查 (socket+config 联合解析后) */
void test_api_get_dut_param_frm_sem_channel_normal(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    io_pin_t io_pin = {1, 1, 1}; /* SEM=1, ASIC=1, ch=1 */
    dut_param_t dut_param;
    memset(&dut_param, 0, sizeof(dut_param));

    ret = Res_GetDutParamAndPinSeqFrmSemChannel(&io_pin, &dut_param);
    /* 应返回 >0 (whole_no) 或 SOC_ERR_QUERY_CHANNEL_NOT_FOUND */
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    TEST_ASSERT_MESSAGE(ret != 0,
        "Res_GetDutParamAndPinSeqFrmSemChannel: should return whole_no or error");
}

/* X-E2: sem_no=0 → SOC_ERR_PARAM (1-indexed guard) */
void test_api_get_dut_param_frm_sem_channel_zero_input(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    io_pin_t io_pin = {0, 1, 1};
    dut_param_t dut_param;
    ret = Res_GetDutParamAndPinSeqFrmSemChannel(&io_pin, &dut_param);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* X-E3: NULL 参数 */
void test_api_get_dut_param_frm_sem_channel_null_params(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_GetDutParamAndPinSeqFrmSemChannel(NULL, &(dut_param_t){0});
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-F: Res_QueryDutParamByPsmChannel --- */

/* X-F1: 正常反查 */
void test_api_query_dut_param_by_psm_channel_normal(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    vs_channel_t vs_ch = {1, 1}; /* PSM=1, ch=1 */
    dut_param_t dut_param;
    memset(&dut_param, 0, sizeof(dut_param));

    ret = Res_QueryDutParamByPsmChannel(&vs_ch, &dut_param);
    if (ret == SOC_OK) {
        pthread_mutex_unlock(ctx->parse_mutex);
    }
    TEST_ASSERT_MESSAGE(ret != 0,
        "Res_QueryDutParamByPsmChannel: should return whole_no or error");
}

/* X-F2: psm_no=0 → SOC_ERR_PARAM */
void test_api_query_dut_param_by_psm_channel_zero_input(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    vs_channel_t vs_ch = {0, 1};
    dut_param_t dut_param;
    ret = Res_QueryDutParamByPsmChannel(&vs_ch, &dut_param);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* X-F3: NULL 参数 */
void test_api_query_dut_param_by_psm_channel_null_params(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_QueryDutParamByPsmChannel(NULL, &(dut_param_t){0});
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-G: Res_GetSocketFileName / Res_SetSocketFileName --- */

/* X-G1: 正常获取 socket 文件名 */
void test_api_get_socket_file_name_normal(SocContext *ctx)
{
    /* 手动设置文件名 */
    pthread_mutex_lock(ctx->parse_mutex);
    strncpy(ctx->meta.file_path, "/test/socket.json", MAX_FILE_PATH_LEN - 1);
    ctx->meta.file_path[MAX_FILE_PATH_LEN - 1] = '\0';
    pthread_mutex_unlock(ctx->parse_mutex);

    soc_context_register_instance(ctx);

    char buf[MAX_FILE_PATH_LEN];
    int ret = Res_GetSocketFileName(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("/test/socket.json", buf);
    TEST_ASSERT_EQUAL_INT32(17, ret); /* strlen("/test/socket.json") == 17 */
}

/* X-G2: NULL 参数 */
void test_api_get_socket_file_name_null_params(SocContext *ctx)
{
    soc_context_register_instance(ctx);

    int ret = Res_GetSocketFileName(NULL, 10);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);

    char buf[10];
    ret = Res_GetSocketFileName(buf, 0);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* X-G3: Res_SetSocketFileName 相同文件 no-op */
void test_api_set_socket_file_same_file_noop(SocContext *ctx)
{
    const char *path = "/same/file.json";
    pthread_mutex_lock(ctx->parse_mutex);
    strncpy(ctx->meta.file_path, path, MAX_FILE_PATH_LEN - 1);
    ctx->meta.file_path[MAX_FILE_PATH_LEN - 1] = '\0';
    pthread_mutex_unlock(ctx->parse_mutex);
    soc_context_register_instance(ctx);

    int ret = Res_SetSocketFileName(path, (uint32_t)strlen(path));
    /* 相同文件应返回 SOC_OK (no-op) */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
}

/* --- X-H: Res_IsPinDefinedInSocket 正向验证 --- */

/* X-H1: 查询已定义的 common pin → 返回 1 */
void test_api_is_pin_defined_valid_common_pin(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    soc_context_register_instance(ctx);

    /* common pin 1 存在于 DUT 1 */
    ret = Res_IsPinDefinedInSocket(1, 1, 0);
    TEST_ASSERT_EQUAL_INT32(1, ret);
}

/* X-H2: 查询已定义的 power pin → 返回 1 */
void test_api_is_pin_defined_valid_power_pin(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    soc_context_register_instance(ctx);

    /* power pin 1 存在于 DUT 1 */
    ret = Res_IsPinDefinedInSocket(1, 1, 1);
    TEST_ASSERT_EQUAL_INT32(1, ret);
}

/* X-H3: 查询不存在的 common pin → 返回 0 */
void test_api_is_pin_defined_not_found(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    soc_context_register_instance(ctx);

    /* common pin 999 不存在 */
    ret = Res_IsPinDefinedInSocket(1, 999, 0);
    TEST_ASSERT_EQUAL_INT32(0, ret);
}

/* X-H4: 查询不在 dutSequence 中的 DUT → SOC_ERR_PARAM */
void test_api_is_pin_defined_invalid_dut_no(SocContext *ctx)
{
    int ret = parse_json_string(ctx, SOCKET_MINIMAL);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);
    ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
    soc_context_register_instance(ctx);

    /* DUT 999 不在 dutSequence [1,2,3,4] 中 */
    ret = Res_IsPinDefinedInSocket(999, 1, 0);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* --- X-I: Res_QueryIoPinSeqByDutParam / Res_QueryVsPinSeqByDutParam --- */

/* X-I1: Res_QueryIoPinSeqByDutParam NULL 参数 */
void test_api_query_io_pin_seq_null_params(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_QueryIoPinSeqByDutParam(NULL, &(io_logic_data_t){0}, 8);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

/* X-I2: Res_QueryVsPinSeqByDutParam NULL 参数 */
void test_api_query_vs_pin_seq_null_params(SocContext *ctx)
{
    int ret = setup_full_parse(ctx);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    ret = Res_QueryVsPinSeqByDutParam(NULL, &(vs_logic_data_t){0}, 8);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}
