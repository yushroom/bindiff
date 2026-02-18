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
    add_compile_options(/W4 /MP)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# 线程库
find_package(Threads REQUIRED)

# ============== LZ4 ==============

# 检查本地 LZ4
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/lz4/include/lz4.h")
    add_library(lz4_static STATIC
        lib/lz4/lib/liblz4.a
    )
    target_include_directories(lz4_static INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/lz4/include
    )
    set(LZ4_AVAILABLE ON)
    set(LZ4_LINK lz4_static)
    message(STATUS "使用本地 LZ4")
else()
    # 尝试系统 LZ4
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LZ4 liblz4)
    endif()
    
    if(LZ4_FOUND)
        set(LZ4_AVAILABLE ON)
        set(LZ4_LINK ${LZ4_LIBRARIES})
        message(STATUS "使用系统 LZ4: ${LZ4_LIBRARIES}")
    else()
        set(LZ4_AVAILABLE OFF)
        set(LZ4_LINK "")
        message(WARNING "LZ4 未找到，压缩功能将不可用")
    endif()
endif()

# ============== 主库 ==============

add_library(bindiff_lib STATIC
    src/bindiff.cpp
    src/core/diff_engine.cpp
    src/core/patch_engine.cpp
    src/core/patch_format.cpp
    src/core/block_processor.cpp
    src/core/matcher.cpp
    src/core/operations.cpp
    src/io/mmap_file.cpp
    src/io/stream_writer.cpp
    src/io/file_utils.cpp
    src/compress/lz4_compressor.cpp
    src/crypto/sha256.cpp
    src/utils/thread_pool.cpp
)

target_include_directories(bindiff_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(bindiff_lib
    PUBLIC Threads::Threads
)

if(LZ4_AVAILABLE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lib/lz4/include/lz4.h")
        target_include_directories(bindiff_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/lz4/include)
    elseif(LZ4_FOUND)
        target_include_directories(bindiff_lib PRIVATE ${LZ4_INCLUDE_DIRS})
    endif()
    target_compile_definitions(bindiff_lib PRIVATE BINDIFF_HAS_LZ4=1)
    target_link_libraries(bindiff_lib PRIVATE ${LZ4_LINK})
else()
    target_compile_definitions(bindiff_lib PRIVATE BINDIFF_HAS_LZ4=0)
endif()

# ============== CLI 工具 ==============

add_executable(bindiff src/main.cpp)
target_link_libraries(bindiff PRIVATE bindiff_lib)

# ============== 测试 ==============

if(BINDIFF_BUILD_TESTS)
    enable_testing()
    
    add_executable(bindiff_tests
        tests/test_main.cpp
        tests/test_mmap.cpp
        tests/test_matcher.cpp
        tests/test_operations.cpp
        tests/test_compress.cpp
    )
    target_link_libraries(bindiff_tests PRIVATE bindiff_lib)
    
    add_test(NAME bindiff_tests COMMAND bindiff_tests)
    
    # 添加集成测试
    add_test(NAME integration_diff_patch
        COMMAND ${CMAKE_COMMAND} 
            -DTEST_PROGRAM=$<TARGET_FILE:bindiff>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestDiffPatch.cmake
    )
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
