cmake_minimum_required(VERSION 3.16)

project(TemplateMatchPlugin)

find_package(Qt5 COMPONENTS Widgets Concurrent Network REQUIRED)

if(NOT TARGET BSCV::BSCV)
    find_package(BSCV REQUIRED)
endif()

set(PLUGIN_SOURCES
    core/TemplateManager.cpp
    core/TemplateManager.h
    core/MatchParams.h
    core/client/UDPMatReceiver.cpp
    core/client/UDPMatReceiver.h
    Widget/matchWidget.cpp
    Widget/matchWidget.h
    Widget/paraWidget.cpp
    Widget/paraWidget.h    
    Scene/MatchScene.cpp
    Scene/MatchScene.h
    camera/camera.cpp
    camera/camera.h
    camera/ImageProcess.cpp
    camera/ImageProcess.h
    TemplatePlugin.h
)

# 将 Qt 资源编译进目标库，以便 dark_style.qss 可以通过 ":/res/dark_style.qss" 在运行时访问
qt5_add_resources(PLUGIN_RESOURCES resources.qrc)

add_library(TemplateMatchPlugin SHARED ${PLUGIN_SOURCES} ${PLUGIN_RESOURCES})

target_link_libraries(TemplateMatchPlugin PRIVATE GuiCommon)

target_compile_definitions(TemplateMatchPlugin PRIVATE TEMPLATE_LIBRARY)

target_include_directories(TemplateMatchPlugin SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/.. 
    ${CMAKE_CURRENT_SOURCE_DIR}/tools 
    ${BSCV_INCLUDE_DIRS}
)

target_link_libraries(TemplateMatchPlugin PRIVATE
    Qt5::Widgets
    Qt5::Concurrent
    Qt5::Network
    ${BSCV_LIBRARIES}
    CalibPlugin
)

set_target_properties(TemplateMatchPlugin PROPERTIES
    AUTOMOC ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
