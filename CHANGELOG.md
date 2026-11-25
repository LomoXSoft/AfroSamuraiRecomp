# Changelog

Newest first. Versions are in `app/VERSION`.

## 0.3.11 – Achievements page
- In-game options (F1 / L3 + R3): a new **Achievements** row at the bottom shows
  "unlocked / total" and the gamerscore, and opens a page with the runtime's
  achievement list: the game's icons, names, descriptions and gamerscore,
  unlocked ones in full colour, summary and progress bar at the top. Same data
  as the runtime's F7 list, which is keyboard only, so controller and Steam Deck
  players can see it too. The page only reads; it never unlocks or saves.
- Options menu on the keyboard: Enter (also the game's Start in the default
  bindings) now selects instead of closing the menu, and Esc (also the game's
  Back) goes back from the Achievements page instead of closing the menu.
- The menu fonts also cover Latin Extended-A and the trademark sign.

## 0.3.10 – Setup without embedded files
- The setup exe no longer carries the project's source files. Antivirus
  engines read a small unsigned exe that holds a packed file tree and starts
  PowerShell on it as a "shell loader" (v0.3.9: Huorong, Rising and AliCloud
  `ShellLoader`/`Strab`, SecureAge), whether the tree was appended (up to
  0.3.7) or a resource (0.3.8, 0.3.9). The tree is now a second release asset,
  `afrosetup-v<version>-files.bin`. When **Install** is pressed the setup uses
  that file if it lies next to the setup, or downloads it from this version's
  GitHub release (WinHTTP), and accepts it only when its size and SHA-256 equal
  the ones compiled into the exe (`afrosetup_files.h`, written by
  `pack_setup_payload.py --header`). A wrong or missing file stops before
  anything is written to the install folder.
- `tools/build_release_setup.ps1`, `tools/build_setup.sh` and the Setup build
  workflow build, check, attest and upload both files.
  `checks/check_setup_packaging.py` fails if the exe has a data resource, an
  overlay or install.ps1 text, or was not compiled with the asset's SHA-256.

## 0.3.9 – Prologue soft-lock fix
- Setup: fewer traits antivirus engines look for. The v0.3.8 setup was flagged
  by 2 of 70 engines on VirusTotal (Huorong `Trojan/ShellLoader.gx`, AliCloud
  `Trojan:Win/Strab.Gen`). The setup no longer lists or opens other processes:
  Cancel stops its job object (install.ps1 and everything it starts), and
  "the game is running" means this install's `game\afrorecomp.exe` cannot be
  opened for writing. install.ps1 runs with `-ExecutionPolicy RemoteSigned`
  instead of `Bypass`; the scripts the setup writes are local files, so
  RemoteSigned runs them. `checks/check_setup_packaging.py` and
  `checks/check_update_contract.py` guard both.
- Prologue soft-lock fix (the original game has it too): in the courtyard fight
  before the boss, an enemy that should walk along the ledge and jump down can
  end up below the ledge instead, for example when Afro stands where it lands.
  It then follows a waypoint it can never reach (5.5 units above it; the game
  needs 1.5), so it neither comes down nor fights and the fight never ends.
  `src/spawn_guard.cpp` watches the enemies the spawn points create, only in
  `combatintro`: one that follows an unreached waypoint more than 1.5 units
  above or below it and does not move for `afro_spawn_guard_s` seconds (10)
  stops following its waypoints and fights. `afro_spawn_guard=false` turns it
  off; `afro_spawn_diag` logs the spawn managers and enemies once a
  second.
