@echo off
set CompileFiles= ..\src\server.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -MTd
set IncludeDirs= -I..\include

set LinkLibs=enet.lib ws2_32.lib winmm.lib

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link -LIBPATH:..\lib %LinkLibs% -INCREMENTAL:NO -OUT:server.exe
popd


