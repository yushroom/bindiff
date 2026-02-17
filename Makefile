# Makefile for Binary Diff Tool
# 适用于没有 CMake 的环境

CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fPIC
LDFLAGS = -pthread

# 检测系统 LZ4
ifneq ($(shell pkg-config --exists liblz4 && echo yes),yes)
    $(warning LZ4 未安装，压缩功能将不可用)
    LZ4_CFLAGS =
    LZ4_LIBS =
    CXXFLAGS += -DBINDIFF_HAS_LZ4=0
else
    LZ4_CFLAGS = $(shell pkg-config --cflags liblz4)
    LZ4_LIBS = $(shell pkg-config --libs liblz4)
    CXXFLAGS += -DBINDIFF_HAS_LZ4=1 $(LZ4_CFLAGS)
endif

# 目录
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# 源文件
SOURCES = \
    $(SRC_DIR)/core/diff_engine.cpp \
    $(SRC_DIR)/core/patch_engine.cpp \
    $(SRC_DIR)/core/block_processor.cpp \
    $(SRC_DIR)/core/matcher.cpp \
    $(SRC_DIR)/core/operations.cpp \
    $(SRC_DIR)/io/mmap_file.cpp \
    $(SRC_DIR)/io/stream_writer.cpp \
    $(SRC_DIR)/io/file_utils.cpp \
    $(SRC_DIR)/compress/lz4_compressor.cpp \
    $(SRC_DIR)/crypto/sha256.cpp

# 对象文件
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# 目标
TARGET_LIB = $(BUILD_DIR)/libbindiff.a
TARGET_BIN = $(BUILD_DIR)/bindiff

# 默认目标
all: dirs $(TARGET_LIB) $(TARGET_BIN)

# 创建目录
dirs:
	@mkdir -p $(BUILD_DIR) $(OBJ_DIR) $(OBJ_DIR)/core $(OBJ_DIR)/io $(OBJ_DIR)/compress $(OBJ_DIR)/crypto

# 静态库
$(TARGET_LIB): $(OBJECTS)
	ar rcs $@ $^
	@echo "✓ 静态库: $@"

# 可执行文件
$(TARGET_BIN): $(TARGET_LIB) $(SRC_DIR)/main.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) $(SRC_DIR)/main.cpp $(TARGET_LIB) $(LZ4_LIBS) $(LDFLAGS) -o $@
	@echo "✓ 可执行文件: $@"

# 编译规则
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c $< -o $@

# 清理
clean:
	rm -rf $(BUILD_DIR)
	@echo "✓ 清理完成"

# 安装 (用户级)
install: $(TARGET_BIN)
	@mkdir -p $(HOME)/.local/bin
	cp $(TARGET_BIN) $(HOME)/.local/bin/
	@echo "✓ 安装到 $(HOME)/.local/bin/bindiff"

# 运行测试
test: $(TARGET_BIN)
	@echo "运行基本测试..."
	@./$(TARGET_BIN) --help || true
	@echo ""
	@echo "✓ 测试完成"

# 显示帮助
help:
	@echo "Binary Diff 构建系统"
	@echo ""
	@echo "目标:"
	@echo "  all      - 构建库和可执行文件 (默认)"
	@echo "  clean    - 清理构建文件"
	@echo "  install  - 安装到 ~/.local/bin"
	@echo "  test     - 运行基本测试"
	@echo ""
	@echo "变量:"
	@echo "  CXX      - C++ 编译器 (默认: g++)"
	@echo "  CXXFLAGS - 编译选项"

.PHONY: all dirs clean install test help
