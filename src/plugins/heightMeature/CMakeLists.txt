cmake_minimum_required(VERSION 3.16)

project(HeightMeaturePlugin)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

if(NOT TARGET BSCV::BSCV)
    find_package(BSCV REQUIRED)
endif()

set(PLUGIN_SOURCES
    core/testHeight.cpp
    core/testHeight.h
    Scene/HeightScene.cpp
    Scene/HeightScene.h
    Widget/HeightMainWindow.cpp
    Widget/HeightMainWindow.h
    HeightMeaturePlugin.h
    height_meature_plugin.json
)

set_source_files_properties(
    Widget/HeightMainWindow.h
    Scene/HeightScene.h
    PROPERTIES SKIP_AUTOMOC OFF
)

add_library(HeightMeaturePlugin SHARED ${PLUGIN_SOURCES})

set_target_properties(HeightMeaturePlugin PROPERTIES
    AUTOMOC ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(HeightMeaturePlugin SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src
    ${MVS_INCLUDE_DIR}
    ${BSCV_INCLUDE_DIRS}
)

target_link_libraries(HeightMeaturePlugin PRIVATE
    GuiCommon
    Qt5::Widgets
    ${BSCV_LIBRARIES}
)

target_compile_definitions(HeightMeaturePlugin PRIVATE HEIGHTMEATURE_LIBRARY)
