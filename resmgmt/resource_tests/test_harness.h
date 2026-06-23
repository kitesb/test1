/*===========================================================================
 * 最小测试框架 — Unity 风格 API（零依赖）
 *
 * 用于 DUT Group 资源层测试，不依赖 socket/JSON 子系统。
 *
 * 用法:
 *   #include "test_harness.h"
 *   void test_xxx(void) {
 *       TEST_ASSERT(1 + 1 == 2);
 *       TEST_ASSERT_EQUAL_UINT32(42, answer());
 *   }
 *   int main(void) { RUN_TEST_GROUP(...); }
 *===========================================================================*/
#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- 全局计数器（在 test_dut_group.c 中定义） --- */
extern int _tests_run;
extern int _tests_failed;
extern int _tests_ignored;
extern const char *_current_test;

/* --- 失败处理 --- */
#define TEST_FAIL(msg) do { \
	printf("  FAIL: %s:%d — %s — %s\n", __FILE__, __LINE__, _current_test, msg); \
	_tests_failed++; \
	return; \
} while (0)

/* --- 断言宏 --- */
#define TEST_ASSERT(cond) do { \
	if (!(cond)) TEST_FAIL("assertion failed: " #cond); \
} while (0)

#define TEST_ASSERT_EQUAL(expected, actual) do { \
	long long _e = (long long)(expected); \
	long long _a = (long long)(actual); \
	if (_e != _a) { \
		printf("  FAIL: %s:%d — %s — expected %lld, got %lld\n", \
		       __FILE__, __LINE__, _current_test, _e, _a); \
		_tests_failed++; return; \
	} \
} while (0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) do { \
	unsigned int _e = (unsigned int)(expected); \
	unsigned int _a = (unsigned int)(actual); \
	if (_e != _a) { \
		printf("  FAIL: %s:%d — %s — expected %u, got %u\n", \
		       __FILE__, __LINE__, _current_test, _e, _a); \
		_tests_failed++; return; \
	} \
} while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
	int _e = (int)(expected); \
	int _a = (int)(actual); \
	if (_e != _a) { \
		printf("  FAIL: %s:%d — %s — expected %d, got %d\n", \
		       __FILE__, __LINE__, _current_test, _e, _a); \
		_tests_failed++; return; \
	} \
} while (0)

#define TEST_ASSERT_EQUAL_INT32(expected, actual) TEST_ASSERT_EQUAL_INT(expected, actual)

#define TEST_ASSERT_NOT_NULL(ptr) do { \
	if ((ptr) == NULL) TEST_FAIL("expected non-NULL, got NULL"); \
} while (0)

#define TEST_ASSERT_NULL(ptr) do { \
	if ((ptr) != NULL) TEST_FAIL("expected NULL, got non-NULL"); \
} while (0)

#define TEST_ASSERT_EQUAL_PTR(expected, actual) do { \
	const void *_e = (const void *)(expected); \
	const void *_a = (const void *)(actual); \
	if (_e != _a) { \
		printf("  FAIL: %s:%d — %s — expected %p, got %p\n", \
		       __FILE__, __LINE__, _current_test, _e, _a); \
		_tests_failed++; return; \
	} \
} while (0)

/* --- Runner --- */
#define RUN_TEST(func) do { \
	_current_test = #func; \
	_tests_run++; \
	func(); \
} while (0)

#define UNITY_BEGIN() do { \
	_tests_run = 0; _tests_failed = 0; _tests_ignored = 0; \
	printf("\n========== Test Suite Start ==========\n"); \
} while (0)

#define UNITY_END() do { \
	printf("\n========== Test Suite End ==========\n"); \
	printf("  Total:  %d\n", _tests_run); \
	printf("  Passed: %d\n", _tests_run - _tests_failed - _tests_ignored); \
	printf("  Failed: %d\n", _tests_failed); \
	if (_tests_ignored) printf("  Ignored: %d\n", _tests_ignored); \
	printf("========================================\n"); \
} while (0)

static inline int unity_result(void) {
	return (_tests_failed > 0) ? 1 : 0;
}

#endif /* TEST_HARNESS_H */
