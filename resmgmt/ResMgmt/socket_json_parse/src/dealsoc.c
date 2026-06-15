#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "dealsoc_internal.h"
#include "HashTable.h"
#include "internal.h"
#include "log_mgmt.h"

/* 工作线程内部使用的文件静态变量 */
static pthread_t    g_soc_thread = 0;
static atomic_int   g_stop_flag  = 0;

/*===========================================================================
 * 编译子进程 (fork + exec, 支持中断) — 原 compile.c, 现模块内 static
 *===========================================================================*/
static int socket_json_compile(SocContext *ctx, const char *input_path,
                                char *output_path, uint32_t output_len)
{
	pid_t pid;
	/*编译器的路径："/usr/local/bin/xxx.exe"*/
	const char *compiler = ctx->config.compiler_path;

	snprintf(output_path, output_len, "%s.json", input_path);

	pid = fork();
	if (pid < 0) {
		DBG_ERR("socket_json_compile: fork failed, errno=%d\n", errno);
		return SOC_ERR_INTERNAL;
	}

	if (pid == 0) {
		/* 子进程: 执行外部编译器 */
		/* TODO: 替换为实际的编译器路径和参数 */
		execlp(compiler, compiler, input_path, output_path, NULL);
		_exit(1);
	}

	/* 父进程: 轮询等待子进程退出, 同时检查中断请求 */
	while (1) {
		int status;
		pid_t ret;

		ret = waitpid(pid, &status, WNOHANG);
		if (ret == pid) {
			/* 子进程已退出 */
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
				DBG_INFO("socket_json_compile: compiler success\n");
				return SOC_OK;
			}
			DBG_ERR("socket_json_compile: compiler failed, exit=%d\n",
				WIFEXITED(status) ? WEXITSTATUS(status) : -1);
			return SOC_ERR_CHANNEL_COMPILE_FAILED;
		}
		if (ret < 0) {
			DBG_ERR("socket_json_compile: waitpid failed, errno=%d\n", errno);
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			return SOC_ERR_INTERNAL;
		}

		/* 50ms 轮询间隔内检查中断请求 */
		if (ctx->restart_needed) {
			DBG_INFO("socket_json_compile: interrupted, killing child pid=%d\n", pid);
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
			/* 清理可能写了一半的中间文件 */
			remove(output_path);
			return SOC_ERR_INTERRUPTED;
		}

		usleep(50000); /* 50ms 轮询间隔 */
	}
}

/*===========================================================================
 * 编译解析周期 (同步执行三阶段)
 *===========================================================================*/
int pipeline_run_cycle(SocContext *ctx)
{
	int ret;

	/* 阶段1: 编译 (fork+exec, 50ms轮询可中断) */
	ret = socket_json_compile(ctx, ctx->meta.file_path,
		ctx->meta.compiled_json_path, MAX_FILE_PATH_LEN);
	if (ret == SOC_ERR_INTERRUPTED || ctx->restart_needed) {
		return CYCLE_RESTART;
	}
	if (ret != SOC_OK) {
		DBG_ERR("pipeline_run_cycle: socket_json_compile failed, ret=%d\n", ret);
		return CYCLE_ERROR;
	}

	/* 阶段2: 解析 socket.json */
	ret = parse_socket_json(ctx, ctx->meta.compiled_json_path);
	if (ret == SOC_ERR_INTERRUPTED || ctx->restart_needed) {
		return CYCLE_RESTART;
	}
	if (ret != SOC_OK) {
		DBG_ERR("pipeline_run_cycle: parse_socket_json failed, ret=%d\n", ret);
		return CYCLE_ERROR;
	}
	ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

	/* 阶段3: 解析 config.json */
	ctx->meta.on_socket_board = DEV_GET_SOCKET_BOARD_STATUS();

	if (ctx->meta.on_socket_board
		&& ctx->meta.config_json_path[0] != '\0') {
		ctx->meta.json_parse_state = JSON_PARSE_NEED;
		ret = parse_config_json(ctx, ctx->meta.config_json_path);
		if (ret == SOC_ERR_INTERRUPTED || ctx->restart_needed) {
			return CYCLE_RESTART;
		}
		if (ret == SOC_OK) {
			ctx->meta.json_parse_state = JSON_PARSE_FINISHED;
		} else {
			DBG_ERR("pipeline_run_cycle: parse_config_json failed, ret=%d\n", ret);
			ctx->meta.json_parse_state = SOCKET_PARSE_IDLE;
			hash_destroy(&ctx->hash_common);
			hash_destroy(&ctx->hash_power);
			return CYCLE_ERROR;
		}
	} else {
		DBG_INFO("pipeline_run_cycle: skip config json (on_socket_board=%u)\n",
			ctx->meta.on_socket_board);
	}

	hash_destroy(&ctx->hash_common);
	hash_destroy(&ctx->hash_power);

	return CYCLE_OK;
}

