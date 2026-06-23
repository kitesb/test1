#!/bin/bash
# Build and run DUT Group unit tests
set -e

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${TEST_DIR}/.."
RES_RESOURCE="${PROJECT_ROOT}/ResMgmt/res_resource"
API_DIR="${PROJECT_ROOT}/ResMgmt/API"
SOCKET_TESTS="${PROJECT_ROOT}/socket_tests"

INC_DIRS="-I${RES_RESOURCE}/inc"
INC_DIRS="${INC_DIRS} -I${API_DIR}/inc"
INC_DIRS="${INC_DIRS} -I${PROJECT_ROOT}/ResMgmt/inc"
INC_DIRS="${INC_DIRS} -I${PROJECT_ROOT}/ResMgmt/io/inc"
INC_DIRS="${INC_DIRS} -I${TEST_DIR}"

SRC_FILES="${TEST_DIR}/test_dut_group.c"
SRC_FILES="${SRC_FILES} ${RES_RESOURCE}/src/dut_group.c"
SRC_FILES="${SRC_FILES} ${API_DIR}/src/ResDutGroup.c"

CFLAGS="-Wall -Wextra -DDUT_GROUP_SELF_TEST -g -O0"

# 为测试提供 printf_log2 桩实现（避免引入完整的 log_mgmt 子系统）
STUB_LOG='/tmp/stub_log_mgmt.c'
cat > "${STUB_LOG}" << 'STUBEOF'
#include <stdio.h>
#include <stdarg.h>
int printf_log2(int level, const char *fmt, ...) {
    (void)level;
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}
STUBEOF

echo "=== Building DUT Group Tests ==="
gcc ${CFLAGS} ${INC_DIRS} ${SRC_FILES} "${STUB_LOG}" -o /tmp/test_dut_group -lpthread
rm -f "${STUB_LOG}"

echo "=== Running DUT Group Tests ==="
/tmp/test_dut_group
echo "=== Done ==="
