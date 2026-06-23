/*
 * test_socket_dump.c — Socket JSON Memory Dump Test
 *
 * Parses a socket JSON file (e.g. SOC_384DUT_96B.json) via parse_socket_json(),
 * then prints every section of the parsed in-memory data structures to stdout
 * for visual inspection.
 *
 * Build:  make test_dump
 * Run:    ./test_dump [socket_json_path]
 *         (defaults to SOC_384DUT_96B.json)
 */

#include "HashTable.h"
#include "socket_internal.h"
#include "soc_context.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Print helpers
 *===========================================================================*/

static void print_banner(const char *title)
{
    printf("\n"
           "================================================================================\n"
           "  %s\n"
           "================================================================================\n",
           title);
}

static uint32_t count_valid_duts(const _dut_pin_t *pins, uint32_t max_count)
{
    uint32_t n = 0;
    if (pins == NULL) return 0;
    for (uint32_t i = 0; i < max_count; i++) {
        if (pins[i].ch_name[0] != '\0') n++;
    }
    return n;
}

/*===========================================================================
 * Section printers
 *===========================================================================*/

static void print_meta(const SocContext *ctx)
{
    const soc_meta_t *m = &ctx->meta;
    print_banner("META -- Scalar Statistics (from \"multiple\" node)");
    printf("  dut_count         : %u\n",   m->dut_count);
    printf("  common_pin_num    : %u\n",   m->common_pin_num);
    printf("  power_pin_num     : %u\n",   m->power_pin_num);
    printf("  dr_pin_count      : %u\n",   m->dr_pin_count);
    printf("  io_pin_count      : %u\n",   m->io_pin_count);
    printf("  STN               : %u\n",   m->stn);
    printf("  SITE              : %u\n",   m->site);
    printf("  CHAIN             : %u\n",   m->chain);
    printf("  parallel_vs_map_num: %u\n",   m->parallel_vs_map_num);
    printf("  on_socket_board   : %u\n",   m->on_socket_board);
    printf("  socket_parse_state: %d\n",   (int)m->socket_parse_state);
    printf("  json_parse_state  : %d\n",   (int)m->json_parse_state);
    printf("  file_path         : \"%s\"\n", m->file_path);
}

static void print_pin_names(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("pinNames (optional, root-level)");
    if (ex == NULL || ex->pin_names == NULL || ex->pin_name_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->pin_name_count);
    printf("  %-4s  %-34s %-18s %s\n", "Idx", "pinName", "pinNum", "dutPinNum");
    printf("  %-4s  %-34s %-18s %s\n", "---", "------", "------", "---------");
    for (uint32_t i = 0; i < ex->pin_name_count; i++) {
        const pin_name_t *p = &ex->pin_names[i];
        printf("  %-4u  %-34s %-18s %u\n", i, p->pin_name, p->pin_num, p->dut_pin_num);
    }
}

static void print_symbol(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("symbol (optional, root-level)");
    if (ex == NULL || ex->symbols == NULL || ex->symbol_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->symbol_count);
    printf("  %-4s  %-34s %s\n", "Idx", "pin_name", "vs_name");
    printf("  %-4s  %-34s %s\n", "---", "--------", "-------");
    for (uint32_t i = 0; i < ex->symbol_count; i++) {
        const symbol_entry_t *s = &ex->symbols[i];
        printf("  %-4u  %-34s %s\n", i, s->pin_name, s->vs_name);
    }
}

static void print_vs_defs(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("vsDefinitions (optional, root-level)");
    if (ex == NULL || ex->vs_defs == NULL || ex->vs_def_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->vs_def_count);
    for (uint32_t i = 0; i < ex->vs_def_count; i++) {
        const vs_def_t *d = &ex->vs_defs[i];
        printf("\n  [%u] name=\"%s\"  pps_type=\"%s\"  channel_count=%u\n",
               i, d->name, d->pps_type, d->channel_count);
        if (d->channels != NULL && d->channel_count > 0) {
            printf("      channels: ");
            uint32_t limit = d->channel_count < 20 ? d->channel_count : 20;
            for (uint32_t c = 0; c < limit; c++) {
                printf("%u", d->channels[c]);
                if (c + 1 < limit) printf(", ");
            }
            if (d->channel_count > 20) printf(", ... (+%u more)", d->channel_count - 20);
            printf("\n");
        }
    }
}

