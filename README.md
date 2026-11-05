# AfroSamuraiRecomp – Afro Samurai (Xbox 360) static recompilation

Unofficial, work-in-progress static recompilation of **Afro Samurai** (Xbox 360,
Europe, title ID 4E4D07D8) to native Windows x64 using
[ReXGlue](https://github.com/rexglue/rexglue-sdk).

**This repository contains no game files.** You need your own original disc
(or an ISO you dumped from it). No binaries are released: the executable
contains code translated from the game, so everyone builds it from their own copy.

## Quick start

**You need:**
- Windows 10 or 11 (64-bit) and a graphics card with Direct3D 12 (current drivers).
- An ISO of **your own** Afro Samurai disc, **Europe** version only
  (SHA-1 `29394DB1854DFC59C767D768C8D35E0A08ACB73D`). See [Getting your game files](#getting-your-game-files).
- About 20 GB of free disk space and an internet connection (about 4 GB is downloaded).
- A controller (Xbox or any controller Windows/SDL recognises).

**Steps:**
1. On this page click **Code > Download ZIP** and extract the ZIP into a folder
   (it creates an `AfroSamuraiRecomp-main` folder; `install.bat` is inside it).
   (You can also download just [`install.bat`](https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/install.bat).)
   The installer always builds the latest version of this repository from GitHub.
2. Double-click **`install.bat`**.
3. Answer the questions at the start: language, install folder, your ISO,
   the Microsoft Visual Studio license (needed for the compiler) and shortcuts.
   After that it runs on its own.
4. Wait. It downloads the tools and builds the game; it takes about 20-30 minutes
   (17-19 minutes in our tests), up to about 1 hour on slower PCs.
   It ends with `INSTALL OK`.
5. Play from the **desktop shortcut** (or `game\Play.bat` in the install folder).

At the end the installer offers to delete the build files (default No).
Saying Yes frees about 8 GB but a later update then needs a full new install.

Nothing is installed system-wide and no admin rights are needed. To uninstall,
delete the install folder and the shortcuts.

### Your saves
Saves are in `Documents\afrorecomp`. In the launcher (shown at start; hold
**Shift** while starting to show it again) use **Manage saved games...** to open,
back up, restore or delete them. Details: [Saves](#saves).

### If something goes wrong
- Run `install.bat` again: it continues where it stopped.
- The installer log is `install.log` in the install folder; the game log is `afro.log`.
- Open an issue at <https://github.com/LomoXSoft/AfroSamuraiRecomp/issues> with what
  happened and the log. **Never attach game files** (ISO, extracted files).
  More help: [Troubleshooting](#troubleshooting).

This is a preview tested on one PC; expect bugs. **Original (30) is the default.** Experimental (60) is incomplete and opt-in. See [Status and limitations](#status-and-limitations).

---

## Advanced

Everything below is for technical users: how it works, the installer options,
the manual build and the developer options.

### What it is
The game's PowerPC executable (`default.xex`) is translated ahead of time into
C++ by ReXGlue's code generator and compiled into a native Windows program. The
Xbox 360 kernel, GPU and audio are provided by the ReXGlue runtime (derived from
Xenia). This repository holds only the project configuration (function
boundaries, hooks), a small launcher, build scripts and a GPU plugin patch.

### Status and limitations
Preview (**0.0.16**). Base gameplay path tested on one PC (Windows, AMD Radeon RX 9060 XT)
at **Original (30)**. Experimental (60) is an **incomplete opt-in** for public testing
(see [Experimental 60 FPS](#experimental-60-fps-incomplete-opt-in)); do not treat it as stable.
- Boots, plays videos, menus and gameplay; EN/ES/FR/IT language selection.
- Default GPU path is **RTV** (host render targets): correct image at a stable
  30 FPS with **Original (30)** (launcher default). The black frames of earlier versions
  are fixed by the `afro-edram-no-wrap` GPU plugin patch.
- **ROV** is still available as an optional fallback (correct image but slow,
  can drop below 15 FPS): Advanced options (`--advanced`) or
  `--render_target_path_d3d12=rov`.
- Voices work, including all cinematic dialogue (0.0.13 `afro-xma-fix-log` and
  0.0.14 `afro-xma-frameless-chain` runtime patches).
- Known issues (also apply at Original unless noted):
  - FPS can dip (including below 30 / near dual-camera stretches ~30). Cause is
    **not proven** without profiling; do not assume a single GPU root cause.
  - If two tutorial prompts appear at once, the first one can stay stuck on screen
    (original game behaviour; restarting the level clears it).
  - **Experimental (60) only:** intro and cutscenes can run too fast; other
    gameplay / Focus / combat timing may be wrong and is largely unverified.
- Only the Europe disc (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) is supported. Other versions will not work.
- Windows x64 only for now (D3D12).
- Expect bugs; saves and settings may change between versions.

### Experimental 60 FPS (incomplete, opt-in)
**Default remains Original (30).** Experimental (60) is for testers who want to help;
it is **not** claimed smooth, complete, or timing-correct.

**Enable:** Launcher → **Advanced settings (experimental)...** → Frame rate →
**Experimental (60)** → Play. Tick **Remember** only if you want it sticky.
**Revert:** same dialog → **Original (30)** (or delete the `fps=` line / set `fps=0`
in the launcher config under `%USERPROFILE%\Documents\afrorecomp\`).

What it does (high level): unlocks the guest present interval (`afro_present_interval=1`),
applies a host 60 FPS cap, and enables gated guest dt / 1/30→1/60 constant hooks.
Original does **not** apply those Experimental dt/const patches; present interval
stays at the retail default (2).

**Tester guide (please report):**
1. Confirm **Original (30)** still feels like previous releases for you.
2. Switch to **Experimental (60)** for a short session (intro + one cutscene + a bit of gameplay).
3. Note mode, internal resolution, CPU/GPU, Windows version, cutscene/location, and whether
   you reverted to Original.
4. Copy `afro.log` immediately after the run (it is overwritten next launch).
5. Open a normal GitHub issue (see the issue template). Privacy: attach the log only after
   a quick check that it has no personal paths you must redact; **never** attach ISO,
   `gamedata`, saves, or other game files unless a maintainer specifically asks.

### Logs (where `afro.log` actually is)
- **Installer / `game\Play.bat`:** `<install-root>\afro.log` (same folder as `install.log`).
  `Play.bat` passes `--log_file="%~dp0..\afro.log"`.
- **Dev `tools\run_win.bat`:** repo-adjacent log: from `tools\`, `R` is `..\..`, so
  `--log_file="%R%\afro.log"` is the folder that contains `app\` and `tools\`
  (the checkout / `afro360recomp`-style root), **not** next to the `.exe`.
- Each launch **overwrites** the log; copy it right after a problem.

### Installer details
1. Make an ISO of your own Afro Samurai (Europe) disc (see [Getting your game files](#getting-your-game-files)).
2. Download [`install.bat`](https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/install.bat)
   (right-click, *Save link as*) into an empty folder, or clone/download this repository
   and use the `install.bat` at its root.
3. Double-click `install.bat`. It opens a folder picker (a new `AfroSamuraiRecomp` folder is
   created inside the folder you choose) and a file picker for your ISO,
   then does everything else. Started from a clone, it builds that checkout and
   proposes `AfroSamuraiRecomp-install` next to it as the install folder.
4. It first asks the language (English, Espanol, Francais, Italiano; your Windows
   language is preselected). The installer messages are shown in it, and the launcher
   and the game start in it too (you can still change it in the launcher).
   All other questions (install folder, ISO, Microsoft license, shortcuts) come next;
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
(`<root>\downloads` can be deleted afterwards). A full run takes about 20-30 minutes on
a fast connection with a modern CPU; expect up to about 1 hour on slower ones.
It shows the overall progress (e.g. `[ 42%] Step 5/14: LLVM/Clang ... (downloading ...: 312/896 MB)`)
in the console, the window title and the log. Re-running it skips finished steps, so after an error just fix the cause and run it again.
The log is `<root>\install.log`. To uninstall, delete the install folder (and
the shortcuts; saves are separate, see [Saves](#saves)).

Command-line options (`install.bat -Option value`): `-InstallDir <dir>` (full install folder, skips the folder picker), `-Iso <file>`,
`-AcceptMsvcLicense`, `-Shortcut yes|no`, `-CleanupAfterInstall` / `-KeepBuildFiles`, `-Language en|es|fr|it`, `-Ref <branch|tag>`. Advanced:
`-GameDataDir <dir>` uses an already extracted disc folder (containing `default.xex`)
instead of an ISO.

### Requirements
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

### Getting your game files
Only the Europe disc is supported (Redump 17884, ISO SHA-1
`29394DB1854DFC59C767D768C8D35E0A08ACB73D`). Make the ISO from your own disc,
for example with a DVD drive that can read Xbox 360 discs and a disc dumping
tool, or from a backup of your own disc that you made with your console.
Check the SHA-1 with `certutil -hashfile "Afro Samurai.iso" SHA1`.
This project does not help with obtaining the game in any other way.
Never share the ISO, the extracted files or anything generated from them.

### Manual build
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

### Playing

    tools\run_win.bat

Run it from the `afrorecomp` folder (or use the installer's shortcut). It starts
the game with `<root>\gamedata` and writes the log to `<root>\afro.log`.
Hold **Shift** while starting to show the launcher again.

#### Controls
An Xbox controller or any other controller SDL recognises (XInput, DualShock/DualSense,
etc.) works out of the box; the game was designed for a gamepad and prompts show
Xbox buttons. Keyboard and mouse are **not** enabled by default. There is
experimental keyboard emulation of a controller from ReXGlue, untested with this game:
start with `tools\run_win.bat --mnk_mode=true` (WASD = left stick, arrow keys = right stick,
Space = A, Backspace = B, L = X, P = Y, Q/E = triggers, 1/3 = bumpers, Enter = Start,
Tab = Back, Shift+arrows = D-pad).

#### Saves
Saves are stored in your Documents folder, in `Documents\afrorecomp\` (the save
itself is under `<profile id>\4E4D07D8\00000001\AFRO_0.SAV`; the shader cache is in
`Documents\afrorecomp\cache`). To back them up, copy the whole
`Documents\afrorecomp` folder while the game is closed; to restore, copy it back.
Deleting or rebuilding the install folder does not touch your saves.

The launcher (shown at start, or hold Shift) has a **Manage saved games...** button
under *Advanced settings*. It opens a window with:
- **Open saves folder**: opens `Documents\afrorecomp` in Explorer (created if missing).
- **Back up saves**: copies the saves (not the shader `cache`) to a new
  `Documents\afrorecomp-backups\saves-YYYYMMDD-HHMMSS\` folder. Never overwrites anything.
- **Restore selected backup**: pick a backup from the list (newest first) and confirm.
  The current saves are first backed up automatically (`saves-...-before-restore`), then
  replaced by the chosen backup. The shader cache is kept.
- **Open backups folder**: opens `Documents\afrorecomp-backups` in Explorer (created if missing).
- **Delete save data...**: after a strong confirmation (default No), the current saves are
  backed up automatically (`saves-...-before-delete`) and then deleted. The shader cache
  and all backups are kept.

Restore and delete are refused while the game is running. Backups are plain folders:
you can also restore one by hand (close the game, copy the contents of a `saves-...`
folder back into `Documents\afrorecomp\`).

### Launcher and options
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

### Troubleshooting
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

#### Getting a log
See [Logs](#logs-where-afrolog-actually-is). Copy the file right after the problem
(each launch overwrites it). Useful extra flags for `tools\run_win.bat` or `Play.bat`:
- `--afro_xma_log=true` – audio (XMA) decoding details, for sound or voice problems.
- `--afro_perf_log=true` – per-second performance counters, for FPS drops.
- `--afro_fps_log=false` – disable the bounded ~2s FPS/dt lines if the log grows too large.

#### Reporting an issue
Prefer the **Experimental 60 / bug report** issue template when relevant. Include:
reproduction steps, frame-rate mode (Original vs Experimental), internal resolution,
CPU/GPU (+ driver), Windows version, cutscene or location, and `afro.log` as an
**attachment** after a quick privacy review. Also useful: version or commit
(`git log -1 --oneline` in the source checkout the installer used, when present)
and `install.log` / build output for install problems.
**Never attach game files**: no ISO, `default.xex`, `default.pe`, `gamedata` contents,
`app\assets`, `app\generated`, saves, or anything else extracted from the disc
unless a maintainer specifically asks.

### Repository layout
- `app/` – ReXGlue project: manifest, `afro_functions.toml` (extra function
  boundaries), `afro_hooks.toml` (mid-asm hook), launcher and hooks in `src/`.
- `install.bat`, `tools/install.ps1` – one-click installer.
- `tools/` – build/run scripts and helpers (`xiso_extract.py`, `xex_to_pe.py`,
  `gen_switch_tables.py`, `extract_icon.py`, analysis helpers).
- `patches/rexglue/` – ReXGlue GPU plugin and runtime patches (BSD-3, see its README).

### Plans
- Performance work; controller and audio polish.
- Possibly other game versions and Linux/Vulkan.

### License and legal notice
The code in this repository is licensed under the **GNU GPL v3.0** (see
`LICENSE`), except `patches/rexglue/`, which modifies ReXGlue/Xenia code and is
under their BSD 3-Clause license.

This is a fan project, not affiliated with or endorsed by Namco Bandai, GONZO,
Microsoft or any rights holder. "Afro Samurai" and related names are trademarks
of their respective owners. No game code, assets or data are distributed here;
you must own the original game.
