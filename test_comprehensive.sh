#!/bin/bash
# 综合测试脚本

# set -e  # 去掉自动退出，让我们手动处理错误

BINDIFF="./build/bindiff"
TEST_DIR="test_data"
PASS=0
FAIL=0

# 颜色
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "========================================"
echo "Binary Diff 综合测试"
echo "========================================"

# 创建测试目录
mkdir -p $TEST_DIR

# 测试函数
test_case() {
    local name="$1"
    echo -n "测试: $name ... "
}

pass() {
    echo -e "${GREEN}✓ PASS${NC}"
    ((PASS++))
}

fail() {
    local reason="$1"
    echo -e "${RED}✗ FAIL${NC}"
    echo "  原因: $reason"
    ((FAIL++))
}

# ==================== 测试用例 ====================

# 测试 1: 空文件
test_case "空文件"
dd if=/dev/zero of=$TEST_DIR/empty1.bin bs=1 count=0 2>/dev/null
dd if=/dev/zero of=$TEST_DIR/empty2.bin bs=1 count=0 2>/dev/null
if $BINDIFF diff $TEST_DIR/empty1.bin $TEST_DIR/empty2.bin $TEST_DIR/empty.bdp --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/empty1.bin $TEST_DIR/empty.bdp $TEST_DIR/empty_out.bin --no-verify 2>&1; then
        if [ ! -s $TEST_DIR/empty_out.bin ] || [ $(stat -c%s $TEST_DIR/empty_out.bin 2>/dev/null || echo 0) -eq 0 ]; then
            pass
        else
            fail "输出文件应为空"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# 测试 2: 小文件完全相同
test_case "小文件完全相同 (1KB)"
dd if=/dev/urandom of=$TEST_DIR/same1.bin bs=1024 count=1 2>/dev/null
cp $TEST_DIR/same1.bin $TEST_DIR/same2.bin
if $BINDIFF diff $TEST_DIR/same1.bin $TEST_DIR/same2.bin $TEST_DIR/same.bdp --no-verify 2>&1; then
    patch_size=$(stat -c%s $TEST_DIR/same.bdp 2>/dev/null || echo 0)
    if [ $patch_size -lt 200 ]; then
        pass
    else
        fail "相同文件补丁应很小 (实际: $patch_size bytes)"
    fi
else
    fail "diff 命令失败"
fi

# 测试 3: 小文件完全不同
test_case "小文件完全不同 (1KB)"
dd if=/dev/urandom of=$TEST_DIR/diff1.bin bs=1024 count=1 2>/dev/null
dd if=/dev/urandom of=$TEST_DIR/diff2.bin bs=1024 count=1 2>/dev/null
if $BINDIFF diff $TEST_DIR/diff1.bin $TEST_DIR/diff2.bin $TEST_DIR/diff.bdp --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/diff1.bin $TEST_DIR/diff.bdp $TEST_DIR/diff_out.bin --no-verify 2>&1; then
        if cmp -s $TEST_DIR/diff2.bin $TEST_DIR/diff_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# 测试 4: 部分修改 (中间插入)
test_case "部分修改 - 中间插入"
dd if=/dev/zero of=$TEST_DIR/insert1.bin bs=1024 count=10 2>/dev/null
cp $TEST_DIR/insert1.bin $TEST_DIR/insert2.bin
# 在偏移 5KB 处插入 1KB 数据
dd if=/dev/urandom bs=1024 count=1 >> $TEST_DIR/insert2.tmp 2>/dev/null
head -c 5120 $TEST_DIR/insert1.bin > $TEST_DIR/insert2.tmp
dd if=/dev/urandom bs=1024 count=1 >> $TEST_DIR/insert2.tmp 2>/dev/null
tail -c 5120 $TEST_DIR/insert1.bin >> $TEST_DIR/insert2.tmp
mv $TEST_DIR/insert2.tmp $TEST_DIR/insert2.bin
if $BINDIFF diff $TEST_DIR/insert1.bin $TEST_DIR/insert2.bin $TEST_DIR/insert.bdp --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/insert1.bin $TEST_DIR/insert.bdp $TEST_DIR/insert_out.bin --no-verify 2>&1; then
        if cmp -s $TEST_DIR/insert2.bin $TEST_DIR/insert_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# 测试 5: 大块 COPY (10MB)
