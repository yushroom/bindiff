#!/bin/bash
# 压力测试 - 模拟真实场景

BINDIFF="./build/bindiff"
TEST_DIR="test_stress"

echo "========================================"
echo "Binary Diff 压力测试"
echo "========================================"

mkdir -p $TEST_DIR

# 测试 1: 100MB 文件 (模拟小型游戏包)
echo ""
echo "测试 1: 100MB 文件 (5% 修改)"
echo "----------------------------------------"
dd if=/dev/urandom of=$TEST_DIR/100mb_old.bin bs=1M count=100 2>/dev/null
cp $TEST_DIR/100mb_old.bin $TEST_DIR/100mb_new.bin
# 修改前 5MB
dd if=/dev/urandom of=$TEST_DIR/100mb_new.bin bs=1M count=5 conv=notrunc 2>/dev/null

time $BINDIFF diff $TEST_DIR/100mb_old.bin $TEST_DIR/100mb_new.bin $TEST_DIR/100mb.bdp --threads 4 --progress
time $BINDIFF patch $TEST_DIR/100mb_old.bin $TEST_DIR/100mb.bdp $TEST_DIR/100mb_out.bin --progress

if cmp -s $TEST_DIR/100mb_new.bin $TEST_DIR/100mb_out.bin; then
    echo "✓ 验证通过"
else
    echo "✗ 验证失败"
fi

# 测试 2: 500MB 文件 (模拟中型游戏包)
echo ""
echo "测试 2: 500MB 文件 (10% 修改)"
echo "----------------------------------------"
dd if=/dev/urandom of=$TEST_DIR/500mb_old.bin bs=1M count=500 2>/dev/null
cp $TEST_DIR/500mb_old.bin $TEST_DIR/500mb_new.bin
# 修改前 50MB
dd if=/dev/urandom of=$TEST_DIR/500mb_new.bin bs=1M count=50 conv=notrunc 2>/dev/null

time $BINDIFF diff $TEST_DIR/500mb_old.bin $TEST_DIR/500mb_new.bin $TEST_DIR/500mb.bdp --threads 4 --progress
time $BINDIFF patch $TEST_DIR/500mb_old.bin $TEST_DIR/500mb.bdp $TEST_DIR/500mb_out.bin --progress

if cmp -s $TEST_DIR/500mb_new.bin $TEST_DIR/500mb_out.bin; then
    echo "✓ 验证通过"
else
    echo "✗ 验证失败"
fi

# 测试 3: 1GB 文件 (模拟大型游戏包)
echo ""
echo "测试 3: 1GB 文件 (10% 修改)"
echo "----------------------------------------"
dd if=/dev/urandom of=$TEST_DIR/1gb_old.bin bs=1M count=1024 2>/dev/null
cp $TEST_DIR/1gb_old.bin $TEST_DIR/1gb_new.bin
# 修改前 100MB
dd if=/dev/urandom of=$TEST_DIR/1gb_new.bin bs=1M count=100 conv=notrunc 2>/dev/null

time $BINDIFF diff $TEST_DIR/1gb_old.bin $TEST_DIR/1gb_new.bin $TEST_DIR/1gb.bdp --threads 4 --progress
time $BINDIFF patch $TEST_DIR/1gb_old.bin $TEST_DIR/1gb.bdp $TEST_DIR/1gb_out.bin --progress

if cmp -s $TEST_DIR/1gb_new.bin $TEST_DIR/1gb_out.bin; then
    echo "✓ 验证通过"
else
    echo "✗ 验证失败"
fi

echo ""
echo "========================================"
echo "压力测试完成"
echo "========================================"

# 清理
# rm -rf $TEST_DIR
