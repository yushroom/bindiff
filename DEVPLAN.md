# Binary Diff/Patch Tool - 开发计划

> 针对 Unreal Engine 游戏包体的二进制差分工具  
> 目标: 50GB+ 文件，每日更新，高性能

---

## 项目概述

### 目标场景
- **用途**: Unreal Engine 游戏 `.pak` 文件增量更新
- **文件大小**: 50GB - 100GB
- **更新频率**: 每日
- **编译器支持**: MSVC (Windows), Clang (Linux/macOS)

### 性能目标

| 指标 | 目标值 |
|------|--------|
| 50GB Diff 时间 | < 10 分钟 (SSD, 16核) |
| 50GB Patch 时间 | < 3 分钟 |
| 内存占用 | < 2GB (固定缓冲区) |
| Patch 大小 | 差异部分 × 1.3 (高压缩) |
| 最小匹配长度 | 32 bytes |

---

## 技术选型

### 语言与标准
- **C++17** (跨平台, Unreal 兼容)

### 依赖库
| 依赖 | 用途 | 许可证 |
|------|------|--------|
| LZ4 | 压缩 (快速, UE原生支持) | BSD |
| SHA-256 | 校验和 | 内置实现 |
| cxxopts | CLI 参数解析 | MIT |

### 构建系统
- **CMake 3.20+**
- 支持 MSVC 2022, Clang 14+, GCC 11+

---

## 核心算法

### 1. 分块并行 Diff
```
50GB 文件 → 128 个 400MB blocks → 128 线程并行处理
每个 block 独立生成 diff，最后合并
```

### 2. Rabin-Karp 滚动哈希
- 快速计算滑动窗口指纹
- 时间复杂度: O(n)
- 用于快速匹配定位

### 3. 双向扫描优化
```
Step 1: 正向扫描 → 找公共前缀 (跳过)
Step 2: 反向扫描 → 找公共后缀 (跳过)
Step 3: 中间部分 → 滚动哈希 + 最长匹配
```

### 4. 内存映射 I/O
- **Windows**: `CreateFileMapping` + `MapViewOfFile`
- **Linux/macOS**: `mmap`
- 只映射当前处理的 block，用完 unmap

---

## Patch 文件格式

### 文件扩展名
`.bdp` (Binary Delta Patch)

### 二进制布局

```
┌─────────────────────────────────────┐
│          Header (64 bytes)          │
├─────────────────────────────────────┤
│          Block Index                │
│   (num_blocks × 8 bytes offsets)    │
├─────────────────────────────────────┤
│          Block 1 Data               │
│   (LZ4 compressed operations)       │
├─────────────────────────────────────┤
│          Block 2 Data               │
├─────────────────────────────────────┤
│          ...                        │
├─────────────────────────────────────┤
│          Block N Data               │
└─────────────────────────────────────┘
```

### Header 结构

| 字段 | 类型 | 大小 | 说明 |
|------|------|------|------|
| magic | char[4] | 4 | "UEBD" |
| version | uint16 | 2 | 格式版本 (1) |
| flags | uint16 | 2 | 保留 |
| block_size | uint32 | 4 | 块大小 (64MB) |
| old_size | uint64 | 8 | 原文件大小 |
| new_size | uint64 | 8 | 新文件大小 |
| num_blocks | uint32 | 4 | 块数量 |
| reserved | uint32 | 4 | 保留 |
| old_sha256 | uint8[32] | 32 | 原文件校验 |
| new_sha256 | uint8[32] | 32 | 新文件校验 |
| **Total** | | **64** | |

### 操作指令

| OpCode | 名称 | 格式 | 说明 |
|--------|------|------|------|
| 0x00 | COPY | `[offset:u64][length:u32]` | 从 old 复制 |
| 0x01 | INSERT | `[length:u32][data:bytes]` | 插入新数据 |

---

## 项目结构

