/*--- 模块头文件 ---*/
#include "socket_internal.h"

/*--- 项目头文件 ---*/
#include "log_mgmt.h"

/*--- 系统头文件 ---*/
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int socket_json_compile(SocContext *ctx, const char *input_path,
                                char *output_path, uint32_t output_len)
{
	pid_t pid;
	/*编译器的路径："/usr/local/bin/xxx.exe"*/
	const char *compiler = ctx->config.compiler_path;
	if (compiler[0] == '\0') {
		compiler = "socket_compiler";
	}

	{
		int _n = snprintf(output_path, output_len, "%s.json", input_path);
		if (_n < 0 || (uint32_t)_n >= output_len) {
			DBG_ERR("socket_json_compile: output path too long (%d >= %u)\n",
			        _n, output_len);
			return SOC_ERR_PARAM;
		}
	}

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
		if (ctx->restart_needed || ctx->stop_flag) {
			int _st;
			DBG_INFO("socket_json_compile: interrupted, pid=%d\n", pid);
			/* 重新检查子进程是否已退出, 防止 PID 重用竞态:
			 * 若已退出则不 kill, 避免误杀被内核重分配的同 PID 进程 */
			if (waitpid(pid, &_st, WNOHANG) == 0) {
				/* 子进程仍在运行, 安全终止 */
				kill(pid, SIGKILL);
				waitpid(pid, NULL, 0);
			}
			/* 清理可能写了一半的中间文件 */
			remove(output_path);
			return SOC_ERR_INTERRUPTED;
		}

		usleep(50000); /* 50ms 轮询间隔 */
	}
}
