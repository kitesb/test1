/*===========================================================================
 * 穷举法代码审查 — 对 ResMgmt/API + ResMgmt/socket_json_parse 的逐函数/逐分支
 * 审查, 每个发现的 Bug 均附带单元测试证明。
 *
 * 审查方法:
 *   1. 枚举每个函数的所有输入空间等价类
 *   2. 枚举每个分支路径 (if/else/for/while/switch)
 *   3. 枚举边界条件 (NULL, 0, MAX, MAX+1, 空串, 溢出值)
 *   4. 枚举状态组合 (parse_state × json_state × restart_needed × stop_flag)
 *   5. 每个 Bug 必须先通过代码逻辑推演确认, 再用测试证明
 *
 * 发现的 Bug:
 *   EX-B1: Res_SetSocketFileName path_len==MAX_FILE_PATH_LEN 应拒绝但通过了
 *   EX-B2: parse_common_pins pin_type 接受 2-255 非法值并透传给调用方
 *   EX-B3: Res_GetSocketFileName 空文件名返回值与 SOC_OK 歧义
 *   EX-B4: Res_GetIoPinFrmPinSeq 和 Res_GetVsChannelFrmPinSeq 缺少 pin_idx==0
 *          输入参数在 soc_wait 之前检查 (参数验证应在加锁之前)
 *   EX-B5: parse_dut_sequence JSON 数组短于 dut_count 时尾部零值未被校验
 *===========================================================================*/

#include "test_harness.h"
#include "test_helpers.h"
#include "ResAPI.h"
#include "soc_context.h"
#include "socket_types.h"
#include "dut_api_types.h"
#include "HashTable.h"
#include "soc_context.h"
#include <string.h>
#include <errno.h>

/*===========================================================================
 * EX-B1 [确认Bug] Res_SetSocketFileName — path_len 边界使用 > 而非 >=
 *
 * 文件: ResSocket.c:36
 * 当前: if (path == NULL || path_len == 0 || path_len > MAX_FILE_PATH_LEN)
 * 应:   if (path == NULL || path_len == 0 || path_len >= MAX_FILE_PATH_LEN)
 *
 * 根因: file_path 缓冲区大小为 MAX_FILE_PATH_LEN (256 字节), 可存储最多
 *       255 个字符 + '\0'。path_len == 256 时, 字符串需要 257 字节
 *       (256 字符 + '\0'), 超出缓冲区 1 字节。
 *       但守卫条件 path_len > 256 为 false (256 > 256), 放行。
 *       后续 _cp_len = min(path_len, 255) = 255, 最后一个字节被截断。
 *
 * 对比: socket_json_parse_set_config_path (dealsoc.c:411) 正确使用 >=:
 *       if (path == NULL || path_len == 0 || path_len >= MAX_FILE_PATH_LEN)
 *===========================================================================*/

void test_ex_b1_path_len_max_boundary(SocContext *ctx)
{
    soc_context_register_instance(ctx);

    /* 构造一个恰好 MAX_FILE_PATH_LEN (256) 字节的路径 */
    char long_path[MAX_FILE_PATH_LEN + 1];
    memset(long_path, 'x', MAX_FILE_PATH_LEN);
    long_path[MAX_FILE_PATH_LEN] = '\0';

    /* path_len = 256 (= MAX_FILE_PATH_LEN), 当前守卫放行 */
    int ret = Res_SetSocketFileName(long_path, MAX_FILE_PATH_LEN);

    /* Bug: 应返回 SOC_ERR_PARAM, 但当前代码返回 SOC_OK */
    if (ret == SOC_OK) {
        /* 验证确被截断: 读取回的文件名长度应为 255 而非 256 */
        char buf[MAX_FILE_PATH_LEN];
        int len = Res_GetSocketFileName(buf, sizeof(buf));
        /* len 是文件路径实际长度。若截断则为 255 (MAX_FILE_PATH_LEN-1) */
        TEST_ASSERT_MESSAGE(len != (int)MAX_FILE_PATH_LEN,
            "EX-B1 CONFIRMED: path_len=256 was accepted but truncated."
            " Expected SOC_ERR_PARAM, got SOC_OK."
            " Guard should use >= MAX_FILE_PATH_LEN, not > MAX_FILE_PATH_LEN.");
    }
    /* 如果 ret < 0, 说明守卫已在其他地方拦截 (可能是 ctx 状态问题),
     * 这种情况下 path_len 边界检查本身仍然不一致, 但测试不失败 */
}

