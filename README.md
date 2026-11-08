# AfroSamuraiRecomp – Afro Samurai (Xbox 360) static recompilation

Unofficial, work-in-progress static recompilation of **Afro Samurai** (Xbox 360,
Europe, title ID 4E4D07D8) to native Windows x64 using
[ReXGlue](https://github.com/rexglue/rexglue-sdk).

**This repository contains no game files.** You need your own original disc
(or an ISO you dumped from it). The game executable is not released: it
contains code translated from the game, so everyone builds it from their own copy.
The only program attached to a release is `afrosetup-v0.1.1.exe`, the setup installer.

## Quick start

### Prerequisites
- Windows 10 or 11 (64-bit) and a graphics card with Direct3D 12 (current drivers).
- **Your own** Afro Samurai disc, **Europe** version only: an ISO
  (SHA-1 `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) or a folder you already
  extracted that contains `default.xex`. See [Getting your game files](#getting-your-game-files).
- About 20 GB of free disk space and an internet connection (the build tools are downloaded).
- A controller (Xbox or any controller Windows/SDL recognises).

### Install
1. Download **[`afrosetup-v0.1.1.exe`](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest/download/afrosetup-v0.1.1.exe)**
   from the [latest release](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest).
   That file is the setup program, not the game. It builds the game on your PC from your disc.
2. Run `afrosetup-v0.1.1.exe`. Choose a language, then **Install**.
3. Choose a folder, then your Europe ISO or the extracted game folder (`default.xex` must be inside it).
4. Accept the Microsoft Visual Studio Build Tools license (the box starts unchecked). Shortcuts are optional. You can keep the downloaded tools or delete them when setup finishes. Deleting them does not remove `game`, the extracted game, the logs, your ISO, or your saves.
5. The window says this takes several minutes. It does not promise a fixed duration.
HDD installation can take a long time; SSD/NVMe recommended.
6. On success the window says **Installation finished** and shows **Play**. **Play** starts `game\Play.bat` in that folder, and only when you click it. **Finish** and closing the window do not start the game. A shortcut, if you asked for one, also starts that `Play.bat`.

Nothing is installed system-wide and no admin rights are needed. To uninstall,
delete the install folder and the shortcuts. Saves are not in that folder.

### Update
Run `afrosetup-v0.1.1.exe` again, choose **Update**, and select the existing installation
folder (the one that contains `game\Play.bat`). Close the game first. If
`gamedata\default.xex` is already there, the ISO is not asked for again. Tools
still on disk are reused, and only missing tools are downloaded again. The
extracted game, your saves, and `game\afro_language.cfg` stay. The program files
are rebuilt and replaced only after that build succeeds, including when the
folder is already 0.1.0 or 0.1.1.

### Your saves
Paste this into File Explorer:

`%USERPROFILE%\Documents\afrorecomp`

If Windows has redirected your Documents folder, the saves are in that Documents
folder instead. In the launcher (shown at start; hold **Shift** while starting
to show it again) use **Manage saved games...** to open, back up, restore or
delete them. Details: [Saves](#saves).

### If something goes wrong
- If the window says **Installation did not finish**, use **Show details** and read `install.log` in the install folder. Run `afrosetup-v0.1.1.exe` again. Use **Update** when that folder already contains `game\Play.bat`, and close the game first. **Back** on the failure page returns to the summary and does not start the build by itself. This is not a promise that an interrupted download always resumes.
- [`install.bat`](https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/install.bat) is a separate console fallback. See [Console installer](#console-installer). Without `-Ref` it builds `main`.
- The game log is `afro.log` in the install folder.
- Open an issue at <https://github.com/LomoXSoft/AfroSamuraiRecomp/issues> with what
  happened and the log. **Never attach game files** (ISO, extracted files).
  More help: [Troubleshooting](#troubleshooting).

This is a preview. **30 (original)** is the default. **60 (experimental)** is incomplete and opt-in. See [Status and limitations](#status-and-limitations).

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
Preview (**0.1.1**). **30 (original)** remains the default. On the author's Windows PC,
a clean install with the 0.1.0 setup started the game, and an update of that
folder started it from Play. That is the author's report, not an independent
playthrough of the whole game. **60 (experimental)** is an
**incomplete opt-in** for public testing (see [Experimental 60 FPS](#experimental-60-fps-incomplete-opt-in));
do not treat it as stable — cinematics can run fast and performance dips remain.
- Boots, plays videos, menus and gameplay; EN/ES/FR/IT language selection.
- Default GPU path is **RTV (original)** (host render targets): correct image at a stable
  30 FPS with **30 (original)** (launcher default). The black frames of earlier versions
  are fixed by the `afro-edram-no-wrap` GPU plugin patch.
- **ROV (experimental)** is still available as an optional fallback (correct image but slow,
  can drop below 15 FPS): **Advanced settings...** or
  `--render_target_path_d3d12=rov`.
- Voices work, including all cinematic dialogue (0.0.13 `afro-xma-fix-log` and
  0.0.14 `afro-xma-frameless-chain` runtime patches).
- Known issues (also apply at Original unless noted):
  - FPS can dip, including below 30 and especially above native internal resolution.
    Cause is **not proven** without profiling; do not assume a single GPU root cause.
  - Black textures, including the headband.
  - Occasional odd audio.
  - If two tutorial prompts appear at once, the first one can stay stuck on screen
    (original game behaviour; restarting the level clears it).
  - **Experimental (60) only:** intro and cutscenes can run too fast and frames
    can still drop. Other gameplay / Focus / combat timing may be wrong and is
    largely unverified. Not a finished 60 FPS mode.
  - **Invulnerability (experimental)** does not currently prevent death. It is
    off by default, lasts for that launch only, and is not saved.
- Only the Europe disc (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) is supported. Other versions will not work.
- Windows x64 only for now (D3D12).
- Expect bugs; saves and settings may change between versions.

### Experimental 60 FPS (incomplete, opt-in)
**Default remains Original (30).** Experimental (60) is for testers who want to help;
it is **not** claimed smooth, complete, or timing-correct.

**Enable:** Launcher → **Advanced settings...** → Frame rate →
**60 (experimental)** → OK, then Play. Tick **Remember** only if you want that
choice kept.
**Revert:** same dialog → **30 (original)**. The `fps=` line is in
`afro_language.cfg` next to the executable (`game\afro_language.cfg` after setup),
not in `Documents\afrorecomp`.

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

### Console installer
`install.bat` is a fallback for a source checkout. The program players download
is [`afrosetup-v0.1.1.exe`](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest/download/afrosetup-v0.1.1.exe)
from the [latest release](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest).
That setup builds the source stored inside the file. It does not clone public `main`.

`install.bat` with no `-Ref` builds the `main` branch. To build the tagged 0.1.1
source from the console installer, pass `-Ref v0.1.1`.

1. Make an ISO of your own Afro Samurai (Europe) disc, or use an extracted folder
   that contains `default.xex` (see [Getting your game files](#getting-your-game-files)).
2. Use `install.bat` from a checkout, or download
   [`install.bat`](https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/install.bat)
   (right-click, *Save link as*). A lone `install.bat` then downloads `tools/install.ps1` from `main`.
3. Double-click `install.bat`. It asks for a folder and your ISO (`-GameDataDir` takes
   an extracted folder instead). When it says `INSTALL OK`, play with the shortcut or
   `<root>\game\Play.bat`.
4. It asks the language first (English, Espanol, Francais, Italiano; your Windows
   language is preselected). The installer messages are shown in it, and a new install
   records that language in `game\afro_language.cfg`.
5. At the end it can delete the toolchain, ReXGlue SDK and source, build files, and
   downloads, and keep `<root>\game`, `<root>\gamedata`, and the logs. Your ISO and
   your saves are not part of that delete. A later **Update** in `afrosetup-v0.1.1.exe`
   downloads again only the tools that are missing and keeps the extracted game.

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
(`<root>\downloads` can be deleted afterwards). A run takes several minutes; there is
no fixed duration. The console shows its own progress lines. A normal console re-run
skips steps already stamped in `<root>\.install`. **Update** in `afrosetup-v0.1.1.exe` clears
the compile stamps and rebuilds the program. Neither path promises that an interrupted
download always resumes. The log is `<root>\install.log`. To uninstall, delete the
install folder and the shortcuts. Saves stay in `Documents\afrorecomp`
(see [Saves](#saves)).

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
Saves are stored in `%USERPROFILE%\Documents\afrorecomp` (the save
itself is under `<profile id>\4E4D07D8\00000001\AFRO_0.SAV`; the shader cache is in
`Documents\afrorecomp\cache`). To back them up, copy the whole
`Documents\afrorecomp` folder while the game is closed; to restore, copy it back.
Deleting or rebuilding the install folder does not touch your saves.

The launcher (shown at start, or hold Shift) has a **Manage saved games...** button
on that window, next to **Advanced settings...**. It opens a window with:
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
**Advanced settings...** opens frame rate (**30 (original)** or **60 (experimental)**),
render path (**RTV (original)** or **ROV (experimental)**), the sepia filter, and
**Cheats...**. The Cheats checkbox is **Invulnerability (experimental)**. It starts
off, lasts for that launch only, and is not written to `afro_language.cfg`. It does
not currently prevent death.
Settings you keep with Remember are stored in `afro_language.cfg` next to the exe
(`game\afro_language.cfg` after setup). Without Remember, the next start uses
**30 (original)** and 1x resolution. A render-path change is written only when
Remember is ticked. Saves are not in that file; they are in `Documents\afrorecomp`.

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
- `install.bat`, `tools/install.ps1` – console installer. `tools/afrosetup.cpp` builds the graphical setup. The release file is `afrosetup-v0.1.1.exe`, not committed.
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
