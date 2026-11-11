@echo off
rem Build afrorecomp on Windows from your own game files.
rem usage: tools\build_win.bat   (run from the repo folder)
rem Expected layout (see README): <root>\afrorecomp (this repo), <root>\sdk\win-amd64,
rem <root>\rexglue-sdk (source), <root>\gamedata (your extracted disc), optional <root>\tools.
setlocal
set R=%~dp0..\..
call "%~dp0env_win.bat" || exit /b 1
cd /d "%~dp0..\app"
if not exist assets mkdir assets
copy /y "%R%\gamedata\default.xex" assets\default.xex >nul || (echo gamedata\default.xex not found & exit /b 1)
rem Europe or USA: writes app\region (hooks and addresses for this disc).
python ..\tools\region_config.py . || exit /b 1
rem Game-derived inputs (private, gitignored): PE basefile, switch tables, icon.
if not exist afro_switch_tables.toml (
  python ..\tools\xex_to_pe.py assets\default.xex assets\default.pe "%R%\rexglue-sdk" || exit /b 1
  python ..\tools\gen_switch_tables.py assets\default.pe afro_switch_tables.toml || exit /b 1
  python ..\tools\extract_icon.py assets\default.pe afro_icon.ico
)
rem Codegen when stamp missing. Also regenerate when region\afro_hooks.toml content changes
rem (present-interval / tick mid-asm hooks): compare a sidecar hash.
set HOOKHASH=
for /f %%H in ('certutil -hashfile region\afro_hooks.toml SHA1 ^| findstr /v ":"') do set HOOKHASH=%%H
if not exist generated\default\gen_v3.stamp goto dogegen
if not exist generated\default\hooks.sha1 goto dogegen
set /p OLDHOOK=<generated\default\hooks.sha1
if /I not "%OLDHOOK%"=="%HOOKHASH%" goto dogegen
echo codegen not forced: region\afro_hooks.toml unchanged; the build re-checks the other inputs
goto ogen
:dogegen
echo AFROPHASE codegen
echo Running rexglue codegen...
rem The MSVC bin holds the CRT that rexglue imports. Copy those DLLs beside
rem rexglue so Python's vcruntime, which is earlier on PATH, is not used.
set "REXBIN=%R%\sdk\win-amd64\bin"
set "CRTBIN=%VCToolsInstallDir%bin\Hostx64\x64"
python "%~dp0check_pe_imports.py" --stage-from "%CRTBIN%" "%REXBIN%\rexglue.exe"
if errorlevel 1 exit /b 2
set "CODELOG=%R%\codegen_rexglue.txt"
rem Same command line as the codegen rule in generated\rexglue.cmake. rexglue
rem fingerprints its flags: the max_jump_table_entries 8192 this used to pass
rem made that Ninja rule disagree and run the whole codegen a second time with
rem the default (512). Both emit byte-identical code for this game, and the
rem default is what the Ninja rule always compiled last.
"%REXBIN%\rexglue.exe" codegen afrorecomp_manifest.toml > "%CODELOG%" 2>&1
rem Not RC: CMake reads the environment variable RC as the resource compiler.
set GEN_EXIT=%ERRORLEVEL%
if exist "%CODELOG%" type "%CODELOG%"
if not "%GEN_EXIT%"=="0" (
  echo CODEGEN FAILED exit %GEN_EXIT%
  exit /b %GEN_EXIT%
)
echo ok> generated\default\gen_v3.stamp
echo %HOOKHASH%> generated\default\hooks.sha1
:ogen
echo AFROPHASE compile
rem A previous run stored the exit code 0 in RC. CMake then looked for a
rem compiler named "0" and left that in the cache. Drop only that cache.
set "BCACHE=%~dp0..\app\out\build\win-amd64-release"
set "CF=%BCACHE%\CMakeCache.txt"
if exist "%CF%" (
  findstr /R /C:"^CMAKE_RC_COMPILER:[A-Za-z]*=0$" "%CF%" >nul
  if not errorlevel 1 (
    echo Removing CMake cache that stored RC=0 as the resource compiler.
    del /q "%CF%"
    if exist "%BCACHE%\CMakeFiles" rmdir /s /q "%BCACHE%\CMakeFiles"
  )
)
if "%RC%"=="0" set "RC="
cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH="%R%\sdk\win-amd64" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
set CMAKE_EXIT=%ERRORLEVEL%
if not "%CMAKE_EXIT%"=="0" exit /b %CMAKE_EXIT%
cmake --build out\build\win-amd64-release -j %NUMBER_OF_PROCESSORS%
set BUILD_EXIT=%ERRORLEVEL%
if not "%BUILD_EXIT%"=="0" exit /b %BUILD_EXIT%
rem The app build copies the stock SDK DLLs next to the exe: put the patched ones back
rem (built by tools\build_gpu_plugin.bat) so the order of the two scripts does not matter.
set D=%~dp0..\app\out\build\win-amd64-release
set P=%R%\rexglue-sdk\out\win-amd64\Release
for %%F in (rexgpu-xenos.dll rexruntime.dll) do if exist "%P%\%%F" (copy /y "%P%\%%F" "%D%\%%F" >nul && echo patched %%F restored)
echo BUILD OK
