/*===========================================================================
 * 测试辅助函数 — write-temp-parse 模式
 *===========================================================================*/
#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "socket_internal.h"
#include "soc_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 将 socket JSON 字符串写入临时文件，调用 parse_socket_json，返回错误码 */
static inline int parse_json_string(SocContext *ctx, const char *json_str)
{
    char tmp_path[256];
    FILE *fp;
    int ret;

    snprintf(tmp_path, sizeof(tmp_path), "/tmp/_test_socket_%d.json", getpid());
    fp = fopen(tmp_path, "w");
    if (fp == NULL) return -999;
    fputs(json_str, fp);
    fclose(fp);

    ret = parse_socket_json(ctx, tmp_path);
    remove(tmp_path);
    return ret;
}

/* 将 config JSON 字符串写入临时文件，调用 parse_config_json，返回错误码 */
static inline int parse_config_json_string(SocContext *ctx, const char *json_str)
{
    char tmp_path[256];
    FILE *fp;
    int ret;

    snprintf(tmp_path, sizeof(tmp_path), "/tmp/_test_config_%d.json", getpid());
    fp = fopen(tmp_path, "w");
    if (fp == NULL) return -999;
    fputs(json_str, fp);
    fclose(fp);

    ret = parse_config_json(ctx, tmp_path);
    remove(tmp_path);
    return ret;
}

#endif /* TEST_HELPERS_H */
