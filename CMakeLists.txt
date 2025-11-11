cmake_minimum_required(VERSION 3.19)
project(MainWindow LANGUAGES CXX)

# Qt 6
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "src/*.c"
    "src/*.cc"
    "src/*.cxx"
    "src/*.cpp"
    "src/*.h"
    "src/*.hh"
    "src/*.hpp"
    "src/*.hxx"
    "src/*.ui"
    "src/*.qrc"
    "src/*.qml"
)

qt_add_executable(MainWindow
    MACOSX_BUNDLE
    ${PROJECT_SOURCES}
)

# 1. 配置OpenCV（使用自己编译的版本）
# 如需自定义，请在配置时传入 -DOpenCV_DIR=... 覆盖下面的默认值
set(OpenCV_DIR "C:/opencv/opencv_build" CACHE PATH "OpenCV build directory")
find_package(OpenCV REQUIRED)  # 查找OpenCV库

target_link_libraries(MainWindow
    PRIVATE
        Qt::Core
        Qt::Widgets
        ${OpenCV_LIBS}
)

target_include_directories(MainWindow
    PRIVATE
        ${OpenCV_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(GNUInstallDirs)

# 在 Windows 上避免默认安装到 Program Files（需要管理员权限）
if (WIN32)
    # 如果安装前缀包含 "Program Files"，则改为构建目录下的 install
    if (CMAKE_INSTALL_PREFIX MATCHES "Program Files")
        set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path" FORCE)
    endif()
endif()




install(TARGETS MainWindow
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET MainWindow
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# 可选：拷贝 OpenCV 运行时 DLL（如果提供了 OPENCV_DLL_DIR）
set(OPENCV_DLL_DIR "" CACHE PATH "Directory containing OpenCV runtime DLLs (e.g. .../bin)")
if (OPENCV_DLL_DIR)
    file(GLOB OpenCV_DLLS "${OPENCV_DLL_DIR}/*.dll")
    if (OpenCV_DLLS)
        install(FILES ${OpenCV_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()


# 在构建目录自动部署 Qt 运行时依赖（使 VS Code 直接“运行目标”即可弹窗）
if (WIN32)
    # 查找 windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS
            "$ENV{QT_DIR}/bin"
            "$ENV{Qt6_DIR}/bin"
            "$ENV{QTDIR}/bin"
            "C:/Qt/6.9.1/mingw_64/bin" # 常见安装路径（可按需调整）
        PATHS ENV PATH
    )

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MainWindow POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:MainWindow>" --force
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:MainWindow>"
            COMMENT "Deploying Qt runtime dependencies to build directory with windeployqt"
            VERBATIM
        )
    else()
        message(STATUS "windeployqt not found. You can still run 'cmake --install' to deploy, or set WINDEPLOYQT_EXECUTABLE.")
    endif()
    

endif()
