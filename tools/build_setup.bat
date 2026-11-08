@echo off
rem Build tools\afrosetup-v<app\VERSION>.exe. This is the setup program, not the game.
rem It does not upload or replace a GitHub release asset.
rem Requires clang++ (LLVM 20 from the portable toolchain, or Visual Studio's clang)
rem and Python 3. install.bat does not need this script.
setlocal
cd /d "%~dp0"
where clang++ >nul 2>&1 || (echo clang++ was not found. Install LLVM or run from the portable tools\llvm\bin. & exit /b 1)
where python >nul 2>&1 || (echo python was not found. & exit /b 1)
set "VER="
for /f "usebackq delims=" %%V in ("%~dp0..\app\VERSION") do if not defined VER set "VER=%%V"
if not defined VER (echo app\VERSION is empty. & exit /b 1)
set "OUT=afrosetup-v%VER%.exe"
python make_setup_art.py
windres -O coff afrosetup.rc -o afrosetup.res 2>nul
if exist afrosetup.res (
  clang++ -std=c++17 -O2 -Wall -Wextra -Werror -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -mwindows -static -static-libgcc -static-libstdc++ -o "%OUT%" afrosetup.cpp afrosetup.res -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -lcomctl32 || exit /b 1
) else (
  clang++ -std=c++17 -O2 -Wall -Wextra -Werror -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -mwindows -static -static-libgcc -static-libstdc++ -o "%OUT%" afrosetup.cpp -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -lcomctl32 || exit /b 1
)
python pack_setup_payload.py --exe "%OUT%" --repo "%~dp0.." --overlay "%~dp0.." || exit /b 1
echo built %~dp0%OUT%