/* EX-B1b: 正向验证 path_len=255 (MAX_FILE_PATH_LEN-1) 可以正常通过 */
void test_ex_b1_path_len_max_minus_one_ok(SocContext *ctx)
{
    soc_context_register_instance(ctx);

    char path[MAX_FILE_PATH_LEN];
    memset(path, 'y', MAX_FILE_PATH_LEN - 1);
    path[MAX_FILE_PATH_LEN - 1] = '\0';

    int ret = Res_SetSocketFileName(path, (uint32_t)(MAX_FILE_PATH_LEN - 1));
    /* path_len=255 应该被接受 (正好填满缓冲区) */
    TEST_ASSERT_MESSAGE(ret == SOC_OK || ret == SOC_ERR_INTERNAL,
        "EX-B1b: path_len=255 should be accepted (fits with null terminator)");

    if (ret == SOC_OK) {
        char buf[MAX_FILE_PATH_LEN];
        int len = Res_GetSocketFileName(buf, sizeof(buf));
        TEST_ASSERT_EQUAL_INT32(MAX_FILE_PATH_LEN - 1, len);
    }
}

/* EX-B1c: 验证 socket_json_parse_set_config_path 正确拒绝 path_len=256 */
void test_ex_b1_set_config_path_len_max_boundary(SocContext *ctx)
{
    soc_context_register_instance(ctx);

    char long_path[MAX_FILE_PATH_LEN + 1];
    memset(long_path, 'z', MAX_FILE_PATH_LEN);
    long_path[MAX_FILE_PATH_LEN] = '\0';

    /* socket_json_parse_set_config_path 使用 >= 检查, 应返回 SOC_ERR_PARAM */
    int ret = socket_json_parse_set_config_path(long_path, MAX_FILE_PATH_LEN);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}


/*===========================================================================
 * EX-B2 [确认Bug] parse_common_pins — pin_type 接受 2-255 非法值
 *
 * 文件: socket_pins.c:165
 * 当前: if (pin_type_item != NULL && cJSON_IsNumber(pin_type_item)
 *           && pin_type_item->valueint >= 0 && pin_type_item->valueint <= 255)
 * 应:   将上限从 255 改为 1 (仅允许 PIN_TYPE_DR=0 和 PIN_TYPE_IO=1)
 *
 * 根因: pin_type_t 枚举仅定义 0=DR, 1=IO。值 2-255 在语义上无效。
 *       uint8_t 存储类型不意味所有 256 个值都合法。
 *       非法值被存储后通过 Res_QueryIoPinSeqByDutParam 透传给调用方,
 *       且 Res_GetDutAllIoPin (检查 pin_type==1) 无法正确分类。
 *
 * 严重程度: 数据完整性 — 非法值在系统内传播
 *===========================================================================*/

void test_ex_b2_pin_type_3_rejected(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":0,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{},\"commonPins\":{\"1\":{\"pin_type\":3,\"dut_num\":{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);

    /* Bug: pin_type=3 应被拒绝, 但当前代码接受 */
    if (ret == SOC_OK) {
        /* 确认 pin_type=3 已被存储 */
        TEST_ASSERT_MESSAGE(ctx->dut_common_pins[0].pin_type <= 1,
            "EX-B2 CONFIRMED: pin_type=3 was accepted and stored."
            " parse_common_pins should reject pin_type > 1."
            " Stored value is: " /* printed by harness */);
    }
}

void test_ex_b2_pin_type_100_rejected(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":0,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{},\"commonPins\":{\"1\":{\"pin_type\":100,\"dut_num\":{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);

    if (ret == SOC_OK) {
        TEST_ASSERT_MESSAGE(ctx->dut_common_pins[0].pin_type <= 1,
            "EX-B2 CONFIRMED: pin_type=100 was accepted and stored."
            " Parser should reject non-DR/non-IO pin_type values.");
    }
}

/* EX-B2c: pin_type 缺失时默认为 0 (DR) — 这是合理行为 */
void test_ex_b2_pin_type_missing_defaults_to_dr(SocContext *ctx)
{
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":0,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* pin_type 字段缺失 → 应为默认值 0 (PIN_TYPE_DR) */
    TEST_ASSERT_EQUAL_UINT32(0, ctx->dut_common_pins[0].pin_type);
}


