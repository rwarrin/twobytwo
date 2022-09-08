@echo off

SET CompilerFlags=/nologo /Z7 /Od /Ob0 /GS- /Oi /fp:fast /FC
REM SET CompilerFlags=/nologo /O2 /Oi /fp:fast /FC /GS-
SET LinkerFlags=/incremental:no

IF NOT EXIST build mkdir build

pushd build

rc.exe /nologo ../TwoByTwo/code/app.rc
REM cl.exe %CompilerFlags% ../TwoByTwo/code/twobytwo.cpp /link %LinkerFlags%
cl.exe %CompilerFlags% ../TwoByTwo/code/app.res ../TwoByTwo/code/win32_tbt.cpp /link %LinkerFlags% user32.lib gdi32.lib Comdlg32.lib Shell32.lib

popd
