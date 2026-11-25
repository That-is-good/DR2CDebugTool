cmake_minimum_required(VERSION 3.16)

# ============================================================================
# DR2CAgent —— 注入到 32 位游戏进程内的无界面 Agent。
#
# 这是一个独立工程，必须用 32 位 MinGW 工具链构建（Qt 前端是 64 位）：
#   cmake -S Agent -B Agent/build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
#   cmake --build Agent/build -j
# 产物 DR2CAgent.dll 需与 DR2CDebugTool.exe 放在同一目录。
# ============================================================================

project(DR2CAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "DR2CAgent 必须使用 32 位 MinGW 工具链构建（当前指针大小为 ${CMAKE_SIZEOF_VOID_P}）。")
endif()

add_library(DR2CAgent SHARED
    dllmain.cpp
    agent_server.cpp
    agent_server.h
    agent_resolver.cpp
    agent_resolver.h
    agent_memory.h
    agent_offsets.h
    ../Common/protocol.h
)

target_include_directories(DR2CAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common
)

# 注入到游戏进程后不能依赖外部 MinGW 运行库，必须静态链接。
target_compile_options(DR2CAgent PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
)
target_link_options(DR2CAgent PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
)

set_target_properties(DR2CAgent PROPERTIES
    PREFIX ""              # 输出 DR2CAgent.dll 而不是 libDR2CAgent.dll
    OUTPUT_NAME "DR2CAgent"
)
