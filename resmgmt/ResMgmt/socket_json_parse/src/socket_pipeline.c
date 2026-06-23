/*--- 模块头文件 ---*/
#include "socket_internal.h"

/*--- 项目头文件 ---*/
#include "HashTable.h"
#include "log_mgmt.h"

/*--- 系统头文件 ---*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
	pthread_mutex_lock(ctx->parse_mutex);
	ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;
	pthread_mutex_unlock(ctx->parse_mutex);

	/* 阶段3: 解析 config.json */
	ctx->meta.on_socket_board = DEV_GET_SOCKET_BOARD_STATUS();

	if (ctx->meta.on_socket_board
		&& ctx->meta.config_json_path[0] != '\0') {
		pthread_mutex_lock(ctx->parse_mutex);
		ctx->meta.json_parse_state = JSON_PARSE_NEED;
		pthread_mutex_unlock(ctx->parse_mutex);
		ret = parse_config_json(ctx, ctx->meta.config_json_path);
		if (ret == SOC_ERR_INTERRUPTED || ctx->restart_needed) {
			return CYCLE_RESTART;
		}
		if (ret == SOC_OK) {
			pthread_mutex_lock(ctx->parse_mutex);
			ctx->meta.json_parse_state = JSON_PARSE_FINISHED;
			pthread_mutex_unlock(ctx->parse_mutex);
		} else {
			DBG_ERR("pipeline_run_cycle: parse_config_json failed, ret=%d\n", ret);
			pthread_mutex_lock(ctx->parse_mutex);
		/* 这个也是必要的，因为如果失败，pthread的语义保证外部查询被阻塞
			这样能够保证free_socket_data时不会被外部读到旧数据 */
			ctx->meta.socket_parse_state = PARSE_IDLE;
			ctx->meta.json_parse_state   = PARSE_IDLE;
			pthread_mutex_unlock(ctx->parse_mutex);
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

 int32_t socket_json_parse_stop(void)
{
	SocContext *ctx = soc_context_get_instance();

	if (ctx != NULL) {
		ctx->stop_flag = SOC_THREAD_STOP_REQUESTED;
		pthread_cond_broadcast(ctx->parse_cond);
		/* pthread_join 统一由 soc_context_destroy 执行,
		 * 避免与 destroy 并发时对同一 work_thread 双重 join (CR3) */
	}

	DBG_INFO("socket_json_parse_stop: stop requested\n");

	return 0;
}

/*===========================================================================
 * 对外入口 — socket_json_parse_start 内部实现 (接受 ctx, 供 init 复用)
 *===========================================================================*/

int32_t socket_json_parse_start(SocContext *ctx, const char *socket_file_path)
{
	if (ctx == NULL) {
		DBG_ERR("socket_json_parse_start: context not initialized\n");
		return SOC_ERR_INTERNAL;
	}

	if (socket_file_path == NULL || socket_file_path[0] == '\0') {
		DBG_ERR("socket_json_parse_start: empty path\n");
		return SOC_ERR_SOCKET_NOT_FOUND;
	}

	pthread_mutex_lock(ctx->parse_mutex);

	strncpy(ctx->meta.file_path, socket_file_path, MAX_FILE_PATH_LEN - 1);
	ctx->meta.file_path[MAX_FILE_PATH_LEN - 1] = '\0';

	ctx->meta.socket_parse_state = SOCKET_PARSE_NEED;
	ctx->meta.json_parse_state   = PARSE_IDLE;

	pthread_cond_signal(ctx->parse_cond);
	pthread_mutex_unlock(ctx->parse_mutex);

	DBG_INFO("socket_json_parse_start: triggered for %s\n", socket_file_path);

	return SOC_OK;
}

int32_t socket_json_parse_set_config_path(const char *path, uint32_t path_len)
{
	SocContext *ctx = soc_context_get_instance();

	if (ctx == NULL) {
		DBG_ERR("socket_json_parse_set_config_path: context not initialized\n");
		return SOC_ERR_INTERNAL;
	}

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
		DBG_ERR("socket_json_parse_compile: context not initialized\n");
		return SOC_ERR_INTERNAL;
	}
	return socket_json_compile(ctx, input_path, output_path, output_len);
}

/*===========================================================================
 * 模块初始化 (由 res_Init 内部调用)
 *===========================================================================*/

int32_t socket_json_parse_init(const char *socketName)
{
	int ret;
	SocContext *ctx;
	int created = 0;

	DBG_INFO("socket_json_parse_init: start, socketName=%s\n",
		(socketName != NULL) ? socketName : "(null)");

	/* 创建默认 context */
	ctx = soc_context_get_instance();
	if (ctx == NULL) {
		ctx = soc_context_create(NULL);
		if (ctx == NULL) {
			DBG_ERR("socket_json_parse_init: context create failed\n");
			return SOC_ERR_MEMORY_ALLOC;
		}
		created = 1;
	}

	/* 1. 逻辑配置默认值 (解析 config.json 后覆盖) */
	ctx->logic_config = ctx->config.default_logic_config;

	/* 2. 初始化元数据默认值 */
	snprintf(ctx->meta.config_json_path, MAX_FILE_PATH_LEN,
		"%s", ctx->config.default_config_json_path);

	/* 3. 防御重复初始化泄漏 */
	if (ctx->dut_common_pins != NULL || ctx->dut_power_pins != NULL) {
		free_socket_data(ctx);
	}

	/* 4. 启动 soc 工作线程 */
	ret = init_thread_soc(ctx);
	if (ret != SOC_OK) {
		DBG_ERR("socket_json_parse_init: init_thread_soc failed\n");
		if (created) {
			soc_context_destroy(ctx);
		}
		return ret;
	}

	/* 5. 若有 socketName 则在注册单例前通过内部实现设置 NEED 状态,
	 * 防止查询线程在注册后看到 IDLE 而误报 SOC_ERR_SOCKET_NOT_FOUND。
	 * 使用 _socket_parse_start_impl 直接传入 ctx, 绕过 get_instance 依赖。 */
	if (socketName != NULL && socketName[0] != '\0') {
		socket_json_parse_start(ctx, socketName);
	}

	/* 线程启动成功后才注册单例 (此时状态已为 NEED, 查询线程进入阻塞等待) */
	if (created) {
		soc_context_register_instance(ctx);
	}


	DBG_INFO("socket_json_parse_init: done\n");

	return SOC_OK;
}

/*===========================================================================
 * 文件读取工具
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

	if (fsize < 0) {
		DBG_ERR("read_file_to_string: ftell failed for %s\n", path);
		fclose(fp);
		return NULL;
	}

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
