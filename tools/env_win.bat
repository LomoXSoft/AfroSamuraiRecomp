@echo off
rem Toolchain PATH setup shared by the build scripts. Uses portable tools under
rem <root>\tools if present, otherwise whatever is on PATH (VS Developer Prompt,
rem LLVM, CMake, Ninja, Git, Python).
set R=%~dp0..\..
if exist "%R%\tools\msvc\setup_x64.bat" call "%R%\tools\msvc\setup_x64.bat"
if exist "%R%\tools\llvm\bin" set "PATH=%R%\tools\llvm\bin;%PATH%"
if exist "%R%\tools\git\cmd" set "PATH=%R%\tools\git\cmd;%PATH%"
set "PATH=%LOCALAPPDATA%\Microsoft\WinGet\Links;%PATH%"
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Kitware.CMake_*") do for /d %%E in ("%%D\cmake-*") do call set "PATH=%%E\bin;%%PATH%%"
for /d %%D in ("%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_*") do call set "PATH=%%D;%%PATH%%"
where clang++ >nul 2>&1 || (echo clang++ not found & exit /b 1)
where cmake >nul 2>&1 || (echo cmake not found & exit /b 1)
where ninja >nul 2>&1 || (echo ninja not found & exit /b 1)
exit /b 0