```
bindiff/
├── CMakeLists.txt
├── CMakePresets.json           # MSVC/Clang 预设
├── LICENSE
├── README.md
│
├── include/
│   ├── bindiff.hpp             # 主接口
│   ├── core/
│   │   ├── diff_engine.hpp     # Diff 引擎
│   │   ├── patch_engine.hpp    # Patch 引擎
│   │   ├── block_processor.hpp # 分块处理器
│   │   ├── matcher.hpp         # 字符串匹配
│   │   └── operations.hpp      # 指令定义
│   ├── io/
│   │   ├── mmap_file.hpp       # 内存映射
│   │   ├── stream_writer.hpp   # 流式写入
│   │   └── file_utils.hpp      # 文件工具
│   ├── compress/
│   │   ├── compressor.hpp      # 压缩接口
│   │   └── lz4_compressor.hpp  # LZ4 实现
│   ├── crypto/
│   │   └── sha256.hpp          # SHA-256
│   └── utils/
│       ├── thread_pool.hpp     # 线程池
│       ├── types.hpp           # 类型定义
│       └── progress.hpp        # 进度回调
│
├── src/
│   ├── main.cpp                # CLI 入口
│   ├── core/
│   │   ├── diff_engine.cpp
│   │   ├── patch_engine.cpp
│   │   ├── block_processor.cpp
│   │   ├── matcher.cpp
│   │   └── operations.cpp
│   ├── io/
│   │   ├── mmap_file_win.cpp   # Windows mmap
│   │   ├── mmap_file_posix.cpp # Linux/macOS mmap
│   │   ├── stream_writer.cpp
│   │   └── file_utils.cpp
│   ├── compress/
│   │   └── lz4_compressor.cpp
│   └── crypto/
│       └── sha256.cpp
│
├── lib/
│   ├── lz4/                    # git submodule
│   └── cxxopts/                # git submodule
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   ├── test_mmap.cpp
│   ├── test_matcher.cpp
│   ├── test_compress.cpp
│   ├── test_diff_patch.cpp
│   └── benchmark/
│       ├── bench_large.cpp     # 大文件基准
│       └── bench_50gb.cpp
│
├── examples/
│   ├── simple_usage.cpp
│   └── unreal_plugin/
│       ├── BindiffPlugin.uplugin
│       ├── Source/
│       │   └── BindiffPlugin/
│       │       ├── Public/
│       │       └── Private/
│       └── README.md
│
└── docs/
    ├── API.md
    ├── FORMAT.md
    └── INTEGRATION.md
```

---

## 开发阶段

### Phase 1: 基础框架 (3 天)

**目标**: 项目骨架可编译运行

- [ ] 创建项目结构
- [ ] CMakeLists.txt + CMakePresets.json
- [ ] 第三方库集成 (LZ4, cxxopts)
- [ ] `MMapFile` 实现 (Windows + Linux)
- [ ] `ThreadPool` 实现
- [ ] 基础类型定义 (`types.hpp`)
- [ ] CLI 框架 (`main.cpp`)
- [ ] 验证编译: MSVC + Clang

**验收标准**:
```bash
mkdir build && cd build
cmake .. --preset msvc-release
cmake --build .
./bindiff --help
```

---

### Phase 2: 核心算法 (4 天)

**目标**: 单块 diff/patch 可用

- [ ] `PatchHeader` 结构 + 序列化
- [ ] `Operation` (COPY/INSERT) 定义
- [ ] `RollingHash` 滚动哈希实现
- [ ] `BlockMatcher` 最长匹配算法
- [ ] `DiffEngine` 单块版本
- [ ] `PatchEngine` 单块版本
- [ ] `LZ4Compressor` 实现
- [ ] `SHA256` 实现
- [ ] 单元测试 (`test_matcher`, `test_compress`)

**验收标准**:
```bash
# 100MB 测试
./bindiff diff test_old.bin test_new.bin test.bdp
./bindiff patch test_old.bin test.bdp test_out.bin
diff test_new.bin test_out.bin  # 应该相同
```

---

### Phase 3: 多块并行 (3 天)

**目标**: 支持大文件

- [ ] `BlockProcessor` 分块逻辑
- [ ] 并行 diff 实现
- [ ] Patch 索引生成
- [ ] 并行 patch 实现
- [ ] 流式写入 (避免内存爆炸)
- [ ] 进度回调机制
- [ ] 10GB 文件测试

**验收标准**:
```bash
# 10GB 测试
./bindiff diff 10gb_old.pak 10gb_new.pak patch.bdp --progress
# 应在 2 分钟内完成，内存 < 1GB
```

