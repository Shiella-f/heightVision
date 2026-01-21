cmake_minimum_required(VERSION 3.16)

project(GuiCommon)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(BSCV REQUIRED)

set(COMMON_SOURCES
    Scene/ImageDisplayScene.cpp
    Scene/ImageDisplayScene.h
    Scene/ImageSceneBase.cpp
    Scene/ImageSceneBase.h
    tools/bscvTool.cpp
    tools/bscvTool.h
    Widget/CustomTitleBar.cpp
    Widget/CustomTitleBar.h
    Widget/baseWidget.cpp
    Widget/baseWidget.h 
)

add_library(GuiCommon STATIC ${COMMON_SOURCES})

target_link_libraries(GuiCommon PUBLIC Qt5::Widgets ${BSCV_LIBRARIES})

target_include_directories(GuiCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${BSCV_INCLUDE_DIRS})