/*===========================================================================
 * 解析等待 (供 ResSocket.c 调用)
 *===========================================================================*/
int soc_wait_socket_parse(SocContext *ctx)
{
	pthread_mutex_lock(ctx->parse_mutex);

	if (ctx->meta.socket_parse_state == SOCKET_PARSE_IDLE) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_SOCKET_NOT_FOUND;
	}

	while (ctx->meta.socket_parse_state != SOCKET_PARSE_FINISHED) {
		pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
		if (ctx->meta.socket_parse_state == SOCKET_PARSE_IDLE) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return SOC_OK;
}

int soc_wait_both_parse(SocContext *ctx)
{
	pthread_mutex_lock(ctx->parse_mutex);

	if (ctx->meta.socket_parse_state == SOCKET_PARSE_IDLE
		&& ctx->meta.json_parse_state == SOCKET_PARSE_IDLE) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_SOCKET_NOT_FOUND;
	}

	if (ctx->meta.socket_parse_state == SOCKET_PARSE_FINISHED
		&& ctx->meta.json_parse_state == SOCKET_PARSE_IDLE) {
		pthread_mutex_unlock(ctx->parse_mutex);
		return SOC_ERR_JSON_NOT_FOUND;
	}

	while (ctx->meta.socket_parse_state != SOCKET_PARSE_FINISHED
		|| ctx->meta.json_parse_state != JSON_PARSE_FINISHED) {
		pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
		if (ctx->meta.socket_parse_state == SOCKET_PARSE_IDLE
			&& ctx->meta.json_parse_state == SOCKET_PARSE_IDLE) {
			pthread_mutex_unlock(ctx->parse_mutex);
			return SOC_ERR_PARSE_FAILED;
		}
	}

	pthread_mutex_unlock(ctx->parse_mutex);
	return SOC_OK;
}

/*===========================================================================
 * 工作线程主循环
 *===========================================================================*/
