# AfroSamuraiRecomp v0.3.0 – USA compatibility

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.0.exe`

SHA-256 `32675e112275759cc899d80c5cd34425b872a4e0c4aaf28465d44f9325833da9`

1893036 bytes.

Already on 0.1.x or 0.2.x: run it, choose **Update** and pick your install folder.
Your extracted game, saves and launcher settings stay. **Update** rebuilds only
what changed. The first update to 0.3.0 also runs the game's code generation
again (about a minute on the test PC); later updates skip it when nothing changed.

## New

### USA disc
- The setup now accepts the **USA** disc as well as the Europe one: choose your
  ISO or an extracted folder that contains `default.xex`.
- The build checks which disc `default.xex` comes from and uses the matching
  addresses. The USA game is an older build (December 2008) with almost the same
  code. Every address the game's features use was found in it
  (`app/regions/usa.toml`); one address used only by a diagnostic log was not,
  and that log skips it on the USA disc. A `default.xex` from any other version
  stops the install with a clear message.
- The USA disc has English and French text only. With Spanish or Italian chosen,
  the launcher stays in that language and the game runs in English.
- Saves are shared between both versions: a Europe save loads in the USA game.
- No SHA-1 is recorded yet for a USA ISO: a USA ISO shows the "not verified"
  note, and its `default.xex` is checked after extraction.

## Tested

- USA, fresh install of this release from the extracted USA files (French, with
  the keyboard): title, menus, level select, subtitles and loading text in
  French; the Europe save's levels opened; a new game started and saved, and its
  opening played up to the first fight's tutorial; the loading screen held the
  lore and continued on a button; the in-game options menu; 60 FPS in gameplay
  and in cutscenes. The game's A, B, X, Y and RT prompts were recognised as the
  same images as on the Europe disc; A and B were seen replaced by the keyboard
  icons.
- USA, a pre-release build with the same game code (English, with the keyboard):
  a Europe save loaded at its last checkpoint, and Game Journalist kept the player
  alive in a fight at 60 FPS.
- USA with a controller, played by the author after the release: menus, gameplay
  and the in-game options menu with the controller's button icons.
- Europe, fresh install of this release from the ISO (Redump 17884 verified,
  Spanish; the install was resumed once after the test PC ran two installs at the
  same time): built with the same hooks as before, and the game played at 60 FPS.
- Update of a 0.2.2 install to 0.3.0: built and started; a second update skipped
  code generation and compiled one file.
- The checks in `checks/` pass; `test_pe_imports.py` skips when `rexglue.exe` is
  not extracted.

## Still not claimed

- The USA disc was not played through.
- A USA ISO was not tested; only the extracted USA files were.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