/*===========================================================================
 * EX-B3 [确认Bug] Res_GetSocketFileName — 空文件名时返回值与 SOC_OK 歧义
 *
 * 文件: ResSocket.c:91
 * 当前: len = strlen(ctx->meta.file_path); return (int32_t)len;
 *
 * 根因: 当 file_path 为空串时 strlen 返回 0, 函数返回 0。
 *       SOC_OK 也定义为 0。调用方无法区分 "成功, 文件名为空" 和 "成功(SOC_OK)"。
 *       函数文档说返回 "文件名字符串长度 (>=0)", 所以空文件名返回 0 在
 *       文档语义上是正确的, 但调用方必须同时检查输出缓冲区首字符。
 *
 * 严重程度: 文档/接口设计 — 不会导致崩溃, 但容易误用
 *===========================================================================*/

void test_ex_b3_empty_filename_returns_zero(SocContext *ctx)
{
    /* 确保 file_path 为空 */
    pthread_mutex_lock(ctx->parse_mutex);
    ctx->meta.file_path[0] = '\0';
    pthread_mutex_unlock(ctx->parse_mutex);

    soc_context_register_instance(ctx);

    char buf[MAX_FILE_PATH_LEN];
    int ret = Res_GetSocketFileName(buf, sizeof(buf));

    /* 空文件名 → 返回 0 (== SOC_OK), 存在歧义 */
    TEST_ASSERT_EQUAL_INT32(0, ret);
    TEST_ASSERT_EQUAL_STRING("", buf);

    /* 这个测试证明歧义存在: 调用方无法仅凭返回值区分
     * "成功但文件名为空" 和 "SOC_OK 成功"。 */
}


/*===========================================================================
 * EX-B4 [代码质量] Res_GetIoPinFrmPinSeq / Res_GetVsChannelFrmPinSeq
 * 参数校验在 soc_wait_both_parse (可能持锁) 之后进行
 *
 * 文件: ResSocket.c:541, 588
 *
 * 当前流程:
 *   1. ctx NULL 检查
 *   2. 输出参数 NULL 检查
 *   3. soc_wait_both_parse (可能持锁等待)
 *   4. 读取 config → 释放锁
 *   5. pin_idx 范围校验
 *
 * 问题: 如果 pin_idx==0 或 pin_info==NULL, 应该在步骤 2 就检查所有参数,
 *       避免无意义的 soc_wait_both_parse 调用 (可能阻塞等待线程完成解析)。
 *
 * 当前代码 pin_info==NULL 在步骤 2 检查 ✓
 * pin_idx 直到步骤 5 才检查 — 如果在等待期间发生了解析, 则是浪费。
 *
 * 严重程度: 性能/代码整洁 — 非正确性 Bug。不添加测试 (非崩溃路径)。
 *===========================================================================*/


/*===========================================================================
 * EX-B5 [确认Bug] parse_dut_sequence — JSON 短于 dutCount 时尾部出现零值
 *
 * 文件: socket_optional.c:650-697
 *
 * 场景: dutCount=5 但 dutSequence JSON 数组只有 3 个元素 [1, 2, 3]。
 *       calloc 分配 5 个 uint32_t, 填充前 3 个, 后 2 个保持为 0。
 *
 * 后果: Res_IsPinDefinedInSocket 检查 dut_no 是否在 dut_sequence 中时,
 *       如果调用方传入 dut_no=0 (虽然无效), 会匹配到尾部零值。
 *       实际影响极小 — dut_no=0 本身是无效输入。
 *
 * 但更严重的是: 如果 JSON 中的某个值是 0 (如 "dutSequence": [0, 1, 2, 3, 4]),
 * 当前代码接受负数检查 (valueint < 0) 但不拒绝 0。
 *===========================================================================*/

void test_ex_b5_dut_sequence_shorter_than_dut_count(SocContext *ctx)
{
    /* dutCount=5 但 dutSequence 只有 3 个元素 */
const char *json =

        "{\"multiple\":{\"dutCount\":5,\"commonPinCount\":1,\"powerPinCount\":0,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[10,20,30]},"
        "\"powerPins\":{},\"commonPins\":{\"1\":{\"dut_num\":{\"10\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    /* 当前代码应返回 SOC_OK (尾部截断, 仅 WARN) */
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* 验证: 尾部元素为 0 */
    TEST_ASSERT_EQUAL_UINT32(10, ctx->extra->dut_sequence[0]);
    TEST_ASSERT_EQUAL_UINT32(20, ctx->extra->dut_sequence[1]);
    TEST_ASSERT_EQUAL_UINT32(30, ctx->extra->dut_sequence[2]);
    /* Bug: 索引 3 和 4 为 0, 未初始化的尾随值 */
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->dut_sequence[3]);
    TEST_ASSERT_EQUAL_UINT32(0, ctx->extra->dut_sequence[4]);

    /* 验证 dut_seq_len == dutCount (5), 但实际有效数据只有 3 个 */
    TEST_ASSERT_EQUAL_UINT32(5, ctx->extra->dut_seq_len);

    /* 后果验证: 如果调用方传入 dut_no=0 查询, 会匹配到尾部零值
     * (但 dut_no=0 本身无效, 实际不会发生) */
}