- Linux release assets, first attached to v0.3.8:
  `afrorecomp-linux-v<ver>.tar.gz` (the tag's source with LF line endings) and
  `AfroSamuraiRecomp-Linux-v<ver>.desktop`, a double-click launcher that
  downloads that tarball, checks its SHA-256 and runs `install.sh` (with
  `--update` for an existing `~/Games/AfroSamuraiRecomp`). Built by
  `tools/linux/make_release_assets.py`; `checks/test_linux_installer.py` checks
  the launcher.

## 0.3.8 – Setup payload as a resource
- The setup (`afrosetup-v0.3.8.exe`) carries its files as a normal Windows
  resource (RCDATA 101) instead of bytes appended after the program (an
  overlay), and reads them from memory instead of opening its own exe file.
  Both are traits antivirus engines look for in droppers. The released 0.3.7
  setup was flagged by 3 of 71 engines on VirusTotal (Bkav, SecureAge/APEX,
  Symantec) and held back on RecompLauncher. A test build of 0.3.7 with only
  this change was flagged by 1 of 71 (Huorong).
- `pack_setup_payload.py --out` writes `afrosetup_payload.bin`, which
  `afrosetup.rc` embeds, so the payload is packed before the resource step
  (`build_release_setup.ps1`, `build_setup.bat`, `build_setup.sh`).
  `check_setup_packaging.py` reads the payload from the resource table and
  fails if anything is appended after the program.
- The setup can be built on GitHub Actions: `.github/workflows/setup.yml`
  (started by hand) runs the new `tools/build_release_setup.ps1`. That script is
  the local release recipe: a `git archive` export of one commit, clang 20.1.8
  (the LLVM that `install.ps1` pins) and the same payload. A build of v0.3.7
  with it on the release PC matches the released `afrosetup-v0.3.7.exe` except
  for the link timestamps. On GitHub (windows-2022, MSVC 14.44 instead of the
  14.51 that `install.ps1` pins) the payload is the same and the program part is
  about 35 KB larger. The workflow keeps the setup as an artifact with a build provenance
  attestation, and attaches it only to a draft release. This is also the build
  that free code signing for open source (SignPath) needs.
- The game and `install.sh` are unchanged from 0.3.7.

## 0.3.7 – Linux installer (Proton)
- `install.sh`: builds the Windows game on Linux and SteamOS from your own
  disc (Europe or USA) to run through Proton on Steam Deck, Steam Machine and
  other Linux PCs. No root: llvm-mingw (clang 20.1.8), CMake, Ninja, xwin
  (MSVC CRT 14.44 and Windows SDK 10.0.26100), the ReXGlue nightly (its Linux
  `rexglue` does the codegen) and the ReXGlue source are downloaded pinned and
  SHA-256 checked. It applies the same patches as `build_gpu_plugin.bat`, sets
  up `game/` and prints the Steam shortcut steps. `--update` keeps the game
  files, region, saves and settings. The launcher window is skipped there
  (no controller support); the options are in the game (F1 or both stick
  clicks).
- Tested on a Steam Machine under Proton: installed with these steps, the game
  runs in 60 FPS mode at 1440p, with some drops to 40–50 FPS in places. Not
  tested on a Steam Deck yet.
- `tools/linux/`: the cross-compile toolchain file, `sync_tree.py` and
  `xwin_case_links.py` (symlinks for header and library names that the sources
  spell in another case).
- `docs/LINUX_FEASIBILITY.md`: what was checked in WSL. Linux codegen gives the
  same functions as Windows, and the three program files have the same DLL
  imports as the Windows build.
- The Windows setup and the game are unchanged from 0.3.6.

## 0.3.6 – Setup antivirus fix
- The setup (`afrosetup-v0.3.6.exe`) is flagged by fewer antivirus engines. The
  v0.3.5 setup was flagged by 19 of 74 engines on VirusTotal (0.3.4: 3 of 71) and
  was held back on RecompLauncher. It looked like a dropper: an unsigned program
  with no version information that started a hidden PowerShell suspended, put it
  in a job, then resumed it.
- The setup now has version information (company, product, description and
  version in Properties > Details), written by `make_setup_art.py` from
  `app/VERSION` into `afrosetup_version.rc2`.
- PowerShell is no longer started suspended: the job is created first and
  PowerShell joins it right after it starts. Cancel still stops the whole run.
- `checks/check_setup_packaging.py` checks both. The game itself is unchanged
  from 0.3.5.

## 0.3.5 – Ultrawide
- Aspect ratio setting for ultrawide screens: **16:9 (original)**, the default,
  **21:9 (ultrawide)**, **32:9 (super ultrawide)** or **Auto** (the shape of the
  screen the game opens on, from 16:9 up to 32:9). In the launcher (Settings ->
  Graphics), the in-game options (F1, after restart) and RecompLauncher, saved as
  `aspect` (0, 1, 2, 3) in `afro_language.cfg`. It applies when the game starts.
- Wider than 16:9 is Hor+: the game shows more of the scene at the sides. The game
  draws a 1280x720 picture with every camera at 16:9; the runtime now renders that
  picture with more columns than rows (`draw_resolution_scale_x` above `_y`, 2x1
  at 1x internal resolution), the presenter shows it at the wide aspect, and three
  hooks (`AfroAspect*Hook`, cvar `afro_aspect`) widen each 16:9 camera: perspective
  cameras for the 3D scene, orthographic ones for the menus and HUD, which stay
  16:9 in the middle. The game itself still sees a 1280x720 display.
- Start-up logos and full-screen movies (Bink, 1280x720, found by a hook on
  `BinkWait`) are drawn for 16:9: they show at 16:9 with bars at the sides. New
  runtime patch `zzzzzzzzzzzzz-afro-aspect-screens.patch` lets the executable
  switch the presenter's aspect per frame (`afro_display_aspect`).
- Loading screens and in-engine cutscenes stay wide. The loading screen's
  "Loading" / "Press any button to continue" text is drawn in screen pixels; a hook
  before it is drawn (`AfroAspectLoadingTextHook`) gives it its place and width in
  the 16:9 middle.
- `checks/test_aspect.cpp` checks the ratios, scales and video modes per choice.

## 0.3.4 – Black screen fix
- Black screen fix: started without `--gpu_plugin` (a shortcut to
  `afrorecomp.exe`, a double-click in the game folder), the game ran with no GPU
  emulation and the window stayed black. It now loads the `xenos` GPU plugin by
  itself and logs "no --gpu_plugin given, using xenos". `--gpu_plugin` on the
  command line still wins.

## 0.3.3 – FSR upscaling
- Upscaling setting: **Bilinear (original)**, the default and the look of
  earlier versions, **AMD FSR 1** or **AMD CAS**, with an **FSR sharpness** slider
  (0 to 100 %, default 90 %) and a **CAS sharpness** slider (0 to 100 %, default
  0 %). In the launcher (Settings -> Graphics), the in-game options (F1) and
  RecompLauncher, saved as `upscale` (0, 1, 2), `fsr_sharpness` and
  `cas_sharpness` in `afro_language.cfg`. FSR 1 upscales the game's picture (720p
  at 1x internal resolution) to a larger screen or window with sharper edges; at
  the same size it only sharpens, like CAS.
