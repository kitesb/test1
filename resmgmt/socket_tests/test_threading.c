/*===========================================================================
 * G. 线程安全与生命周期测试（7 用例）
 *
 * 覆盖:
 *   T1 — soc_context_create → soc_context_destroy（无工作线程），验证 C1 修复
 *   T2 — socket_json_parse_init(NULL) → socket_json_parse_stop()（线程启停）
 *   T3 — soc_context_destroy 自动停止工作线程
 *   T4 — 双重销毁安全性
 *   T5 — soc_wait_socket_parse IDLE 路径（worker 未启动）
 *   T6 — 并发 soc_context_get_instance 无竞态
 *   T7 — 销毁后 singleton 正确清除
 *
 * 注: 由于需要外部 socket_compiler 二进制, pipeline_run_cycle 的
 *     CYCLE_OK / CYCLE_RESTART / CYCLE_ERROR 端到端路径不在本测试覆盖范围内.
 *     这些路径的竞态安全性由代码审查 + TSan（步骤 5）保障.
 *===========================================================================*/
#include "test_harness.h"
#include "test_helpers.h"
#include "soc_context.h"
#include <pthread.h>
#include <unistd.h>

/* =================================================================
 * T1. 无工作线程的 create→destroy 生命周期
 * 验证 C1 修复: soc_context_destroy 在 work_thread==0 时不崩溃
 * ================================================================= */
void test_context_lifecycle_no_thread(void)
{
    SocContext *ctx = soc_context_create(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_NOT_NULL(ctx->extra);
    TEST_ASSERT_NOT_NULL(ctx->parse_mutex);
    TEST_ASSERT_NOT_NULL(ctx->parse_cond);

    /* 销毁不应崩溃（work_thread 未启动） */
    soc_context_destroy(ctx);

    /* 二次销毁应安全处理（ctx 为 NULL 但调用者为 dangling pointer，不测试） */
}

/* =================================================================
 * T2. 线程启停: init(NULL) → stop()
 * 验证工作线程可正常创建和停止
 * ================================================================= */
void test_thread_start_stop(void)
{
    int ret;

    ret = socket_json_parse_init(NULL);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    /* 验证 singleton 存在且 mutex 可用 */
    SocContext *ctx = soc_context_get_instance();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 线程应已启动 */
    TEST_ASSERT_MESSAGE(ctx->work_thread != 0,
        "work_thread should be non-zero after init");

    /* 初始状态应为 IDLE（未触发解析） */
    TEST_ASSERT_EQUAL_INT(PARSE_IDLE, ctx->meta.socket_parse_state);

    ret = socket_json_parse_stop();
    TEST_ASSERT_EQUAL_INT(0, ret);

    /* stop 后清理 singleton（模拟 stop 后的 destroy 流程） */
    soc_context_destroy(ctx);
}

/* =================================================================
 * T3. soc_context_destroy 自动停止工作线程
 * 验证 destroy 内部 stop_flag → broadcast → pthread_join 流程
 * ================================================================= */
void test_destroy_stops_thread(void)
{
    int ret;

    ret = socket_json_parse_init(NULL);
    TEST_ASSERT_EQUAL_INT(SOC_OK, ret);

    SocContext *ctx = soc_context_get_instance();
    TEST_ASSERT_NOT_NULL(ctx);

    /* 直接 destroy（不预先 stop），应自动停止线程 */
    soc_context_destroy(ctx);

    /* 销毁后 singleton 应为 NULL */
    TEST_ASSERT_NULL(soc_context_get_instance());
}

/* =================================================================
 * T4. 双重 soc_context_destroy 安全（singleton 已清除）
 * ================================================================= */
void test_double_destroy_safety(void)
{
    SocContext *ctx = soc_context_create(NULL);
    TEST_ASSERT_NOT_NULL(ctx);

    soc_context_destroy(ctx);
    /* 第二次 destroy(NULL) 应安全（NULL guard） */
    soc_context_destroy(NULL);

    /* 验证 singleton 已清除 */
    TEST_ASSERT_NULL(soc_context_get_instance());
}

/* =================================================================
 * T5. soc_wait_socket_parse IDLE 路径
 * 当 parse_state 为 IDLE 时应返回 SOC_ERR_SOCKET_NOT_FOUND
 * ================================================================= */
void test_wait_socket_parse_idle(void)
{
    SocContext *ctx = soc_context_create(NULL);
    TEST_ASSERT_NOT_NULL(ctx);

    /* 初始状态为 IDLE, wait 应立即返回错误（不阻塞） */
    int ret = soc_wait_socket_parse(ctx);
    TEST_ASSERT_EQUAL_INT(SOC_ERR_SOCKET_NOT_FOUND, ret);
    /* 错误返回时锁已释放, 不需要 unlock */

    soc_context_destroy(ctx);
}

/* =================================================================
 * T6. 并发 soc_context_get_instance（无 worker 线程）
 * 多线程同时获取 singleton 指针应安全（只读操作）
 * ================================================================= */

#define CONCURRENT_READERS 8
#define CONCURRENT_ITERATIONS 1000

static void *concurrent_get_instance(void *arg)
{
    (void)arg;
    uint32_t i;
    for (i = 0; i < CONCURRENT_ITERATIONS; i++) {
        SocContext *ctx = soc_context_get_instance();
        /* ctx 可能为 NULL（before create / after destroy），但不崩溃 */
        if (ctx != NULL) {
            /* 验证 mutex 可正常 lock/unlock */
            pthread_mutex_lock(ctx->parse_mutex);
            pthread_mutex_unlock(ctx->parse_mutex);
        }
    }
    return NULL;
}

void test_concurrent_get_instance(void)
{
    SocContext *ctx = soc_context_create(NULL);
    TEST_ASSERT_NOT_NULL(ctx);

    pthread_t readers[CONCURRENT_READERS];
    int i;

    for (i = 0; i < CONCURRENT_READERS; i++) {
        int rc = pthread_create(&readers[i], NULL, concurrent_get_instance, NULL);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }

    for (i = 0; i < CONCURRENT_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    soc_context_destroy(ctx);
}

/* =================================================================
 * T7. 销毁后 singleton 正确清除
 * ================================================================= */
void test_singleton_cleared_after_destroy(void)
{
    SocContext *ctx = soc_context_create(NULL);
    TEST_ASSERT_NOT_NULL(ctx);
    soc_context_register_instance(ctx);
    TEST_ASSERT_EQUAL_PTR(ctx, soc_context_get_instance());

    soc_context_destroy(ctx);
    TEST_ASSERT_NULL(soc_context_get_instance());

    /* 可以重新创建 */
    SocContext *ctx2 = soc_context_create(NULL);
    TEST_ASSERT_NOT_NULL(ctx2);
    soc_context_register_instance(ctx2);
    TEST_ASSERT_EQUAL_PTR(ctx2, soc_context_get_instance());
    soc_context_destroy(ctx2);
}
