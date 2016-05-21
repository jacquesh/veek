@echo off

ctime -begin veek_time.ctm

set CompileFiles= ..\src\main.cpp ..\src\graphics.cpp ..\src\graphicsutil.cpp ..\src\vecmath.cpp ..\src\audio.cpp ..\src\video.cpp ..\src\ringbuffer.cpp ..\src\win32_platform.cpp ..\src\escapi.cpp ..\imgui\gl3w.cpp ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_impl_glfw_gl3.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -MDd
set IncludeDirs= -I..\include

set GLFWLibs=..\lib\glfw3.lib gdi32.lib shell32.lib
set EnetLibs=..\lib\enet.lib ws2_32.lib winmm.lib
set OpusLibs=..\lib\opus.lib ..\lib\celt.lib ..\lib\silk_common.lib ..\lib\silk_fixed.lib ..\lib\silk_float.lib
set TheoraLibs=..\lib\libtheora_static.lib ..\lib\libogg_static.lib
set LinkLibs= OpenGL32.lib ..\lib\escapi.lib %GLFWLibs% %EnetLibs% %OpusLibs% %TheoraLibs% ..\lib\libsoundio.dll.a

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link %LinkLibs% -INCREMENTAL:NO -OUT:main.exe
popd

ctime -end veek_time.ctm %ERRORLEVEL%