- Anti-aliasing setting in the same three places: **Off** (default), **FXAA** or
  **FXAA (extreme quality)**, saved as `fxaa` (0, 1, 2). It is the GPU plugin's
  `swap_post_effect`.
- A change in the in-game options applies at once (FXAA from the next frame,
  once the options are closed).
- New runtime patch `zzzzzzzzzzz-afro-fsr.patch`: ReXGlue's presenter already has
  FSR 1 and CAS as precompiled shaders, but they were only built with the
  FidelityFX SDK download. They are now built without it, and the presenter
  applies a change of `present_effect` or its sharpness while the game runs.
- New GPU plugin patch `zzzzzzzzzzzz-afro-fxaa-live.patch`: `swap_post_effect`
  was read once at start-up; a change now reaches the command processor at once.

## 0.3.2 – Options hint
- Start-up hint: a few seconds after the game starts, a small note at the
  bottom left shows how to open the options while playing ("Hold [LS] + [RS]
  or [F1]  Options", with the controller's icons once one is used; "[F1]  Options"
  when Controls is set to Keyboard), then fades out after about 10 s, or as soon
  as the menu opens.
  Useful when the game is started without the launcher (RecompLauncher).
  `--afro_options_hint=false` turns it off.
- Options while playing: the confirm button (A) steps to the next value on a
  setting, like right does, so its hint now reads **Change** there with the
  left and right icons; **Select** shows only on "Back to the game".

## 0.3.1 – Camera invert
- Camera invert for the X and Y axes in the launcher (Settings -> Controls),
  the in-game options (F1) and RecompLauncher. It is the game's own
  Options -> Camera setting: the camera reads two bytes every frame (X at
  0x8285CCBF, Y at 0x828E55EB) and the save keeps them in profile byte 13.
  `frame_pacing.cpp` puts the setting over the save when the game reads it
  (hook in `sub_8269B2E8`) and after the start-up profile reset
  (`sub_8269B9A8`), so the game's next save keeps it, and takes a change made
  in the game's Camera menu back into `afro_language.cfg` (`invert_x`,
  `invert_y`). Without a value there, the save's setting is kept and written
  to the cfg. The USA disc's menu and save have no X option, but its camera
  reads the X byte too: X invert works there from the recomp's settings.

