# AfroSamuraiRecomp v0.3.2 – Options hint

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.2.exe`

SHA-256 `48d9dd767d2801be1e4e9e9b5306b31061d3aa426088e2907c83cf4c67659013`

1920888 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (under a minute from 0.3.1 on the test PC).

## New

### How to open the options, at start-up
- A few seconds after the game starts, a small note at the bottom left says how
  to open the options while playing: **Hold [LS] + [RS] or [F1]  Options**, or
  **[F1]  Options** when Controls is set to Keyboard. It fades out after about
  10 seconds, or as soon as the options open. It helps when the game is started
  without its launcher window, as RecompLauncher does.
- The controller icons are Xbox ones until a controller is used, then that
  controller's own.
- `--afro_options_hint=false` on the command line turns the note off.

### Options while playing
- On a setting, the confirm button (A) steps to the next value, like right
  does, so its hint now reads **Change** next to the left and right icons.
  **Select** shows only on **Back to the game**.

## Tested

- Europe, fresh install of this release from the ISO (Spanish, Controls Auto):
  the note showed "Mantén [LS] + [RS] o [F1] Opciones" during the opening logos,
  and the options' hints were checked.
- USA, fresh install of this release from the extracted USA files (French,
  Controls Keyboard): the note showed "[F1] Options"; on **Retour au jeu** the
  hint read "Sélectionner".
- Update of a 0.3.1 USA install to 0.3.2 (English): built and started; the note
  and the options' **Change** hint showed, and the camera invert setting was
  still put over the save. A second update skipped code generation.
- On a test build with the same code, Controls Keyboard showed "[F1] Options"
  only; in Auto, the note faded out by itself.
- The checks in `checks/` pass; `test_pe_imports.py` skips when `rexglue.exe` is
  not extracted.

## Still not claimed

- The note with PlayStation, Switch or Steam icons was not checked; only the Xbox
  ones (no controller was used during the logos) and the keyboard key.
- RecompLauncher itself was not run with this release.
- The USA disc was not played through.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
