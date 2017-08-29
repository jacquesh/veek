@echo off

ctime -begin veek_test_time.ctm

set CompileFiles= ..\test\main.cpp ..\test\audio_resample_test.cpp ..\test\ringbuffer_test.cpp ..\src\audio_resample.cpp ..\src\ringbuffer.cpp ..\src\platform.cpp ..\src\logging.cpp
set CompileFlags= -nologo -Zi -Gm- -W4 -wd4100 -D_CRT_SECURE_NO_WARNINGS -Od -DNOMINMAX -MTd -EHsc-
set IncludeDirs= -I..\include -I..\src

pushd build
cl %CompileFlags% %CompileFiles% %IncludeDirs% -link -INCREMENTAL:NO -OUT:test.exe User32.lib
.\test.exe
popd

ctime -end veek_test_time.ctm %ERRORLEVEL%