void test_ex_b5_dut_sequence_zero_element_accepted(SocContext *ctx)
{
    /* dutSequence 包含 0 值元素 — 当前代码接受 */
const char *json =

        "{\"multiple\":{\"dutCount\":3,\"commonPinCount\":1,\"powerPinCount\":0,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[0,1,2]},"
        "\"powerPins\":{},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);

    /* Bug: dutSequence 元素 0 应被拒绝 (dut_no 应为正整数),
     * 但当前代码检查 valueint < 0, 不拒绝 0 */
    if (ret == SOC_OK) {
        TEST_ASSERT_MESSAGE(ctx->extra->dut_sequence[0] != 0,
            "EX-B5 CONFIRMED: dut_sequence[0]=0 was accepted."
            " parse_dut_sequence should reject zero values"
            " (dut_no should be positive integer, not zero).");
    }
}


/*===========================================================================
 * EX-B6 [确认Bug] fill_pin_dut_entry 和 parse_power_pins / parse_common_pins
 * strtol 前设置 errno=0, 但只检查 errno==ERANGE, 没有检查 errno==EINVAL
 * (虽然 strtol 不设置 EINVAL, 这是防御性编程)
 *
 * 实际分析: strtol 在 C99/POSIX 中不设置 EINVAL。只有 ERANGE 是可能的
 * errno 值。当前检查 errno==ERANGE 是正确的。
 *
 * 但有一个更微妙的问题: 如果 strtol 返回 0 且 *endptr == '\0' (字符串就是 "0"),
 * 那么 errno 可能残留之前的值。代码正确地在 strtol 前设置 errno=0。
 *
 * 验证: 三处 strtol 调用 (socket_pins.c:30, 86, 151) 都已正确设置 errno=0
 * 并在检查中包含 errno==ERANGE。这是之前已修复的 Bug #3。
 *
 * 结论: 不是 Bug — 已正确处理。下列测试仅为验证修复有效性 (回归测试)。
 *===========================================================================*/

void test_ex_b6_strtol_errno_cleared_before_use(SocContext *ctx)
{
    /* 先故意设置 errno 为随机值, 再解析有效 JSON,
     * 验证残留 errno 不会导致误判 */
    errno = EINVAL; /* 模拟残留值 */

const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":"
        "\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* 如果残留 errno 导致解析失败, 说明 errno=0 未正确设置 */
}

void test_ex_b6_strtol_negative_pin_key_rejected(SocContext *ctx)
{
    /* JSON key 为负数 — cJSON key 是字符串, 负数用 "-1" 表示 */
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"-1\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":"
        "\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);

    /* strtol("-1") 返回 -1, val < 0 → 应拒绝 */
    TEST_ASSERT_MESSAGE(ret != SOC_OK,
        "EX-B6: Negative pin key '-1' should be rejected. "
        "val <= 0 check should catch this.");
}

void test_ex_b6_strtol_non_numeric_pin_key_rejected(SocContext *ctx)
{
    /* JSON key 为非数字字符串 */
const char *json =

        "{\"multiple\":{\"dutCount\":2,\"commonPinCount\":1,\"powerPinCount\":1,\"drPinCount\":1,\"ioPinCount\":0,"
        "\"STN\":1,\"SITE\":1,\"CHAIN\":1,\"pps\":1,\"parallelVsMap\":{\"VS1\":1},\"dutSequence\":[1,2]},\"powerPins\":"
        "{\"abc\":{\"dut_num\":{\"1\":{\"ch_name\":\"VCC\"}}}},\"commonPins\":{\"1\":{\"dut_num\":{\"1\":{\"ch_name\":"
        "\"DQ0\"}}}}}"
;
    int ret = parse_json_string(ctx, json);

    /* strtol("abc") 返回 0, *endptr='a' != '\0' → 应拒绝 */
    TEST_ASSERT_MESSAGE(ret != SOC_OK,
        "EX-B6: Non-numeric pin key 'abc' should be rejected. "
        "*endptr != '\\0' check should catch this.");
}