## 0.3.0 – USA compatibility
- USA disc support. The build reads the SHA-1 of `default.xex` and picks Europe or
  USA (`tools/region_config.py`, writes `app/region/`). `app/regions/usa.toml` maps
  each Europe address the project uses to the USA build (an older build, December
  2008, with the same code apart from about 80 functions); hooks, function list and
  `frame_pacing.cpp` (`AfroGameAddr`) use it. The installer accepts the USA files;
  on the USA disc Spanish and Italian run the game in English.
- Update keeps `app/region/` (found while testing 0.3.0, never released): removing
  it gave the region files new timestamps and ran codegen again on every update.
  `checks/test_source_sync.ps1` reads the keep list from `install.ps1`.

## 0.2.2 – Options while playing
- Options while playing: **F1** (or hold L3 + R3) opens the main settings over the
  paused game, styled like the game's own menus (red ink strokes, the selected
  row in a red stroke with arrows). Its hints use the same button icons as the
  game's prompts (`glyphs\<device>\*.png`: Xbox, PlayStation, Switch, Steam, or
  the bound key caps on the keyboard). Changes are
  saved to `afro_language.cfg` and most apply at once; internal resolution,
  language and control mode after a restart. New `app/src/afro_menu.cpp`; the
  runtime adds `afro_pad_buttons` and `afro_ui_input_block` (C exports in the
  `zzzzzzzzzz-afro-interlude` patch) for the menu's input.
- README goals: the "Afro!" voice item is dropped; the speakers play the decoded
  sound sample for sample and the cut could not be reproduced.

## 0.2.1 – Controls and loading screens
- Setup: fresh installs work again. The setup now pins MSVC 14.51 and Windows SDK
  28000: Microsoft's newest MSVC (14.52) refuses the setup's Clang 20, so new
  installs stopped at the game build. An install that got 14.52 is repaired on the
  next run.
- Setup **Update** rebuilds only what changed (about 10 s instead of minutes when
  little changed).
- Key bindings: Settings has a **Controls** tab (control mode, mouse sliders, **Key
  bindings...**). Every keyboard/mouse action can be rebound; a key in use moves to
  the action you changed. Saved as `bind_*` lines in `afro_language.cfg`.
- Gameplay tab: the invulnerability checkbox is now a **Difficulty** drop-down,
  **Normal** (default) or **Game Journalist** (invulnerable). Saved like the other
  settings (the checkbox was for that launch only).
- Keyboard button icons follow rebound keys: `key_<Name>.png` caps for every key
  (`tools/make_glyphs.ps1`), picked by the new `zzzzzzzz-afro-keyboard-glyph-binds`
  patch when a binding differs from the default.
- Controller remap: the Key bindings window has a **Controller** list (Keyboard and
  mouse; Xbox, generic pads and Steam Deck; PlayStation; Nintendo Switch). Each pad
  family has its own layout, one button list per action, with the names from the
  original manual. Button icons in game follow the remap (new `xbox` icon set, used
  only for remapped Xbox buttons, and D-pad up/left/right icons). New
  `zzzzzzzzz-afro-pad-remap` patch; saved as `padmap_*` lines.
- Display has three choices: **Borderless fullscreen** (default, what "Fullscreen"
  already was: a window over the whole screen, instant Alt+Tab), **Window** and
  **Exclusive fullscreen** (the runtime's `fullscreen_exclusive`, saved as
  `exclusive=1`).
- Switching between keyboard and controller no longer leaves the mouse cursor on
  screen: the mouse capture is taken again whenever the button icons switch device
  (`zzzzzzz-afro-mouse-recapture` patch, like the startup/focus/resize recapture).
