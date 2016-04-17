@echo off
set CompileFiles= ..\src\main.cpp ..\src\render.cpp ..\src\vecmath.cpp ..\src\escapi.cpp ..\imgui\gl3w.cpp ..\imgui\imgui.cpp ..\imgui\imgui_draw.cpp ..\imgui\imgui_impl_sdl_gl3.cpp
set CompileFlags= -nologo -Zi -Gm-
set IncludeDirs= -I..\src -I..\include -I..\dge
set LinkLibs= OpenGL32.lib ..\lib\OpenAL32.lib SDL2.lib ..\lib\escapi.lib ..\lib\enet64.lib ws2_32.lib winmm.lib

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link %LinkLibs% -OUT:main.exe
popd


