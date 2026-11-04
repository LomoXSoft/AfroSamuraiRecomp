# afrorecomp – Afro Samurai (Xbox 360) static recompilation

Unofficial, work-in-progress static recompilation of **Afro Samurai** (Xbox 360,
Europe, title ID 4E4D07D8) to native Windows x64 using
[ReXGlue](https://github.com/rexglue/rexglue-sdk).

**This repository contains no game files.** You need your own original disc
(or an ISO you dumped from it). No binaries are released: the executable
contains code translated from the game, so everyone builds it from their own copy.

## Contents
- [What it is](#what-it-is)
- [Status and limitations](#status-and-limitations)
- [Requirements](#requirements)
- [Getting your game files](#getting-your-game-files)
- [Building](#building)
- [Playing](#playing)
- [Launcher and options](#launcher-and-options)
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
Preview (0.0.13). Tested on one PC (Windows, AMD Radeon RX 9060 XT).
- Boots, plays videos, menus and gameplay; EN/ES/FR/IT language selection.
- Default GPU path is **RTV** (host render targets): correct image at a stable
  30 FPS. The black frames of earlier versions are fixed by the
  `afro-edram-no-wrap` GPU plugin patch.
- **ROV** is still available as an optional fallback (correct image but slow,
  can drop below 15 FPS): Advanced options (`--advanced`) or
  `--render_target_path_d3d12=rov`.
- Voices work, including cinematic dialogue (fixed in 0.0.13 by the
  `afro-xma-fix-log` runtime patch).
- Known issue: some tutorial pop-ups advance too fast (before the voice line ends).
- Only the Europe disc (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) is supported. Other versions will not work.
- Windows x64 only for now (D3D12).
- Expect bugs; saves and settings may change between versions.

## Requirements
- Windows 10/11 x64, a D3D12 GPU.
- Original Afro Samurai disc (Europe) and a way to make an ISO of it.
- ~10 GB free disk space.
- Toolchain (on PATH, or portable copies under `<root>\tools`, see below):
  - Visual Studio 2022 Build Tools (MSVC + Windows SDK) – run the build from a
    "x64 Native Tools" prompt, or put a portable MSVC in `<root>\tools\msvc` with `setup_x64.bat`.
  - LLVM/Clang 20 (`clang++`), CMake ≥ 3.25, Ninja, Git.
  - Python 3 with pycryptodome: `python -m pip install pycryptodome`
    (or inside a virtual environment: `python -m venv <root>\venv` and run the
    build from a prompt where `<root>\venv\Scripts\activate` was called).
- ReXGlue SDK nightly **0.10.0.24-dev.gbd833a2** (prebuilt) and the ReXGlue
  source at the same commit.

## Getting your game files
1. Make an ISO of your own disc (e.g. with a compatible drive and dumping tool).
2. Create a root folder, e.g. `C:\afro`, and extract the whole disc into
   `C:\afro\gamedata`:

       python tools\xiso_extract.py "Afro Samurai.iso" C:\afro\gamedata /

   `gamedata\default.xex` must exist afterwards.

Never share these files or anything generated from them.

## Building
Folder layout (`<root>` = e.g. `C:\afro`):

    <root>\afrorecomp      this repository (cloned into a folder named afrorecomp)
    <root>\sdk\win-amd64   ReXGlue SDK nightly (bin\rexglue.exe inside)
    <root>\rexglue-sdk     ReXGlue source at bd833a2
    <root>\gamedata        your extracted disc
    <root>\tools           optional portable toolchain (msvc, llvm, git)

Steps:

    cd C:\afro
    git clone https://github.com/LomoXSoft/AfroSamuraiRecomp.git afrorecomp
    rem SDK: download rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip from
    rem https://github.com/rexglue/rexglue-sdk/releases/tag/nightly-20261002-bd833a2a
    rem and extract it so that C:\afro\sdk\win-amd64\bin\rexglue.exe exists.
    git clone --recurse-submodules https://github.com/rexglue/rexglue-sdk.git rexglue-sdk
    cd rexglue-sdk && git checkout bd833a2 && git submodule update --init --recursive && cd ..
    cd afrorecomp
    tools\build_win.bat
    tools\build_gpu_plugin.bat

`build_win.bat` copies your `default.xex`, extracts (privately, gitignored) the
PE image, jump-table overrides and icon from it, runs the ReXGlue code
generator (a few minutes, ~130 MB of generated C++) and compiles. The output is
`app\out\build\win-amd64-release\afrorecomp.exe`. `build_gpu_plugin.bat` builds
the patched GPU plugin and the patched runtime (`rexgpu-xenos.dll` and
`rexruntime.dll`, see `patches/rexglue/README.md`); recommended, and required for
correct image and voices.

To rebuild from scratch after updating, delete `app\generated\default` and
`app\afro_switch_tables.toml`.

## Playing

    tools\run_win.bat

Runs the game with `gamedata` from `<root>` and writes `<root>\afro.log`.
Hold **Shift** while starting to show the launcher again.

## Launcher and options
The launcher (shown on first run, after each update, or with Shift) offers:
language (English, Español, Français, Italiano), frame rate (original / 30 / 60),
fullscreen or window, internal resolution (1x/2x/3x) and "remember and don't ask again".
Settings are stored in `afro_language.cfg` next to the exe.

Developer options: start with `--advanced` (e.g. `tools\run_win.bat --advanced`)
to show the "Advanced options..." button (render path, resolve readback, game
effect toggles, GPU diagnostics). Any ReXGlue cvar can also be passed on the
command line, e.g. `--render_target_path_d3d12=rov`.

## Repository layout
- `app/` – ReXGlue project: manifest, `afro_functions.toml` (extra function
  boundaries), `afro_hooks.toml` (mid-asm hook), launcher and hooks in `src/`.
- `tools/` – build/run scripts and helpers (`xiso_extract.py`, `xex_to_pe.py`,
  `gen_switch_tables.py`, `extract_icon.py`, analysis helpers).
- `patches/rexglue/` – ReXGlue GPU plugin and runtime patches (BSD-3, see its README).

## Plans
- Fix the tutorial pop-ups that advance too fast.
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
