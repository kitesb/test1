/*===========================================================================
 * DUT Group — TDD test suite
 *
 * Covers: internal functions (dut_group_*) + API wrappers (Res_*)
 * Build:  bash build_test.sh
 *===========================================================================*/
#include "test_harness.h"
#include "ResAPI.h"
#include "dut_group.h"
#include <string.h>

int _tests_run       = 0;
int _tests_failed    = 0;
int _tests_ignored   = 0;
const char *_current_test = NULL;

extern Res_Manager g_res_mgr;

/*--------------------------------------------------------------------
 * Internal layer tests
 *--------------------------------------------------------------------*/

void test_dut_group_init_normal(void)
{
	int32_t ret = dut_group_init(512);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_OK, ret);
	TEST_ASSERT(g_res_mgr.initialized);
	TEST_ASSERT_EQUAL_UINT32(512, g_res_mgr.dut_group.num_duts);
	TEST_ASSERT(g_res_mgr.dut_group.alloc_duts > 512);
	TEST_ASSERT_NOT_NULL(g_res_mgr.dut_group.ddut);
	TEST_ASSERT_NOT_NULL(g_res_mgr.dut_group.sdut);
	TEST_ASSERT_NOT_NULL(g_res_mgr.dut_group.gfdut);
}

void test_dut_group_init_repeat(void)
{
	dut_group_init(256);
	void *old_ddut = g_res_mgr.dut_group.ddut;
	dut_group_init(512);
	TEST_ASSERT(old_ddut != g_res_mgr.dut_group.ddut);
	TEST_ASSERT_EQUAL_UINT32(512, g_res_mgr.dut_group.num_duts);
}

void test_dut_group_is_init_false_before_init(void)
{
	memset(&g_res_mgr, 0, sizeof(g_res_mgr));
	TEST_ASSERT(!dut_group_is_init());
}

void test_dut_group_is_init_true_after_init(void)
{
	dut_group_init(512);
	TEST_ASSERT(dut_group_is_init());
}

void test_dut_group_add_normal(void)
{
	uint32_t duts[] = {1, 5, 10};
	dut_group_init(512);
	dut_group_clear(DUT_GROUP_SDUT);
	int32_t ret = dut_group_add(DUT_GROUP_SDUT, duts, 3);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_OK, ret);

	uint32_t list[10];
	int32_t count = dut_group_get(DUT_GROUP_SDUT, list, 10);
	TEST_ASSERT_EQUAL_INT32(3, count);
	TEST_ASSERT_EQUAL_UINT32(1, list[0]);
	TEST_ASSERT_EQUAL_UINT32(5, list[1]);
	TEST_ASSERT_EQUAL_UINT32(10, list[2]);
}

void test_dut_group_add_out_of_range(void)
{
	uint32_t duts[] = {600};
	dut_group_init(512);
	int32_t ret = dut_group_add(DUT_GROUP_SDUT, duts, 1);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_ERR_DUT_NO, ret);
}

void test_dut_group_remove_normal(void)
{
	uint32_t duts[] = {1, 2, 3, 4, 5};
	dut_group_init(512);
	dut_group_clear(DUT_GROUP_SDUT);
	dut_group_add(DUT_GROUP_SDUT, duts, 5);
	dut_group_remove(DUT_GROUP_SDUT, duts, 2);

	uint32_t list[10];
	int32_t count = dut_group_get(DUT_GROUP_SDUT, list, 10);
	TEST_ASSERT_EQUAL_INT32(3, count);
	TEST_ASSERT_EQUAL_UINT32(3, list[0]);
}

void test_dut_group_remove_idempotent(void)
{
	uint32_t duts[] = {1};
	dut_group_init(512);
	dut_group_clear(DUT_GROUP_SDUT);
	int32_t ret = dut_group_remove(DUT_GROUP_SDUT, duts, 1);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_OK, ret);
}

void test_dut_group_clear(void)
{
	uint32_t duts[] = {1, 2, 3};
	dut_group_init(512);
	dut_group_add(DUT_GROUP_SDUT, duts, 3);
	dut_group_clear(DUT_GROUP_SDUT);

	int32_t count = dut_group_get(DUT_GROUP_SDUT, NULL, 0);
	TEST_ASSERT_EQUAL_INT32(0, count);
}

void test_dut_group_get_count_only(void)
{
	uint32_t duts[] = {10, 20, 30};
	dut_group_init(512);
	dut_group_clear(DUT_GROUP_SDUT);
	dut_group_add(DUT_GROUP_SDUT, duts, 3);
	int32_t count = dut_group_get(DUT_GROUP_SDUT, NULL, 0);
	TEST_ASSERT_EQUAL_INT32(3, count);
}

void test_dut_group_resize_expand(void)
{
	dut_group_init(256);
	uint32_t old_alloc = g_res_mgr.dut_group.alloc_duts;
	int32_t ret = dut_group_resize(400);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_OK, ret);
	TEST_ASSERT(g_res_mgr.dut_group.alloc_duts > old_alloc);
	TEST_ASSERT_EQUAL_UINT32(400, g_res_mgr.dut_group.num_duts);
}

void test_dut_group_resize_reuse(void)
{
	dut_group_init(512);
	uint32_t old_alloc = g_res_mgr.dut_group.alloc_duts;
	void *old_ddut = g_res_mgr.dut_group.ddut;
	int32_t ret = dut_group_resize(100);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_OK, ret);
	TEST_ASSERT_EQUAL_UINT32(old_alloc, g_res_mgr.dut_group.alloc_duts);
	TEST_ASSERT_EQUAL_PTR(old_ddut, g_res_mgr.dut_group.ddut);
	TEST_ASSERT_EQUAL_UINT32(100, g_res_mgr.dut_group.num_duts);
}

/*--------------------------------------------------------------------
 * API layer tests
 *--------------------------------------------------------------------*/

void test_api_dut_group_init(void)
{
	int32_t ret = Res_DutGroupInit();
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_OK, ret);
	TEST_ASSERT(dut_group_is_init());
}

void test_api_get_dut_group_invalid_type(void)
{
	Res_DutGroupInit();
	int32_t ret = Res_GetDutGroup(0, DUT_GROUP_MAX, NULL, 0);
	TEST_ASSERT_EQUAL_INT32(DUT_GROUP_ERR_GROUP, ret);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_dut_group_init_normal);
	RUN_TEST(test_dut_group_init_repeat);
	RUN_TEST(test_dut_group_is_init_false_before_init);
	RUN_TEST(test_dut_group_is_init_true_after_init);
	RUN_TEST(test_dut_group_add_normal);
	RUN_TEST(test_dut_group_add_out_of_range);
	RUN_TEST(test_dut_group_remove_normal);
	RUN_TEST(test_dut_group_remove_idempotent);
	RUN_TEST(test_dut_group_clear);
	RUN_TEST(test_dut_group_get_count_only);
	RUN_TEST(test_dut_group_resize_expand);
	RUN_TEST(test_dut_group_resize_reuse);
	RUN_TEST(test_api_dut_group_init);
	RUN_TEST(test_api_get_dut_group_invalid_type);

	UNITY_END();
	return unity_result();
}
