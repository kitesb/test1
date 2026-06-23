#include <stdio.h>
#include <string.h>

#include "ResAPI.h"
#include "soc_context.h"
#include "dealsoc_internal.h"
#include "HashTable.h"

#define DATA_MLEN  1024

int main(int argc, char *argv[])
{
    char data[DATA_MLEN];

    SocContext *ctx;

    printf("=== ResMgmt Test Program ===\n\n");

    /* 创建独立 context (不启动线程, 手动同步调用测试) */
    ctx = soc_context_create(NULL);
    if (ctx == NULL) {
        printf("FAIL: soc_context_create returned NULL\n");
        return 1;
    }
    ctx->meta.on_socket_board = 1;  /* 强制使能 config 解析 (DEV_GET_SOCKET_BOARD_STATUS 硬编码返回0) */

    /*===========================================================================
     * 阶段1: 解析 socket.json 并观察哈希表
     *===========================================================================*/
    {
        const char *json_path;
        int32_t ret;
        uint32_t i, j;
        uint32_t hash_common_after_socket, hash_power_after_socket;

        json_path = (argc > 1) ? argv[1] : "SOC_384DUT_96B.json";

        printf("=== Phase 1: Socket JSON Parser ===\n");
        printf("Input: %s\n\n", json_path);

        ret = parse_socket_json(ctx, json_path);
        if (ret != SOC_OK) {
            printf("FAIL: parse_socket_json returned %d\n", ret);
            goto cleanup;
        }
        printf("PASS: parse_socket_json returned SOC_OK\n\n");

        /* parse_socket_json 不设置此状态, 需要手动设为 FINISHED */
        ctx->meta.socket_parse_state = SOCKET_PARSE_FINISHED;

        printf("  Metadata:\n");
        printf("    dut_count      = %u\n", ctx->meta.dut_count);
        printf("    common_pin_num = %u\n", ctx->meta.common_pin_num);
        printf("    power_pin_num  = %u\n", ctx->meta.power_pin_num);

        /* 统计 socket 解析后的哈希表 */
        hash_common_after_socket = 0;
        for (i = 0; i < HASH_TABLE_SIZE; i++) {
            hash_node_t *node = ctx->hash_common.buckets[i];
            while (node != NULL) { hash_common_after_socket++; node = node->next; }
        }
        hash_power_after_socket = 0;
        for (i = 0; i < HASH_TABLE_SIZE; i++) {
            hash_node_t *node = ctx->hash_power.buckets[i];
            while (node != NULL) { hash_power_after_socket++; node = node->next; }
        }
        printf("\n  [After socket parse] Hash tables:\n");
        printf("    g_hash_common entries: %u\n", hash_common_after_socket);
        printf("    g_hash_power entries:  %u\n", hash_power_after_socket);

        /*===========================================================================
         * 阶段2: 解析 config.json — 通过哈希表填充 whole_no
         *===========================================================================*/
        printf("\n=== Phase 2: Config JSON Parser ===\n");
        printf("Input: config-local.json\n\n");

        ret = parse_config_json(ctx, "config-local.json");
        if (ret != SOC_OK) {
            printf("FAIL: parse_config_json returned %d\n", ret);
            goto cleanup;
        }
        printf("PASS: parse_config_json returned SOC_OK\n\n");

        printf("  g_logic_config:\n");
        printf("    sem_num = %u\n", ctx->logic_config.sem_num);
        printf("    asic_num = %u\n", ctx->logic_config.asic_num);
        printf("    dr_num  = %u\n", ctx->logic_config.dr_num);
        printf("    io_num  = %u\n", ctx->logic_config.io_num);
        printf("    psm_num = %u\n", ctx->logic_config.psm_num);
        printf("    pps_num = %u\n", ctx->logic_config.pps_num);
        printf("    dut_num = %u\n", ctx->logic_config.dut_num);

        /*===========================================================================
         * 阶段3: 统计 whole_no 填充情况 (socket ch_name + config whole_no)
         *===========================================================================*/
        {
            uint32_t common_filled = 0, common_total = 0;
            uint32_t power_filled = 0, power_total = 0;

            for (i = 0; i < ctx->meta.common_pin_num; i++) {
                dut_common_pin_t *pin = &ctx->dut_common_pins[i];
                if (pin->pCommonDutPin == NULL) continue;
                for (j = 0; j < ctx->meta.dut_count; j++) {
                    if (pin->pCommonDutPin[j].dut_no == 0
                        && pin->pCommonDutPin[j].ch_name[0] == '\0')
                        continue;
                    common_total++;
                    if (pin->pCommonDutPin[j].whole_no != 0) {
                        common_filled++;
                        if (common_filled <= 5) {
                            printf("  [sample] common_pin=%u dut=%u ch_name=%-16s whole_no=%u\n",
                                pin->common_pin_num, j,
                                pin->pCommonDutPin[j].ch_name,
                                pin->pCommonDutPin[j].whole_no);
                        }
                    }
                }
            }

            for (i = 0; i < ctx->meta.power_pin_num; i++) {
                dut_power_pin_t *pin = &ctx->dut_power_pins[i];
                if (pin->pPowerDutPin == NULL) continue;
                for (j = 0; j < ctx->meta.dut_count; j++) {
                    if (pin->pPowerDutPin[j].dut_no == 0
                        && pin->pPowerDutPin[j].ch_name[0] == '\0')
                        continue;
                    power_total++;
                    if (pin->pPowerDutPin[j].whole_no != 0) {
                        power_filled++;
                        if (power_filled <= 5) {
                            printf("  [sample] power_pin=%u  dut=%u ch_name=%-16s whole_no=%u\n",
                                pin->vs_pin_num, j,
                                pin->pPowerDutPin[j].ch_name,
                                pin->pPowerDutPin[j].whole_no);
                        }
                    }
                }
            }

            printf("\n=== Phase 3: whole_no Fill Summary ===\n");
            printf("  Common pins: %u / %u filled (%.1f%%)\n",
                common_filled, common_total,
                common_total > 0 ? 100.0 * common_filled / common_total : 0.0);
            printf("  Power pins:  %u / %u filled (%.1f%%)\n",
                power_filled, power_total,
                power_total > 0 ? 100.0 * power_filled / power_total : 0.0);
        }

        /*===========================================================================
         * 阶段4: 销毁哈希表并验证
         *===========================================================================*/
        printf("\n=== Phase 4: Hash Table Lifecycle ===\n");

        hash_destroy(&ctx->hash_common);
        hash_destroy(&ctx->hash_power);

        /* 验证哈希表已清空 */
        {
            uint32_t hash_common_after, hash_power_after;

            hash_common_after = 0;
            for (i = 0; i < HASH_TABLE_SIZE; i++) {
                hash_node_t *node = ctx->hash_common.buckets[i];
                while (node != NULL) { hash_common_after++; node = node->next; }
            }
            hash_power_after = 0;
            for (i = 0; i < HASH_TABLE_SIZE; i++) {
                hash_node_t *node = ctx->hash_power.buckets[i];
                while (node != NULL) { hash_power_after++; node = node->next; }
            }
            printf("  [After hash_destroy]\n");
            printf("    g_hash_common entries: %u %s\n",
                hash_common_after, hash_common_after == 0 ? "(OK - destroyed)" : "(FAIL - NOT destroyed!)");
            printf("    g_hash_power entries:  %u %s\n",
                hash_power_after, hash_power_after == 0 ? "(OK - destroyed)" : "(FAIL - NOT destroyed!)");
        }

        /* 释放 socket 数据 */
        free_socket_data(ctx);
        printf("\n  free_socket_data() done.\n");

cleanup:
        soc_context_destroy(ctx);
        ;  /* 标签必须后跟语句 */
    }

    /* interactive send/recv test */
    printf("\n--- Interactive Test ---\n");
    while (1)
    {
        printf("\ninput a string (or 'q' to quit):\n");
        memset(data, 0, DATA_MLEN);
        if (scanf("%s", data) != 1) break;
        getchar();
        if (data[0] == 'q' && data[1] == '\0') break;

        printf("recv from stdio:(%s)\n", data);
        tst_send(data);
        tst_recv();
    }

    printf("\n=== Test Complete ===\n");
    return 0;
}
