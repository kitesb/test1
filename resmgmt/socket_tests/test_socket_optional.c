/*===========================================================================
 * B. 可选节点测试（17 用例）— 签名: void test_xxx(SocContext *ctx)
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"

#define FULL_JSON    "test_fixtures/full.json"
#define MINIMAL_JSON "test_fixtures/minimal.json"

/* =================================================================
 * pinNames
 * ================================================================= */

void test_pinNames_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->pin_names);
    TEST_ASSERT_EQUAL_UINT32(10, ctx->extra->pin_name_count);
    TEST_ASSERT_EQUAL_STRING("VDD", ctx->extra->pin_names[0].pin_name);
    TEST_ASSERT_EQUAL_STRING("P0001", ctx->extra->pin_names[0].pin_num);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->extra->pin_names[0].dut_pin_num);
}

void test_pinNames_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->pin_names);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->pin_name_count);
}

/* =================================================================
 * symbol
 * ================================================================= */

void test_symbol_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->symbols);
    TEST_ASSERT_EQUAL_UINT32(3, ctx->extra->symbol_count);
    TEST_ASSERT_EQUAL_STRING("VDD", ctx->extra->symbols[0].pin_name);
    TEST_ASSERT_EQUAL_STRING("VS1", ctx->extra->symbols[0].vs_name);
}

void test_symbol_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->symbols);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->symbol_count);
}

/* =================================================================
 * vsDefinitions
 * ================================================================= */

void test_vsDefinitions_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->vs_defs);
    TEST_ASSERT_EQUAL_UINT32(3, ctx->extra->vs_def_count);
    TEST_ASSERT_EQUAL_STRING("VS1", ctx->extra->vs_defs[0].name);
}

void test_vsDefinitions_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->vs_defs);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->vs_def_count);
}

/* =================================================================
 * vsSiteDefinitions
 * ================================================================= */

void test_vsSiteDefinitions_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->vs_site_defs);
    TEST_ASSERT_MESSAGE(ctx->extra->vs_site_def_count > 0, "expected vs_site_def_count > 0");
}

void test_vsSiteDefinitions_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->vs_site_defs);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->vs_site_def_count);
}

/* =================================================================
 * hvppsDefinitions
 * ================================================================= */

void test_hvppsDefinitions_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->hvpps_defs);
    TEST_ASSERT_MESSAGE(ctx->extra->hvpps_def_count > 0, "expected hvpps_def_count > 0");
}

void test_hvppsDefinitions_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->hvpps_defs);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->hvpps_def_count);
}

/* =================================================================
 * hvppsSiteDefinitions
 * ================================================================= */

void test_hvppsSiteDefinitions_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->hvpps_site_defs);
    TEST_ASSERT_MESSAGE(ctx->extra->hvpps_site_def_count > 0, "expected hvpps_site_def_count > 0");
}

void test_hvppsSiteDefinitions_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->hvpps_site_defs);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->hvpps_site_def_count);
}

/* =================================================================
 * separateEntities
 * ================================================================= */

void test_separateEntities_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->entities);
    TEST_ASSERT_EQUAL_UINT32(3, ctx->extra->entity_count);
    TEST_ASSERT_EQUAL_STRING("PPS", ctx->extra->entities[0].type);
    TEST_ASSERT_EQUAL_UINT32(88, ctx->extra->entities[0].number);
    TEST_ASSERT_EQUAL_UINT32(2,  ctx->extra->entities[0].mode);
}

void test_separateEntities_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->entities);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->entity_count);
}

/* =================================================================
 * systemDut
 * ================================================================= */

void test_systemDut_present(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->extra->system_duts);
    TEST_ASSERT_EQUAL_UINT32(2, ctx->extra->system_dut_count);
    TEST_ASSERT_EQUAL_UINT32(1, ctx->extra->system_duts[0].system_dut_num);
}

void test_systemDut_absent(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, MINIMAL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->system_duts);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->system_dut_count);
}

/* =================================================================
 * B17. 消重验证
 * ================================================================= */

void test_vs_hvpps_dedup(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra->vs_defs);
    TEST_ASSERT_NOT_NULL(ctx->extra->hvpps_defs);
    TEST_ASSERT_MESSAGE(ctx->extra->vs_def_count > 0, "vs_def_count should be > 0");
    TEST_ASSERT_MESSAGE(ctx->extra->hvpps_def_count > 0, "hvpps_def_count should be > 0");

    TEST_ASSERT_NOT_NULL(ctx->extra->vs_site_defs);
    TEST_ASSERT_NOT_NULL(ctx->extra->hvpps_site_defs);
    TEST_ASSERT_MESSAGE(ctx->extra->vs_site_def_count > 0, "vs_site_def_count should be > 0");
    TEST_ASSERT_MESSAGE(ctx->extra->hvpps_site_def_count > 0, "hvpps_site_def_count should be > 0");
}
