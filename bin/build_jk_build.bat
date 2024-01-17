@echo off

set script_directory=%~dp0
set jk_build_source_path=%script_directory%..\jk_src\jk_build\jk_build.c

if not defined DevEnvDir call "%script_directory%shell.bat"

pushd "%script_directory%..\bin"
cl /W4 /D _CRT_SECURE_NO_WARNINGS /Zi /std:c++20 /EHsc %jk_build_source_path%
popd

exit /b %ERRORLEVEL%