static void print_vs_site_defs(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("vsSiteDefinitions (optional, root-level)");
    if (ex == NULL || ex->vs_site_defs == NULL || ex->vs_site_def_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->vs_site_def_count);
    for (uint32_t i = 0; i < ex->vs_site_def_count; i++) {
        const vs_site_entry_t *s = &ex->vs_site_defs[i];
        printf("\n  [%u] site=\"%s\"  vs=\"%s\"  pps_type=\"%s\"  channel_count=%u\n",
               i, s->site_name, s->vs_name, s->pps_type, s->channel_count);
        if (s->channels != NULL && s->channel_count > 0) {
            printf("      channels: ");
            uint32_t limit = s->channel_count < 20 ? s->channel_count : 20;
            for (uint32_t c = 0; c < limit; c++) {
                printf("%u", s->channels[c]);
                if (c + 1 < limit) printf(", ");
            }
            if (s->channel_count > 20) printf(", ... (+%u more)", s->channel_count - 20);
            printf("\n");
        }
    }
}

static void print_hvpps_defs(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("hvppsDefinitions (optional, inside \"multiple\")");
    if (ex == NULL || ex->hvpps_defs == NULL || ex->hvpps_def_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->hvpps_def_count);
    for (uint32_t i = 0; i < ex->hvpps_def_count; i++) {
        const vs_def_t *d = &ex->hvpps_defs[i];
        printf("  [%u] name=\"%s\"  pps_type=\"%s\"  channel_count=%u\n",
               i, d->name, d->pps_type, d->channel_count);
        if (d->channels != NULL && d->channel_count > 0) {
            printf("      channels: ");
            uint32_t limit = d->channel_count < 20 ? d->channel_count : 20;
            for (uint32_t c = 0; c < limit; c++) {
                printf("%u", d->channels[c]);
                if (c + 1 < limit) printf(", ");
            }
            if (d->channel_count > 20) printf(", ... (+%u more)", d->channel_count - 20);
            printf("\n");
        }
    }
}

static void print_hvpps_site_defs(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("hvppsSiteDefinitions (optional, inside \"multiple\")");
    if (ex == NULL || ex->hvpps_site_defs == NULL || ex->hvpps_site_def_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->hvpps_site_def_count);
    for (uint32_t i = 0; i < ex->hvpps_site_def_count; i++) {
        const vs_site_entry_t *s = &ex->hvpps_site_defs[i];
        printf("  [%u] site=\"%s\"  vs=\"%s\"  pps_type=\"%s\"  channel_count=%u\n",
               i, s->site_name, s->vs_name, s->pps_type, s->channel_count);
        if (s->channels != NULL && s->channel_count > 0) {
            printf("      channels: ");
            uint32_t limit = s->channel_count < 20 ? s->channel_count : 20;
            for (uint32_t c = 0; c < limit; c++) {
                printf("%u", s->channels[c]);
                if (c + 1 < limit) printf(", ");
            }
            if (s->channel_count > 20) printf(", ... (+%u more)", s->channel_count - 20);
            printf("\n");
        }
    }
}

static void print_parallel_vs_map(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("parallelVsMap (optional, inside \"multiple\")");
    if (ex == NULL || ex->vs_map == NULL || ctx->meta.parallel_vs_map_num == 0) {
        printf("  (not present)\n"); return;
    }
    uint32_t count = ctx->meta.parallel_vs_map_num;
    printf("  Count: %u\n", count);
    for (uint32_t i = 0; i < count; i++)
        printf("  VS%-3u = %u\n", i + 1, ex->vs_map[i]);
}

static void print_separate_entities(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("separateEntities (optional, inside \"multiple\")");
    if (ex == NULL || ex->entities == NULL || ex->entity_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->entity_count);
    printf("  %-4s  %-18s %-8s %s\n", "Idx", "type", "number", "mode");
    printf("  %-4s  %-18s %-8s %s\n", "---", "----", "------", "----");
    for (uint32_t i = 0; i < ex->entity_count; i++) {
        const separate_entity_t *e = &ex->entities[i];
        printf("  %-4u  %-18s %-8u %u\n", i, e->type, e->number, e->mode);
    }
}

