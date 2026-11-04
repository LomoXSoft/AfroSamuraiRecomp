@echo off
rem Toolchain PATH setup shared by the build scripts (changes this console only).
rem Portable tools under <root>\tools (as installed by install.bat) take precedence;
rem otherwise whatever is on PATH is used (VS Developer Prompt, LLVM, CMake, Ninja,
rem Git, Python) plus winget per-user installs of CMake and Ninja.
set R=%~dp0..\..
set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Kitware.CMake_*") do for /d %%E in ("%%D\cmake-*") do call set "PATH=%%E\bin;%%PATH%%"
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_*") do call set "PATH=%%D;%%PATH%%"
if exist "%R%\tools\msvc\setup_x64.bat" call "%R%\tools\msvc\setup_x64.bat"
if exist "%R%\tools\llvm\bin" set "PATH=%R%\tools\llvm\bin;%PATH%"
if exist "%R%\tools\git\cmd" set "PATH=%R%\tools\git\cmd;%PATH%"
if exist "%R%\tools\cmake\bin" set "PATH=%R%\tools\cmake\bin;%PATH%"
if exist "%R%\tools\ninja\ninja.exe" set "PATH=%R%\tools\ninja;%PATH%"
if exist "%R%\tools\python\python.exe" set "PATH=%R%\tools\python;%PATH%"
where clang++ >nul 2>&1 || (echo clang++ not found & exit /b 1)
where cmake >nul 2>&1 || (echo cmake not found & exit /b 1)
where ninja >nul 2>&1 || (echo ninja not found & exit /b 1)
where git >nul 2>&1 || (echo git not found & exit /b 1)
where python >nul 2>&1 || (echo python not found & exit /b 1)
exit /b 0
