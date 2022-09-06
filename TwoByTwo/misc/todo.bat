@echo off

pushd ..\code
FINDSTR /S "TODO" *
popd
