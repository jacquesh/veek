pushd build
CompileFiles="../src/main.cpp ../src/render.cpp ../src/audio.cpp ../src/audio_resample.cpp ../src/ringbuffer.cpp ../src/platform.cpp ../src/logging.cpp ../src/user.cpp ../src/user_client.cpp ../src/network.cpp ../src/network_client.cpp"
CompileFlags="-g  -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -std=c++11 -I../include"

GLFWLibs="-lglfw -lX11 -lXrandr -lXcursor -lXxf86vm -lXinerama -ldl"
EnetLibs="-lenet"
OpusLibs="-lopus"
SoundIOLibs="-lsoundio -lpulse -lasound"
LinkLibs="-limgui -lGL $GLFWLibs $EnetLibs $OpusLibs $SoundIOLibs -lpthread"

g++ $CompileFiles $CompileFlags -DSOUNDIO_STATIC_LIBRARY -L../lib $LinkLibs -omain
popd
