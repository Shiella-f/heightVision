if(WIN32)
    # 标记找到 BSCV（使用仓库内的预编译库与头文件）
    set(BSCV_FOUND TRUE)
    # 查找 lib 目录下的库（Debug/Release）
    file(GLOB libFiles_RELEASE "${CMAKE_CURRENT_LIST_DIR}/bscv/lib/Release/*.lib")
    file(GLOB libFiles_DEBUG "${CMAKE_CURRENT_LIST_DIR}/bscv/lib/Debug/*.lib")
    # dll 可能位于 bin 目录（如果存在）
    file(GLOB dllFiles_RELEASE "${CMAKE_CURRENT_LIST_DIR}/bscv/bin/Release/*.dll")
    file(GLOB dllFiles_DEBUG "${CMAKE_CURRENT_LIST_DIR}/bscv/bin/Debug/*.dll")

    # 默认 include 指向仓库内的 include 子目录（仓库已包含 opencv2 头）
    set(BSCV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/bscv/include" CACHE STRING "BSCV_INCLUDE_DIRS" FORCE)

    # 形成按配置选择的库列表，并使用标准变量名 BSCV_LIBRARIES
    set(BSCV_LIBRARIES "$<$<CONFIG:Debug>:${libFiles_DEBUG}>$<$<CONFIG:Release>:${libFiles_RELEASE}>" CACHE STRING "BSCV_LIBRARIES" FORCE)
    set(BSCV_DLLS "$<$<CONFIG:Debug>:${dllFiles_DEBUG}>$<$<CONFIG:Release>:${dllFiles_RELEASE}>" CACHE STRING "BSCV_DLLS" FORCE)

    # 如果仓库内包含 OpenCV 头与 opencv_world 库，则优先使用仓库内的 OpenCV
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/bscv/include/opencv2")
        # 设置 OpenCV include 到仓库内的 include
        set(OpenCV_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/bscv/include" CACHE PATH "OpenCV include (from BSCV)" FORCE)
        # 查找仓库内的 opencv_world 库（Debug/Release）
        file(GLOB local_opencv_release "${CMAKE_CURRENT_LIST_DIR}/bscv/lib/Release/opencv_world*.lib")
        file(GLOB local_opencv_debug "${CMAKE_CURRENT_LIST_DIR}/bscv/lib/Debug/opencv_world*.lib")
        if(local_opencv_release OR local_opencv_debug)
            set(OpenCV_LIBS "$<$<CONFIG:Debug>:${local_opencv_debug}>$<$<CONFIG:Release>:${local_opencv_release}>" CACHE STRING "OpenCV libs (from BSCV)" FORCE)
            # 把本地 OpenCV 库追加到 BSCV_LIBRARIES，这样上层 target_link_libraries 使用 ${BSCV_LIBRARIES} 会包含 OpenCV
            set(BSCV_LIBRARIES "$<$<CONFIG:Debug>:${libFiles_DEBUG};${local_opencv_debug}>$<$<CONFIG:Release>:${libFiles_RELEASE};${local_opencv_release}>" CACHE STRING "BSCV_LIBRARIES" FORCE)
            # 同时把 include 追加（冗余但安全）
            set(BSCV_INCLUDE_DIRS "${BSCV_INCLUDE_DIRS};${OpenCV_INCLUDE_DIRS}" CACHE STRING "BSCV_INCLUDE_DIRS" FORCE)
            set(BSCV_OPENCV_INCLUDED TRUE CACHE BOOL "BSCV includes local OpenCV" FORCE)
        endif()
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BSCV DEFAULT_MSG BSCV_INCLUDE_DIRS BSCV_LIBRARIES BSCV_DLLS)