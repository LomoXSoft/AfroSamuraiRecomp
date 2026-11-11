# AfroSamuraiRecomp v0.3.0 – USA compatibility

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.0.exe`

SHA-256 `2183eb407e58c9f4b521ca62fe80eb2d83638df84655cbffcd0e66701377ea13`

1890743 bytes.

Already on 0.1.x or 0.2.x: run it, choose **Update** and pick your install folder.
Your extracted game, saves and launcher settings stay. **Update** rebuilds only
what changed; this update runs the game's code generation once more.

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

## Tested

- Fresh installs into empty folders with this source: one from the USA files, one
  from the Europe ISO. Both built and started.
- USA, played with the keyboard on the author's PC: title and menus, a Europe save
  loaded, the loading screen with "Press any button to continue", the in-game
  options menu, 60 FPS, Game Journalist in a fight, and the game's A/B prompts
  replaced by the keyboard icons.

## Still not claimed

- The USA disc was checked in short sessions, not played through. Controllers and
  the X, Y and trigger prompts were not checked on it.
- At 60, scenes that show two camera views at once can still drop below 60.
- No install time is guaranteed on HDD, SSD, or NVMe.
