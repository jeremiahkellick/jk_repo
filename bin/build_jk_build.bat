@echo off

set script_directory=%~dp0
set jk_build_source_path=%script_directory%\..\jk_src\jk_build\jk_build.c

if not defined DevEnvDir call "%script_directory%\jk_msvc_shell.bat"

pushd "%script_directory%\..\bin"
cl /W4 /wd4100 /nologo /Gm- /GR- /D _CRT_SECURE_NO_WARNINGS /Zi /std:c11 /EHa- /Od "%jk_build_source_path%" /link /INCREMENTAL:NO
popd

exit /b %ERRORLEVEL%