static void print_dut_sequence(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("dutSequence (optional, inside \"multiple\")");
    if (ex == NULL || ex->dut_sequence == NULL || ex->dut_seq_len == 0) {
        printf("  (not present)\n"); return;
    }
    uint32_t len = ex->dut_seq_len;
    printf("  Total entries: %u\n", len);
    uint32_t head = len < 10 ? len : 10;
    printf("  First %u: [", head);
    for (uint32_t i = 0; i < head; i++) {
        printf("%u", ex->dut_sequence[i]);
        if (i + 1 < head) printf(", ");
    }
    printf("]\n");
    if (len > 15) {
        printf("  ... (%u entries omitted) ...\n", len - 15);
        printf("  Last 5:  [");
        for (uint32_t i = len - 5; i < len; i++) {
            printf("%u", ex->dut_sequence[i]);
            if (i + 1 < len) printf(", ");
        }
        printf("]\n");
    } else if (len > 10) {
        printf("  Remaining: [");
        for (uint32_t i = 10; i < len; i++) {
            printf("%u", ex->dut_sequence[i]);
            if (i + 1 < len) printf(", ");
        }
        printf("]\n");
    }
}

static void print_system_dut(const SocContext *ctx)
{
    const socket_extra_t *ex = ctx->extra;
    print_banner("systemDut (optional, inside \"multiple\")");
    if (ex == NULL || ex->system_duts == NULL || ex->system_dut_count == 0) {
        printf("  (not present)\n"); return;
    }
    printf("  Count: %u\n", ex->system_dut_count);
    printf("  %-4s  %-14s %-10s %s\n", "Idx", "systemDutNum", "siteNum", "dutNum");
    printf("  %-4s  %-14s %-10s %s\n", "---", "-------------", "--------", "------");
    for (uint32_t i = 0; i < ex->system_dut_count; i++) {
        const system_dut_t *d = &ex->system_duts[i];
        printf("  %-4u  %-14u %-10u %u\n", i, d->system_dut_num, d->site_num, d->dut_num);
    }
}

static void print_power_pins(const SocContext *ctx)
{
    uint32_t pin_count = ctx->meta.power_pin_num;
    uint32_t dut_count = ctx->meta.dut_count;
    const dut_power_pin_t *pins = ctx->dut_power_pins;

    print_banner("powerPins (root-level)");
    if (pins == NULL || pin_count == 0) { printf("  (not present)\n"); return; }
    printf("  Summary: %u power pins x %u DUTs = %u total slots\n",
           pin_count, dut_count, pin_count * dut_count);

    for (uint32_t outer = 0; outer < pin_count; outer++) {
        const dut_power_pin_t *pin = &pins[outer];
        uint32_t valid = count_valid_duts(pin->pPowerDutPin, dut_count);
        printf("\n  ------------------------------------------------------------------\n"
               "  Power Pin #%u  (vs_pin_num=%u)  valid_DUTs=%u / %u\n"
               "  ------------------------------------------------------------------\n",
               outer, pin->vs_pin_num, valid, dut_count);
        if (pin->pPowerDutPin == NULL || valid == 0) {
            printf("  (no valid DUT entries)\n"); continue;
        }
        uint32_t show = valid < 5 ? valid : 5;
        printf("  %-6s  %-8s  %-34s %s\n", "dut_idx", "dut_no", "ch_name", "whole_no");
        printf("  %-6s  %-8s  %-34s %s\n", "-------", "-------", "-------", "--------");
        uint32_t printed = 0;
        for (uint32_t i = 0; i < dut_count && printed < show; i++) {
            if (pin->pPowerDutPin[i].ch_name[0] == '\0') continue;
            const _dut_pin_t *dp = &pin->pPowerDutPin[i];
            printf("  %-6u  %-8u  %-34s %u\n", i, dp->dut_no, dp->ch_name, dp->whole_no);
            printed++;
        }
        if (valid > show)
            printf("  ... and %u more valid entries (total %u)\n", valid - show, valid);
    }
}

