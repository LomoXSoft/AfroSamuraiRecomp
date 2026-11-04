# AfroSamuraiRecomp – Afro Samurai (Xbox 360) static recompilation

Unofficial, work-in-progress static recompilation of **Afro Samurai** (Xbox 360,
Europe, title ID 4E4D07D8) to native Windows x64 using
[ReXGlue](https://github.com/rexglue/rexglue-sdk).

**This repository contains no game files.** You need your own original disc
(or an ISO you dumped from it). No binaries are released: the executable
contains code translated from the game, so everyone builds it from their own copy.

## Contents
- [What it is](#what-it-is)
- [Status and limitations](#status-and-limitations)
- [Quick start (installer)](#quick-start-installer)
- [Requirements](#requirements)
- [Getting your game files](#getting-your-game-files)
- [Manual build](#manual-build)
- [Playing](#playing) ([Controls](#controls), [Saves](#saves))
- [Launcher and options](#launcher-and-options)
- [Troubleshooting](#troubleshooting)
- [Repository layout](#repository-layout)
- [Plans](#plans)
- [License and legal notice](#license-and-legal-notice)

## What it is
The game's PowerPC executable (`default.xex`) is translated ahead of time into
C++ by ReXGlue's code generator and compiled into a native Windows program. The
Xbox 360 kernel, GPU and audio are provided by the ReXGlue runtime (derived from
Xenia). This repository holds only the project configuration (function
boundaries, hooks), a small launcher, build scripts and a GPU plugin patch.

## Status and limitations
Preview (0.0.14). Tested on one PC (Windows, AMD Radeon RX 9060 XT).
- Boots, plays videos, menus and gameplay; EN/ES/FR/IT language selection.
- Default GPU path is **RTV** (host render targets): correct image at a stable
  30 FPS. The black frames of earlier versions are fixed by the
  `afro-edram-no-wrap` GPU plugin patch.
- **ROV** is still available as an optional fallback (correct image but slow,
  can drop below 15 FPS): Advanced options (`--advanced`) or
  `--render_target_path_d3d12=rov`.
- Voices work, including all cinematic dialogue (0.0.13 `afro-xma-fix-log` and
  0.0.14 `afro-xma-frameless-chain` runtime patches).
- Known issues: FPS drops when objects break or the camera splits;
  if two tutorial prompts appear at once, the first one can stay
  stuck on screen (original game behaviour; restarting the level clears it).
- Only the Europe disc (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) is supported. Other versions will not work.
- Windows x64 only for now (D3D12).
- Expect bugs; saves and settings may change between versions.

## Quick start (installer)
1. Make an ISO of your own Afro Samurai (Europe) disc (see [Getting your game files](#getting-your-game-files)).
2. Download [`install.bat`](https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/install.bat)
   (right-click, *Save link as*) into an empty folder, or clone/download this repository
   and use the `install.bat` at its root.
3. Double-click `install.bat`. It opens a folder picker (a new `AfroSamuraiRecomp` folder is
   created inside the folder you choose) and a file picker for your ISO,
   then does everything else. Started from a clone, it builds that checkout and
   proposes `AfroSamuraiRecomp-install` next to it as the install folder.
4. All questions (install folder, ISO, Microsoft license, shortcuts) come first;
   then it runs to the end without stopping. When it says `INSTALL OK`, play with the
   desktop shortcut or `<root>\game\Play.bat`.
5. Finally it offers (default No) to delete what is only needed for building
   (toolchain, ReXGlue SDK/source, build files, downloads) and keep just `<root>\game`,
   `<root>\gamedata` and the logs. Updating later then needs a full new install.

What the installer does (`tools/install.ps1`, run with the PowerShell execution
policy bypassed for that process only):
- Checks free disk space (about 20 GB) and lists your GPU (Direct3D 12 needed).
- Verifies the ISO SHA-1 (`29394DB1854DFC59C767D768C8D35E0A08ACB73D`) and warns if it does not match.
- Downloads a **portable** toolchain into `<root>\tools`, only from official
  sources, each pinned and SHA-256-verified: MinGit, CMake, Ninja, LLVM/Clang
  20.1.8, Python 3.12 (embeddable) + pycryptodome, and the MSVC compiler + Windows SDK
  (fetched from Microsoft's Visual Studio servers with
  [portable-msvc.py](https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977);
  you are asked to accept the Microsoft Visual Studio license).
- Downloads the ReXGlue SDK nightly, clones the ReXGlue source at `bd833a2` and this repository,
  extracts your disc into `<root>\gamedata`, runs both build scripts and checks the output.
- Optionally creates desktop and Start menu shortcuts.

No admin rights needed; nothing is installed system-wide (no PATH, registry or
Visual Studio installer changes) and nothing is uploaded. Downloads are about
1 GB of archives (LLVM alone is 900 MB), about 1 GB of MSVC/Windows SDK packages
and about 2 GB of ReXGlue source; the whole folder ends up around 12-13 GB
(`<root>\downloads` can be deleted afterwards). A full run took about 30 minutes on
a fast connection with a modern CPU; expect up to 1-2 hours on slower ones.
It shows the overall progress (e.g. `[ 42%] Step 5/14: LLVM/Clang ... (downloading ...: 312/896 MB)`)
in the console, the window title and the log. Re-running it skips finished steps, so after an error just fix the cause and run it again.
The log is `<root>\install.log`. To uninstall, delete the install folder (and
the shortcuts; saves are separate, see [Saves](#saves)).

Command-line options (`install.bat -Option value`): `-InstallDir <dir>` (full install folder, skips the folder picker), `-Iso <file>`,
`-AcceptMsvcLicense`, `-Shortcut yes|no`, `-CleanupAfterInstall` / `-KeepBuildFiles`, `-Ref <branch|tag>`. Advanced:
`-GameDataDir <dir>` uses an already extracted disc folder (containing `default.xex`)
instead of an ISO.

## Requirements
- Windows 10/11 x64, a GPU with Direct3D 12 and current drivers.
- Original Afro Samurai disc (Europe) and a way to make an ISO of it.
- About 20 GB free disk space (the finished install uses about 12-13 GB).
- A controller (see [Controls](#controls)).
- For a manual build only (the installer downloads all of this):
  - MSVC + Windows SDK: Visual Studio 2022 Build Tools ("Desktop development with C++"),
    run from an "x64 Native Tools Command Prompt", or a portable MSVC in
    `<root>\tools\msvc` with `setup_x64.bat` (as produced by portable-msvc.py).
  - LLVM/Clang 20 (`clang++`), CMake >= 3.25, Ninja, Git.
  - Python 3.8 or newer with pycryptodome: `python -m pip install pycryptodome`.
  - Each of these can be on PATH or portable under `<root>\tools`
    (`git\cmd`, `llvm\bin`, `cmake\bin`, `ninja`, `python`, `msvc`); portable copies win.
- ReXGlue SDK nightly **0.10.0.24-dev.gbd833a2** (prebuilt) and the ReXGlue
  source at the same commit.

## Getting your game files
Only the Europe disc is supported (Redump 17884, ISO SHA-1
`29394DB1854DFC59C767D768C8D35E0A08ACB73D`). Make the ISO from your own disc,
for example with a DVD drive that can read Xbox 360 discs and a disc dumping
tool, or from a backup of your own disc that you made with your console.
Check the SHA-1 with `certutil -hashfile "Afro Samurai.iso" SHA1`.
This project does not help with obtaining the game in any other way.
Never share the ISO, the extracted files or anything generated from them.

## Manual build
Folder layout (`<root>` = e.g. `C:\afro`; avoid spaces and non-ASCII characters in it):

    <root>\afrorecomp      this repository (any folder name works)
    <root>\sdk\win-amd64   ReXGlue SDK nightly (bin\rexglue.exe inside)
    <root>\rexglue-sdk     ReXGlue source at bd833a2
    <root>\gamedata        your extracted disc (default.xex inside)
    <root>\tools           optional portable toolchain

1. Open a command prompt (the "x64 Native Tools" one if you use Visual Studio Build Tools) and create the root:

       mkdir C:\afro
       cd /d C:\afro

2. Clone this repository:

       git clone https://github.com/LomoXSoft/AfroSamuraiRecomp.git afrorecomp

3. Download `rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip` from
   <https://github.com/rexglue/rexglue-sdk/releases/tag/nightly-20261002-bd833a2a>
   and extract it so that `C:\afro\sdk\win-amd64\bin\rexglue.exe` exists.
4. Clone the ReXGlue source at the same commit (a few GB with submodules):

       git clone https://github.com/rexglue/rexglue-sdk.git rexglue-sdk
       cd rexglue-sdk
       git checkout bd833a2
       git submodule update --init --recursive
       cd ..

5. Extract your disc into `C:\afro\gamedata` (takes a few minutes):

       python afrorecomp\tools\xiso_extract.py "D:\path\to\Afro Samurai.iso" C:\afro\gamedata /

   `C:\afro\gamedata\default.xex` must exist afterwards.
6. Build the game and then the patched GPU plugin and runtime:

       cd afrorecomp
       tools\build_win.bat
       tools\build_gpu_plugin.bat

   Both end with `BUILD OK` and `GPU PLUGIN + RUNTIME OK`.

`build_win.bat` copies your `default.xex`, extracts (privately, gitignored) the
PE image, jump-table overrides and icon from it, runs the ReXGlue code
generator (a few minutes, ~130 MB of generated C++, log in `<root>\codegen_win.log`)
and compiles. The output is
`app\out\build\win-amd64-release\afrorecomp.exe`. `build_gpu_plugin.bat` applies
`patches\rexglue\*.patch` to `<root>\rexglue-sdk` and builds the patched
`rexgpu-xenos.dll` and `rexruntime.dll` next to the exe (see `patches/rexglue/README.md`);
it is required for a correct image and voices.

To update: `git pull` in `afrorecomp`, then run both scripts again. To rebuild
from scratch, also delete `app\generated\default` and `app\afro_switch_tables.toml`.

## Playing

    tools\run_win.bat

Run it from the `afrorecomp` folder (or use the installer's shortcut). It starts
the game with `<root>\gamedata` and writes the log to `<root>\afro.log`.
Hold **Shift** while starting to show the launcher again.

### Controls
An Xbox controller or any other controller SDL recognises (XInput, DualShock/DualSense,
etc.) works out of the box; the game was designed for a gamepad and prompts show
Xbox buttons. Keyboard and mouse are **not** enabled by default. There is
experimental keyboard emulation of a controller from ReXGlue, untested with this game:
start with `tools\run_win.bat --mnk_mode=true` (WASD = left stick, arrow keys = right stick,
Space = A, Backspace = B, L = X, P = Y, Q/E = triggers, 1/3 = bumpers, Enter = Start,
Tab = Back, Shift+arrows = D-pad).

### Saves
Saves are stored in your Documents folder, in `Documents\afrorecomp\` (the save
itself is under `<profile id>\4E4D07D8\00000001\AFRO_0.SAV`; the shader cache is in
`Documents\afrorecomp\cache`). To back them up, copy the whole
`Documents\afrorecomp` folder while the game is closed; to restore, copy it back.
Deleting or rebuilding the install folder does not touch your saves.

The launcher (shown at start, or hold Shift) has two buttons for this:
- **Open saves folder** opens `Documents\afrorecomp` in Explorer (created if missing).
- **Back up saves** copies the whole folder (except the shader `cache`) to a new
  `Documents\afrorecomp-backups\saves-YYYYMMDD-HHMMSS\` folder and offers to open it.
  It never deletes or overwrites anything.

To restore a backup: close the game, then copy the contents of a `saves-...` folder
back into `Documents\afrorecomp\` (keep a copy of the current folder first if unsure).

## Launcher and options
The launcher (shown on first run, after each update, or with Shift) offers:
language (English, Español, Français, Italiano), fullscreen or window, internal
resolution (1x native by default / 2x / 3x) and "remember and don't ask again".
The **Advanced settings (experimental)** button opens the frame-rate cap
(default "Original (30 capped)": the game's own 30 FPS pacing, no override;
30 / 60 are experimental) and the sepia filter toggle.
Settings are stored in `afro_language.cfg` next to the exe; without "remember",
resolution, frame rate and sepia go back to their defaults on the next start.

Developer options: start with `--advanced` (e.g. `tools\run_win.bat --advanced`)
to show the "Advanced options..." button (render path, resolve readback, game
effect toggles, GPU diagnostics). Any ReXGlue cvar can also be passed on the
command line, e.g. `--render_target_path_d3d12=rov`.

## Troubleshooting
- **`xex_to_pe.py needs pycryptodome`**: run `python -m pip install pycryptodome`
  with the same `python` the build uses (`where python`). The installer's portable Python already has it.
- **`clang++ not found` / `cmake not found` / `ninja not found`**: the tool is neither
  on PATH nor under `<root>\tools`. Install it, use the installer, or check the folder names in the layout above.
- **`PATCH FAILED: <name>.patch`** in `build_gpu_plugin.bat`: the patches are applied
  to `<root>\rexglue-sdk` in name order and stamped in `rexglue-sdk\.git\afro-patches`,
  so re-running is safe. A failure usually means the ReXGlue source is not at `bd833a2`
  or was modified. Reset it with `git -C <root>\rexglue-sdk checkout -f bd833a2`,
  `git -C <root>\rexglue-sdk clean -fd src include` and delete `.git\afro-patches`, then run the script again.
- **`CODEGEN FAILED`**: see `<root>\codegen_win.log`. Usually `gamedata\default.xex`
  is not from the Europe disc or the extraction was incomplete.
- **Black screen, black frames or broken image**: make sure `build_gpu_plugin.bat`
  ran after the last `build_win.bat` and printed `GPU PLUGIN + RUNTIME OK`, and update the GPU driver.
  As a fallback, try the ROV path: `tools\run_win.bat --render_target_path_d3d12=rov` (slower).
- **No voices in cutscenes or no sound**: the patched `rexruntime.dll` is missing
  (run `build_gpu_plugin.bat` again). Check that Windows' default audio output is the device you expect.
- **Game does not start / crashes**: look at the end of `<root>\afro.log`.

### Getting a log
Every run writes `<root>\afro.log` (overwritten each time, so copy it right after the problem).
Useful extra flags for `tools\run_win.bat`:
- `--afro_xma_log=true` – audio (XMA) decoding details, for sound or voice problems.
- `--afro_perf_log=true` – per-second performance counters, for FPS drops.

### Reporting an issue
Open an issue at <https://github.com/LomoXSoft/AfroSamuraiRecomp/issues> with:
the version or commit (`git -C <root>\afrorecomp log -1 --oneline`), your Windows version,
GPU and driver, what you did and what happened (a screenshot helps), and `afro.log`
(and `install.log` or the build output for build problems).
**Never attach game files**: no ISO, `default.xex`, `default.pe`, `gamedata` contents,
`app\assets`, `app\generated` or anything else extracted from the disc.

## Repository layout
- `app/` – ReXGlue project: manifest, `afro_functions.toml` (extra function
  boundaries), `afro_hooks.toml` (mid-asm hook), launcher and hooks in `src/`.
- `install.bat`, `tools/install.ps1` – one-click installer.
- `tools/` – build/run scripts and helpers (`xiso_extract.py`, `xex_to_pe.py`,
  `gen_switch_tables.py`, `extract_icon.py`, analysis helpers).
- `patches/rexglue/` – ReXGlue GPU plugin and runtime patches (BSD-3, see its README).

## Plans
- Performance work; controller and audio polish.
- Possibly other game versions and Linux/Vulkan.

## License and legal notice
The code in this repository is licensed under the **GNU GPL v3.0** (see
`LICENSE`), except `patches/rexglue/`, which modifies ReXGlue/Xenia code and is
under their BSD 3-Clause license.

This is a fan project, not affiliated with or endorsed by Namco Bandai, GONZO,
Microsoft or any rights holder. "Afro Samurai" and related names are trademarks
of their respective owners. No game code, assets or data are distributed here;
you must own the original game.
