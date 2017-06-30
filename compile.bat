@echo off

ctime -begin veek_time.ctm

For /f "tokens=1-4 delims=/ " %%a in ("%DATE%") do (set BuildDate=%%a-%%b-%%c)
For /f "tokens=1-2 delims=/:/ " %%a in ("%TIME%") do (set BuildTime=%%a-%%b)
FOR /f %%H IN ('git log -n 1 --oneline') DO set VersionHash=%%H
set CompileFiles= ..\src\main.cpp ..\src\graphics.cpp ..\src\vecmath.cpp ..\src\video.cpp ..\src\audio.cpp  ..\src\ringbuffer.cpp ..\src\platform.cpp ..\src\logging.cpp ..\src\user.cpp ..\src\user_client.cpp ..\src\network.cpp ..\src\videoinput.cpp ..\src\crossCap.cpp ..\src\happyhttp.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -Od -DNOMINMAX -MTd -EHsc- -DBUILD_VERSION=\"%VersionHash%_%BuildDate%_%BuildTime%\"
set IncludeDirs= -I..\include

set GLFWLibs=glfw3.lib gdi32.lib shell32.lib
set EnetLibs=enet.lib ws2_32.lib winmm.lib
set OpusLibs=opus.lib celt.lib silk_common.lib silk_fixed.lib silk_float.lib
set TheoraLibs=libtheora_static.lib libogg_static.lib
set SoundIOLibs=soundio_static.lib ole32.lib
set videoinputLibs=OleAut32.lib Strmiids.lib
set LinkLibs= OpenGL32.lib imgui.lib %GLFWLibs% %EnetLibs% %OpusLibs% %TheoraLibs% %SoundIOLibs% %videoinputLibs%

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -DSOUNDIO_STATIC_LIBRARY -link -LIBPATH:..\lib %LinkLibs% -INCREMENTAL:NO -OUT:main.exe
popd

ctime -end veek_time.ctm %ERRORLEVEL%