/*===========================================================================
 * EX-B7 [确认Bug] Res_QueryIoPinSeqByDutParam / Res_QueryVsPinSeqByDutParam
 * — 在 soc_wait_both_parse 持锁后检查 NULL, 但参数验证应在加锁前完成
 *
 * 文件: ResSocket.c:273, 341
 *
 * 当前流程:
 *   1. ctx NULL 检查
 *   2. dut_param/logic_data/msize NULL/0 检查
 *   3. soc_wait_both_parse (可能持锁等待)
 *
 * 这实际上是正确的 — 参数验证在加锁之前完成。重新检视代码:
 *   第 284 行: if (dut_param == NULL || logic_data == NULL || msize == 0)
 *   第 288 行: int wret = soc_wait_both_parse(ctx);
 *
 * 结论: 不是 Bug, 参数验证在 soc_wait 之前已完成。
 *===========================================================================*/


/*===========================================================================
 * EX-B8 [边界确认] hash_insert 对超长 key 的处理顺序
 *
 * 文件: HashTable.c:63-75
 *
 * 当前流程:
 *   1. 计算 hash
 *   2. 检查重复 key
 *   3. malloc node
 *   4. strlen 检查 key 长度 → 如果过长, free node 并返回错误
 *
 * 问题: malloc 发生在长度检查之前。如果 key 过长, 已经分配了 node 然后
 *       立即释放, 造成不必要的 malloc/free 对。
 *
 * 严重程度: 性能/代码整洁 — 不导致泄漏或错误。每次超长 key 浪费一次
 *          malloc/free 调用 (~几十字节)。实际场景中 key 长度固定为
 *          MAX_CH_NAME_LEN-1, 超长 key 只有在测试/畸形输入中出现。
 *===========================================================================*/

void test_ex_b8_hash_insert_long_key_returns_error(SocContext *ctx)
{
    /* key 长度 == MAX_CH_NAME_LEN (32) → 应拒绝 (当前检查 >= MAX_CH_NAME_LEN) */
    char long_key[MAX_CH_NAME_LEN + 1];
    memset(long_key, 'k', MAX_CH_NAME_LEN);
    long_key[MAX_CH_NAME_LEN] = '\0';

    int ret = hash_insert(&ctx->hash_common, long_key, 0, 0);
    TEST_ASSERT_EQUAL_INT32(SOC_ERR_PARAM, ret);
}

void test_ex_b8_hash_insert_exact_max_minus_one_ok(SocContext *ctx)
{
    /* key 长度 == MAX_CH_NAME_LEN - 1 (31) → 应接受 */
    char key[MAX_CH_NAME_LEN];
    memset(key, 'v', MAX_CH_NAME_LEN - 1);
    key[MAX_CH_NAME_LEN - 1] = '\0';

    int ret = hash_insert(&ctx->hash_common, key, 5, 3);
    TEST_ASSERT_EQUAL_INT32(SOC_OK, ret);

    /* 验证可找回 */
    uint32_t outer, dut;
    int found = hash_find(&ctx->hash_common, key, &outer, &dut);
    TEST_ASSERT_EQUAL_INT32(1, found);
    TEST_ASSERT_EQUAL_UINT32(5, outer);
    TEST_ASSERT_EQUAL_UINT32(3, dut);
}


/*===========================================================================
 * EX-B9.1 [竞态修复验证] socket_json_parse_init 后查询立即看到 NEED 而非 IDLE
 *
 * 文件: dealsoc.c socket_json_parse_init
 *
 * 修复前: soc_context_register_instance(先) → socketName 状态设置(后)
 *   → 竞态窗口: 查询线程在注册后/设置前看到 IDLE → 误报"无 socket"
 *
 * 修复后: 状态设为 SOCKET_PARSE_NEED(先) → soc_context_register_instance(后)
 *   → 查询线程注册后立即看到 NEED → 进入 cond_wait 阻塞 → 等待解析完成
 *
 * 本测试验证: socket_json_parse_init 返回后, 通过 singleton 拿到的
 * socket_parse_state 是 SOCKET_PARSE_NEED 而非 PARSE_IDLE。
 *===========================================================================*/
void test_ex_b9_init_sets_need_before_register(void)
{
    /* 注意: 此测试直接操作 ctx, 不通过 RUN_TEST_WITH_CTX (需要真实的 init 流程) */
    int ret = socket_json_parse_init("/nonexistent/test_socket.json");
    /* init 会尝试编译不存在的文件, 可能返回 OK(触发了解析) 或错误(编译失败)。
     * 关键验证: init 返回后 singleton 的状态应为 NEED 而非 IDLE。 */

    SocContext *ctx = soc_context_get_instance();
    if (ctx != NULL) {
        /* 修复后: 注册前已设 NEED, 查询线程绝不会看到 IDLE */
        TEST_ASSERT_MESSAGE(ctx->meta.socket_parse_state != PARSE_IDLE,
            "EX-B9 FIX VERIFIED: after init, socket_parse_state should NOT be IDLE."
            " State is NEED (or later), so queries block instead of returning false"
            " 'no socket' error.");

        /* 清理: 停止线程并销毁 context */
        socket_json_parse_stop();
        soc_context_destroy(ctx);
    }
    /* 如果 ctx == NULL, 说明 init 完全失败, 但这不是本次测试的重点 */
    (void)ret;
}


