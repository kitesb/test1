#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "soc_context.h"
#include "dealsoc_internal.h"
#include "dealsoc_types.h"
#include "HashTable.h"
#include "log_mgmt.h"

/*===========================================================================
 * 单例
 *===========================================================================*/
static SocContext *g_singleton_ctx = NULL;

SocContext *soc_context_get_instance(void)
{
    return g_singleton_ctx;
}

/*===========================================================================
 * 生命周期实现
 *===========================================================================*/

SocContext *soc_context_create(const soc_config_t *config)
{
    SocContext *ctx;

    /* 重复创建: 先销毁旧实例 */
    if (g_singleton_ctx != NULL) {
        soc_context_destroy(g_singleton_ctx);
    }

    ctx = calloc(1, sizeof(SocContext));
    if (ctx == NULL) return NULL;

    if (config != NULL)
        ctx->config = *config;
    else {
        soc_config_t defaults = SOC_CONFIG_INIT;
        ctx->config = defaults;
    }

    /* context 拥有自己的 mutex/cond */
    ctx->parse_mutex = malloc(sizeof(pthread_mutex_t));
    ctx->parse_cond  = malloc(sizeof(pthread_cond_t));
    if (ctx->parse_mutex == NULL || ctx->parse_cond == NULL) {
        free(ctx->parse_mutex);
        free(ctx->parse_cond);
        free(ctx);
        return NULL;
    }
    pthread_mutex_init(ctx->parse_mutex, NULL);
    pthread_cond_init(ctx->parse_cond, NULL);

    ctx->restart_needed = 0;
    ctx->parse_pending  = 0;

    /* 设置默认值 */
    ctx->logic_config = ctx->config.default_logic_config;
    memset(&ctx->meta, 0, sizeof(ctx->meta));
    ctx->meta.dut_count      = ctx->config.default_dut_count;
    ctx->meta.common_pin_num = ctx->config.default_common_pin_num;
    ctx->meta.power_pin_num  = ctx->config.default_power_pin_num;
    snprintf(ctx->meta.config_json_path, MAX_FILE_PATH_LEN,
            "%s", ctx->config.default_config_json_path);

    /* 注册单例 */
    g_singleton_ctx = ctx;

    DBG_INFO("soc_context_create: ctx=%p\n", (void *)ctx);

    return ctx;
}

void soc_context_destroy(SocContext *ctx)
{
    if (ctx == NULL) return;
    DBG_INFO("soc_context_destroy: ctx=%p\n", (void *)ctx);

    /* 清单例 */
    if (g_singleton_ctx == ctx) {
        g_singleton_ctx = NULL;
    }

    free_socket_data(ctx);

    if (ctx->parse_mutex != NULL) {
        pthread_mutex_destroy(ctx->parse_mutex);
        free(ctx->parse_mutex);
    }
    if (ctx->parse_cond != NULL) {
        pthread_cond_destroy(ctx->parse_cond);
        free(ctx->parse_cond);
    }

    free(ctx);
}

