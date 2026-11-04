@echo off
rem AfroSamuraiRecomp installer: double-click me. Runs tools\install.ps1 with the
rem PowerShell execution policy bypassed for this process only (nothing is changed system-wide).
setlocal
set "PS1=%~dp0tools\install.ps1"
if not exist "%PS1%" set "PS1=%~dp0install.ps1"
if not exist "%PS1%" (
  echo Downloading install.ps1 from GitHub...
  set "PS1=%~dp0install.ps1"
  powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol='Tls12'; Invoke-WebRequest -UseBasicParsing https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/tools/install.ps1 -OutFile '%~dp0install.ps1'" || (echo Download failed. & pause & exit /b 1)
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%" %*
set ERR=%ERRORLEVEL%
pause
exit /b %ERR%
