# AfroSamuraiRecomp – Afro Samurai (Xbox 360) static recompilation

Unofficial, work-in-progress static recompilation of **Afro Samurai** (Xbox 360,
Europe, title ID 4E4D07D8) to native Windows x64 using
[ReXGlue](https://github.com/rexglue/rexglue-sdk).

**This repository contains no game files.** You need your own original disc
(or an ISO you dumped from it). The game executable is not released: it
contains code translated from the game, so everyone builds it from their own copy.
The only program attached to a release is `afrosetup-v0.2.0.exe`, the setup installer.

## Quick start

### Prerequisites
- Windows 10 or 11 (64-bit) and a graphics card with Direct3D 12 (current drivers).
- **Your own** Afro Samurai disc, **Europe** version only: an ISO
  (SHA-1 `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) or a folder you already
  extracted that contains `default.xex`. See [Getting your game files](#getting-your-game-files).
- About 20 GB of free disk space and an internet connection (the build tools are downloaded).
- A controller (Xbox, PlayStation, Switch, Steam Deck or any controller Windows/SDL
  recognises), or keyboard and mouse.

### Install
1. Download **[`afrosetup-v0.2.0.exe`](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest/download/afrosetup-v0.2.0.exe)**
   from the [latest release](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest).
   That file is the setup program, not the game. It builds the game on your PC from your disc.
2. Run `afrosetup-v0.2.0.exe`. Choose a language, then **Install**.
3. Choose a folder, then your Europe ISO or the extracted game folder (`default.xex` must be inside it).
4. Accept the Microsoft Visual Studio Build Tools license (the box starts unchecked). Shortcuts are optional. You can keep the downloaded tools or delete them when setup finishes. Deleting them does not remove `game`, the extracted game, the logs, your ISO, or your saves.
5. The window says this takes several minutes. It does not promise a fixed duration.
HDD installation can take a long time; SSD/NVMe recommended.
6. On success the window says **Installation finished** and shows **Play**. **Play** starts `game\Play.bat` in that folder, and only when you click it. **Finish** and closing the window do not start the game. A shortcut, if you asked for one, also starts that `Play.bat`.

Nothing is installed system-wide and no admin rights are needed. To uninstall,
delete the install folder and the shortcuts. Saves are not in that folder.

### Update
Run `afrosetup-v0.2.0.exe` again, choose **Update**, and select the existing installation
folder (the one that contains `game\Play.bat`). Close the game first. If
`gamedata\default.xex` is already there, the ISO is not asked for again. Tools
still on disk are reused, and only missing tools are downloaded again. The
extracted game, your saves, and `game\afro_language.cfg` stay. The program files
are rebuilt and replaced only after that build succeeds, including when the
folder is already 0.1.x or 0.2.0.

### Your saves
Paste this into File Explorer:

`%USERPROFILE%\Documents\afrorecomp`

If Windows has redirected your Documents folder, the saves are in that Documents
folder instead. In the launcher (shown at start; hold **Shift** while starting
to show it again) use **Manage saved games...** to open, back up, restore or
delete them. Details: [Saves](#saves).

### If something goes wrong
- If the window says **Installation did not finish**, read the details (shown by default) and `install.log` in the install folder. Run `afrosetup-v0.2.0.exe` again. Use **Update** when that folder already contains `game\Play.bat`, and close the game first. **Back** on the failure page returns to the summary and does not start the build by itself. This is not a promise that an interrupted download always resumes.
- [`install.bat`](https://raw.githubusercontent.com/LomoXSoft/AfroSamuraiRecomp/main/install.bat) is a separate console fallback. See [Console installer](#console-installer). Without `-Ref` it builds `main`.
- The game log is `afro.log` in the install folder.
- Open an issue at <https://github.com/LomoXSoft/AfroSamuraiRecomp/issues> with what
  happened and the log. **Never attach game files** (ISO, extracted files).
  More help: [Troubleshooting](#troubleshooting).

This is a preview. The default is the original Xbox 360 experience: 720p (1x) at 30 FPS.
**60 FPS** is a full option in **Settings**: gameplay, cutscenes and dialogue scenes all run
at 60. See [60 FPS](#60-fps) and [Status and limitations](#status-and-limitations).

## Improvements over the Xbox 360 version
- **Native Windows x64** program, no emulator. Fullscreen or window.
- **Up to 4K**: internal resolution 1x (native), 2x or 3x.
- **Native multi-controller support**: Xbox, PlayStation (DualShock/DualSense, USB
  and Bluetooth), Switch, Steam Deck and anything SDL recognises.
- **Button prompts that follow your controller** (0.1.2): PlayStation, Switch,
  Steam and keyboard icons replace the Xbox ones, and change mid-game when you
  switch device. Xbox controllers keep the original icons.
- **Keyboard and mouse** (0.1.2): mouse camera, left/right click attacks, Q block,
  E kick, R focus, Esc menu, arrows for the D-pad. Mouse sensitivity, dead zone and
  smoothing are sliders in the launcher.
- **60 FPS** (0.2.0): the whole game, cutscenes included, at the right speed.
- **Launcher**: language, Play, Settings and saved games. Settings has Graphics
  (display, resolution, frame rate, VSync), Gameplay (difficulty), Controls
  (control mode, mouse, key bindings) and Experimental (effects, render path) tabs.
- **Fixes over earlier recompilation builds**: all cutscene voices, black frames,
  and the headband texture.

## Goals
- Borderless window and key rebinding (planned for 0.2.1).
- Launcher options in game, while playing.
- Fix the loading screens: they hold short lore cutscenes that are skipped
  entirely because the game now loads too fast.
- One-hit kill cheat.
- USA disc (`.iso`) support.
- Linux, macOS and Android versions.

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
Preview (**0.2.0**). The default is the original 30 FPS at 720p. **60 FPS** is a
supported option (see [60 FPS](#60-fps)): gameplay, cutscenes and dialogue scenes were
played at 60 on one PC, also at 2x resolution. That is not an independent playthrough
of the whole game.
- Boots, plays videos, menus and gameplay; EN/ES/FR/IT language selection.
- Default GPU path is **RTV (default)** (host render targets): correct image at a stable
  30 FPS with **30 (default)** (launcher default). The black frames of earlier versions
  are fixed by the `afro-edram-no-wrap` GPU plugin patch.
- **ROV** is still available as an optional fallback (correct image but slow,
  can drop below 15 FPS): **Settings → Experimental** or
  `--render_target_path_d3d12=rov`.
- Voices work, including all cinematic dialogue (0.0.13 `afro-xma-fix-log` and
  0.0.14 `afro-xma-frameless-chain` runtime patches).
- Known issues (also apply at Original unless noted):
  - At 60 FPS, scenes that show two camera views at once can still drop below 60.
  - Tested: the headband looked correct in play of 0.1.1. Other black textures are not claimed fixed.
  - Occasional odd audio.
  - If two tutorial prompts appear at once, the first one can stay stuck on screen
    (original game behaviour; restarting the level clears it).
  - **Invulnerability** (Settings → Gameplay → Difficulty → **Game Journalist**)
    works in the tested build. It is not a guarantee for the whole game. It is off
    by default and saved like the other settings.
  - Button prompts: A, B, X, Y, LB, LT, RT and the D-pad are replaced. RB, Start,
    Back and the stick clicks have not been found yet and keep the Xbox icon.
    Keyboard icons follow the keys you rebind (the first key of each binding).
- Only the Europe disc (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) is supported. Other versions will not work.
- Windows x64 only for now (D3D12).
- Expect bugs; saves and settings may change between versions.

### 60 FPS
**The default stays the original 30 FPS.** To play at 60: **Settings → Graphics →
Frame rate → 60**, then OK and Play. VSync switches to **Off** with it: with VSync on,
every frame that misses 60 waits for the next refresh and shows at 30. Set VSync back
to **On** if you see tearing and prefer it.

How it works:
- The game's present interval is unlocked and a host 60 FPS cap is applied.
- The game's per-frame time step (1/30 s in the original, less during Focus) is
  scaled by the real frame time. Game speed does not follow the frame rate, and Focus
  slow motion and pauses still work.
- Cutscenes and dialogue scenes get the same step, so they play at normal speed at 60.
  `--afro_cutscene_30=true` drops them back to 30 if a scene ever needs it.
- GPU side (ReXGlue patch `zzzzzz-afro-gpu-fast-path`): no per-write register lookup,
  short spins before sleeping on GPU waits, and register blocks written in one batch.
  Unchanged data is no longer re-uploaded every frame (`clear_memory_page_state=false`),
  and rendered images are no longer copied back to the CPU.

If a scene runs too fast or too slow at 60, `--afro_cine_diag=true` logs which game
parts were updated and the time step each one got (in `afro.log`, every 2 seconds).
Please attach that log to the issue.

### Logs (where `afro.log` actually is)
- **Installer / `game\Play.bat`:** `<install-root>\afro.log` (same folder as `install.log`).
  `Play.bat` passes `--log_file="%~dp0..\afro.log"`.
- **Dev `tools\run_win.bat`:** repo-adjacent log: from `tools\`, `R` is `..\..`, so
  `--log_file="%R%\afro.log"` is the folder that contains `app\` and `tools\`
  (the checkout / `afro360recomp`-style root), **not** next to the `.exe`.
- Each launch **overwrites** the log; copy it right after a problem.

### Console installer
`install.bat` is a fallback for a source checkout. The program players download
is [`afrosetup-v0.2.0.exe`](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest/download/afrosetup-v0.2.0.exe)
from the [latest release](https://github.com/LomoXSoft/AfroSamuraiRecomp/releases/latest).
That setup builds the source stored inside the file. It does not clone public `main`.

When it asks its questions interactively, `install.bat` with no `-Ref` builds the
`main` branch. With `-NonInteractive` it refuses to build `main`, both with no
`-Ref` and with `-Ref main`: pass an explicit `-Ref` that is not `main`, or
`-LocalTree` with a 0.2.0 source tree. To build the tagged 0.2.0 source from the
console installer, pass `-Ref v0.2.0`.

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
   your saves are not part of that delete. A later **Update** in `afrosetup-v0.2.0.exe`
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
skips steps already stamped in `<root>\.install`. **Update** in `afrosetup-v0.2.0.exe` clears
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
Switch, Steam Deck, etc.) works out of the box. The button prompts follow the device
you use last: Xbox icons for Xbox pads, and PlayStation, Switch, Steam or keyboard
icons for the others (`--afro_button_glyphs=auto|xbox|playstation|nintendo|steam|keyboard`
forces one).

The launcher's **Control** row picks **Auto** (default: keyboard and mouse are on,
and a controller takes over when it connects or is used), **Controller**, or
**Keyboard**. Keyboard and mouse:

| Action | Key | Pad button |
|---|---|---|
| Move | W A S D | left stick |
| Camera | mouse (or Shift + arrows) | right stick |
| Light attack | left click | X |
| Heavy attack | right click | Y |
| Jump | Space | A |
| Kick / menu back | E | B |
| Guard | Q | RT |
| Focus (hold) | R | LT |
| Overfocus (full Focus meter) / RB | 1 / 3 | LB / RB |
| D-pad (down: Ninja Ninja compass) | arrow keys | D-pad |
| Pause | Enter (or X) | Start |
| Skill menu | Esc | Back |
| Dash / center camera | F / K | L3 / R3 |

The action names are the ones in the original manual.

Every key can be changed in **Settings → Controls → Key bindings...**: click an
action, then press the new key or mouse button (hold Shift, Ctrl or Alt for a
combination). A key already in use moves to the action you changed, and the keyboard
button icons follow. The bindings are the runtime's `keybind_*` settings, saved as
`bind_*` lines in `afro_language.cfg`; a `--keybind_*` on the command line still wins.

The **Controller** list at the top of that window also remaps the pad buttons, one
layout per family: **Xbox, generic pads and Steam Deck**, **PlayStation (DualSense,
DualShock)** and **Nintendo Switch**. Pick the button for each action; a button in use
swaps with the action you changed, and the button icons in game follow. Buttons are
named as printed on that pad (on Switch the bottom button is B, which is the Xbox A
position). Saved as `padmap_xbox`, `padmap_playstation` and `padmap_nintendo` in
`afro_language.cfg` (the runtime's `afro_padmap_*` settings). This works with the
default SDL input; the `--input_backend=xinput` option is not remapped.
Mouse sensitivity, dead zone and smoothing are sliders on the same tab.

#### Saves
Saves are stored in `%USERPROFILE%\Documents\afrorecomp` (the save
itself is under `<profile id>\4E4D07D8\00000001\AFRO_0.SAV`; the shader cache is in
`Documents\afrorecomp\cache`). To back them up, copy the whole
`Documents\afrorecomp` folder while the game is closed; to restore, copy it back.
Deleting or rebuilding the install folder does not touch your saves.

The launcher (shown at start, or hold Shift) has a **Manage saved games...** button.
It opens a window with:
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
The launcher (shown on first run, after each update, or with Shift) has the
language (English, Español, Français, Italiano) and three buttons: **Play**,
**Settings...** and **Manage saved games...**. Settings has four tabs:
- **Graphics**: fullscreen or window, internal resolution (1x native by default /
  2x / 3x), frame rate (**30 (original, default)** or **60**) and VSync.
- **Gameplay**: **Difficulty**: **Normal** (default, the game as it is) or **Game
  Journalist**, which makes the player invulnerable. Saved like the other settings.
- **Controls**: Auto (keyboard and mouse, a controller takes over when used),
  Controller or Keyboard; mouse sensitivity, dead zone and smoothing; and
  **Key bindings...**.
- **Experimental** (not fully tested): sepia filter, blood effect, depth of field,
  bloom, focus effect, and the render path (**RTV (default)** or **ROV**).

At the bottom, **Skip this window at launch** starts the game straight away next
time (hold Shift to show the launcher again). Settings are saved in
`afro_language.cfg` next to the exe (`game\afro_language.cfg` after setup) each time
you press Play. Saves are not in that file; they are in `Documents\afrorecomp`.

Developer options: start with `--advanced` (e.g. `tools\run_win.bat --advanced`)
to show the "Advanced options..." button on the Experimental tab (resolve readback,
GPU diagnostics). Any ReXGlue cvar can also be passed on the
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
Prefer the **60 FPS / bug report** issue template when relevant. Include:
reproduction steps, frame rate (30 or 60), internal resolution,
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
- `install.bat`, `tools/install.ps1` – console installer. `tools/afrosetup.cpp` builds the graphical setup. The release file is `afrosetup-v0.2.0.exe`, not committed.
- `tools/` – build/run scripts and helpers (`xiso_extract.py`, `xex_to_pe.py`,
  `gen_switch_tables.py`, `extract_icon.py`, analysis helpers).
- `patches/rexglue/` – ReXGlue GPU plugin and runtime patches (BSD-3, see its README).

### Plans
See [Goals](#goals). Also: performance work and audio polish.

### License and legal notice
The code in this repository is licensed under the **GNU GPL v3.0** (see
`LICENSE`), except `patches/rexglue/`, which modifies ReXGlue/Xenia code and is
under their BSD 3-Clause license.

This is a fan project, not affiliated with or endorsed by Namco Bandai, GONZO,
Microsoft or any rights holder. "Afro Samurai" and related names are trademarks
of their respective owners. No game code, assets or data are distributed here;
you must own the original game.
