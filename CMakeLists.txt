cmake_minimum_required(VERSION 3.2)
project(veek)

string(TIMESTAMP CURRENT_TIME "%Y-%m-%d_%H:%M:%S" UTC)

include(FindOpenGL) # For OPENGL_gl_LIBRARY
include(GNUInstallDirs) # For CMAKE_INSTALL_LIBDIR
include(ExternalProject) # For ExternalProject_Add

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SRC_FILES ${SRC_DIR}/main.cpp
              ${SRC_DIR}/render.cpp
              ${SRC_DIR}/interface.cpp
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
              ${SRC_DIR}/jitterbuffer.cpp
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


find_package(PkgConfig REQUIRED)
#pkg_search_module(GLFW REQUIRED glfw3) # NOTE: This doesn't work on Ubuntu 14.04, but does on 16.04
pkg_search_module(OPUS REQUIRED opus)
pkg_search_module(THEORA REQUIRED theoraenc)
pkg_search_module(V4L2 REQUIRED libv4l2)
pkg_search_module(PULSE libpulse)
pkg_search_module(ALSA REQUIRED alsa)

find_library(OPUS_STATIC_LIBRARIES libopus.a)
find_library(THEORA_ENCODE_STATIC_LIB libtheoraenc.a)
find_library(THEORA_DECODE_STATIC_LIB libtheoradec.a)
find_library(OGG_STATIC_LIB libogg.a)
set(THEORA_STATIC_LIBRARIES ${THEORA_ENCODE_STATIC_LIB}
                            ${THEORA_DECODE_STATIC_LIB}
                            ${OGG_STATIC_LIB})

find_package(Threads REQUIRED)
find_package(X11)

ExternalProject_Add(
    libsoundio
    GIT_REPOSITORY https://github.com/andrewrk/libsoundio.git
    GIT_TAG 1778b8d10f
    CMAKE_ARGS -DENABLE_JACK=OFF
               -DENABLE_PULSEAUDIO=ON
               -DENABLE_ALSA=ON
               -DBUILD_TESTS=OFF
               -DBUILD_EXAMPLE_PROGRAMS=OFF
               -DBUILD_DYNAMIC_LIBS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin
               -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    PREFIX libsoundio
    UPDATE_COMMAND ""
    )
set(SOUNDIO_BINDIR ${CMAKE_CURRENT_BINARY_DIR}/libsoundio/bin)
set(SOUNDIO_STATIC_LIBRARIES ${SOUNDIO_BINDIR}/${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}soundio${CMAKE_STATIC_LIBRARY_SUFFIX})
set(SOUNDIO_INCLUDE_DIRS ${SOUNDIO_BINDIR}/include)

# NOTE: This is only here because TravisCI is running on Ubuntu 14.04 and doesn't have libglfw3-dev
ExternalProject_Add(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.2.1
    CMAKE_ARGS -DGLFW_BUILD_EXAMPLES=OFF
               -DGLFW_BUILD_TESTS=OFF
               -DGLFW_BUILD_DOCS=OFF
               -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/glfw/bin
               -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    PREFIX glfw
    UPDATE_COMMAND ""
    )
find_library(Xrandr_LIB Xrandr)
find_library(Xcursor_LIB Xcursor)
find_library(Xxf86vm_LIB Xxf86vm)
find_library(Xinerama_LIB Xinerama)
set(GLFW_STATIC_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/glfw/bin/lib/${CMAKE_STATIC_LIBRARY_PREFIX}glfw3${CMAKE_STATIC_LIBRARY_SUFFIX} ${Xrandr_LIB} ${Xcursor_LIB} ${Xxf86vm_LIB} ${Xinerama_LIB})
set(GLFW_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/glfw/bin/include)


ExternalProject_Add(
    enet
    GIT_REPOSITORY ...
    GIT_TAG ...
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/enet/bin
               -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    PREFIX enet
    UPDATE_COMMAND ""
    )
set(ENET_BINDIR ${CMAKE_CURRENT_BINARY_DIR}/enet/bin)
set(ENET_STATIC_LIBRARIES ${ENET_BINDIR}/${CMAKE_INSTALL_LIBDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}enet${CMAKE_STATIC_LIBRARY_SUFFIX})
set(ENET_INCLUDE_DIRS ${ENET_BINDIR}/include)

add_executable(veek ${SRC_FILES} ${IMGUI_SRC_FILES})
add_dependencies(veek libsoundio glfw)
include_directories(veek ${CMAKE_SOURCE_DIR}/include
                         ${SOUNDIO_INCLUDE_DIRS}
                         ${GLFW_INCLUDE_DIRS}
                         ${ENET_INCLUDE_DIRS}
                         ${OPUS_INCLUDE_DIRS}
                         )
target_link_libraries(veek ${OPENGL_gl_LIBRARY}
                           ${SOUNDIO_STATIC_LIBRARIES}
                           ${GLFW_STATIC_LIBRARIES}
                           ${ENET_STATIC_LIBRARIES}
                           ${OPUS_STATIC_LIBRARIES}
                           ${THEORA_STATIC_LIBRARIES}
                           ${CMAKE_DL_LIBS}
                           ${CMAKE_THREAD_LIBS_INIT}
                           ${X11_LIBRARIES}
                           ${PULSE_LIBRARIES}
                           ${ALSA_LIBRARIES}
                           ${V4L2_LIBRARIES}
                           )
target_compile_definitions(veek PRIVATE SOUNDIO_STATIC_LIBRARY
                                        BUILD_VERSION="${CURRENT_TIME}_UNKNOWNCOMMIT")

add_executable(server ${SERVER_SRC_FILES})
include_directories(server ${ENET_INCLUDE_DIRS})
target_link_libraries(server ${ENET_STATIC_LIBRARIES}
                             ${CMAKE_THREAD_LIBS_INIT})
