cmake_minimum_required(VERSION 3.30)
project(djiDockServer)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)
include_directories(${CMAKE_CURRENT_LIST_DIR})

link_directories(${CMAKE_CURRENT_LIST_DIR}/lib/${CMAKE_HOST_SYSTEM_PROCESSOR})
link_directories(${CMAKE_BINARY_DIR})
#link_libraries(-ledgesdk -lstdc++)
#link_libraries(-lcrypto)
#link_libraries(-lssh2)


# dji_thermal_sdk
include_directories(${CMAKE_CURRENT_LIST_DIR}/dji_thermal_sdk/tsdk-core/api)
# link_directories 会在执行make install前指定应用程序的RUNPATH的，
# 但是通过readelf -d查看DJI TSDK的时候库的入口库libdirp.so的RUNPATH是空的。这时在不改变动态库属性的情况下，必须将TSDK的动态库拷贝到系统的动态库加载路径中
link_directories(${CMAKE_CURRENT_LIST_DIR}/dji_thermal_sdk/tsdk-core/lib/linux/release_x64)
set(DJI_THERMAL_LIBS dirp)

find_package(yaml-cpp REQUIRED)

set(PocoLibs PocoUtil PocoFoundation PocoJSON PocoXML PocoNet PocoData PocoDataSQLite)
set(DJI_LIBS edgesdk crypto ssh2  ${PocoLibs})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(utility)
include(CPM)
# opencv
find_package(OpenCV QUIET)
if (OpenCV_FOUND)
    message("\n${PROJECT_NAME}...")
    message(STATUS "Found OpenCV installed in the system, will use it to display image in AdvancedSensing APIs")
    message(STATUS " - Includes: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS " - Libraries: ${OpenCV_LIBRARIES}")
    add_definitions(-DOPEN_CV_INSTALLED)
    execute_process(COMMAND opencv_version OUTPUT_VARIABLE OPENCV_VERSION)
    if (${OPENCV_VERSION} STRLESS "4.0.0")
        add_definitions(-DOPEN_CV_VERSION_3)
    else ()
        add_definitions(-DOPEN_CV_VERSION_4)
    endif ()
    link_libraries(${OpenCV_LIBS})
else ()
    message(STATUS "\nDID NOT FIND OPENCV IN THE SYSTEM, SOME EXECUTABLE FILES WILL NOT BE BUILT!!\n")
endif ()

# ffmpeg
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party;${CMAKE_MODULE_PATH}")
find_package(FFMPEG QUIET)
if (FFMPEG_FOUND)
    message(STATUS "Found FFMPEG installed in the system")
    message(STATUS " - Includes: ${FFMPEG_INCLUDE_DIR}")
    message(STATUS " - Libraries: ${FFMPEG_LIBRARIES}")

    EXECUTE_PROCESS(COMMAND ffmpeg -version
            OUTPUT_VARIABLE ffmpeg_version_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCH "version.*Copyright" ffmpeg_version_line ${ffmpeg_version_output})
    string(REGEX MATCH " .* " ffmpeg_version ${ffmpeg_version_line})
    string(REGEX MATCH "^ 5.*$" ffmpeg_major_version ${ffmpeg_version})

    if (HEAD${ffmpeg_major_version} STREQUAL "HEAD")
        message(STATUS " - Version: ${ffmpeg_version}")
    else ()
        message(FATAL_ERROR " - Not support FFMPEG version: ${ffmpeg_major_version}, please install 4.x.x instead.")
    endif ()

    include_directories(${FFMPEG_INCLUDE_DIR})
    add_definitions(-DFFMPEG_INSTALLED)
    link_libraries(${FFMPEG_LIBRARIES})
else ()
    message(STATUS "\nDID NOT FIND FFMPEG IN THE SYSTEM, SOME EXECUTABLE FILES WILL NOT BE BUILT!!\n")
endif (FFMPEG_FOUND)

##源文件列表相关
set(filtration_dirlist cmake-build-debug .idea .git cmake scripts test 3rd_libs)
SUBDIRLISTINCLUDE(SRC_DIRS ${CMAKE_SOURCE_DIR} 2 ${filtration_dirlist})
message(STATUS "SRC_DIRS:${SRC_DIRS}")
foreach (src_dir ${SRC_DIRS})
    aux_source_directory(${src_dir} SRC_FILES)
endforeach (src_dir)
list(REMOVE_ITEM SRC_FILES ./main.cpp)



set(LIBS m pthread gflags glog yaml-cpp ${DJI_LIBS} ${DJI_THERMAL_LIBS})
add_executable(djiDockServer main.cpp ${SRC_FILES})
target_link_libraries(djiDockServer ${LIBS})

add_executable(test_ffmpeg_decode_h264 test/test_ffmpeg_decode_h264.cpp)
target_link_libraries(test_ffmpeg_decode_h264 ${LIBS})

add_executable(rtsp_push test/rtsp_push.cpp)
target_link_libraries(rtsp_push ${LIBS})

add_executable(test_dji_thermal test/test_dji_thermal.cpp
        DJI/DJIThermal.cpp)
target_link_libraries(test_dji_thermal ${LIBS})