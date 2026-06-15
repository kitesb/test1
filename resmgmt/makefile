
# 编译配置 (WSL 原生 gcc)
CC      := gcc
CXX     := g++

# 目录
SRC_DIR                := ResMgmt/src
SOC_DIR                := ResMgmt/soc
IO_DIR                 := ResMgmt/io
INC_DIR                := ResMgmt/inc
SOCKET_JSON_PARSE_DIR  := ResMgmt/socket_json_parse
OBJ_DIR                := build

# ---- 第三方库路径  ----
CJSON_INC := /usr/local/include/cjson
CJSON_LIB := /usr/local/lib64
# ----------------------------------

# 自动搜索源文件
C_SOURCES := $(shell find $(SRC_DIR) $(SOC_DIR) $(IO_DIR) $(SOCKET_JSON_PARSE_DIR)/src -type f -name '*.c' ! -name 'stubs.c')
OBJECTS   := $(addprefix $(OBJ_DIR)/, $(notdir $(C_SOURCES:.c=.o)))

# 头文件路径
INC_FLAGS := -I$(INC_DIR) -I$(SOC_DIR) -I$(IO_DIR) -I$(SOCKET_JSON_PARSE_DIR)/inc -I$(CJSON_INC)

# 链接
LINK_FLAGS := -L$(CJSON_LIB) -lcjson -lpthread -Wl,-rpath,$(CJSON_LIB)

# 编译选项
COMMON_FLAGS := -Wall -Wextra -fPIC -Wno-unused-parameter -fvisibility=hidden

BUILD_MODE ?= release
ifeq ($(BUILD_MODE), debug)
    COMMON_FLAGS += -g -O0 -DDEBUG
else
    COMMON_FLAGS += -O2 -DNDEBUG
endif

CFLAGS := $(COMMON_FLAGS) -std=gnu11

# 目标
TARGET_LIB  := libresmgmt.so
TARGET_TEST := test

.PHONY: all clean info

all: info $(TARGET_LIB) $(TARGET_TEST)

info:
	@echo "=========================================="
	@echo "Build mode: $(BUILD_MODE)"
	@echo "Target: $(TARGET_LIB) + $(TARGET_TEST)"
	@echo "=========================================="

# 共享库
$(TARGET_LIB): $(OBJECTS)
	$(CC) $^ -o $@ -shared -Wl,--as-needed $(LINK_FLAGS)
	@echo "Generate: $(TARGET_LIB)"

# 测试程序
$(TARGET_TEST): tst.c $(TARGET_LIB)
	$(CC) $(CFLAGS) $(INC_FLAGS) -o $@ tst.c ./$(TARGET_LIB)
	@echo "Generate: $(TARGET_TEST)"

# 编译规则 (扁平输出到 build/)
define compile_rule
$(OBJ_DIR)/$(notdir $(1:.c=.o)): $(1)
	@mkdir -p $(OBJ_DIR)
	$$(CC) $$(CFLAGS) $$(INC_FLAGS) -c $$< -o $$@
endef

$(foreach src,$(C_SOURCES),$(eval $(call compile_rule,$(src))))

# 包含生成的依赖
DEPS := $(shell find $(OBJ_DIR) -name '*.d' 2>/dev/null)
-include $(DEPS)

# 清理
clean:
	@rm -rf $(OBJ_DIR) $(TARGET_LIB) $(TARGET_TEST)
	@echo "Cleanup complete"
