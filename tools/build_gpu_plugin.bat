@echo off
rem Build a patched rexgpu-xenos.dll (ReXGlue GPU plugin) from source, locally.
rem Applies every patches\rexglue\*.patch (sorted by name, idempotent) to ..\..\rexglue-sdk (ReXGlue
rem development branch, same commit as the SDK nightly in ..\..\sdk) and builds
rem ONLY the GPU plugin and rexruntime.dll (audio/kernel patches live there). Copies it next to afrorecomp.exe; the stock DLL is kept
rem once as rexgpu-xenos.dll.stock. Run from project\ AFTER tools\build_win.bat.
setlocal
set R=%~dp0..\..
call "%~dp0env_win.bat" || exit /b 1
cd /d "%R%\rexglue-sdk" || exit /b 1
for /f "delims=" %%F in ('dir /b /on "%~dp0..\patches\rexglue\*.patch"') do call :apply "%~dp0..\patches\rexglue\%%F" || exit /b 1
cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF || exit /b 1
rem libmspack ships git symlinks that Windows checks out as text files: replace them by copies.
powershell -NoProfile -Command "gci -Recurse -File thirdparty\libmspack | ? Length -lt 200 | %% { $t=[IO.File]::ReadAllText($_.FullName).Trim(); if($t.Contains([char]47) -and -not $t.Contains([char]32)){ $s=Join-Path $_.DirectoryName $t; if(Test-Path $s -PathType Leaf){ Copy-Item $s $_.FullName -Force } } }"
cmake --build out\build\win-amd64 --config Release --target rexgpu-xenos rexruntime -j %NUMBER_OF_PROCESSORS% || exit /b 1
set D=%~dp0..\app\out\build\win-amd64-release
if not exist "%D%\rexgpu-xenos.dll.stock" copy /y "%D%\rexgpu-xenos.dll" "%D%\rexgpu-xenos.dll.stock" >nul
copy /y out\win-amd64\Release\rexgpu-xenos.dll "%D%\rexgpu-xenos.dll" >nul || (echo COPY FAILED & exit /b 1)
if not exist "%D%\rexruntime.dll.stock" copy /y "%D%\rexruntime.dll" "%D%\rexruntime.dll.stock" >nul
copy /y out\win-amd64\Release\rexruntime.dll "%D%\rexruntime.dll" >nul || (echo COPY FAILED & exit /b 1)
echo GPU PLUGIN + RUNTIME OK
exit /b 0

:apply
git apply --ignore-whitespace --reverse --check "%~1" >nul 2>&1 && (echo %~nx1 already applied& exit /b 0)
git apply --ignore-whitespace "%~1" || (echo PATCH FAILED: %~nx1& exit /b 1)
echo %~nx1 applied
exit /b 0
