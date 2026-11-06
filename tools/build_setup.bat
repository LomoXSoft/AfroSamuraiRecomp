@echo off
rem Build tools\afrosetup.exe on Windows. This is the setup program, not the game.
rem Requires clang++ (LLVM 20 from the portable toolchain, or Visual Studio's clang)
rem and Python 3. install.bat does not need this script.
setlocal
cd /d "%~dp0"
where clang++ >nul 2>&1 || (echo clang++ was not found. Install LLVM or run from the portable tools\llvm\bin. & exit /b 1)
where python >nul 2>&1 || (echo python was not found. & exit /b 1)
python make_setup_icon.py
windres -O coff afrosetup.rc -o afrosetup.res 2>nul
if exist afrosetup.res (
  clang++ -std=c++17 -O2 -Wall -Wextra -Werror -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -mwindows -static -static-libgcc -static-libstdc++ -o afrosetup.exe afrosetup.cpp afrosetup.res -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -lcomctl32 || exit /b 1
) else (
  clang++ -std=c++17 -O2 -Wall -Wextra -Werror -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -municode -mwindows -static -static-libgcc -static-libstdc++ -o afrosetup.exe afrosetup.cpp -lcomdlg32 -lshell32 -lole32 -luuid -lgdi32 -lcomctl32 || exit /b 1
)
python pack_setup_payload.py --exe afrosetup.exe --repo "%~dp0.." --overlay "%~dp0.." || exit /b 1
echo built %~dp0afrosetup.exe
