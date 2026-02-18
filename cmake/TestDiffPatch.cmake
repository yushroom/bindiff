# 集成测试: diff 和 patch 完整流程

# 创建测试文件
file(WRITE ${CMAKE_BINARY_DIR}/test_old.bin "Hello World! This is the old file.\n")
file(WRITE ${CMAKE_BINARY_DIR}/test_new.bin "Hello OpenClaw! This is the new file.\n")

# 执行 diff
execute_process(
    COMMAND ${TEST_PROGRAM} diff 
        ${CMAKE_BINARY_DIR}/test_old.bin
        ${CMAKE_BINARY_DIR}/test_new.bin
        ${CMAKE_BINARY_DIR}/test.bdp
    RESULT_VARIABLE diff_result
    OUTPUT_VARIABLE diff_output
    ERROR_VARIABLE diff_error
)

if(NOT diff_result EQUAL 0)
    message(FATAL_ERROR "diff 失败: ${diff_error}")
endif()

# 检查 patch 文件是否存在
if(NOT EXISTS ${CMAKE_BINARY_DIR}/test.bdp)
    message(FATAL_ERROR "patch 文件未创建")
endif()

# 执行 patch
execute_process(
    COMMAND ${TEST_PROGRAM} patch
        ${CMAKE_BINARY_DIR}/test_old.bin
        ${CMAKE_BINARY_DIR}/test.bdp
        ${CMAKE_BINARY_DIR}/test_output.bin
    RESULT_VARIABLE patch_result
    OUTPUT_VARIABLE patch_output
    ERROR_VARIABLE patch_error
)

if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "patch 失败: ${patch_error}")
endif()

# 验证输出文件
file(READ ${CMAKE_BINARY_DIR}/test_output.bin output_content)
file(READ ${CMAKE_BINARY_DIR}/test_new.bin expected_content)

if(NOT output_content STREQUAL expected_content)
    message(FATAL_ERROR "输出文件与预期不符")
endif()

message(STATUS "✓ 集成测试通过")
