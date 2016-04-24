@echo off
set CompileFiles= ..\src\server.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -MD
set IncludeDirs= -I..\include

set LinkLibs=..\lib\enet64.lib ws2_32.lib winmm.lib

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link %LinkLibs% -INCREMENTAL:NO -OUT:server.exe
popd