- Loading screens: the lore interlude shown while a level loads now plays one full
  pass (a looping one until its first line comes round again, a linear one such
  as Okiku's until 9 s after its last line) instead of being cut after about a second (PC loads much faster than the
  360). Once the level is loaded the "Loading" text reads **Press any button to
  continue** (in the game language) and a button skips it. After it the screen
  stays faded until the "Afro!" eyes, instead of showing the lore again for a
  moment; Settings -> Gameplay ->
  **Loading screens** -> **Fast** restores the old behaviour. Runtime side in the
  new `zzzzzzzzzz-afro-interlude` patch (voice lines in play order, button presses).
- RecompLauncher (recomplauncher.com): the setup installs `recomplauncher.json`
  next to `afrorecomp.exe`; the installed `game` folder can be added there as a
  build, which starts the game and shows its main settings. Installing from
  RecompLauncher runs the setup. `afrorecomp.exe` started without
  `--game_data_root`/`--log_file` now uses `..\gamedata` and `..\afro.log` of a
  setup install, like `Play.bat`. Started from it
  (`RECOMPLAUNCHER` set), the game's own launcher window stays closed unless Shift
  is held.
- Keyboard action names follow the manual (Light/Heavy attack, Guard, Overfocus,
  Dash, Center camera, Pause, Skill menu).
- Docs: README controller and button-prompt text matches what was tested (Xbox,
  DualShock 4/DualSense, Switch and generic controllers; Steam icons not checked
  in game; RB, Start, Back and stick clicks still Xbox). Impersonal wording in the
  0.2.0 notes. `docs/KNOWN_ISSUES_0.2.0.md` added; the 0.1.1 list is marked as
  superseded.

## 0.2.0 – 60 FPS
- 60 FPS is a supported option, no longer experimental. The default stays the
  original 30 FPS at 720p.
- Game step scaled by the real frame time (1/60..1/30 s, average of 4 frames), so
  game speed no longer follows the frame rate; Focus slow motion and pauses work.
- Cutscenes and dialogue scenes play at 60 at normal speed (`afro_cutscene_30` now
  off by default). The engine passes its game step to the cutscene players, and the
  step hook now changes that copy too.
- `afro_cine_diag`: logs the engine's update functions, their time channel and step.
- GPU (`zzzzzz-afro-gpu-fast-path` patch): no register lookup per write, short spin
  before GPU-wait sleeps, type-0 register blocks written in one batch.
- No re-upload of unchanged data every frame (`clear_memory_page_state=false`) and
  no copy of rendered images back to the CPU (`readback_resolve` left at none).
- VSync option; off by default with 60. With it off, 30 FPS scenes are held at 30
  by the frame limiter.
- Launcher: language, Play, Settings and Manage saved games. Settings has Graphics,
  Gameplay and Experimental tabs; "Skip this window at launch" moved into Settings. Settings are
  saved on every Play. Raw GPU switches only with `--advanced`.
- 0.1.x configs: a saved 60 stays 60 (with VSync off); old effect switches reset.
- Setup shows its details (the install log) by default.
- Keyboard and mouse in fullscreen: the cursor no longer stays on screen until an
  alt-tab (`zzzzzzz-afro-mouse-recapture` patch).

## 0.1.2 – Controller button prompts + keyboard and mouse
- Setup program `afrosetup-v0.1.2.exe` (release asset, not committed). It carries this 0.1.2 source.
- Button prompts follow the controller in use: PlayStation, Switch, Steam and
  keyboard icons; Xbox keeps the original ones. Live switch mid-game. Replaced:
  A, B, X, Y, LB, LT, RT, D-pad. New patch `zzzz-afro-button-glyphs`, icons in
  `tools/artwork/glyphs` (copied to `game\glyphs` by setup).
- Keyboard-as-controller crash fixed (`zzzzz-afro-input-lock`).
- Keyboard and mouse defaults for an action game, mouse camera by speed with
  dead-zone skip; `afro_mouse_deadzone`, `afro_mouse_window_ms`, `mnk_sensitivity`.
- Launcher: Control row (Auto/Controller/Keyboard); Advanced settings has render
  path, mouse sliders and Invulnerability; Experimental... has the warning, frame
  rate and sepia. Labels say (default).
- The `v0.1.2` tag points to the release source. Three later documentation-only
  commits (release notes moved to `docs/releases`, tested results) are on `main`
  only; the tag was not moved.