static void print_common_pins(const SocContext *ctx)
{
    uint32_t pin_count = ctx->meta.common_pin_num;
    uint32_t dut_count = ctx->meta.dut_count;
    const dut_common_pin_t *pins = ctx->dut_common_pins;

    print_banner("commonPins (root-level)");
    if (pins == NULL || pin_count == 0) { printf("  (not present)\n"); return; }
    printf("  Summary: %u common pins x %u DUTs = %u total slots\n",
           pin_count, dut_count, pin_count * dut_count);

    uint32_t dr_total = 0, io_total = 0, grand_valid = 0;

    for (uint32_t outer = 0; outer < pin_count; outer++) {
        const dut_common_pin_t *pin = &pins[outer];
        uint32_t valid = count_valid_duts(pin->pCommonDutPin, dut_count);
        const char *type_str = (pin->pin_type == PIN_TYPE_DR) ? "DR" :
                               (pin->pin_type == PIN_TYPE_IO) ? "IO" : "??";
        if (pin->pin_type == PIN_TYPE_DR) dr_total++;
        else if (pin->pin_type == PIN_TYPE_IO) io_total++;
        grand_valid += valid;

        if (outer < 3) {
            printf("\n  ------------------------------------------------------------------\n"
                   "  Common Pin #%u  (common_pin_num=%u)  type=%s(%u)  valid_DUTs=%u / %u\n"
                   "  ------------------------------------------------------------------\n",
                   outer, pin->common_pin_num, type_str, pin->pin_type, valid, dut_count);
            if (pin->pCommonDutPin == NULL || valid == 0) { printf("  (no valid DUT entries)\n"); continue; }
            uint32_t show = valid < 5 ? valid : 5;
            printf("  %-6s  %-8s  %-34s %s\n", "dut_idx", "dut_no", "ch_name", "whole_no");
            printf("  %-6s  %-8s  %-34s %s\n", "-------", "-------", "-------", "--------");
            uint32_t printed = 0;
            for (uint32_t i = 0; i < dut_count && printed < show; i++) {
                if (pin->pCommonDutPin[i].ch_name[0] == '\0') continue;
                const _dut_pin_t *dp = &pin->pCommonDutPin[i];
                printf("  %-6u  %-8u  %-34s %u\n", i, dp->dut_no, dp->ch_name, dp->whole_no);
                printed++;
            }
            if (valid > show)
                printf("  ... and %u more valid entries (total %u)\n", valid - show, valid);
        } else {
            printf("  [%2u] pin#=%-3u  type=%-2s(%u)  valid_DUTs=%u\n",
                   outer, pin->common_pin_num, type_str, pin->pin_type, valid);
        }
    }
    printf("\n  ------------------------------------------------------------------\n"
           "  Common Pin Summary\n"
           "  ------------------------------------------------------------------\n");
    printf("  Total DR pins : %u\n", dr_total);
    printf("  Total IO pins : %u\n", io_total);
    printf("  Total valid DUT entries: %u\n", grand_valid);
}

static void print_hash_stats(const hash_table_t *ht, const char *name)
{
    uint32_t non_empty = 0, total_nodes = 0, sample_count = 0;
    printf("\n  --- %s ---\n", name);
    printf("  %-8s  %-34s  %s\n", "Bucket", "Key (ch_name)", "Value (outer, dut_idx)");
    printf("  %-8s  %-34s  %s\n", "------", "-------------", "---------------------");
    for (uint32_t i = 0; i < HASH_TABLE_SIZE; i++) {
        hash_node_t *node = ht->buckets[i];
        if (node == NULL) continue;
        non_empty++;
        if (sample_count < 3) {
            hash_node_t *cur = node;
            while (cur != NULL) {
                printf("  %-8u  %-34s  (outer=%u, dut=%u)\n",
                       i, cur->key, cur->val.outer_idx, cur->val.dut_idx);
                total_nodes++;
                cur = cur->next;
            }
            sample_count++;
        } else {
            while (node != NULL) { total_nodes++; node = node->next; }
        }
    }
    printf("\n  Total buckets    : %d\n"
           "  Non-empty buckets: %u\n"
           "  Total nodes      : %u\n"
           "  Load factor      : %.2f\n",
           HASH_TABLE_SIZE, non_empty, total_nodes,
           (double)total_nodes / (double)HASH_TABLE_SIZE);
}