---

### Phase 4: 优化与稳定性 (3 天)

**目标**: 50GB+ 生产可用

- [ ] IO 优化 (预读、缓冲策略)
- [ ] 内存占用优化
- [ ] 错误处理 + 边界情况
- [ ] 断点续传支持 (可选)
- [ ] 50GB 基准测试
- [ ] 性能报告生成
- [ ] Windows/Linux 双平台测试

**验收标准**:
```bash
# 50GB 测试
./bindiff diff 50gb_old.pak 50gb_new.pak patch.bdp \
    --threads 16 --progress
# 应在 10 分钟内完成，内存 < 2GB
```

---

### Phase 5: 文档与集成 (2 天)

**目标**: 可交付

- [ ] API 文档 (`docs/API.md`)
- [ ] Patch 格式文档 (`docs/FORMAT.md`)
- [ ] 使用示例 (`examples/`)
- [ ] C API 导出 (供其他语言调用)
- [ ] DLL/SO 构建
- [ ] Unreal 插件示例
- [ ] README 完善

**验收标准**:
- 新用户能按文档编译使用
- Unreal 项目能集成示例插件

---

## CLI 接口设计

### 命令概览

```bash
bindiff <command> [options]

Commands:
  diff    Create a patch from old to new file
  patch   Apply a patch to create new file
  verify  Verify patch correctness
  info    Show patch file information
```

### diff 命令

```bash
bindiff diff <old_file> <new_file> <patch_file> [options]

Options:
  -t, --threads <N>      Number of threads (default: auto)
  -b, --block-size <MB>  Block size in MB (default: 64)
  -c, --compress <0-12>  LZ4 compression level (default: 1)
  --no-verify           Skip checksum verification
  --progress            Show progress bar
  -v, --verbose         Verbose output
  -h, --help            Show help
```

### patch 命令

```bash
bindiff patch <old_file> <patch_file> <new_file> [options]

Options:
  --no-verify           Skip checksum verification
  --progress            Show progress bar
  -v, --verbose         Verbose output
  -h, --help            Show help
```

### verify 命令

```bash
bindiff verify <old_file> <new_file> <patch_file>

验证 patch 是否正确：
1. 验证 old_file 的 SHA256
2. 应用 patch 生成临时文件
3. 验证临时文件 SHA256 == new_file SHA256
```

### info 命令

```bash
bindiff info <patch_file>

输出示例:
Patch File: v1.0_to_v1.1.bdp
Version:    1
Block Size: 64 MB
Old Size:   52.3 GB (SHA256: a1b2c3...)
New Size:   52.5 GB (SHA256: d4e5f6...)
Num Blocks: 820
Patch Size: 1.2 GB
Compression: LZ4 level 1
```

---

## C++ API 设计

### 主接口

```cpp
#include <bindiff.hpp>

namespace bindiff {

struct DiffOptions {
    uint32_t block_size = 64 * 1024 * 1024;  // 64MB
    int compression_level = 1;                // LZ4: 1-12
    int num_threads = 0;                      // 0 = auto
    bool verify = true;
};

struct PatchOptions {
    bool verify = true;
};

struct Result {
    bool success = false;
    std::string error;
    size_t bytes_processed = 0;
    double elapsed_seconds = 0.0;
};

class ProgressCallback {
public:
    virtual ~ProgressCallback() = default;
    virtual void on_progress(float percent, const char* stage) = 0;
    virtual void on_complete(const Result& result) = 0;
};

// 核心函数
Result create_diff(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path,
    const DiffOptions& options = {},
    ProgressCallback* callback = nullptr
);

Result apply_patch(
    const std::string& old_path,
    const std::string& patch_path,
    const std::string& new_path,
    const PatchOptions& options = {},
    ProgressCallback* callback = nullptr
);

Result verify_patch(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path
);

PatchInfo get_patch_info(const std::string& patch_path);

} // namespace bindiff
```

### C API (供 Unreal 等调用)

