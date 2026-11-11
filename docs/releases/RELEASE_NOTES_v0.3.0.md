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
what changed; this first update to 0.3.0 runs the game's code generation once
more (about a minute).

## New

### USA disc
- The setup now accepts the **USA** disc as well as the Europe one: choose your
  ISO or an extracted folder that contains `default.xex`.
- The build checks which disc `default.xex` comes from and uses the matching
  addresses. The USA game is an older build (December 2008) with almost the same
  code; every address this project uses was found in it
  (`app/regions/usa.toml`). Anything else stops the install with a clear message.
- The USA disc has English and French text only. With Spanish or Italian chosen,
  the launcher stays in that language and the game runs in English.
- Saves are shared between both versions: a Europe save loads in the USA game.
- No SHA-1 is recorded yet for a USA ISO, so a USA ISO shows the "not verified"
  note. Its game files are still checked after extraction.

### Fixes
- **Update** no longer removes the build's generated region files, so a later
  update does not run code generation again when nothing changed.

## Tested

- Fresh install from the USA files (French): title, menus, level select, subtitles
  and loading text in French; a Europe save loaded; a new game started and saved;
  the opening cutscene and the first fight; the loading screen held the lore and
  continued on a button; the in-game options menu; 60 FPS in gameplay and in
  cutscenes; Game Journalist kept the player alive in a fight; the game's A, B, X,
  Y and RT prompts were recognised (same images as the Europe disc) and replaced
  by the keyboard icons.
- Fresh install from the Europe ISO (Redump 17884 verified, Spanish): built, the
  region step picked Europe with hooks identical to 0.2.2, and the game played at
  60 FPS.
- Update of a 0.2.2 install to 0.3.0: built and started; a second update skipped
  code generation and compiled one file.
- All checks in `checks/` pass.

## Still not claimed

- The USA disc was checked in short sessions with the keyboard, not played
  through. Controllers and the LT prompt were not checked on it.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