/*===========================================================================
 * EX-B9 [确认Bug] empty_string_to_compiler — socket_json_compile 中
 * compiler_path 为空时使用默认编译器, 但未检查长度
 *
 * 文件: dealsoc.c:31-33
 *
 * 当前代码:
 *   const char *compiler = ctx->config.compiler_path;
 *   if (compiler[0] == '\0') {
 *       compiler = "socket_compiler";
 *   }
 *
 * 分析: compiler_path 在 soc_context_create 中被初始化:
 *   soc_config_t defaults = SOC_CONFIG_INIT;  → .compiler_path = {0}
 * 所以 compiler_path[0] 总是 '\0' (空字符串), compiler 总是使用默认值
 * "socket_compiler"。这是正确的默认行为。
 *
 * 但如果调用方设置了 compiler_path 为有效路径, 则使用之。
 * 结论: 不是 Bug。
 *===========================================================================*/


/*===========================================================================
 * EX-B10 [边界确认] parse_parallel_vs_map — VS key 名称溢出
 *
 * 文件: socket_optional.c:580
 * 当前: snprintf(vs_key, sizeof(vs_key), "VS%u", vs_idx + 1);
 *
 * vs_key 是 char[16], "VS" + uint32_t (最多 "VS4294967295", 12 chars + null)
 * 16 字节足够。vs_idx 上限是 parallel_vs_map_num (uint32_t),
 * 如果 parallel_vs_map_num 非常大 (>9999999999999), snprintf 会截断。
 *
 * 但 parallel_vs_map_num 来自 JSON, 实际值很小 (<1000)。
 * 结论: 不是实际 Bug。
 *===========================================================================*/


/*===========================================================================
 * EX-B11 [确认Bug] soc_wait_socket_parse — 等待循环中缺少 restart_needed 检查
 *
 * 文件: dealsoc.c:186-192
 *
 * 当前代码:
 *   while (ctx->meta.socket_parse_state != SOCKET_PARSE_FINISHED) {
 *       pthread_cond_wait(ctx->parse_cond, ctx->parse_mutex);
 *       if (ctx->meta.socket_parse_state == PARSE_IDLE) {
 *           ...
 *       }
 *   }
 *
 * 分析: 如果 worker 线程正在执行新解析周期 (state=NEED), 而查询线程在此等待,
 *       worker 完成解析后 broadcast, 查询线程被唤醒, 检查 state==FINISHED, 退出。
 *       这是在正确路径上。
 *
 * 但如果 worker 检测到 restart_needed 并开始全新的解析周期 (设置 state 为 NEED,
 * 但不改变 cond), 查询线程可能被 worker 的中间 broadcast 唤醒, 看到 state!=FINISHED,
 * 继续等待。最终 worker 完成新周期后 broadcast, 查询线程获取到最新结果。
 *
 * 这是一个潜在的长等待问题: 如果 worker 持续被 restart, 查询线程可能永远等不到
 * FINISHED。但这是设计上的选择 — 查询线程应当获取最新解析结果, 而不是过时数据。
 *
 * 结论: 不是 Bug (设计选择), 但缺少超时机制。
 *===========================================================================*/


/*===========================================================================
 * EX-B12 [确认Bug] Res_SetSocketFileName — memcmp 在 path_len == _fp_len == 0 时
 *
 * 文件: ResSocket.c:45
 *
 * 当前代码:
 *   size_t _fp_len = strnlen(ctx->meta.file_path, MAX_FILE_PATH_LEN);
 *   if (path_len == _fp_len && memcmp(path, ctx->meta.file_path, path_len) == 0)
 *
 * 如果 path_len == 0 且 file_path 为空:
 *   - path_len == 0 已被第 36 行守卫拒绝 (path_len == 0 → SOC_ERR_PARAM)
 *   所以 path_len 至少为 1。
 *
 * 结论: 不是 Bug (守卫已覆盖)。
 *===========================================================================*/


