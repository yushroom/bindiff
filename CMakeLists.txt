cmake_minimum_required(VERSION 3.20)

project(bindiff 
    VERSION 1.0.0
    DESCRIPTION "Binary diff/patch tool for large files"
    LANGUAGES CXX C
)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 选项
option(BINDIFF_BUILD_TESTS "Build tests" ON)
option(BINDIFF_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BINDIFF_BUILD_EXAMPLES "Build examples" ON)

# 编译器特定选项
if(MSVC)
    # MSVC: 最高优化、多线程编译、警告级别4
    add_compile_options(/W4 /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # Release 配置
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /GL /Gy")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Clang/GCC: 最高优化、警告
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# 大文件支持
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)  # Windows 7+
endif()

# 线程库
find_package(Threads REQUIRED)

# ============== LZ4 (内嵌源码) ==============

# 如果 lib/lz4 目录存在，使用它
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/lz4/lib/lz4.c")
    add_library(lz4_static STATIC
        lib/lz4/lib/lz4.c
        lib/lz4/lib/lz4hc.c
    )
    target_include_directories(lz4_static PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/lz4/lib
    )
    set(LZ4_AVAILABLE ON)
else()
    # 尝试系统安装的 lz4
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LZ4 liblz4)
    endif()
    
    if(LZ4_FOUND)
        set(LZ4_AVAILABLE ON)
        message(STATUS "使用系统 LZ4: ${LZ4_LIBRARIES}")
    else()
        set(LZ4_AVAILABLE OFF)
        message(WARNING "LZ4 未找到，压缩功能将不可用")
    endif()
endif()

# ============== 主库 ==============

add_library(bindiff_lib STATIC
    # Core
    src/core/diff_engine.cpp
    src/core/patch_engine.cpp
    src/core/block_processor.cpp
    src/core/matcher.cpp
    src/core/operations.cpp
    
    # I/O
    src/io/mmap_file.cpp
    src/io/stream_writer.cpp
    src/io/file_utils.cpp
    
    # Compression (条件编译)
    src/compress/lz4_compressor.cpp
    
    # Crypto
    src/crypto/sha256.cpp
)

target_include_directories(bindiff_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(bindiff_lib
    PUBLIC 
        Threads::Threads
)

if(LZ4_AVAILABLE)
    if(TARGET lz4_static)
        target_link_libraries(bindiff_lib PRIVATE lz4_static)
        target_compile_definitions(bindiff_lib PRIVATE BINDIFF_HAS_LZ4=1)
    elseif(LZ4_FOUND)
        target_include_directories(bindiff_lib PRIVATE ${LZ4_INCLUDE_DIRS})
        target_link_libraries(bindiff_lib PRIVATE ${LZ4_LIBRARIES})
        target_compile_definitions(bindiff_lib PRIVATE BINDIFF_HAS_LZ4=1)
    endif()
else()
    target_compile_definitions(bindiff_lib PRIVATE BINDIFF_HAS_LZ4=0)
endif()

# ============== CLI 工具 ==============

add_executable(bindiff src/main.cpp)
target_link_libraries(bindiff PRIVATE bindiff_lib)

# ============== 测试 ==============

if(BINDIFF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============== 示例 ==============

if(BINDIFF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============== 安装 ==============

include(GNUInstallDirs)

install(TARGETS bindiff bindiff_lib
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

message(STATUS "")
message(STATUS "Binary Diff 配置:")
message(STATUS "  版本: ${PROJECT_VERSION}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  LZ4: ${LZ4_AVAILABLE}")
message(STATUS "  测试: ${BINDIFF_BUILD_TESTS}")
message(STATUS "")
