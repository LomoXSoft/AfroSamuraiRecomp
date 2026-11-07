# Setup program for 0.1.0

The wizard asks, in order: language (Español / Français with accents), install folder, one of game ISO or extracted folder, the Build Tools license, shortcuts and whether to delete the downloaded tools, then a short summary. The source page is gone. The included 0.1.0 tree is used. `--local-tree=PATH` is a hidden test switch, not a page. There is a progress bar driven by `install.ps1` percent lines, with the log behind Show details. Success shows "Instalación finalizada" and Jugar, which starts `game\Play.bat` only when that button is clicked and the file exists. Finalizar and the window X do not start the game. Back on that page returns to the summary and does not install again.

`tools/afrosetup-v0.1.0.exe` is a Windows setup program. It is not the game, and it
does not contain a disc image, `default.xex`, or `default.pe`. It asks for an
install folder and your own Europe disc, then runs the bundled
`tools/install.ps1`. That script downloads the portable toolchain and builds
`afrorecomp.exe`.

`install.bat` is unchanged. It is still the console fallback and still calls
`tools\install.ps1` with whatever arguments you pass. Its default branch, when
you do not pass `-Ref`, remains `main`. Do not use that default to test 0.1.0.

## What the setup program actually runs

Only after you press Install. Cancel, Esc, and the window X before that start
no process.

PowerShell is `System32\WindowsPowerShell\v1.0\powershell.exe` (not a `PATH`
lookup). The working directory is the `tools` folder of the source tree. The
script path is absolute. Arguments are quoted with Windows command-line rules,
including spaces, quotes, and a trailing backslash.

The command is `-File` plus:

- `-InstallDir`
- `-Language en|es|fr|it`
- `-Shortcut yes|no`
- `-AcceptMsvcLicense` only if the checkbox was turned on (it starts off)
- `-KeepBuildFiles` or `-CleanupAfterInstall`
- `-LocalTree` the 0.1.0 source
- `-NonInteractive`
- `-Iso` or `-GameDataDir`

It does not pass `-Ref` or `-RepoUrl`. `install.ps1` refuses a non-interactive
run that would check out public `main`. The source must contain `app\VERSION`
equal to `0.1.0` and a copy of this `install.ps1` (the one that understands
`-LocalTree`). The default choice is the 0.1.0 tree stored inside the exe.
You can instead point at a folder you prepared from this archive, overlaid on
a full tree.

The license page links to `https://go.microsoft.com/fwlink/?LinkId=2327714`.
The checkbox is not pre-checked.

While PowerShell is running, X, Esc, and Cancel do not call
`TerminateProcess` and do not put the build in a kill-on-close job. The window
stays up and says the build continues. Output is shown in the window.
`InstallDir\install.log` is the script log. `InstallDir\afrosetup-command.txt`
is the exact command line, UTF-16. A bad Europe ISO fails instead of asking a
hidden yes/no question. On success the script's own layout is
`InstallDir\game\Play.bat`.

The manifest is `asInvoker`. There is no PATH change and no registry change.
Shortcuts, if you ask for them, are the ones `install.ps1` already creates.

## This environment

Built with `x86_64-w64-mingw32-g++` 13 (`-mwindows -municode -static`). The
file is PE32+ x86-64, Windows GUI subsystem, `asInvoker`, with the source
snapshot in an `AFSTP001` overlay (55 files, `app/VERSION` `0.1.0`).

Checked here:

- command-policy tests (`checks/test_setup_cmd.cpp`), including spaces,
  Unicode, trailing backslash, no public `main`, license not passed when
  unchecked, and close-never-kills
- the built exe's payload parses back to VERSION `0.1.0`, the local-tree
  `install.ps1`, `install.bat`, and the meter-death hook
- the earlier invulnerability static checks

The author later ran a clean Windows install with the first 0.1.0 setup
program, and a same-version update with a later one. Jugar started the game.
That is the author's use, not an independent playthrough of the whole game.
The Cheats warning and the shorter progress line shipped in this release were
not run again on Windows after they were added.

## Rebuild the setup exe on Windows

From a full tree that already contains these 0.1.0 files:

```
tools\build_setup.bat
```

That writes `tools\afrosetup-v0.1.0.exe` from `app\VERSION`. It does not replace the file already attached to the v0.1.0 release.

That needs `clang++` and Python 3. It does not need the game disc. The cross
build used here is `bash tools/build_setup.sh` with `REPO` pointing at a
complete checkout and the overlay equal to this drop.
