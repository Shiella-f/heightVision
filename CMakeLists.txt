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

find_package(Qt6 COMPONENTS Widgets Concurrent Multimedia MultimediaWidgets Network REQUIRED)
find_package(BSCV REQUIRED)

# ========================
# GuiCommon (static)
# ========================
set(COMMON_SOURCES
    src/common/Scene/ImageDisplayScene.cpp
    src/common/Scene/ImageDisplayScene.h
    src/common/Scene/ImageSceneBase.cpp
    src/common/Scene/ImageSceneBase.h
    src/common/tools/bscvTool.cpp
    src/common/tools/bscvTool.h
)

add_library(GuiCommon STATIC ${COMMON_SOURCES})

target_include_directories(GuiCommon PUBLIC
    ${CMAKE_SOURCE_DIR}/src/common
    ${BSCV_INCLUDE_DIRS}
)

target_link_libraries(GuiCommon PUBLIC
    Qt6::Widgets
    ${BSCV_LIBRARIES}
)

# ========================
# CalibPlugin (shared)
# ========================
set(CALIB_PLUGIN_SOURCES
    src/plugins/Calib/PostPerspectiveRoi.cpp
    src/plugins/Calib/PostPerspectiveRoi.h
    src/plugins/Calib/PostPerspectiveRoiPort.h
    src/plugins/Calib/PlaneModel.cpp
    src/plugins/Calib/PlaneModel.h
    src/plugins/Calib/PlaneCalibration.cpp
    src/plugins/Calib/PlaneCalibrationPort.h
    src/plugins/Calib/BoardDetection.cpp
    src/plugins/Calib/BoardDetection.h
    src/plugins/Calib/PerspectiveRemap.cpp
    src/plugins/Calib/PerspectiveRemap.h
    src/plugins/Calib/PerspectiveFitReport.h
    src/plugins/Calib/MeasurementCalibration.cpp
    src/plugins/Calib/MeasurementCalibration.h
    src/plugins/Calib/Mirrorcalib/Core/MirrorCalib.cpp
    src/plugins/Calib/Mirrorcalib/Core/MirrorCalib.h
    src/plugins/Calib/Mirrorcalib/Function/MirrorCalibFunction.cpp
    src/plugins/Calib/Mirrorcalib/Function/MirrorCalibFunction.h
    src/plugins/Calib/Mirrorcalib/Scene/MirrorScene.cpp
    src/plugins/Calib/Mirrorcalib/Scene/MirrorScene.h
    src/plugins/Calib/CameraCalib/Core/Calib.cpp
    src/plugins/Calib/CameraCalib/Core/Calib.h
    src/plugins/Calib/CameraCalib/Function/CameraCalibFunction.cpp
    src/plugins/Calib/CameraCalib/Function/CameraCalibFunction.h
    src/plugins/Calib/CameraCalib/Scene/CameraCalibScene.cpp
    src/plugins/Calib/CameraCalib/Scene/CameraCalibScene.h
    src/plugins/Calib/CameraWarperspect/Core/Warpective.cpp
    src/plugins/Calib/CameraWarperspect/Core/Warpective.h
    src/plugins/Calib/CameraWarperspect/Function/CameraWarpectiveFunction.cpp
    src/plugins/Calib/CameraWarperspect/Function/CameraWarpectiveFunction.h
    src/plugins/Calib/CameraWarperspect/Function/CameraWarptivePort.h
    src/plugins/Calib/CameraWarperspect/Scene/CameraWarpectiveScene.cpp
    src/plugins/Calib/CameraWarperspect/Scene/CameraWarpectiveScene.h
)

set(CALIB_PLUGIN_MOC_HEADERS
    src/plugins/Calib/Mirrorcalib/Core/MirrorCalib.h
    src/plugins/Calib/Mirrorcalib/Function/MirrorCalibFunction.h
    src/plugins/Calib/Mirrorcalib/Scene/MirrorScene.h
    src/plugins/Calib/CameraCalib/Core/Calib.h
    src/plugins/Calib/CameraCalib/Function/CameraCalibFunction.h
    src/plugins/Calib/CameraCalib/Scene/CameraCalibScene.h
    src/plugins/Calib/CameraWarperspect/Core/Warpective.h
    src/plugins/Calib/CameraWarperspect/Function/CameraWarpectiveFunction.h
    src/plugins/Calib/CameraWarperspect/Scene/CameraWarpectiveScene.h
)

