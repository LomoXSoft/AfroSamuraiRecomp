# AfroSamuraiRecomp v0.3.3 – FSR upscaling

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.3.exe`

SHA-256 `b908014418a09dfa06aedbb252f35c305f6e3dc76932b704b826f1db6f083d4e`

1955614 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (about a minute from 0.3.2 on the test PC).

## New

### Upscaling: AMD FSR 1 and CAS
- **Settings → Graphics → Upscaling** sets how the game's picture is stretched to
  your screen: **Bilinear (original)**, the default and the look of earlier
  versions; **AMD FSR 1**, which keeps edges sharp when the screen or window is
  larger than the game's picture (1280x720 at 1x internal resolution, 2560x1440
  at 2x); or **AMD CAS**, a sharpening filter.
- **FSR sharpness** (0 to 100 %, default 90 %) and **CAS sharpness** (0 to 100 %,
  default 0 %, CAS's lightest). When the screen is not larger than the game's
  picture, FSR only sharpens, with CAS, so CAS sharpness applies there.
- FSR 1 and CAS are AMD's open shaders and are not limited to AMD GPUs.

### Anti-aliasing: FXAA
- **Settings → Graphics → Anti-aliasing**: **Off** (default), **FXAA** or **FXAA
  (extreme quality)**. It smooths jagged edges of the game's picture before it is
  upscaled.

### In the launcher, while playing and in RecompLauncher
- The four settings are in the launcher, in the in-game options (F1, or hold
  L3 + R3) and in RecompLauncher, and they stay in step (`afro_language.cfg`:
  `upscale`, `fsr_sharpness`, `cas_sharpness`, `fxaa`).
- A change in the in-game options applies at once, also to the paused picture
  behind the options. FXAA changes from the next frame, so once the options are
  closed.
- `afro.log` shows the choice at start-up and after each change.

## Tested

- Europe, fresh install of this release from the ISO (Italian): the ISO matched
  Redump 17884. The game started as installed (Bilinear) and with AMD FSR 1, CAS
  sharpness 40 % and FXAA set in `afro_language.cfg`; `afro.log` showed both at
  start-up.
- USA, fresh install of this release from the extracted USA files (French): the
  same two starts.
- Update of a 0.3.2 Europe install to 0.3.3: about a minute, code generation
  skipped, launcher settings kept; the same two starts.
- On a test build with the same code (AMD Radeon RX 9060 XT, borderless
  fullscreen at 2560x1440):
  - the launcher's Graphics tab in English, Spanish, French and Italian;
  - each setting chosen in the launcher was used at start-up;
  - in a level loaded from a save, every setting changed from the in-game options
    at once and was saved, and the launcher then showed the new values;
  - FSR and CAS gave sharper edges than Bilinear on the intro logos and on the
    picture paused behind the options;
  - at 60 FPS, gameplay with FSR and FXAA (extreme quality) on held a steady
    60 FPS (slowest frame 19.3 ms).
- RecompLauncher, with this release's settings file on a test install: shows the
  four settings and writes them to `afro_language.cfg`.
- The checks in `checks/` pass; `test_pe_imports.py` skips when `rexglue.exe` is
  not extracted (it passed with the SDK files in place).

## Still not claimed

- Checked on one AMD GPU only; NVIDIA and Intel GPUs were not checked.
- FXAA changed little in the scenes checked (dark, low-contrast edges).
- RecompLauncher did not install this release; its settings page was checked
  with this release's settings file on a test install.
- The USA disc was not played through.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
