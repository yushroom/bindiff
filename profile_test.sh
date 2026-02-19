#!/bin/bash
# 性能分析测试

BINDIFF="./build/bindiff"
TEST_DIR="test_perf"

mkdir -p $TEST_DIR

# 生成 100MB 测试文件
echo "生成测试文件..."
dd if=/dev/urandom of=$TEST_DIR/old.bin bs=1M count=100 2>/dev/null
cp $TEST_DIR/old.bin $TEST_DIR/new.bin
dd if=/dev/urandom of=$TEST_DIR/new.bin bs=1M count=5 conv=notrunc 2>/dev/null

# 使用 perf 分析
echo "性能分析..."
perf record -g $BINDIFF diff $TEST_DIR/old.bin $TEST_DIR/new.bin $TEST_DIR/patch.bdp --no-verify 2>&1

echo ""
echo "热点函数分析:"
perf report --stdio --sort symbol -n --percent-limit 1 2>&1 | head -50
