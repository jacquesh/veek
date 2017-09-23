pushd build
CompileFiles="../src/server.cpp ../src/user.cpp ../src/network.cpp ../src/platform.cpp ../src/logging.cpp"
CompileFlags="-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -fpermissive"
IncludeDirs="-I../include"

LinkLibs="-lenet"

g++ $CompileFiles $CompileFlags $IncludeDirs -L../lib $LinkLibs -o server
popd


