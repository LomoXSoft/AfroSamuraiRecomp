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
rem An older quad-depth patch names a private enum. Upgrade that copy in place
rem before the stamp check, and do not delete the Ninja build.
python "%~dp0repair_quad_depth_access.py" "%CD%" || exit /b 1
rem Whole stack already in the tree? Reverse-apply all patches (last first) one after another
rem on a scratch index (the working tree is not touched): robust to later patches touching the
rem same lines. If that works, every patch is stamped as applied.
set "GIT_INDEX_FILE=%TEMP%\afro-patch-check.idx"
git read-tree HEAD && git add -u >nul 2>&1 && git add -A -- src include >nul 2>&1
set AFRO_STAMP_ONLY=1
for /f "delims=" %%F in ('dir /b /o-n "%~dp0..\patches\rexglue\*.patch"') do git apply --ignore-whitespace --reverse --cached "%~dp0..\patches\rexglue\%%F" >nul 2>&1 || set AFRO_STAMP_ONLY=
del "%GIT_INDEX_FILE%" >nul 2>&1
set "GIT_INDEX_FILE="
if defined AFRO_STAMP_ONLY echo all patches already applied (stack check)
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
rem Idempotent: a stamp per patch (name + git blob hash) in .git\afro-patches marks it applied;
rem without a stamp, a clean --reverse --check also counts (but fails once later patches touch
rem the same files, hence the stamp).
set "H="
for /f %%H in ('git hash-object "%~1"') do set H=%%H
set "ST=.git\afro-patches\%~nx1.%H%"
if not exist .git\afro-patches mkdir .git\afro-patches
if exist "%ST%" (
  if /I not "%~nx1"=="zzz-afro-quad-depth-offset.patch" (echo %~nx1 already applied [stamp]& exit /b 0)
  findstr /C:"PipelineCache::PipelineGeometryShader geometry_shader" "src\graphics\d3d12\pipeline_cache.cpp" >nul 2>&1
  if errorlevel 1 (echo %~nx1 already applied [stamp]& exit /b 0)
  echo %~nx1 stamp kept a helper that cannot see the private enum
)
if defined AFRO_STAMP_ONLY (
  if /I "%~nx1"=="zzz-afro-quad-depth-offset.patch" (
    findstr /C:"PipelineCache::PipelineGeometryShader geometry_shader" "src\graphics\d3d12\pipeline_cache.cpp" >nul 2>&1
    if not errorlevel 1 (echo %~nx1 stack stamp would hide the private-enum helper& exit /b 1)
  )
  type nul > "%ST%"
  echo %~nx1 already applied [stack]
  exit /b 0
)
git apply --ignore-whitespace --reverse --check "%~1" >nul 2>&1 && (echo %~nx1 already applied& type nul > "%ST%"& exit /b 0)
git apply --ignore-whitespace "%~1" || (
  if /I not "%~nx1"=="zzz-afro-quad-depth-offset.patch" (echo PATCH FAILED: %~nx1 - if it is already in the tree, create "%ST%"& exit /b 1)
  findstr /C:"bool PipelineCache::AfroQuadDepthShiftEnabled" "src\graphics\d3d12\pipeline_cache.cpp" >nul 2>&1 || (echo PATCH FAILED: %~nx1 - if it is already in the tree, create "%ST%"& exit /b 1)
  findstr /C:"static bool AfroQuadDepthShiftEnabled" "include\rex\graphics\d3d12\pipeline_cache.h" >nul 2>&1 || (echo PATCH FAILED: %~nx1 missing the private helper declaration& exit /b 1)
  echo %~nx1 already contained the repaired helper
)
type nul > "%ST%"
echo %~nx1 applied
exit /b 0
