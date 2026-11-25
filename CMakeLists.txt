cmake_minimum_required(VERSION 3.19)
project(DR2CDebugTool LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)
include(CTest)

qt_standard_project_setup()

qt_add_executable(DR2CDebugTool
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    Memory/memorymanager.h
    Memory/memorymanager.cpp
    Memory/gameresolver.h
    Memory/gamedatareader.h
    Memory/gamedatareader.cpp
    Interaction/gameoverlay.h
    Interaction/gameoverlay.cpp
    Common/protocol.h
    Delegates/spinboxdelegate.h
    Delegates/spinboxdelegate.cpp
    WeaponDialog/weapondialog.h
    WeaponDialog/weapondialog.cpp
    Spawn/spriteatlas.h
    Spawn/spriteatlas.cpp
    Spawn/spawndialog.h
    Spawn/spawndialog.cpp
    Setting/addrsetting.h
    Setting/addrsetting.cpp
    Setting/addrsetting.ui
    resource.qrc

)

qt_add_translations(
    TARGETS DR2CDebugTool
    TS_FILES Languages/DR2CDebugTool_zh_CN.ts
    TS_FILES Languages/DR2CDebugTool_ja.ts
    TS_FILES Languages/DR2CDebugTool_en.ts
)

target_link_libraries(DR2CDebugTool
    PRIVATE
        Qt::Core
        Qt::Widgets
)

# Windows 可执行文件图标（资源管理器/任务栏里显示的 exe 图标）
if(WIN32)
    enable_language(RC)
    target_sources(DR2CDebugTool PRIVATE appicon.rc)
endif()

# Common/protocol.h 同时被 64 位前端与 32 位 Agent 使用
target_include_directories(DR2CDebugTool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 可选：把预先构建好的 DR2CAgent.dll 自动复制到可执行文件目录。
# 在 Qt Creator 的 CMake 配置里设置 DR2C_AGENT_DLL 指向 Agent 构建产物即可。
set(DR2C_AGENT_DLL "" CACHE FILEPATH "预构建的 DR2CAgent.dll 路径（可选）")
if(DR2C_AGENT_DLL)
    add_custom_command(TARGET DR2CDebugTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${DR2C_AGENT_DLL}"
            "$<TARGET_FILE_DIR:DR2CDebugTool>/DR2CAgent.dll"
        COMMENT "复制 DR2CAgent.dll 到输出目录"
    )
endif()

install(TARGETS DR2CDebugTool
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET DR2CDebugTool
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})