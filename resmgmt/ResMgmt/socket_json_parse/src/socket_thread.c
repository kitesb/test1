/*--- 模块头文件 ---*/
#include "socket_internal.h"

/*--- 项目头文件 ---*/
#include "HashTable.h"
#include "log_mgmt.h"

/*--- 系统头文件 ---*/
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * 单例指针 (全局)
 *===========================================================================*/
static SocContext *g_singleton_ctx = NULL;

SocContext *soc_context_get_instance(void)
{
    return g_singleton_ctx;
}

void soc_context_register_instance(SocContext *ctx)
{
    g_singleton_ctx = ctx;
}

int soc_wait_socket_parse(SocContext *ctx)
{
	pthread_mutex_lock(ctx->parse_mutex);

	if (ctx->meta.socket_parse_state == PARSE_IDLE) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_SOCKET_NOT_FOUND;
	}

	while (ctx->meta.socket_parse_state != SOCKET_PARSE_FINISHED) {
		pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
		if (ctx->meta.socket_parse_state == PARSE_IDLE) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
	}

	/* 成功: 持锁返回, 调用方读取完成后负责 unlock */
	return SOC_OK;
}

int soc_wait_both_parse(SocContext *ctx)
{
	pthread_mutex_lock(ctx->parse_mutex);

	if (ctx->meta.socket_parse_state == PARSE_IDLE
		&& ctx->meta.json_parse_state == PARSE_IDLE) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_SOCKET_NOT_FOUND;
	}

	if (ctx->meta.socket_parse_state == SOCKET_PARSE_FINISHED
		&& ctx->meta.json_parse_state == PARSE_IDLE) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_JSON_NOT_FOUND;
	}

	while (ctx->meta.socket_parse_state != SOCKET_PARSE_FINISHED
		|| ctx->meta.json_parse_state != JSON_PARSE_FINISHED) {
		pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
		if (ctx->meta.socket_parse_state == PARSE_IDLE
			&& ctx->meta.json_parse_state == PARSE_IDLE) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
	}

	/* 成功: 持锁返回, 调用方读取完成后负责 unlock */
	return SOC_OK;
}

void *soc_thread_func(void *arg)
{
	SocContext *ctx = (SocContext *)arg;

	DBG_INFO("soc_thread_func: started\n");

	while (1) {
		int result;

		pthread_mutex_lock(ctx->parse_mutex);
		while (ctx->meta.socket_parse_state != SOCKET_PARSE_NEED
			&& !ctx->stop_flag
			&& !ctx->restart_needed) {
			pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
		}

		if (ctx->stop_flag == SOC_THREAD_STOP_REQUESTED) {
			ctx->stop_flag = 0;
			pthread_mutex_unlock(ctx->parse_mutex);
			DBG_INFO("soc_thread_func: exit\n");
			break;
		}
        /* 这个情况是初始化时没有socket文件，而后在测试程序中设置 */
		if (ctx->restart_needed
			&& ctx->meta.socket_parse_state != SOCKET_PARSE_NEED) {
			ctx->restart_needed = 0;
			if (ctx->parse_pending) {
				ctx->parse_pending = 0;
				/* 告知外部查询socket文件内容和通道号时，正在编译解析 */
				ctx->meta.socket_parse_state = SOCKET_PARSE_NEED;
				ctx->meta.json_parse_state   = PARSE_IDLE;
			} else {
				pthread_mutex_unlock(ctx->parse_mutex);
				continue;
			}
		}
		pthread_mutex_unlock(ctx->parse_mutex);

		DBG_INFO("soc_thread_func: start compile-parse cycle\n");

		result = pipeline_run_cycle(ctx);
        
		/*
		 * CYCLE_RESTART: 外部 Res_SetSocketFileName 触发了重解析.
		 * 策略: 设 NEED 但不 broadcast — 查询线程在 cond_wait 中继续阻塞,
		 * 直到新一轮解析完成 (CYCLE_OK) 或彻底失败 (CYCLE_ERROR) 才被唤醒.
		 * 设 NEED 必须在 free_socket_data 之前 + 持 mutex, 保证新来的查询
		 * 线程看到 NEED 后进入 cond_wait, 不会去读即将被释放的旧数据.
		 * restart_needed 本身是 atomic_int, 跨线程可见性由硬件保证.
		 */
		if (result == CYCLE_RESTART) {
			DBG_INFO("soc_thread_func: restart requested\n");
			pthread_mutex_lock(ctx->parse_mutex);
			ctx->meta.socket_parse_state = SOCKET_PARSE_NEED;
			ctx->meta.json_parse_state   = PARSE_IDLE;
			pthread_mutex_unlock(ctx->parse_mutex);

			free_socket_data(ctx);

			pthread_mutex_lock(ctx->parse_mutex);
			ctx->restart_needed = 0;
			ctx->parse_pending  = 0;
			pthread_mutex_unlock(ctx->parse_mutex);
			continue;
		}

		if (result == CYCLE_OK) {
			/*
			 * pipeline_run_cycle 已在 mutex 下将 parse_state 写入 FINISHED,
			 * 但 cond_wait 中的查询线程不会主动轮询 — 必须由 broadcast 唤醒.
			 * 被唤醒后 pthread_cond_wait 重新获取 mutex (acquire 语义),
			 * 配合 pipeline 中的 unlock (release 语义) 保证查询线程看到最新状态.
			 */
			pthread_mutex_lock(ctx->parse_mutex);
			pthread_cond_broadcast(ctx->parse_cond);
			pthread_mutex_unlock(ctx->parse_mutex);
			DBG_INFO("soc_thread_func: cycle finished\n");
			continue;
		}

		/* CYCLE_ERROR */
		DBG_ERR("soc_thread_func: cycle failed\n");
		pthread_mutex_lock(ctx->parse_mutex);
		/* 这里也是必要的，因为pipeline_run_cycle有三种返回CYCLE_ERROR
		的情况 */
		ctx->meta.socket_parse_state = PARSE_IDLE;
		ctx->meta.json_parse_state   = PARSE_IDLE;
		pthread_cond_broadcast(ctx->parse_cond);
		pthread_mutex_unlock(ctx->parse_mutex);
		free_socket_data(ctx);
	}

	DBG_INFO("soc_thread_func: stopped\n");
	return NULL;
}

/*===========================================================================
 * 线程管理
 *===========================================================================*/
int init_thread_soc(SocContext *ctx)
{
	int ret;

	ret = pthread_create(&ctx->work_thread, NULL, soc_thread_func, ctx);
	if (ret != 0) {
		DBG_ERR("init_thread_soc: pthread_create failed, ret=%d\n", ret);
		return SOC_ERR_INTERNAL;
	}

	DBG_INFO("init_thread_soc: thread created, id=%lu\n",
		(unsigned long)ctx->work_thread);

	return SOC_OK;
}


