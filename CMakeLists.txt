cmake_minimum_required(VERSION 3.19)
project(DR2CDebugTool LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(DR2CDebugTool
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    Struct/struct.cpp
    Memory/memorymanager.h
    Memory/memorymanager.cpp
    Memory/gamedatareader.h
    Memory/gamedatareader.cpp
    Memory/spinboxdelegate.h
    Memory/spinboxdelegate.cpp
    Memory/weapondialog.h
    Memory/weapondialog.cpp
)

qt_add_translations(
    TARGETS DR2CDebugTool
    TS_FILES DR2CDebugTool_zh_CN.ts
)

target_link_libraries(DR2CDebugTool
    PRIVATE
        Qt::Core
        Qt::Widgets
)

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