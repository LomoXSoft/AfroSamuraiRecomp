# AfroSamuraiRecomp v0.3.4 – Black screen fix

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.4.exe`

SHA-256 `263e10532d166095e5614b0e3b270f3d1c57065a2dcbdf0ef81e6bf7e1283cfc`

1960961 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (under a minute from 0.3.3 on the test PC).

## Fixed

### Black screen when the game is started directly
- Started without `--gpu_plugin=xenos` (a shortcut to `afrorecomp.exe`, or a
  double-click on it in the game folder), the game ran with no graphics and the
  window stayed black. `afro.log` then said `gpu_plugin not set`.
- The game now loads its graphics plugin by itself in that case and writes
  `afro: no --gpu_plugin given, using xenos` to `afro.log`. `Play.bat`, the
  setup's shortcut and RecompLauncher already passed it and work as before.

## Tested

- Update of a 0.3.3 USA install to 0.3.4: about 30 seconds, code generation
  skipped. Started with no arguments, the game logged
  `afro: no --gpu_plugin given, using xenos`, found the GPU and showed the
  intro instead of a black screen.
- Europe, fresh install of this release from the ISO (English): the ISO matched
  Redump 17884. Started with no arguments: the intro showed. Started with
  `Play.bat`: unchanged, the plugin came from its command line.
- USA, fresh install of this release from the extracted USA files (Spanish):
  started with no arguments, the intro showed.
- All starts on one AMD GPU (Radeon RX 9060 XT).
- The checks in `checks/` pass.

## Still not claimed

- The black screen was reported by a player on a USA install with an AMD Radeon
  RX 5500 XT; the fix was not checked on that PC.
- The upscaling and FXAA settings from 0.3.3 were checked on one AMD GPU only;
  NVIDIA and Intel GPUs were not checked.
- RecompLauncher did not install this release.
- The USA disc was not played through.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
