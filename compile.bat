@echo off

ctime -begin veek_time.ctm

set CompileFiles= ..\src\main.cpp ..\src\graphics.cpp ..\src\graphicsutil.cpp ..\src\vecmath.cpp ..\src\video.cpp ..\src\audio.cpp  ..\src\ringbuffer.cpp ..\src\win32_platform.cpp ..\src\logging.cpp ..\src\escapi.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -MTd -EHsc-
set IncludeDirs= -I..\include

set GLFWLibs=glfw3.lib gdi32.lib shell32.lib
set EnetLibs=enet.lib ws2_32.lib winmm.lib
set OpusLibs=opus.lib celt.lib silk_common.lib silk_fixed.lib silk_float.lib
set TheoraLibs=libtheora_static.lib libogg_static.lib
set SoundIOLibs=soundio_static.lib ole32.lib
set LinkLibs= OpenGL32.lib escapi.lib imgui.lib %GLFWLibs% %EnetLibs% %OpusLibs% %TheoraLibs% %SoundIOLibs%

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -DSOUNDIO_STATIC_LIBRARY -link -LIBPATH:..\lib %LinkLibs% -INCREMENTAL:NO -OUT:main.exe
popd

ctime -end veek_time.ctm %ERRORLEVEL%
