# AfroSamuraiRecomp v0.1.2 – Controller button prompts + keyboard and mouse

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe disc. No game executable and no game
files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.1.2.exe`

SHA-256 `bf5127c1a9b52324739db5cf791e2d5f7c7aa677f09c095106964cc16dd469a1`

1419348 bytes.

Already on 0.1.0 or 0.1.1: run it, choose **Update** and pick your install folder.
Your extracted game, saves and launcher settings stay.

## Improvements

### Button prompts that follow your controller
- The on-screen button icons change to match the device you are using:
  **PlayStation** (cross, circle, square, triangle, L1/L2...), **Switch**
  (B/A/Y/X in Nintendo positions, ZL/ZR...), **Steam Deck / Steam Controller**,
  and **keyboard and mouse** (key caps and a mouse).
- Xbox controllers keep the game's original icons.
- They switch live, mid-game, as soon as you press a button on another device.
- Replaced so far: A, B, X, Y, LB, LT, RT and the D-pad prompt.
- The icons are original art made for this project (`tools/make_glyphs.ps1`).
  `--afro_button_glyphs=playstation|nintendo|steam|keyboard|xbox` forces a set.

### Keyboard and mouse are playable
- Fixed the crash that closed the game 25 to 110 seconds after turning on
  the keyboard (heap corruption from two threads rebuilding the input device list).
- Action-game default keys: left click attack, right click second attack, Space
  jump, E kick (and menu back), Q block, R focus, Esc menu, arrows D-pad, mouse
  camera (Shift + arrows also move the camera).
- The mouse camera follows mouse speed and starts moving right away, whatever the
  game's polling rate. Sensitivity, dead zone and smoothing are sliders in the
  launcher.

### Launcher
- New **Control** row: **Auto** (default: keyboard and mouse on, a controller
  takes over when it connects or is used), **Controller**, **Keyboard**.
- **Advanced settings...** now holds the tested options: render path, the three
  mouse sliders and **Invulnerability** (a checkbox, still for that launch only).
- Its **Experimental...** button holds the options that are not fully tested,
  with the warning: frame rate and the sepia filter.
- Labels say **(default)** instead of (original).

### Setup
- Copies the button icons to `game\glyphs`.
- Applies the two new ReXGlue patches (`zzzz-afro-button-glyphs`,
  `zzzzz-afro-input-lock`) on install and on update.

## Tested

- Played on the author's PC: PlayStation (DualSense) and keyboard icons, switching
  between controller and keyboard mid-game; the LB, LT and D-pad icons; keyboard and
  mouse play with the mouse camera and the arrow-key D-pad.
- Switch icons were checked in the main menu with the icon set forced.
- Keyboard play ran about 6 minutes, 2.5 of them in gameplay, with no crash. Before
  the fix it crashed in 3 runs out of 3.
- Update from a 0.1.1 install to 0.1.2, run with the `install.ps1` command this
  setup uses and the source unpacked from this exact `afrosetup-v0.1.2.exe`:
  finished in 7 minutes, applied the two new patches, copied the icons, and the
  game started.
- Clean install of 0.1.2 into a new folder with the published
  `afrosetup-v0.1.2.exe`, through the graphical setup: finished in 14 minutes,
  applied all nine patches, copied the icons, and the game ran correctly.
- The whole patch stack applies to a clean ReXGlue `bd833a2`.

## Still not claimed

- RB, Start, Back and the stick-click prompts have not been found yet and keep the
  Xbox icon.
- Switch and Steam icons were checked by forcing that icon set. No real Switch or
  Steam Deck controller was tested.
- Keyboard icons show the default keys, not keys you rebind.
- Experimental (60) can still run cutscenes fast and drop frames.
- No install time is guaranteed on HDD, SSD, or NVMe.