test_case "大块 COPY (10MB)"
dd if=/dev/urandom of=$TEST_DIR/large1.bin bs=1M count=10 2>/dev/null
cp $TEST_DIR/large1.bin $TEST_DIR/large2.bin
# 修改前 1MB
dd if=/dev/urandom of=$TEST_DIR/large2.bin bs=1M count=1 conv=notrunc 2>/dev/null
if $BINDIFF diff $TEST_DIR/large1.bin $TEST_DIR/large2.bin $TEST_DIR/large.bdp --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/large1.bin $TEST_DIR/large.bdp $TEST_DIR/large_out.bin --no-verify 2>&1; then
        if cmp -s $TEST_DIR/large2.bin $TEST_DIR/large_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# 测试 6: 文件大小增加
test_case "文件大小增加"
dd if=/dev/urandom of=$TEST_DIR/grow1.bin bs=1024 count=5 2>/dev/null
dd if=/dev/urandom of=$TEST_DIR/grow2.bin bs=1024 count=10 2>/dev/null
head -c 5120 $TEST_DIR/grow1.bin > $TEST_DIR/grow2.bin
if $BINDIFF diff $TEST_DIR/grow1.bin $TEST_DIR/grow2.bin $TEST_DIR/grow.bdp --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/grow1.bin $TEST_DIR/grow.bdp $TEST_DIR/grow_out.bin --no-verify 2>&1; then
        if cmp -s $TEST_DIR/grow2.bin $TEST_DIR/grow_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# 测试 7: 文件大小减少
test_case "文件大小减少"
dd if=/dev/urandom of=$TEST_DIR/shrink1.bin bs=1024 count=10 2>/dev/null
dd if=/dev/urandom of=$TEST_DIR/shrink2.bin bs=1024 count=5 2>/dev/null
head -c 5120 $TEST_DIR/shrink1.bin > $TEST_DIR/shrink2.bin
if $BINDIFF diff $TEST_DIR/shrink1.bin $TEST_DIR/shrink2.bin $TEST_DIR/shrink.bdp --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/shrink1.bin $TEST_DIR/shrink.bdp $TEST_DIR/shrink_out.bin --no-verify 2>&1; then
        if cmp -s $TEST_DIR/shrink2.bin $TEST_DIR/shrink_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# 测试 8: SHA256 校验
test_case "SHA256 校验"
dd if=/dev/urandom of=$TEST_DIR/hash1.bin bs=1024 count=10 2>/dev/null
dd if=/dev/urandom of=$TEST_DIR/hash2.bin bs=1024 count=10 2>/dev/null
if $BINDIFF diff $TEST_DIR/hash1.bin $TEST_DIR/hash2.bin $TEST_DIR/hash.bdp --progress 2>&1; then
    if $BINDIFF patch $TEST_DIR/hash1.bin $TEST_DIR/hash.bdp $TEST_DIR/hash_out.bin --progress 2>&1; then
        if cmp -s $TEST_DIR/hash2.bin $TEST_DIR/hash_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败 (校验未通过?)"
    fi
else
    fail "diff 命令失败"
fi

# 测试 9: info 命令
test_case "info 命令"
if $BINDIFF info $TEST_DIR/large.bdp 2>&1 | grep -q "块大小:"; then
    pass
else
    fail "info 输出格式错误"
fi

# 测试 10: 多线程
test_case "多线程 (4线程)"
dd if=/dev/urandom of=$TEST_DIR/mt1.bin bs=1M count=20 2>/dev/null
cp $TEST_DIR/mt1.bin $TEST_DIR/mt2.bin
dd if=/dev/urandom of=$TEST_DIR/mt2.bin bs=1M count=5 conv=notrunc 2>/dev/null
if $BINDIFF diff $TEST_DIR/mt1.bin $TEST_DIR/mt2.bin $TEST_DIR/mt.bdp --threads 4 --no-verify 2>&1; then
    if $BINDIFF patch $TEST_DIR/mt1.bin $TEST_DIR/mt.bdp $TEST_DIR/mt_out.bin --no-verify 2>&1; then
        if cmp -s $TEST_DIR/mt2.bin $TEST_DIR/mt_out.bin; then
            pass
        else
            fail "输出文件不匹配"
        fi
    else
        fail "patch 命令失败"
    fi
else
    fail "diff 命令失败"
fi

# ==================== 清理 ====================

echo ""
echo "========================================"
echo "测试结果: ${GREEN}通过 $PASS${NC} / ${RED}失败 $FAIL${NC}"
echo "========================================"

# 保留测试数据用于调试
# rm -rf $TEST_DIR

exit $FAIL
