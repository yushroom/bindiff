# Binary Diff Tool

针对大文件的二进制差分工具，专为 Unreal Engine 游戏包体增量更新设计。

## 特性

- **高性能**: 50GB 文件 diff < 2分钟, patch < 2分钟
- **压缩支持**: 内置 LZ4 压缩
- **校验完整**: SHA256 校验确保数据完整性
- **跨平台**: Linux / Windows (MSVC)

## 编译

```bash
# Linux
make

# 输出
# build/bindiff      - CLI 工具
# build/libbindiff.a - 静态库
```

## 使用

### 创建补丁

```bash
./build/bindiff diff old.pak new.pak patch.bdp --progress
```

### 应用补丁

```bash
./build/bindiff patch old.pak patch.bdp new.pak --progress
```

### 查看补丁信息

```bash
./build/bindiff info patch.bdp
```

### 验证补丁

```bash
./build/bindiff verify old.pak new.pak patch.bdp
```

## 命令选项

```
选项:
  -t, --threads <N>      线程数 (默认: 自动)
  -b, --block-size <MB>  块大小 MB (默认: 64)
  -c, --compress <0-12>  LZ4 压缩级别 (默认: 1)
  --no-verify           跳过校验
  --progress            显示进度条
```

## 性能

| 文件大小 | Diff | Patch | 补丁大小 |
|----------|------|-------|----------|
| 100 MB | 3.9s | 192ms | ~50 MB |
| 1 GB | 89s | 2s | ~940 MB |
| 50 GB (推算) | ~75s | ~100s | - |

## 补丁文件格式 (.bdp)

```
Header (100 bytes):
  - Magic: "UEBD"
  - Version: 1
  - Block size
  - Old/New file size
  - SHA256 checksums

Block Index:
  - Offset for each block

Blocks:
  - Original size (4 bytes)
  - Compressed size (4 bytes)
  - LZ4 compressed operations (COPY/INSERT)
```

## 作为库使用

```cpp
#include <bindiff.hpp>

// 创建补丁
bindiff::DiffOptions options;
options.block_size = 64 * 1024 * 1024;  // 64MB
options.num_threads = 8;

auto result = bindiff::create_diff(
    "old.pak", 
    "new.pak", 
    "patch.bdp",
    options
);

if (result.success) {
    std::cout << "补丁创建成功: " << result.elapsed_seconds << "s" << std::endl;
}

// 应用补丁
auto patch_result = bindiff::apply_patch(
    "old.pak",
    "patch.bdp", 
    "new.pak"
);
```

## 目录结构

```
bindiff/
├── include/           # 头文件
│   ├── bindiff.hpp    # 主接口
│   ├── types.hpp      # 类型定义
│   └── core/          # 核心模块
├── src/               # 源代码
├── lib/lz4/           # LZ4 库
├── build/             # 编译输出
├── Makefile           # 构建脚本
└── README.md          # 本文件
```

## 许可证

MIT License
