pushd build
CompileFiles="../src/main.cpp ../src/graphics.cpp ../src/graphicsutil.cpp ../src/vecmath.cpp ../src/audio.cpp ../src/ringbuffer.cpp ../src/unix_platform.cpp ../src/logging.cpp"
CompileFlags="-g  -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -std=c++11 -I../include"

GLFWLibs="-lglfw3 -lX11 -lXrandr -lXcursor -lXxf86vm -lXinerama -ldl"
EnetLibs="-lenet"
OpusLibs="-lopus"
SoundIOLibs="-lsoundio -lpulse -lasound"
LinkLibs="-lGL -limgui $GLFWLibs $EnetLibs $OpusLibs $SoundIOLibs -lpthread"

g++ $CompileFlags $CompileFiles -DSOUNDIO_STATIC_LIBRARY -L../lib $LinkLibs -omain
popd
