#!/bin/bash
# 5GB 性能测试

BINDIFF="./build/bindiff"
TEST_DIR="test_5gb"

echo "生成 5GB 测试文件..."
mkdir -p $TEST_DIR
dd if=/dev/urandom of=$TEST_DIR/old.bin bs=1M count=5120 2>/dev/null
cp $TEST_DIR/old.bin $TEST_DIR/new.bin

# 修改 10% 的数据
echo "修改 10% 数据..."
dd if=/dev/urandom of=$TEST_DIR/new.bin bs=1M count=512 conv=notrunc 2>/dev/null

echo "测试 5GB diff 性能..."
time $BINDIFF diff $TEST_DIR/old.bin $TEST_DIR/new.bin $TEST_DIR/patch.bdp --threads 4 --progress

echo "测试 5GB patch 性能..."
time $BINDIFF patch $TEST_DIR/old.bin $TEST_DIR/patch.bdp $TEST_DIR/out.bin --progress

echo "验证正确性..."
if cmp -s $TEST_DIR/new.bin $TEST_DIR/out.bin; then
    echo "✓ 验证通过"
else
    echo "✗ 验证失败"
fi