static void print_memory_pools(const SocContext *ctx)
{
    print_banner("MEMORY POOLS -- Allocation Tracking");
    uint32_t alloc_c = ctx->alloc_common_pin_num;
    uint32_t alloc_p = ctx->alloc_power_pin_num;
    uint32_t alloc_d = ctx->alloc_dut_count;
    uint32_t spec_c = ctx->meta.common_pin_num;
    uint32_t spec_p = ctx->meta.power_pin_num;
    uint32_t spec_d = ctx->meta.dut_count;
    printf("  POOL_MARGIN(n) = n + n/4 + 8\n\n");
    printf("  %-22s  %8s  %8s  %8s  %s\n",
           "Pool", "Spec", "Alloc", "Expected", "Match?");
    printf("  %-22s  %8s  %8s  %8s  %s\n",
           "--------------------", "--------", "--------", "--------", "------");
    printf("  %-22s  %8u  %8u  %8u  %s\n",
           "common_pin outer array", spec_c, alloc_c, POOL_MARGIN(spec_c),
           (alloc_c == POOL_MARGIN(spec_c)) ? "YES" : "NO");
    printf("  %-22s  %8u  %8u  %8u  %s\n",
           "power_pin outer array", spec_p, alloc_p, POOL_MARGIN(spec_p),
           (alloc_p == POOL_MARGIN(spec_p)) ? "YES" : "NO");
    printf("  %-22s  %8u  %8u  %8u  %s\n",
           "_dut_pin inner array", spec_d, alloc_d, POOL_MARGIN(spec_d),
           (alloc_d == POOL_MARGIN(spec_d)) ? "YES" : "NO");
    printf("\n  Total _dut_pin_t memory: %zu + %zu = %zu bytes\n",
           (size_t)alloc_c * alloc_d * sizeof(_dut_pin_t),
           (size_t)alloc_p * alloc_d * sizeof(_dut_pin_t),
           (size_t)(alloc_c + alloc_p) * alloc_d * sizeof(_dut_pin_t));
}

/*===========================================================================
 * main
 *===========================================================================*/
int main(int argc, char *argv[])
{
    const char *json_path;
    SocContext *ctx;
    int ret;

    json_path = (argc > 1) ? argv[1] : "SOC_384DUT_96B.json";

    printf("===============================================================================\n");
    printf("  SOCKET JSON MEMORY DUMP\n");
    printf("  File: %s\n", json_path);
    printf("===============================================================================\n\n");

    /* --- Create context --- */
    ctx = soc_context_create(NULL);
    if (ctx == NULL) {
        printf("FATAL: soc_context_create() failed\n");
        return 1;
    }

    /* --- Parse --- */
    printf("Parsing %s ...\n", json_path);
    ret = parse_socket_json(ctx, json_path);
    if (ret != SOC_OK) {
        printf("FATAL: parse_socket_json() returned %d\n", ret);
        soc_context_destroy(ctx);
        return 1;
    }
    printf("OK: parse_socket_json returned SOC_OK\n");

    /* --- Print everything --- */
    print_meta(ctx);
    print_pin_names(ctx);
    print_symbol(ctx);
    print_vs_defs(ctx);
    print_vs_site_defs(ctx);
    print_hvpps_defs(ctx);
    print_hvpps_site_defs(ctx);
    print_parallel_vs_map(ctx);
    print_separate_entities(ctx);
    print_dut_sequence(ctx);
    print_system_dut(ctx);
    print_power_pins(ctx);
    print_common_pins(ctx);

    print_banner("HASH TABLE STATISTICS");
    print_hash_stats(&ctx->hash_power, "hash_power (ch_name -> power pin position)");
    print_hash_stats(&ctx->hash_common, "hash_common (ch_name -> common pin position)");

    print_memory_pools(ctx);

    printf("\n"
           "===============================================================================\n"
           "  DUMP COMPLETE\n"
           "===============================================================================\n");

    soc_context_destroy(ctx);
    return 0;
}
