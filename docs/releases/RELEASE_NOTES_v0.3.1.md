# AfroSamuraiRecomp v0.3.1 – Camera invert

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.1.exe`

SHA-256 `4b5aae281cad7a52120caf1a1fadee0932594356e8ee8061935ed051afcc93ac`

1911695 bytes.

Already on 0.1.x, 0.2.x or 0.3.0: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed. The first update to 0.3.1 also runs the game's code generation
again (about three minutes for the whole update on the test PC); later updates
skip it when nothing changed.

## New

### Camera invert
- **Camera X axis** and **Camera Y axis** (Default or Invert) in the launcher
  (**Settings... → Controls**), in the in-game options (**F1**, or hold L3 + R3)
  and in RecompLauncher's settings.
- It is the game's own **Options → Camera** setting, kept in step both ways: the
  game's save stores it, and a change made in the game's Camera menu shows in the
  launcher and in F1. Until you change it in the recomp's settings, it follows
  your save, so an invert you already had stays.
- USA disc: the game's own menu and save have the Y axis only. X invert works
  there from the launcher, F1 or RecompLauncher; it is kept in
  `afro_language.cfg` instead of the save.

## Tested

- Europe, fresh install of this release from the ISO (Spanish): with no camera
  setting, the save's setting (X invert, Y default) was taken as the setting.
- USA, fresh install of this release from the extracted USA files: X invert and
  Y default from the settings were put over a save that had Y invert, and the
  game's own Y-Axis page showed Default.
- Update of a 0.3.0 install to 0.3.1: built and started; X default and Y invert
  from the settings were put over the save and shown in the game's Camera menu. A
  second update skipped code generation.
- On test builds with the same game code (Europe and USA): the F1 menu and the
  launcher change the setting (the launcher's texts also checked in Spanish); a
  change made in the game's
  Camera menu is taken into the settings; the game's next save keeps the setting;
  in gameplay, X invert turned the camera the other way on both discs.
- The checks in `checks/` pass; `test_pe_imports.py` skips when `rexglue.exe` is
  not extracted.

## Still not claimed

- Y invert was not checked in gameplay; it uses the same game setting as the
  game's own Camera menu.
- RecompLauncher itself was not run with this release; only its settings file
  was checked.
- The USA disc was not played through.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
