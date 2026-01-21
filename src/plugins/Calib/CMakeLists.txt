cmake_minimum_required(VERSION 3.16)

project(CalibPlugin)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(PLUGIN_SOURCES
    Calib_plugin.h
    CalibWidget.cpp
    CalibWidget.h
    Mirrorcalib/core/MirrorCalib.cpp
    Mirrorcalib/core/MirrorCalib.h
    Mirrorcalib/Widget/MirrorCalibWidget.cpp
    Mirrorcalib/Widget/MirrorCalibWidget.h
    Mirrorcalib/Scene/MirrorScene.cpp
    Mirrorcalib/Scene/MirrorScene.h
    CameraCalib/core/Calib.cpp
    CameraCalib/core/Calib.h
    CameraCalib/Widget/CameraCalibWidget.cpp
    CameraCalib/Widget/CameraCalibWidget.h
    CameraCalib/Scene/CameraCalibScene.cpp
    CameraCalib/Scene/CameraCalibScene.h
)

set_source_files_properties(
    CalibWidget.h
    PROPERTIES SKIP_AUTOMOC OFF
)

add_library(CalibPlugin SHARED ${PLUGIN_SOURCES})
set_target_properties(CalibPlugin PROPERTIES
    AUTOMOC ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(CalibPlugin SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(CalibPlugin PRIVATE
    GuiCommon
    Qt5::Widgets
)

target_compile_definitions(CalibPlugin PRIVATE CalibPLUGIN_LIBRARY)
