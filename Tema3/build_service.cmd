@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /W4 /EHsc /Fe:Tema3\hello_world_service.exe Tema3\hello_world_service.cpp Advapi32.lib