qt6_wrap_cpp(CALIB_PLUGIN_MOC_SOURCES ${CALIB_PLUGIN_MOC_HEADERS})

add_library(CalibPlugin SHARED ${CALIB_PLUGIN_SOURCES} ${CALIB_PLUGIN_MOC_SOURCES})

set_target_properties(CalibPlugin PROPERTIES
    AUTOMOC OFF
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(CalibPlugin SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/src/plugins/Calib
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(CalibPlugin PRIVATE
    GuiCommon
    Qt6::Widgets
)

target_compile_definitions(CalibPlugin PRIVATE CalibPLUGIN_LIBRARY)
target_sources(CalibPlugin PRIVATE src/plugins/Calib/BoardConfiguration.cpp
    src/plugins/Calib/BoardConfiguration.h src/plugins/Calib/BoardConfigurationPort.h)
target_compile_options(CalibPlugin PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# ========================
# HeightMeaturePlugin (shared)
# ========================
set(HEIGHT_PLUGIN_SOURCES
    src/plugins/heightMeature/core/testHeight.cpp
    src/plugins/heightMeature/core/testHeight.h
    src/plugins/heightMeature/Scene/HeightScene.cpp
    src/plugins/heightMeature/Scene/HeightScene.h
    src/plugins/heightMeature/Function/HeightFunction.cpp
    src/plugins/heightMeature/Function/HeightFunction.h
    src/plugins/heightMeature/Function/HeightTestPort.h

)

add_library(HeightMeaturePlugin SHARED ${HEIGHT_PLUGIN_SOURCES})

set_target_properties(HeightMeaturePlugin PROPERTIES
    AUTOMOC ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_include_directories(HeightMeaturePlugin SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/src/plugins/heightMeature
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_SOURCE_DIR}/src
    ${MVS_INCLUDE_DIR}
    ${BSCV_INCLUDE_DIRS}
)

target_link_libraries(HeightMeaturePlugin PRIVATE
    GuiCommon
    Qt6::Widgets
    ${BSCV_LIBRARIES}
)

target_compile_definitions(HeightMeaturePlugin PRIVATE HEIGHTMEATURE_LIBRARY)

# ========================
# TemplateMatchPlugin (shared)
# ========================
set(TEMPLATE_PLUGIN_SOURCES
    src/plugins/template/core/TemplateManager.cpp
    src/plugins/template/core/TemplateManager.h
    src/plugins/template/core/MatchParams.h
    # src/plugins/template/core/client/UDPMatReceiver.cpp
    # src/plugins/template/core/client/UDPMatReceiver.h
    src/plugins/template/Function/matchFunction.cpp
    src/plugins/template/Function/matchFunction.h
    src/plugins/template/Function/matchPort.h
    src/plugins/template/Scene/MatchScene.cpp
    src/plugins/template/Scene/MatchScene.h
    src/plugins/template/camera/ImageProcess.cpp
    src/plugins/template/camera/ImageProcess.h
    src/plugins/template/camera/ImageprocessPort.h
)

add_library(TemplateMatchPlugin SHARED ${TEMPLATE_PLUGIN_SOURCES} ${TEMPLATE_PLUGIN_RESOURCES})

set_target_properties(TemplateMatchPlugin PROPERTIES
    AUTOMOC ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_compile_definitions(TemplateMatchPlugin PRIVATE TEMPLATE_LIBRARY)

target_include_directories(TemplateMatchPlugin SYSTEM PUBLIC
    ${CMAKE_SOURCE_DIR}/src/plugins/template
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_SOURCE_DIR}/src/plugins/template/tools
    ${BSCV_INCLUDE_DIRS}
)

target_link_libraries(TemplateMatchPlugin PRIVATE
    GuiCommon
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Network
    ${BSCV_LIBRARIES}
    CalibPlugin
    HeightMeaturePlugin
)

include(CTest)
if(BUILD_TESTING)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/tests/CMakeLists.txt")
        add_subdirectory(scripts/tests)
    endif()
endif()
