@echo off

set wd=%cd:~,-13%

SUBST Q: /D
SUBST Q: %wd%

pushd Q:

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
start gvim.exe
