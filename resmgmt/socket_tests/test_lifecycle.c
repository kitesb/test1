/*===========================================================================
 * E. 生命周期 + 边界条件 + 错误路径（6 用例，E6 特殊无 ctx）
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"

#define FULL_JSON "test_fixtures/full.json"

/* --- E1. soc_context_create 后 extra 非 NULL --- */

void test_extra_not_null_after_create(SocContext *ctx)
{
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->vs_map);
    TEST_ASSERT_NULL(ctx->extra->entities);
    TEST_ASSERT_NULL(ctx->extra->dut_sequence);
    TEST_ASSERT_NULL(ctx->extra->pin_names);
    TEST_ASSERT_NULL(ctx->extra->symbols);
    TEST_ASSERT_NULL(ctx->extra->vs_defs);
    TEST_ASSERT_NULL(ctx->extra->vs_site_defs);
    TEST_ASSERT_NULL(ctx->extra->hvpps_defs);
    TEST_ASSERT_NULL(ctx->extra->hvpps_site_defs);
    TEST_ASSERT_NULL(ctx->extra->system_duts);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->entity_count);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->dut_seq_len);
}

/* --- E2. free_socket_data 后 extra 清零 --- */

void test_extra_zeroed_after_free(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra->vs_map);

    free_socket_data(ctx);

    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NULL(ctx->extra->vs_map);
    TEST_ASSERT_NULL(ctx->extra->entities);
    TEST_ASSERT_NULL(ctx->extra->dut_sequence);
    TEST_ASSERT_NULL(ctx->extra->pin_names);
    TEST_ASSERT_NULL(ctx->extra->symbols);
    TEST_ASSERT_NULL(ctx->extra->vs_defs);
    TEST_ASSERT_NULL(ctx->extra->vs_site_defs);
    TEST_ASSERT_NULL(ctx->extra->hvpps_defs);
    TEST_ASSERT_NULL(ctx->extra->hvpps_site_defs);
    TEST_ASSERT_NULL(ctx->extra->system_duts);
}

/* --- E3. 重复解析不泄漏 --- */

void test_double_parse_no_leak(SocContext *ctx)
{
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra->pin_names);
    TEST_ASSERT_EQUAL_UINT32(10, ctx->extra->pin_name_count);

    free_socket_data(ctx);
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra->pin_names);
    TEST_ASSERT_EQUAL_UINT32(10, ctx->extra->pin_name_count);

    free_socket_data(ctx);
    TEST_ASSERT_EQUAL_INT(SOC_OK, parse_socket_json(ctx, FULL_JSON));
    TEST_ASSERT_NOT_NULL(ctx->extra->pin_names);
}

/* --- E4. 文件不存在 --- */

void test_file_not_found(SocContext *ctx)
{
    int ret = parse_socket_json(ctx, "/tmp/_nonexistent_test_file_.json");
    TEST_ASSERT_EQUAL_INT(SOC_ERR_FILE_NOT_FOUND, ret);
}

/* --- E5. 非法 JSON --- */

void test_invalid_json(SocContext *ctx)
{
    const char *json = "this is not json {{{";
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_PARSE_FAILED, ret);
}

/* --- E6. extra=NULL 触发 SOC_ERR_INTERNAL（不通过 wrapper，手动管理 ctx）--- */

void test_extra_null_triggers_internal_error(void)
{
    SocContext *ctx = soc_context_create(NULL);
    if (ctx == NULL) {
        printf("  FAIL: test_extra_null_triggers_internal_error — create failed\n");
        _tests_failed++;
        return;
    }
    ctx->meta.on_socket_board = 1;

    free(ctx->extra);
    ctx->extra = NULL;

    int ret = parse_socket_json(ctx, FULL_JSON);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_INTERNAL, ret);

    /* 手动清理: 必须用 soc_context_destroy 才能清除 singleton */
    free_socket_data(ctx);
    soc_context_destroy(ctx);
}
