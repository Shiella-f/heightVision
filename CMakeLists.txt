cmake_minimum_required(VERSION 3.16)
project(QtVscodeSample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出编译命令数据库，方便 VS Code 和 IntelliSense 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Qt automoc/autouic to generate moc files for classes with Q_OBJECT
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/src/plugins/template/tools")

find_package(Qt5 COMPONENTS Widgets Concurrent Multimedia REQUIRED)
find_package(BSCV REQUIRED)

add_subdirectory(src/common)
add_subdirectory(src/plugins/template)
add_subdirectory(src/plugins/heightMeature)
add_subdirectory(src/plugins/Calib)


file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*.c)
set(PROJECT_SOURCES "")
foreach(SRC ${ALL_SOURCES})
    file(TO_CMAKE_PATH "${SRC}" SRC_NORMALIZED)
    if(NOT SRC_NORMALIZED MATCHES "/plugins/" AND NOT SRC_NORMALIZED MATCHES "/common/")
        list(APPEND PROJECT_SOURCES ${SRC})
    endif()
endforeach()

file(GLOB_RECURSE PLUGIN_HEADERS ${CMAKE_SOURCE_DIR}/src/plugins/*.h)
set_source_files_properties(${PLUGIN_HEADERS} PROPERTIES SKIP_AUTOMOC ON)

add_executable(qt_vscode_app ${PROJECT_SOURCES} ${PLUGIN_HEADERS})

target_include_directories(qt_vscode_app SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/src/plugins
)

target_include_directories(qt_vscode_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${BSCV_INCLUDE_DIRS}
)


target_link_libraries(qt_vscode_app PRIVATE Qt5::Widgets Qt5::Concurrent Qt5::Multimedia ${BSCV_LIBRARIES} GuiCommon HeightMeaturePlugin)





if(BSCV_DLLS)
    add_custom_command(TARGET qt_vscode_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BSCV_DLLS} $<TARGET_FILE_DIR:qt_vscode_app>
        COMMENT "复制 BSCV 运行时 DLL 到输出目录"
    )
endif()

add_custom_command(TARGET qt_vscode_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:TemplateMatchPlugin> $<TARGET_FILE_DIR:qt_vscode_app>
    COMMENT "复制 TemplateMatchPlugin DLL 到输出目录"
)

add_custom_command(TARGET qt_vscode_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:HeightMeaturePlugin> $<TARGET_FILE_DIR:qt_vscode_app>
    COMMENT "复制 HeightMeaturePlugin DLL 到输出目录"
)

add_custom_command(TARGET qt_vscode_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:CalibPlugin> $<TARGET_FILE_DIR:qt_vscode_app>
    COMMENT "复制 CalibPlugin DLL 到输出目录"
)
