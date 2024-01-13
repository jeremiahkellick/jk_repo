@echo off

set script_directory=%~dp0
set file_path=%~f1
set get_dependencies_path=%script_directory%..\build\get_dependencies.exe

if not defined DevEnvDir call "%script_directory%shell.bat"

pushd "%script_directory%..\build"
if not exist "%get_dependencies_path%" cl /W4 /D _CRT_SECURE_NO_WARNINGS /Zi "%script_directory%..\jk_src\c_build_utils\get_dependencies\get_dependencies.c" "%script_directory%..\jk_src\jk_lib\jk_path_utils.c"
for /f "usebackq tokens=*" %%a in (`%get_dependencies_path% "%file_path%"`) do cl /W4 /D _CRT_SECURE_NO_WARNINGS /Zi /std:c++20 /EHsc %%a
popd

exit /b %ERRORLEVEL%