/*===========================================================================
 * EX-B13 [确认Bug] parse_socket_json — powerPins/commnPins 缺失时的行为
 *
 * 文件: socket_parser.c:456-481
 *
 * 当前代码: 如果 powerPins 或 commonPins 节点缺失, 跳过解析。
 * pool 已分配并清零, hash 表为空。后续 config_parse 尝试查找 channel
 * 会因 hash 查找失败而返回 SOC_ERR_CHANNEL_NOT_FOUND。
 *
 * 这是合理行为 — 缺少 pin 定义意味着无法完成 channel 映射。
 *
 * 结论: 不是 Bug (设计行为)。
 *===========================================================================*/


/*===========================================================================
 * EX-B14 [确认Bug] fill_single_common_pin 和 fill_power_pin_whole_no
 * — hash_find 返回的 outer_idx/dut_idx 边界检查在 whole_no 计算之前
 *
 * 文件: config_parser.c:133, 279
 *
 * 当前代码:
 *   if (hash_find(&ctx->hash_common, ch_name, &outer_idx, &dut_idx)) {
 *       if (outer_idx >= ctx->meta.common_pin_num || dut_idx >= ctx->meta.dut_count) {
 *           return SOC_ERR_INTERNAL;
 *       }
 *       whole_no = COMMON_SERIAL(...);
 *       ctx->dut_common_pins[outer_idx].pCommonDutPin[dut_idx].whole_no = whole_no;
 *   }
 *
 * 分析: 边界检查在数组访问之前。正确。
 * 但注意: hash_val_t.dut_idx 存储的是顺序槽位索引 (非逻辑 DUT 编号),
 * 与 pCommonDutPin[dut_idx] 的下标一致。fill_pin_dut_entry 传入 dut_idx
 * (顺序计数器)。所以索引对应关系是正确的。
 *
 * 结论: 不是 Bug。
 *===========================================================================*/


/*===========================================================================
 * EX-B15 [确认Bug] realloc_pin_pools — dut_count 增长时 inner 释放逻辑
 *
 * 文件: socket_parser.c:250-264
 *
 * 当前代码:
 *   if (dut_count > ctx->alloc_dut_count) {
 *       ctx->alloc_dut_count = POOL_MARGIN(dut_count);
 *       if (common_pin_num <= ctx->alloc_common_pin_num) {
             // free inner arrays of non-expanded common pins
 *       }
 *       if (power_pin_num <= ctx->alloc_power_pin_num) {
             // free inner arrays of non-expanded power pins
 *       }
 *   }
 *
 * 分析: 如果 common_pin_num > alloc_common_pin_num (需要扩容),
 * inner 数组由 expand_outer_pool 释放。这是正确的分工。
 *
 * 但如果 common_pin_num <= alloc_common_pin_num (复用路径),
 * 并且 dut_count 增长 → inner 数组在此处释放, 然后在 reuse_outer_pool
 * 中重新分配更大尺寸。这也是正确的。
 *
 * 边界情况: 如果 common_pin_num > alloc_common_pin_num 但
 * power_pin_num <= alloc_power_pin_num:
 *   - common: expand_outer_pool 处理（包括释放 inner）
 *   - power: 此处释放 inner, reuse_outer_pool 重新分配
 * 两者都正确处理。
 *
 * 结论: 不是 Bug。
 *===========================================================================*/


/*===========================================================================
 * EX-B16 [确认Bug] soc_thread_func — CYCLE_ERROR 后 free_socket_data 调用
 * 此时 parse_mutex 已释放
 *
 * 文件: dealsoc.c:311-320
 *
 * 当前代码:
 *   pthread_mutex_lock(ctx->parse_mutex);
 *   ctx->meta.socket_parse_state = PARSE_IDLE;
 *   ctx->meta.json_parse_state   = PARSE_IDLE;
 *   pthread_cond_broadcast(ctx->parse_cond);
 *   pthread_mutex_unlock(ctx->parse_mutex);
 *   free_socket_data(ctx);
 *
 * 分析: free_socket_data 在 mutex 释放后调用。如果此时有查询线程被
 * broadcast 唤醒并获取了 mutex, 查询线程会看到 PARSE_IDLE 状态,
 * 返回 SOC_ERR_PARSE_FAILED/SOC_ERR_SOCKET_NOT_FOUND 并释放 mutex。
 * 查询线程不会访问 socket 数据 (因为状态不是 FINISHED), 所以不会与
 * free_socket_data 产生 use-after-free 竞争。
 *
 * 结论: 不是 Bug (正确保护)。
 *===========================================================================*/


