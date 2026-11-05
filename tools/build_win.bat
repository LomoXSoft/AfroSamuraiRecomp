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
rem Game-derived inputs (private, gitignored): PE basefile, switch tables, icon.
if not exist afro_switch_tables.toml (
  python ..\tools\xex_to_pe.py assets\default.xex assets\default.pe "%R%\rexglue-sdk" || exit /b 1
  python ..\tools\gen_switch_tables.py assets\default.pe afro_switch_tables.toml || exit /b 1
  python ..\tools\extract_icon.py assets\default.pe afro_icon.ico
)
rem Codegen when stamp missing. Also regenerate when afro_hooks.toml content changes
rem (present-interval / tick mid-asm hooks): compare a sidecar hash.
set HOOKHASH=
for /f %%H in ('certutil -hashfile afro_hooks.toml SHA1 ^| findstr /v ":"') do set HOOKHASH=%%H
if not exist generated\default\gen_v3.stamp goto dogegen
if not exist generated\default\hooks.sha1 goto dogegen
set /p OLDHOOK=<generated\default\hooks.sha1
if /I not "%OLDHOOK%"=="%HOOKHASH%" goto dogegen
goto ogen
:dogegen
echo Running rexglue codegen...
"%R%\sdk\win-amd64\bin\rexglue.exe" codegen afrorecomp_manifest.toml --max_jump_table_entries=8192 > ..\..\codegen_win.log 2>&1 || (echo CODEGEN FAILED & exit /b 1)
echo ok> generated\default\gen_v3.stamp
echo %HOOKHASH%> generated\default\hooks.sha1
:ogen
cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH="%R%\sdk\win-amd64" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ || exit /b 1
cmake --build out\build\win-amd64-release -j %NUMBER_OF_PROCESSORS% || exit /b 1
rem The app build copies the stock SDK DLLs next to the exe: put the patched ones back
rem (built by tools\build_gpu_plugin.bat) so the order of the two scripts does not matter.
set D=%~dp0..\app\out\build\win-amd64-release
set P=%R%\rexglue-sdk\out\win-amd64\Release
for %%F in (rexgpu-xenos.dll rexruntime.dll) do if exist "%P%\%%F" (copy /y "%P%\%%F" "%D%\%%F" >nul && echo patched %%F restored)
echo BUILD OK