```cpp
#ifdef __cplusplus
extern "C" {
#endif

// 句柄
typedef void* bindiff_handle_t;

// 创建/销毁
bindiff_handle_t bindiff_create();
void bindiff_destroy(bindiff_handle_t handle);

// 设置回调
typedef void (*progress_callback_t)(float percent, const char* stage);
void bindiff_set_progress_callback(bindiff_handle_t handle, 
                                    progress_callback_t callback);

// 核心操作
int bindiff_diff(bindiff_handle_t handle,
                 const char* old_path,
                 const char* new_path,
                 const char* patch_path);

int bindiff_patch(bindiff_handle_t handle,
                  const char* old_path,
                  const char* patch_path,
                  const char* new_path);

int bindiff_verify(bindiff_handle_t handle,
                   const char* old_path,
                   const char* new_path,
                   const char* patch_path);

// 错误信息
const char* bindiff_get_error(bindiff_handle_t handle);

#ifdef __cplusplus
}
#endif
```

---

## 性能基准

### 测试环境
- **CPU**: Intel i7-8700K (6核12线程) 或 AMD Ryzen 9 5900X
- **内存**: 32GB DDR4
- **存储**: NVMe SSD (读取 3000MB/s+)
- **OS**: Windows 10/11, Ubuntu 22.04

### 预期性能

| 文件大小 | Diff 时间 | Patch 时间 | 内存峰值 | Patch 大小* |
|----------|-----------|------------|----------|-------------|
| 1 GB | 15s | 5s | 500 MB | 差异 × 1.2 |
| 10 GB | 2min | 30s | 1 GB | 差异 × 1.3 |
| 50 GB | 8min | 2min | 1.5 GB | 差异 × 1.3 |
| 100 GB | 15min | 4min | 2 GB | 差异 × 1.3 |

*Patch 大小取决于实际差异比例，假设 5% 内容变更

### 压缩效果对比

| 数据类型 | 无压缩 | LZ4-1 | LZ4-9 | Zstd-3 |
|----------|--------|-------|-------|--------|
| 可执行代码 | 100% | 45% | 40% | 35% |
| 纹理图像 | 100% | 85% | 80% | 75% |
| 音频数据 | 100% | 95% | 92% | 90% |
| 混合 Pak | 100% | 70% | 65% | 60% |

**选择 LZ4 的原因**: 解压速度极快 (500MB/s+)，适合游戏运行时 patch

---

## 风险与应对

### 风险 1: 50GB 文件内存溢出
- **应对**: 严格使用分块处理，每块处理完立即释放
- **验证**: 单元测试监控内存峰值

### 风险 2: Windows/Linux 路径差异
- **应对**: 统一使用 `std::filesystem::path`
- **验证**: 双平台 CI 测试

### 风险 3: 线程竞争
- **应对**: 每个线程独立数据块，无共享写入
- **验证**: ThreadSanitizer 检测

### 风险 4: Patch 文件损坏
- **应对**: 
  1. Header 包含 SHA256 校验
  2. 每 block 独立校验
  3. 可选的纠错码 (未来)

---

## 里程碑

| 里程碑 | 完成标准 | 预计日期 |
|--------|----------|----------|
| M1: 框架可用 | 编译通过，CLI --help | Day 3 |
| M2: 单块可用 | 100MB 文件 diff/patch | Day 7 |
| M3: 大文件可用 | 10GB 文件 diff/patch | Day 10 |
| M4: 生产可用 | 50GB 文件 < 10min | Day 13 |
| M5: 交付就绪 | 文档完整 + 示例 | Day 15 |

---

## 后续规划

### v1.1 (可选功能)
- [ ] 断点续传
- [ ] 增量链 (1.0→1.1→1.2)
- [ ] HTTP/HTTPS 直接下载 patch
- [ ] Zstd 压缩选项

### v2.0 (高级功能)
- [ ] Content-aware diff (解析 pak 结构)
- [ ] 差异可视化工具
- [ ] 服务端 patch 生成服务

---

## 参考资料

- [bsdiff 论文](http://www.daemonology.net/papers/bsdiff.pdf) - Colin Percival
- [VCDIFF RFC 3284](https://tools.ietf.org/html/rfc3284)
- [LZ4 压缩算法](https://github.com/lz4/lz4)
- [Rabin-Karp 算法](https://en.wikipedia.org/wiki/Rabin%E2%80%93Karp_algorithm)

---

*文档版本: 1.0*  
*创建日期: 2026-02-17*  
*作者: yushroom + OpenClaw*
