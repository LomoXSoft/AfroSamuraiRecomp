# Graphical installer

Implemented in this drop as `tools/afrosetup-v0.1.0.exe` (the name comes from `app/VERSION`), `tools/afrosetup.cpp`,
`tools/build_setup.sh`, `tools/build_setup.bat`, and the `-LocalTree` /
`-NonInteractive` additions in `tools/install.ps1`. Usage and the checks that
were actually run are in `docs/SETUP_0.1.0.md`. The notes below are the
original requirements.

# Graphical installer — original plan

Do this after the 0.1.0 source artifact is on the Windows PC. Do not publish.
Do not bundle a game ISO, `default.xex`, `default.pe`, or generated game data.
Keep `install.bat` working as the console fallback. A clean Windows
end-to-end install test is required before any release.

## What already exists

`install.bat` only launches:

`powershell -NoProfile -ExecutionPolicy Bypass -File tools\install.ps1`

`tools/install.ps1` is the whole source-build flow. It already refuses admin
changes to PATH and the registry, downloads a portable toolchain into
`<root>\tools`, clones the repo, checks the Europe ISO SHA-1
`29394DB1854DFC59C767D768C8D35E0A08ACB73D`, extracts the user's disc, and runs
`tools\build_win.bat` plus `tools\build_gpu_plugin.bat`. Re-runs skip steps
stamped in `<root>\.install`. Log: `<root>\install.log`.

Flags the wizard must pass through (do not reimplement the downloads or the
build):

| Wizard choice | `install.ps1` parameter |
|---------------|-------------------------|
| Install folder | `-InstallDir` |
| ISO | `-Iso` |
| Already extracted disc (`default.xex` present) | `-GameDataDir` |
| Language EN/ES (FR/IT if shown) | `-Language en\|es\|fr\|it` |
| MSVC build-tools license accepted | `-AcceptMsvcLicense` |
| Desktop / Start menu shortcut | `-Shortcut yes\|no` |
| Keep toolchain after success | `-KeepBuildFiles` or `-CleanupAfterInstall` |

Default `-Ref` is `main` and `-RepoUrl` is the public GitHub repo. **0.1.0 is
not published.** A wizard built now must not tell the user it will install
0.1.0 from GitHub `main`. Until the user chooses to publish, the test build
has to point `-RepoUrl` at the local tree (or a private ref the user names).
The public installer may default to `main` only after that commit exists.

## Program to add

`tools/afrosetup-v0.1.0.exe`, a small Win32 wizard (same API style as
`app/src/language_picker.cpp`: no new toolkit, no admin manifest). It collects
choices and starts the existing PowerShell script. It does not copy game
bytes into the exe.

Suggested files, all new, none replacing `install.bat`:

- `tools/afrosetup.cpp` — wizard
- `tools/build_setup.bat` — builds `afrosetup-v` plus `app/VERSION` (now `afrosetup-v0.1.0.exe`) with the portable clang or
  MSVC already used by `build_win.bat` (`-mwindows -municode`, subsystem
  windows). Link only `comctl32`, `shell32`, `ole32`.
- `install.bat` stays. Add one line of text pointing at `afrosetup-v0.1.0.exe` if it
  sits next to the bat. Do not remove the PowerShell path.

## Wizard pages (EN and ES, same tags)

1. Language. Preselect from the Windows UI language the way `install.ps1` does.
2. Install folder. Warn if the path has spaces or non-ASCII characters (the
   README already requires that). Show the about-20 GB need. Do not create the
   folder until the user confirms the summary.
3. Game files. One of: ISO file, or a folder that already contains
   `default.xex`. The exe does not download a game. Say the Europe disc SHA-1
   and that `install.ps1` rejects a mismatch.
4. License. Checkbox for the Microsoft Build Tools license. Continue stays
   disabled until it is checked. This becomes `-AcceptMsvcLicense`.
5. Shortcut yes/no, and whether to delete the toolchain after a successful
   build (`-CleanupAfterInstall`) or keep it (`-KeepBuildFiles`).
6. Summary of every flag that will be passed. Back returns without starting.
7. Progress. Run
   `powershell.exe -NoProfile -ExecutionPolicy Bypass -File <ps1> <flags>`
   with the working directory set to the chosen root's script location.
   Stream `install.log` into a read-only box. Cancel asks before killing the
   process; a killed run must leave `.install` stamps so a later run resumes
   rather than deleting a partial toolchain blindly.
8. Result. On success, show the path of `afrorecomp.exe` and the shortcut
   choice. On failure, show the last log lines and the exit code. Do not
   claim the game was tested.

Esc and the window X on pages 1–6 close the wizard and run nothing. They must
not launch PowerShell.

## Requirements checklist

- No UAC / `requireAdministrator` manifest. If a write to the folder fails,
  say so and stop.
- No `setx`, no PATH edit, no registry writes. Shortcuts, if requested, are
  the ones `install.ps1` already creates.
- No game ISO or extracted assets inside the repo, the exe, or the installer
  zip.
- `install.bat` still performs a full install with no exe present.
- Bilingual strings live in the cpp next to the pages, EN and ES at minimum.
- The wizard prints the exact command line (with the ISO path) into
  `install.log` so a failure can be repeated with `install.bat`.

## Windows test before any release

On a clean Windows 10 or 11 x64 user account that is not an administrator:

1. `install.bat` still completes with the user's own ISO (fallback).
2. `afrosetup-v0.1.0.exe` in English: cancel on the summary page, confirm no
   `powershell` child and no partial root.
3. `afrosetup-v0.1.0.exe` in Spanish: full run to `afrorecomp.exe`, Europe ISO hash
   enforced, shortcut matches the choice.
4. Re-run the exe on the same folder and confirm stamped steps are skipped.
5. Point it at a bad ISO and confirm `install.ps1` stops on the SHA-1 check.
6. Confirm the exe contains no `default.xex` and the process token did not
   elevate.
7. Only after that, and only if the user asks to publish, point `-Ref` at the
   published 0.1.0 commit.

This environment cannot run that test. The follow-up that adds the exe still
must not publish.