static void *soc_thread_func(void *arg)
{
	SocContext *ctx = (SocContext *)arg;

	DBG_INFO("soc_thread_func: started\n");

	while (1) {
		int result;

		pthread_mutex_lock(ctx->parse_mutex);
		while (ctx->meta.socket_parse_state != SOCKET_PARSE_NEED
			&& !g_stop_flag
			&& !ctx->restart_needed) {
			pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
		}

		if (g_stop_flag == 2) {
			g_stop_flag = 0;
			pthread_mutex_unlock(ctx->parse_mutex);
			DBG_INFO("soc_thread_func: exit\n");
			break;
		}

		if (ctx->restart_needed
			&& ctx->meta.socket_parse_state != SOCKET_PARSE_NEED) {
			ctx->restart_needed = 0;
			if (ctx->parse_pending) {
				ctx->parse_pending = 0;
				/* 告知外部查询socket文件内容和通道号时，旧数据失效，需要重新编译解析 */
				ctx->meta.socket_parse_state = SOCKET_PARSE_NEED;
				ctx->meta.json_parse_state   = SOCKET_PARSE_IDLE;
			} else {
				pthread_mutex_unlock(ctx->parse_mutex);
				continue;
			}
		}
		pthread_mutex_unlock(ctx->parse_mutex);

		DBG_INFO("soc_thread_func: start compile-parse cycle\n");

		result = pipeline_run_cycle(ctx);

		if (result == CYCLE_RESTART) {
			DBG_INFO("soc_thread_func: restart requested\n");
			pthread_mutex_lock(ctx->parse_mutex);
			free_socket_data(ctx);
			ctx->restart_needed = 0;
			if (ctx->parse_pending) {
				ctx->parse_pending = 0;
				ctx->meta.socket_parse_state = SOCKET_PARSE_NEED;
				ctx->meta.json_parse_state   = SOCKET_PARSE_IDLE;
			} else {
				ctx->meta.socket_parse_state = SOCKET_PARSE_IDLE;
				ctx->meta.json_parse_state   = SOCKET_PARSE_IDLE;
			}
			pthread_mutex_unlock(ctx->parse_mutex);
			continue;
		}

		if (result == CYCLE_OK) {
			/* 唯一同步收口点: 解析成功后同步到全局变量 */
				pthread_cond_broadcast(ctx->parse_cond);
			DBG_INFO("soc_thread_func: cycle finished\n");
			continue;
		}

		/* CYCLE_ERROR */
		DBG_ERR("soc_thread_func: cycle failed\n");
		ctx->meta.socket_parse_state = SOCKET_PARSE_IDLE;
		ctx->meta.json_parse_state   = SOCKET_PARSE_IDLE;
		pthread_cond_broadcast(ctx->parse_cond);
	}

	DBG_INFO("soc_thread_func: stopped\n");
	return NULL;
}

/*===========================================================================
 * 线程管理
 *===========================================================================*/
static int init_thread_soc(void)
{
	int ret;
	SocContext *ctx;

	/* 创建默认 context */
	ctx = soc_context_get_instance();
	if (ctx == NULL) {
		ctx = soc_context_create(NULL);
		if (ctx == NULL) {
			DBG_ERR("init_thread_soc: context create failed\n");
			return -1;
		}
	}

	ret = pthread_create(&g_soc_thread, NULL, soc_thread_func, ctx);
	if (ret != 0) {
		DBG_ERR("init_thread_soc: pthread_create failed, ret=%d\n", ret);
		return ret;
	}
	ctx->thread_running = 1;

	DBG_INFO("init_thread_soc: thread created, id=%lu\n",
		(unsigned long)g_soc_thread);

	return SOC_OK;
}

__attribute__((unused))
static int stop_thread_soc(void)
{
	SocContext *ctx = soc_context_get_instance();

	g_stop_flag = 2;
	if (ctx != NULL) {
		pthread_cond_broadcast(ctx->parse_cond);
	}
	pthread_join(g_soc_thread, NULL);

	if (ctx != NULL) {
		ctx->thread_running = 0;
	}

	DBG_INFO("stop_thread_soc: thread joined\n");

	return 0;
}

/*===========================================================================
 * 对外入口
 *===========================================================================*/
int32_t socket_json_parse_start(const char *socket_file_path)
{
	SocContext *ctx = soc_context_get_instance();

	if (socket_file_path == NULL || socket_file_path[0] == '\0') {
		DBG_ERR("socket_json_parse_start: empty path\n");
		return SOC_ERR_SOCKET_NOT_FOUND;
	}

	pthread_mutex_lock(ctx->parse_mutex);

	strncpy(ctx->meta.file_path, socket_file_path, MAX_FILE_PATH_LEN - 1);
	ctx->meta.file_path[MAX_FILE_PATH_LEN - 1] = '\0';

	ctx->meta.socket_parse_state = SOCKET_PARSE_NEED;
	ctx->meta.json_parse_state   = SOCKET_PARSE_IDLE;

	pthread_cond_signal(ctx->parse_cond);
	pthread_mutex_unlock(ctx->parse_mutex);

	DBG_INFO("socket_json_parse_start: triggered for %s\n", socket_file_path);

	return SOC_OK;
}