## 0.1.1
- Setup program `afrosetup-v0.1.1.exe` (release asset, not committed). It carries this 0.1.1 source and does not clone public main.
- ISO SHA-1 pin unchanged. A mismatch is logged and installation continues. A missing or unreadable ISO still stops the install.
- Quad-list depth offset defaults to half. That choice is no longer in Advanced settings.
- Git `safe.directory` is limited to the repositories this installer owns. It is not global and it is never `*`.
- LLVM extraction no longer presents a finished bar before tar starts.
- GPU plugin build fixes the private pipeline helper and the cvar declarations so the patched plugin links.
- If `install.log` cannot be written, installation continues, the lines go to a fallback file, and a real install error is still reported.
- Finished tools, verified downloads, extracted game data, and a completed SDK are kept on resume. An incomplete step is not marked done.
- HDD installation can take a long time; SSD/NVMe recommended. No duration is promised.
- Invulnerability works in the tested build and remains experimental. It stays off by default and is not a guarantee for the whole game.

## 0.1.0 – Fully playable
- Graphical setup program (`afrosetup-v0.1.0.exe` on the release, not in git). It
  installs or updates, in English, Español, Français, and Italiano, without
  administrator rights. `install.bat` remains the console installer.
- Update keeps the extracted game, saves, and launcher settings, and rebuilds
  even when the folder is already 0.1.0.
- Launcher labels: `30 (original)`, `60 (experimental)`, `RTV (original)`,
  `ROV (experimental)`. Cheats are a separate dialog.
- Invulnerability (experimental) is off by default and is not saved. It does
  not currently prevent death. (Fixed later: it works in the tested build from 0.1.1.)
- Known issues: FPS dips especially above native resolution, black textures
  including the headband, occasional odd audio, Experimental (60) can accelerate
  cutscenes and drop frames.
- One Windows test PC: a clean install with the first 0.1.0 setup started
  the game, and a same-version update started it from Jugar. Not an independent
  full playthrough. The Cheats warning and the shorter progress line were not
  run again on Windows after they were added.

## 0.0.16 – Experimental (60) FPS opt-in (incomplete)
- Launcher Advanced settings: frame rate is now **Original (30)** (default) or
  **Experimental (60)** only. The old host-only 30/60 cap choices and any
  unmapped tick-mode selector are gone.
- Experimental (60) sets `afro_present_interval=1` and a host cap of 60, and
  enables gated guest dt / 1/30→1/60 const hooks. **Original leaves the default
  present interval (2) and does not apply those Experimental dt/const patches.**
- Bounded FPS/dt/present-interval log lines (~2s) to help testers; no game-data
  dumps. `afro_fps_log` still defaults on.
- `tools/build_win.bat` regenerates ReXGlue codegen when `afro_hooks.toml`
  changes (needed for mid-asm hooks on update/rebuild).
- Installer / portable source builds pick this up when the GitHub revision
  updates (re-run `install.bat` / rebuild).
- **Not claimed fixed:** intro and cutscenes can still run too fast at 60;
  other gameplay / Focus / combat timing is unverified. Prefer Original (30)
  unless you are testing. See README Known issues and the tester guide.
- Pre-0.0.16 remembered FPS values are treated as Original (they were only a
  host cap on still-30 gameplay). Choose Experimental (60) explicitly after
  updating.

