@echo off
rem Launch Afro Samurai recomp (Windows). Logs to afro.log next to this repo.
set R=%~dp0..\..
cd /d "%~dp0..\app\out\build\win-amd64-release"
afrorecomp.exe --game_data_root="%R%\gamedata" --gpu_plugin=xenos --readback_resolve=fast --log_file="%R%\afro.log" %*
