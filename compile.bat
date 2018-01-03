@echo off

For /f "tokens=1-4 delims=/ " %%a in ("%DATE%") do (set BuildDate=%%a-%%b-%%c)
For /f "tokens=1-2 delims=/:/ " %%a in ("%TIME%") do (set BuildTime=%%a-%%b)
FOR /f %%H IN ('git log -n 1 --oneline') DO set VersionHash=%%H
set CompileFiles= ..\src\main.cpp ..\src\interface.cpp ..\src\render.cpp ..\src\audio.cpp ..\src\audio_resample.cpp ..\src\ringbuffer.cpp ..\src\platform.cpp ..\src\logging.cpp ..\src\user.cpp ..\src\user_client.cpp ..\src\network.cpp ..\src\network_client.cpp ..\src\video.cpp ..\src\videoinput.cpp ..\src\jitterbuffer.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -wd4100 -D_CRT_SECURE_NO_WARNINGS -Od -DNOMINMAX -MTd -EHsc- -DBUILD_VERSION=\"%VersionHash%_%BuildDate%_%BuildTime%\" -DSOUNDIO_STATIC_LIBRARY -Foobj/
set IncludeDirs= -I..\include -I..\thirdparty\include

set GLFWLibs=glfw3.lib gdi32.lib shell32.lib OpenGL32.lib imgui.lib
set EnetLibs=enet.lib ws2_32.lib winmm.lib
set OpusLibs=opus.lib celt.lib silk_common.lib silk_fixed.lib silk_float.lib
set TheoraLibs=libtheora_static.lib libogg_static.lib
set SoundIOLibs=libsoundio_static.lib ole32.lib
set videoInputLibs=OleAut32.lib Strmiids.lib
set LinkLibs=%GLFWLibs% %EnetLibs% %OpusLibs% %TheoraLibs% %SoundIOLibs% %videoInputLibs%
set LinkFlags=-LIBPATH:..\thirdparty\lib\win64 %LinkLibs% -INCREMENTAL:NO -OUT:main.exe

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link %LinkFlags%
popd