int32_t socket_json_parse_set_config_path(const char *path, uint32_t path_len)
{
	SocContext *ctx = soc_context_get_instance();

	if (path == NULL || path_len == 0 || path_len >= MAX_FILE_PATH_LEN) {
		DBG_ERR("socket_json_parse_set_config_path: invalid param\n");
		return SOC_ERR_PARAM;
	}

	pthread_mutex_lock(ctx->parse_mutex);
	strncpy(ctx->meta.config_json_path, path, MAX_FILE_PATH_LEN - 1);
	ctx->meta.config_json_path[MAX_FILE_PATH_LEN - 1] = '\0';
	pthread_mutex_unlock(ctx->parse_mutex);

	DBG_INFO("socket_json_parse_set_config_path: set to %s\n", path);

	return SOC_OK;
}

int32_t socket_json_parse_compile(const char *input_path, char *output_path, uint32_t output_len)
{
	SocContext *ctx = soc_context_get_instance();
	if (ctx == NULL) {
		ctx = soc_context_create(NULL);
	}
	return socket_json_compile(ctx, input_path, output_path, output_len);
}

/*===========================================================================
 * 模块初始化 (由 res_Init 内部调用)
 *===========================================================================*/
int32_t socket_json_parse_init(uint32_t argc, char *argv)
{
	int ret;
	SocContext *ctx;

	DBG_INFO("socket_json_parse_init: start, argc=%u, argv=%s\n", argc,
		(argv != NULL) ? argv : "(null)");

	/* 创建默认 context */
	ctx = soc_context_get_instance();
	if (ctx == NULL) {
		ctx = soc_context_create(NULL);
		if (ctx == NULL) {
			DBG_ERR("socket_json_parse_init: context create failed\n");
			return SOC_ERR_MEMORY_ALLOC;
		}
	}

	/* 1. 逻辑配置默认值 (解析 config.json 后覆盖) */
	ctx->logic_config = ctx->config.default_logic_config;

	/* 2. 初始化元数据默认值 (解析 socket.json 后覆盖) */
	memset(&ctx->meta, 0, sizeof(ctx->meta));
	ctx->meta.dut_count      = ctx->config.default_dut_count;
	ctx->meta.common_pin_num = ctx->config.default_common_pin_num;
	ctx->meta.power_pin_num  = ctx->config.default_power_pin_num;
	snprintf(ctx->meta.config_json_path, MAX_FILE_PATH_LEN,
		"%s", ctx->config.default_config_json_path);

	/* 3. 数组指针初始为 NULL */
	ctx->dut_common_pins = NULL;
	ctx->dut_power_pins  = NULL;

	/* 4. 启动 soc 工作线程 */
	ret = init_thread_soc();
	if (ret != SOC_OK) {
		DBG_ERR("socket_json_parse_init: init_thread_soc failed\n");
		return ret;
	}

	/* 5. 从 argv 解析 socket 文件名, 若有则触发编译解析 */
	if (argv != NULL && strlen(argv) > 0) {
		socket_json_parse_start(argv);
	}

	DBG_INFO("socket_json_parse_init: done\n");

	return SOC_OK;
}

/*===========================================================================
 * 文件读取工具 (原 file_util.c)
 *===========================================================================*/
char *read_file_to_string(const char *path)
{
	FILE *fp;
	long fsize;
	char *buf;

	fp = fopen(path, "rb");
	if (fp == NULL) {
		DBG_ERR("read_file_to_string: cannot open %s\n", path);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf = (char *)malloc((size_t)fsize + 1);
	if (buf == NULL) {
		DBG_ERR("read_file_to_string: malloc failed for %ld bytes\n", fsize);
		fclose(fp);
		return NULL;
	}

	if (fread(buf, 1, (size_t)fsize, fp) != (size_t)fsize) {
		DBG_ERR("read_file_to_string: fread failed\n");
		free(buf);
		fclose(fp);
		return NULL;
	}
	buf[fsize] = '\0';
	fclose(fp);
	return buf;
}
