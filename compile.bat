@echo off
set CompileFiles= ..\src\main.cpp ..\src\graphics.cpp ..\src\graphicsutil.cpp ..\src\vecmath.cpp ..\src\audio.cpp ..\src\escapi.cpp ..\imgui\gl3w.cpp ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_impl_sdl_gl3.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -MT
set IncludeDirs= -I..\src -I..\include -I..\dge

set EnetLibs=..\lib\enet64.lib ws2_32.lib winmm.lib
set OpusLibs=..\lib\opus.lib ..\lib\celt.lib ..\lib\silk_common.lib ..\lib\silk_fixed.lib ..\lib\silk_float.lib
set DaalaLibs=..\lib\LibDaalaBase.lib ..\lib\libdaaladec.lib ..\lib\libdaalaenc.lib
set LinkLibs= OpenGL32.lib SDL2.lib ..\lib\escapi.lib  %EnetLibs% %OpusLibs% %DaalaLibs% ..\lib\libsoundio.dll.a

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link %LinkLibs% -OUT:main.exe
popd


