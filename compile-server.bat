@echo off
set CompileFiles= ..\src\server.cpp ..\src\user.cpp ..\src\network.cpp ..\src\platform.cpp ..\src\logging.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -MTd -EHsc -Foobj/
set IncludeDirs= -I..\thirdparty\include

set LinkLibs=enet.lib ws2_32.lib winmm.lib user32.lib

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link -LIBPATH:..\thirdparty\lib\win64 %LinkLibs% -INCREMENTAL:NO -OUT:server.exe
popd


