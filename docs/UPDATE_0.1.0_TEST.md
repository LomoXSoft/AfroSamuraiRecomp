# Update test checklist (Windows, not run here)

Use `/opt/cursor/artifacts/afrosetup.exe` from this drop. The earlier clean install
does not test this wizard, this updater, or invulnerability.

Before you click Update, nothing is started. The choice is the screen after language:
Instalar or Actualizar (Install / Update, and the French and Italian lines).

## Same-version refresh (both 0.1.0)

1. Close the game. If `afrorecomp.exe` is still open, Update stays blocked and the setup does not stop the process.
2. Choose Actualizar and select the install folder from the first setup (the folder that contains `game\Play.bat`).
3. The screen shows the version read from `afrorecomp\app\VERSION` or `.install\build.rev`, then `→ 0.1.0`. For this build that is `0.1.0 → 0.1.0`, plus one sentence that the program files are rebuilt and replaced only after the build succeeds. It does not show 0.1.1.
4. If `gamedata\default.xex` is there, the ISO page is skipped.
5. If `tools\msvc\setup_x64.bat` and `.install\msvc.done` are both there, the license checkbox is skipped and the summary says the earlier acceptance is still recorded. The box is never pre-ticked. If either file is missing, the checkbox is shown empty and must be ticked.
6. On the summary, read the choices, then click Actualizar. PowerShell starts only then.
7. Progress shows the percent lines from the installer. Show details holds the log. Downloading is not labeled as compiling.
8. When it finishes, Jugar runs `game\Play.bat` in that same folder, and only if you click it and the new build succeeded. Finalizar and the window X do not start the game.

What should still be there afterwards: `gamedata` (the extracted disc), any ISO you kept elsewhere, saves under Documents\afrorecomp, and `game\afro_language.cfg` (it is not rewritten). Extra files in `game` that are not program files from the build are left in place. `Play.bat` is left as it is if it already exists.

## Missing tools (you chose cleanup last time)

If `tools` was deleted, the summary says the build tools will be downloaded again and the extracted game is kept. Only the missing tools are fetched when some of them are still present. There is no second copy of the whole disc extract when `default.xex` is already there.

## Failure

If the new build does not produce `afrorecomp.exe`, `game\` is not swapped. If the swap itself fails, the program files that were moved aside are put back. That recovery is only for that swap. A finished update is not rolled back.

## Not a published release

`install.bat` is still the console installer and still defaults to public `main` if you do not pass `-Ref`. Do not use that default for this test. This setup does not pass `-Ref` or `-RepoUrl`.