## 0.0.15 – one-click installer
- New `install.bat` (+ `tools/install.ps1`): language first (en/es/fr/it), all
  questions up front, then unattended with total % progress. Portable toolchain
  (no admin, PATH or registry changes), SHA-256-pinned downloads, ISO SHA-1
  check, fast tar extraction, light ReXGlue clone, resumable, refreshes the
  source and rebuilds on re-run, self-contained `game\` folder + shortcuts,
  optional cleanup of build files (~8 GB).
- Launcher: "Manage saved games..." (open saves folder, back up, restore,
  open backups folder, delete; safety backups before restore/delete).
- Launcher: closing the window (X/Esc/Alt+F4) no longer starts the game;
  layout gap fixed.
- README rewritten: simple Quick start for new players, technical content
  under Advanced.

## 0.0.14 – all cutscene voices fixed, experimental advanced settings
- Fixed: cutscene lines silent after a camera cut, and everything after them
  (e.g. the second boss cutscene). Cause: when a stream's next packet holds no
  frame start, the decoder jumped to packet 0 of the next input block (another
  stream). New runtime patch `patches/rexglue/afro-xma-frameless-chain.patch`
  (cvar `afro_xma_frameless_chain`, default true).
- Launcher: new "Advanced settings (experimental)" dialog with the FPS cap
  (default "Original (30 capped)" = no override) and a sepia filter toggle.
- Launcher: internal resolution defaults to native (1x); resolution, FPS and
  sepia are only kept with "remember".
- New diagnostics patch `patches/rexglue/zz-afro-perf-log.patch`: `afro_perf_log`
  and experimental `afro_occlusion_async` (both off).
- Build scripts: `build_gpu_plugin.bat` detects an already-applied patch stack
  (and keeps stamps); `build_win.bat` restores the patched DLLs.
- Known issues: FPS drops when objects break or the camera splits;
  if two tutorial prompts appear at once, the first one can stay
  stuck on screen (original game behaviour; restarting the level clears it).

## 0.0.13 – cinematic voices fixed
- Fixed: cinematic dialogue lines silent or cut off (e.g. at camera cuts).
  Cause: these lines are multi-stream (5.1) XMA with the voice in the centre
  stream; the XMA decoder restarted at packet 0 on every new 32 KB input block,
  so the centre stream was lost. New runtime patch
  `patches/rexglue/afro-xma-fix-log.patch` (cvar `afro_xma_substream`, default true).
- XMA decoder: backport of Xenia-canary's `Work()` loop / output-valid handling
  (cvar `afro_xma_fix`, default true); optional `afro_xma_log` diagnostics.
- `tools/build_gpu_plugin.bat` now also builds and installs `rexruntime.dll`
  (stock kept as `rexruntime.dll.stock`).
- Known issue: some tutorial pop-ups advance too fast.

## 0.0.12 – RTV black frames fixed, stable 30 FPS
- Fixed: black frames in combat and at cutscene camera cuts on the RTV path.
  Cause: EDRAM range wraparound in the render target cache. The end-of-frame
  depth-only pass (at tile 736, estimated 2048 tiles long) wrapped round to tile 0
  and took ownership of the colour image (tiles 0-720); the lossy
  colour->depth->colour transfer blacked out frames whenever the game skipped the
  pass that restores it. New GPU plugin patch `patches/rexglue/afro-edram-no-wrap.patch`
  (cvar `afro_edram_no_wrap`, default true) clamps the range instead of wrapping.
- Default GPU render path is RTV again: correct image at a stable 30 FPS.
  ROV remains selectable as a fallback (Advanced options / `--advanced`, or
  `--render_target_path_d3d12=rov`).
- `tools/build_gpu_plugin.bat` now applies every `patches/rexglue/*.patch` in
  name order, skipping patches that are already applied.
- Known issue: some gameplay voice lines are silent or cut off.

## 0.0.11 – first public preview (source only)
- Default GPU render path is ROV (`render_target_path_d3d12=rov`): no black frames,
  but slow on some GPUs. The faster RTV path still shows black frames in combat and
  at cutscene camera cuts (under investigation).
- Launcher (EN/ES/FR/IT): language, frame rate cap, fullscreen/window, internal
  resolution 1x/2x/3x, "remember" option. Advanced (diagnostic) options are hidden;
  start with `--advanced` to show them. Saved advanced options are ignored without it.
- Fixed: on/off advanced rows whose default is off had no "On" choice.
- Diagnostics off by default (`afro_fx_log`, `afro_int_fetch_log`, `afro_rt_log`).
- New `tools/xex_to_pe.py` and self-contained `build_win.bat` (extracts PE,
  switch tables and icon from your own XEX on first build).
- GPU plugin patch: xenia-canary d119505 part A + render-target diagnostic log.

## 0.0.1 – 0.0.10 (private development)
- ReXGlue codegen project, manual function boundaries and jump-table overrides,
  crash fixes (unregistered code pointers, truncated switch tables), language
  picker and launcher, frame pacing hook, game fullscreen-effect toggles,
  launcher cvars applied to the GPU plugin, d119505 backport, diagnostics.
