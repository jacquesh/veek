cmake_minimum_required(VERSION 3.2)
project(veek)

string(TIMESTAMP CURRENT_TIME "%Y-%m-%d_%H:%M:%S" UTC)

set(CMAKE_CXX_STANDARD 11)
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
set(SERVER_SRC_FILES ${SRC_DIR}/server.cpp
                     ${SRC_DIR}/user.cpp
                     ${SRC_DIR}/network.cpp
                     ${SRC_DIR}/platform.cpp
                     ${SRC_DIR}/logging.cpp
    )

include(FindOpenGL)

find_package(PkgConfig REQUIRED)
#pkg_search_module(GLFW REQUIRED glfw3) # NOTE: This doesn't work on Ubuntu 14.04, but does on 16.04
pkg_search_module(ENET REQUIRED libenet)
pkg_search_module(OPUS REQUIRED opus)

find_package(Threads REQUIRED)
find_package(ALSA REQUIRED)
find_package(X11)

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
set(SOUNDIO_STATIC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin/lib/${CMAKE_STATIC_LIBRARY_PREFIX}soundio${CMAKE_STATIC_LIBRARY_SUFFIX})
set(SOUNDIO_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin/include)

# NOTE: This is only here because TravisCI is running on Ubuntu 14.04 and doesn't have libglfw3-dev
ExternalProject_Add(
    glfw
    GIT_REPOSITORY git@github.com:glfw/glfw.git
    GIT_TAG 3.2.1
    CMAKE_ARGS -DGLFW_BUILD_EXAMPLES=OFF
               -DGLFW_BUILD_TESTS=OFF
               -DGLFW_BUILD_DOCS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/glfw/bin
    PREFIX glfw
    UPDATE_COMMAND ""
    )
find_library(Xrandr_LIB Xrandr)
find_library(Xcursor_LIB Xcursor)
find_library(Xxf86vm_LIB Xxf86vm)
find_library(Xinerama_LIB Xinerama)
set(GLFW_STATIC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/glfw/bin/lib/${CMAKE_STATIC_LIBRARY_PREFIX}glfw3${CMAKE_STATIC_LIBRARY_SUFFIX} ${Xrandr_LIB} ${Xcursor_LIB} ${Xxf86vm_LIB} ${Xinerama_LIB})
set(GLFW_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/glfw/bin/include)

add_executable(veek ${SRC_FILES} ${IMGUI_SRC_FILES})
add_dependencies(veek libsoundio glfw)
include_directories(veek ${CMAKE_SOURCE_DIR}/include
                         ${SOUNDIO_INCLUDE_DIRS}
                         ${GLFW_INCLUDE_DIRS}
                         ${ENET_INCLUDE_DIRS}
                         ${OPUS_INCLUDE_DIRS})
target_link_libraries(veek ${OPENGL_gl_LIBRARY}
                           ${SOUNDIO_STATIC_LIBRARIES}
                           ${GLFW_STATIC_LIBRARIES}
                           ${ENET_STATIC_LIBRARIES}
                           ${OPUS_STATIC_LIBRARIES}
                           ${CMAKE_DL_LIBS}
                           ${CMAKE_THREAD_LIBS_INIT}
                           ${X11_LIBRARIES}
                           ${ALSA_LIBRARIES})
target_compile_definitions(veek PRIVATE SOUNDIO_STATIC_LIBRARY
                                        BUILD_VERSION="${CURRENT_TIME}_UNKNOWNCOMMIT")

add_executable(server ${SERVER_SRC_FILES})
include_directories(server ${ENET_INCLUDE_DIRS})
target_link_libraries(server ${ENET_STATIC_LIBRARIES})
