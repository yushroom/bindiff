cmake_minimum_required(VERSION 3.20)

# 简单示例
add_executable(simple_usage simple_usage.cpp)
target_link_libraries(simple_usage PRIVATE bindiff_lib)
