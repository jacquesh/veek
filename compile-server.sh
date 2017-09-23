pushd build
CompileFiles="../src/server.cpp ../src/user.cpp ../src/network.cpp ../src/platform.cpp ../src/logging.cpp ../src/happyhttp.cpp"
CompileFlags="-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX"
IncludeDirs="-I../include"

LinkLibs="enet.lib ws2_32.lib winmm.lib user32.lib"

g++ $CompileFlags $CompileFiles $IncludeDirs -L../lib $LinkLibs -o server
popd


