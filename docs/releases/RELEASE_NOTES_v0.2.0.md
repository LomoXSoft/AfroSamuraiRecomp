# AfroSamuraiRecomp v0.2.0 – 60 FPS

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe disc. No game executable and no game
files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.2.0.exe`

SHA-256 `a8fb0e72e859878d051c90b96522a83c201e1d77a9cca4d2a995f3ab0f1db4d3`

1362450 bytes.

Already on 0.1.x: run it, choose **Update** and pick your install folder.
Your extracted game, saves and launcher settings stay.

## Improvements

### 60 FPS, the whole game
- **60** is a normal frame-rate option now, not experimental. The default stays
  the original Xbox 360 experience: 720p at 30 FPS.
- Gameplay, cutscenes and in-level dialogue scenes all run at 60 at the right
  speed. Focus slow motion and pauses still work.
- Game speed follows real time, not the frame rate: a scene that drops below 60
  does not turn into slow motion.

### Faster
- The GPU translation does less work per frame: no register lookup per write,
  register blocks written in one batch, and no sleeping in whole milliseconds while
  waiting on the game.
- Unchanged data is no longer sent to the graphics card again every frame, and
  rendered images are no longer copied back to the CPU.
- Tested result: steady 60 at native resolution and at 2x on the test PC
  (i7-10700F, RX 9060 XT).

### Launcher
- The main window has the **language** and three buttons: **Play**,
  **Settings...** and **Manage saved games...**.
- **Settings** has three tabs:
  - **Graphics**: display, internal resolution, frame rate, VSync.
  - **Gameplay**: controls, mouse sliders, invulnerability.
  - **Experimental** (not fully tested): sepia filter, blood effect, depth of
    field, bloom, focus effect, render path.
- Choosing 60 also sets **VSync** to Off; with VSync on, a frame that misses 60
  shows at 30.
- **Skip this window at launch** is in Settings. Settings are saved each time you
  press Play. Hold **Shift** at launch to show the launcher again.
- If you saved 60 in 0.1.x, it stays 60 (with VSync off).

### Keyboard and mouse
- In fullscreen, the mouse cursor no longer stays on screen until you alt-tab.

### Setup
- Shows its details (the install log) by default.
- Applies the updated ReXGlue patch `zzzzzz-afro-gpu-fast-path` and the new
  `zzzzzzz-afro-mouse-recapture` on install and on update.

## Tested

- Played on the test PC at 60: gameplay, the intro cutscene and in-level
  dialogue scenes at normal speed, at 1x and 2x internal resolution.
- Update from a 0.1.2 install to 0.2.0, then a session at the 30 FPS default.
- New launcher windows checked in English and Spanish.
- Clean install into a new folder with a 0.2.0 release candidate, then **Update**
  of that folder with this `afrosetup-v0.2.0.exe`: language on the main window, the
  hidden cursor in fullscreen with keyboard and mouse, and 60 FPS all checked in game.
- Button prompts (A, B, X, Y, LB, LT, RT and the D-pad prompt) checked in game
  with Xbox, DualSense, Switch and generic controllers.
- The whole patch stack applies to a clean ReXGlue `bd833a2`.

## Still not claimed

- At 60, scenes that show two camera views at once can still drop below 60.
- RB, Start, Back and the stick-click prompts keep the Xbox icon.
- Steam icons were not checked in game.
- Keyboard icons show the default keys, not keys you rebind.
- No install time is guaranteed on HDD, SSD, or NVMe. **Update** rebuilds the whole
  game even when little changed, so it takes about as long as the build part of a
  fresh install.

## Next (0.2.1)

- Borderless window.
- Key rebinding.
