#!/bin/bash
# Build and run socket_json_parse unit tests
set -e

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${TEST_DIR}/.."
SOCKET_DIR="${PROJECT_ROOT}/ResMgmt/socket_json_parse"
API_DIR="${PROJECT_ROOT}/ResMgmt/API"
RES_RESOURCE="${PROJECT_ROOT}/ResMgmt/res_resource"

# ---- include paths ----
INC_DIRS="-I${SOCKET_DIR}/inc"
INC_DIRS="${INC_DIRS} -I${API_DIR}/inc"
INC_DIRS="${INC_DIRS} -I${PROJECT_ROOT}/ResMgmt/inc"
INC_DIRS="${INC_DIRS} -I${PROJECT_ROOT}/ResMgmt/io/inc"
INC_DIRS="${INC_DIRS} -I${PROJECT_ROOT}/ResMgmt/ipc/inc"
INC_DIRS="${INC_DIRS} -I${RES_RESOURCE}/inc"
INC_DIRS="${INC_DIRS} -I${TEST_DIR}"
INC_DIRS="${INC_DIRS} -I/usr/local/include/cjson"

# ---- test source files ----
TEST_SRCS=""
for f in "${TEST_DIR}"/*.c; do
    TEST_SRCS="${TEST_SRCS} ${f}"
done

# ---- production source files ----
SOCKET_SRCS=""
for f in "${SOCKET_DIR}/src"/*.c; do
    SOCKET_SRCS="${SOCKET_SRCS} ${f}"
done

# ---- all sources ----
ALL_SRCS="${TEST_SRCS} ${SOCKET_SRCS}"
ALL_SRCS="${ALL_SRCS} ${API_DIR}/src/ResSocket.c"
ALL_SRCS="${ALL_SRCS} ${RES_RESOURCE}/src/dut_group.c"

# ---- compiler flags ----
CFLAGS="-Wall -Wextra -g -O0 -std=gnu11 -DDUT_GROUP_SELF_TEST"

# ---- stub for printf_log2 ----
STUB_LOG='/tmp/stub_log_mgmt.c'
cat > "${STUB_LOG}" << 'STUBEOF'
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
void printf_log2(uint32_t level, int8_t *format, ...) {
    (void)level;
    va_list ap;
    va_start(ap, format);
    vprintf((char*)format, ap);
    va_end(ap);
}
STUBEOF

echo "=== Building Socket JSON Parse Tests ==="
gcc ${CFLAGS} ${INC_DIRS} ${ALL_SRCS} "${STUB_LOG}" -o /tmp/test_socket_json_parse -lcjson -lpthread
rm -f "${STUB_LOG}"

echo "=== Running Socket JSON Parse Tests ==="
export LD_LIBRARY_PATH=/usr/local/lib64:${LD_LIBRARY_PATH}
/tmp/test_socket_json_parse
echo "=== Done ==="
