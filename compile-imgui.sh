mkdir -p lib
pushd imgui
CompileFiles="gl3w.cpp imgui.cpp imgui_draw.cpp imgui_impl_glfw_gl3.cpp"
ObjectFiles="gl3w.o imgui.o imgui_draw.o imgui_impl_glfw_gl3.o"
g++ $CompileFiles -Wall -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -c -I../include
ar crf ../lib/libimgui.a $ObjectFiles
rm ./*.o
popd
