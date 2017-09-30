cmake_minimum_required(VERSION 3.2)
project(veek)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SRC_FILES ${SRC_DIR}/main.cpp
              ${SRC_DIR}/render.cpp
              ${SRC_DIR}/audio.cpp
              ${SRC_DIR}/audio_resample.cpp
              ${SRC_DIR}/ringbuffer.cpp
              ${SRC_DIR}/platform.cpp
              ${SRC_DIR}/logging.cpp
              ${SRC_DIR}/user.cpp
              ${SRC_DIR}/user_client.cpp
              ${SRC_DIR}/network.cpp
              ${SRC_DIR}/network_client.cpp
              ${SRC_DIR}/video.cpp
    )
set(IMGUI_SRC_FILES ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp
                    ${CMAKE_SOURCE_DIR}/imgui/imgui_draw.cpp
                    ${CMAKE_SOURCE_DIR}/imgui/imgui_impl_glfw_gl3.cpp
                    ${CMAKE_SOURCE_DIR}/imgui/gl3w.cpp
    )
add_executable(veek ${SRC_FILES} ${IMGUI_SRC_FILES})

message("${CMAKE_CURRENT_BINARY_DIR}")

include(FindOpenGL)
target_link_libraries(veek ${OPENGL_gl_LIBRARY})

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED libglfw)

find_package(Threads REQUIRED)
find_package(ALSA REQUIRED)
find_package(X11)
find_library(ENET_LIB enet)
find_library(OPUS_LIB opus)
find_library(GLFW_LIB glfw3)

include(ExternalProject)
ExternalProject_Add(
    libsoundio
    GIT_REPOSITORY git@github.com:andrewrk/libsoundio.git
    GIT_TAG 1778b8d10f
    CMAKE_ARGS -DENABLE_JACK=OFF
               -DENABLE_PULSEAUDIO=OFF
               -DBUILD_TESTS=OFF
               -DBUILD_EXAMPLE_PROGRAMS=OFF
               -DBUILD_DYNAMIC_LIBS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin
    PREFIX libsoundio
    UPDATE_COMMAND ""
    )
find_library(SOUNDIO_LIB soundio ${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin)
set(SOUNDIO_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin/soundio)

message(${GLFW_INCLUDE_DIRS})
add_dependencies(veek libsoundio)
include_directories(veek ${CMAKE_SOURCE_DIR}/include ${SOUNDIO_INCLUDE} ${GLFW_INCLUDE_DIRS})
target_link_libraries(veek ${GLFW_STATIC_LIBRARIES} ${SOUNDIO_LIB} ${ENET_LIB} ${OPUS_LIB} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${X11_LIBRARIES})
