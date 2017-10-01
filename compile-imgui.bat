@echo off

set CompileFiles=gl3w.cpp imgui.cpp imgui_draw.cpp imgui_impl_glfw_gl3.cpp
set ObjectFiles=gl3w.obj imgui.obj imgui_draw.obj imgui_impl_glfw_gl3.obj

pushd imgui
cl %CompileFiles% -nologo -Z7 -Gm- -W4 -D_CRT_SECURE_NO_WARNINGS -DNOMINMAX -MTd -c -I..\include -I..\thirdparty\include
lib %ObjectFiles% -nologo -out:..\thirdparty\lib\win64\imgui.lib
rm ./*.obj
popd
