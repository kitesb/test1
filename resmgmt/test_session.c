/*
 * test_session.c — Session 模块独立测试
 *
 * 直接 include session.h 测试内部函数，验证：
 *   - session_init() 参数校验
 *   - 未初始化时的错误返回
 *   - func_key 数组读写
 *   - program_name / test_name 读写
 *   - site_no / site_nos 读写
 *
 * Build: cd resmgmt && gcc -o test_session \
 *          -I ResMgmt/API/inc -I ResMgmt/res_resource/inc -I ResMgmt/inc \
 *          test_session.c ResMgmt/res_resource/src/session.c \
 *          -Wall -Wextra -std=c99
 * Run:   ./test_session
 */

#include "session.h"

#include <stdio.h>
#include <string.h>

/*===========================================================================
 * 测试框架（轻量）
 *===========================================================================*/

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name)  printf("  [TEST] %-50s ", name)
#define PASS()      do { printf("PASS\n"); g_passed++; } while (0)
#define FAIL(fmt, ...) do { printf("FAIL - " fmt "\n", ##__VA_ARGS__); g_failed++; } while (0)
#define CHECK(expr) do { if (!(expr)) { FAIL("%s", #expr); return; } } while (0)

/*===========================================================================
 * T1-T2: session_init 参数校验
 *===========================================================================*/

static void test_init_null(void)
{
    TEST("T1: session_init(NULL) returns SESSION_ERR_PARAM");
    CHECK(session_init(NULL) == SESSION_ERR_PARAM);
    PASS();
}

static void test_init_ok(void)
{
    TEST("T2: session_init() with valid param returns OK");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    CHECK(session_init(&p) == SESSION_OK);
    CHECK(session_is_init() == true);
    PASS();
}

/*===========================================================================
 * T3-T6: func_key
 *===========================================================================*/

static void test_func_key_basic(void)
{
    TEST("T3: func_key with 3 values returns 3");
    uint32_t keys[] = {1, 2, 3};
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.func_keys = keys;
    p.func_key_count = 3;

    CHECK(session_init(&p) == SESSION_OK);

    uint32_t out[4] = {0};
    int32_t ret = session_get_func_key(out, 4);
    CHECK(ret == 3);
    CHECK(out[0] == 1);
    CHECK(out[1] == 2);
    CHECK(out[2] == 3);
    PASS();
}

static void test_func_key_empty(void)
{
    TEST("T4: empty func_key returns 0");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.func_key_count = 0;

    CHECK(session_init(&p) == SESSION_OK);

    int32_t ret = session_get_func_key(NULL, 0);
    CHECK(ret == 0);
    PASS();
}

static void test_func_key_msize_zero(void)
{
    TEST("T5: msize=0 returns count without writing");
    uint32_t keys[] = {10, 20};
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.func_keys = keys;
    p.func_key_count = 2;

    CHECK(session_init(&p) == SESSION_OK);

    int32_t ret = session_get_func_key(NULL, 0);
    CHECK(ret == 2);
    PASS();
}

static void test_func_key_truncate(void)
{
    TEST("T6: msize < count copies msize, returns count");
    uint32_t keys[] = {5, 6, 7};
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.func_keys = keys;
    p.func_key_count = 3;

    CHECK(session_init(&p) == SESSION_OK);

    uint32_t out[2] = {0, 0};
    int32_t ret = session_get_func_key(out, 2);
    CHECK(ret == 3);         /* 返回实际个数 */
    CHECK(out[0] == 5);
    CHECK(out[1] == 6);
    PASS();
}

/*===========================================================================
 * T7-T9: program_name
 *===========================================================================*/

static void test_program_name_basic(void)
{
    TEST("T7: program_name 'FT_V1' read back correctly");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.program_name = "FT_V1";

    CHECK(session_init(&p) == SESSION_OK);

    char buf[RES_MAX_PROGRAM_NAME_LEN] = {0};
    CHECK(session_get_program_name(buf, sizeof(buf)) == SESSION_OK);
    CHECK(strcmp(buf, "FT_V1") == 0);
    PASS();
}

static void test_program_name_empty(void)
{
    TEST("T8: no program_name returns empty string");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));

    CHECK(session_init(&p) == SESSION_OK);

    char buf[RES_MAX_PROGRAM_NAME_LEN];
    memset(buf, 0xFF, sizeof(buf));
    CHECK(session_get_program_name(buf, sizeof(buf)) == SESSION_OK);
    CHECK(buf[0] == '\0');
    PASS();
}

static void test_program_name_null_buf(void)
{
    TEST("T9: NULL buf with max_len > 0 returns SESSION_ERR_PARAM");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));

    CHECK(session_init(&p) == SESSION_OK);

    CHECK(session_get_program_name(NULL, 10) == SESSION_ERR_PARAM);
    PASS();
}

/*===========================================================================
 * T10-T11: test_name
 *===========================================================================*/

static void test_test_name_basic(void)
{
    TEST("T10: test_name 'continuity' read back correctly");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.test_name = "continuity";

    CHECK(session_init(&p) == SESSION_OK);

    char buf[RES_MAX_TEST_NAME_LEN] = {0};
    CHECK(session_get_test_name(buf, sizeof(buf)) == SESSION_OK);
    CHECK(strcmp(buf, "continuity") == 0);
    PASS();
}

static void test_test_name_empty(void)
{
    TEST("T11: no test_name returns empty string");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));

    CHECK(session_init(&p) == SESSION_OK);

    char buf[RES_MAX_TEST_NAME_LEN];
    memset(buf, 0xFF, sizeof(buf));
    CHECK(session_get_test_name(buf, sizeof(buf)) == SESSION_OK);
    CHECK(buf[0] == '\0');
    PASS();
}

/*===========================================================================
 * T12-T14: site_no / site_nos
 *===========================================================================*/

static void test_site_no_basic(void)
{
    TEST("T12: site_no=1, QuerySiteNo returns 1");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.site_no = 1;

    CHECK(session_init(&p) == SESSION_OK);

    CHECK(session_query_site_no() == 1);
    PASS();
}

static void test_site_nos_array(void)
{
    TEST("T13: site_nos array returns count=1 with site_nos[0]=5");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));
    p.site_no = 5;

    CHECK(session_init(&p) == SESSION_OK);

    uint32_t snos[2] = {0, 0};
    int32_t ret = session_query_site_nos(snos, 2);
    CHECK(ret == 1);
    CHECK(snos[0] == 5);
    PASS();
}

static void test_site_no_default(void)
{
    TEST("T14: no --siteNo defaults to 0");
    SessionInitParam p;
    memset(&p, 0, sizeof(p));

    CHECK(session_init(&p) == SESSION_OK);

    CHECK(session_query_site_no() == 0);
    PASS();
}

/*===========================================================================
 * main
 *===========================================================================*/

int main(void)
{
    printf("\n=== Session Module Tests ===\n\n");

    printf("[init]\n");
    test_init_null();
    test_init_ok();

    printf("\n[func_key]\n");
    test_func_key_basic();
    test_func_key_empty();
    test_func_key_msize_zero();
    test_func_key_truncate();

    printf("\n[program_name]\n");
    test_program_name_basic();
    test_program_name_empty();
    test_program_name_null_buf();

    printf("\n[test_name]\n");
    test_test_name_basic();
    test_test_name_empty();

    printf("\n[site_no]\n");
    test_site_no_basic();
    test_site_nos_array();
    test_site_no_default();

    printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
