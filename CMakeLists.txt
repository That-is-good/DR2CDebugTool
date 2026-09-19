cmake_minimum_required(VERSION 3.19)
project(DR2CInternalOverlay LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "DR2CInternalOverlay must be built with a 32-bit MinGW toolchain")
endif()

set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/imgui-1.92.9b")
set(MINHOOK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/MinHook")

add_library(DR2CInternalOverlay SHARED
    dllmain.cpp
    imgui_ui.cpp
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_win32.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(DR2CInternalOverlay PRIVATE
    ${IMGUI_ROOT}
    ${IMGUI_ROOT}/backends
    ${MINHOOK_ROOT}/include
)

target_link_libraries(DR2CInternalOverlay PRIVATE
    "${MINHOOK_ROOT}/lib/MinHook.a"
    opengl32
    gdi32
    user32
    dwmapi
    gdiplus
)

target_compile_features(DR2CInternalOverlay PRIVATE cxx_std_17)
target_compile_options(DR2CInternalOverlay PRIVATE -m32 -O2 -Wall -Wextra -fno-exceptions -fno-rtti)
target_link_options(DR2CInternalOverlay PRIVATE
    -m32
    -static
    -static-libgcc
    -static-libstdc++
    -Wl,--enable-stdcall-fixup
)

set_target_properties(DR2CInternalOverlay PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DR2CInternalOverlay"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET DR2CInternalOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Icons"
        "$<TARGET_FILE_DIR:DR2CInternalOverlay>/Icons"
)