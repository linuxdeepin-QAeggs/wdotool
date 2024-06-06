cmake_minimum_required(VERSION 3.0)

project(wdotoold)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
# 查找X11库
find_package(X11 REQUIRED)

include_directories(3rdparty/json-c/include) 

add_executable(wdotoold wdotoold.c)

link_directories(3rdparty/json-c/lib)

target_link_libraries(wdotoold ${CMAKE_SOURCE_DIR}/3rdparty/json-c/lib/libjson-c.a X11)