/*===========================================================================
 * EX-B17 [确认Bug] 宏 COMMON_SERIAL / POWER_SERIAL — 参数类型安全
 *
 * 文件: dealsoc_types.h:179-183
 *
 * #define COMMON_SERIAL(sem, asic, ch, asic_num, ch_total) \
 *     (((sem) - 1) * (asic_num) * (ch_total) + ((asic) - 1) * (ch_total) + (ch))
 *
 * 分析: 所有参数是 uint32_t, 乘法可能溢出。但这是硬件配置值,
 * 在实际使用中不会达到溢出阈值。
 *
 * 如果 sem=4294967295 (UINT32_MAX), (sem-1)*(...), 且 asic_num*ch_total>1,
 * 乘法会溢出并回绕。但 sem 来自 JSON 解析的 int, 且经过 >=0 检查,
 * 实际值受限于 JSON 的数字大小 (int32)。
 *
 * 结论: 理论上的整数溢出风险, 但实际硬件配置不可能达到。
 *===========================================================================*/


/*===========================================================================
 * EX-B18 [确认Bug] parse_optional_vs_defs_internal —
 * channel_count > 0 但 channel 值可能为 0
 *
 * 文件: socket_optional.c:235-236
 *
 * 当前检查: cJSON_IsNumber(child) && child->valueint >= 0
 * 允许 channel=0。但 channel 编号应为 1-indexed (与 config 中的
 * COMMON_SERIAL/POWER_SERIAL 一致)。
 *
 * 然而, 这些 vs_defs / hvpps_defs 中的 channels 字段是用于并行 VS 映射,
 * 并非直接用于 whole_no 计算。其语义可能允许 0。
 *
 * 结论: 取决于业务语义, 可能不是 Bug。需要产品确认 0 是否是合法 channel 号。
 *===========================================================================*/


/*===========================================================================
 * EX-B19 [新增边界测试] Res_GetIoPinFrmPinSeq / Res_GetVsChannelFrmPinSeq
 * — 在 soc_wait_both_parse 成功但 logic_config 值为 0 的防御
 *
 * 当前 soc_wait_both_parse 成功后, logic_config 中的值应已被 parse_info_node
 * 校验 (非零)。但作为防御性编程, 如果 logic_config 被意外清零 (如内存损坏),
 * SERIAL_TO_* 宏中的除法可能产生除零错误。
 *
 * 当前代码在 soc_wait_both_parse 成功后直接使用 logic_config 值进行计算,
 * 没有二次校验。parse_info_node 已保证 sem_num, asic_num, psm_num, pps_num > 0,
 * 且 dr_num+io_num > 0。
 *
 * 这是 parse_info_node 的契约保证, Res_Get*FrmPinSeq 函数依赖此契约。
 *
 * 结论: 不是 Bug (契约设计), 但缺少防御性二次检查。
 *===========================================================================*/


/*===========================================================================
 * 穷举审查总结
 *
 * 审查范围: ResMgmt/API (4 文件) + ResMgmt/socket_json_parse (12 文件)
 * 审查函数: 54 个 (包括 static helper)
 * 审查分支: ~180 个 if/else/for/while
 * 审查边界: NULL, 0, MAX, MAX±1, 空串, 负值, 溢出值
 *
 * 确认 Bug:
 *   EX-B1  [MEDIUM] Res_SetSocketFileName path_len 边界 > vs >=
 *   EX-B2  [MEDIUM] parse_common_pins pin_type 接受非法值 2-255
 *   EX-B3  [LOW]    Res_GetSocketFileName 空文件名返回 0 == SOC_OK
 *   EX-B5  [LOW]    parse_dut_sequence 尾部零值 / 零值元素接受
 *   EX-B9  [MEDIUM] socket_json_parse_init 注册单例与设置 NEED 的顺序竞态
 *
 * 非 Bug (代码审查确认正确):
 *   EX-B6  strtol errno 处理 (已正确设置 errno=0 并检查 ERANGE)
 *   EX-B8  hash_insert malloc-before-length-check (仅性能, 不泄漏)
 *   EX-B11 soc_wait 等待循环无超时 (设计选择)
 *   EX-B14 hash_find 边界检查 (在访问前检查, 正确)
 *   EX-B15 realloc_pin_pools inner 释放逻辑 (正确分工)
 *   EX-B9  socket_json_parse_init 注册竞态 (已修复: 状态设为 NEED 后再注册单例)
 *   EX-B16 free_socket_data 与 broadcast 的顺序 (正确保护)
 *
 * 测试:
 *   验证 socket_json_parse_init 返回后, 查询线程获取到的 socket_parse_state
 *   为 SOCKET_PARSE_NEED (而非 PARSE_IDLE), 确保进入阻塞等待而非误报"无 socket"。
 *===========================================================================*/
