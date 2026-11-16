# AfroSamuraiRecomp v0.3.5 – Ultrawide

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.5.exe`

SHA-256 `1b60ac1cce45743fddaf3ba1d940bbd45a742dbea8fb5bc98d64760bbc91042b`

1990011 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (about 2.5 minutes from 0.3.4 on the test PC).

## New

### Aspect ratio: ultrawide screens
- **Settings → Graphics → Aspect ratio**: **16:9 (original)**, the default,
  **21:9 (ultrawide)**, **32:9 (super ultrawide)** or **Auto**, which takes the
  shape of the screen the game opens on (16:9, 16:10 and 4:3 screens keep 16:9;
  up to 32:9).
- Wider than 16:9, the game shows more of the scene at the sides instead of
  black bars, and nothing is stretched (Hor+). Gameplay, in-engine cutscenes and
  loading screens are wide. Menus, the HUD, subtitles and the loading screen's
  text stay 16:9 in the middle.
- The start-up logos and the full-screen movies were made for 16:9: they show
  at 16:9 with black bars at the sides, and so do the menus over the title
  screen's movie.
- The wider picture is rendered with more columns than rows: 21:9 at 1x internal
  resolution renders 2560x720, 32:9 at 2x renders 5120x1440. It costs GPU time
  accordingly.
- In the launcher, in the in-game options (F1, applies after a restart) and in
  RecompLauncher, saved as `aspect` in `afro_language.cfg`.

## Tested

- Update of a 0.3.4 Europe install (built from the `v0.3.4` tag) to 0.3.5: about
  2.5 minutes, including about a minute of code generation for the new hooks.
  The launcher settings and `Play.bat` stayed. Started with no arguments and
  **Aspect ratio** 21:9: the game used its graphics plugin by itself and showed
  the start-up logos at 16:9 as intended.
- Europe, fresh install of this release from the ISO (Italian): the ISO matched
  Redump 17884, 15 minutes. Started with no arguments at 21:9: logos and intro.
- USA, fresh install of this release from the extracted USA files (Spanish): 14
  minutes. Started with no arguments at 21:9 in a window: the intro movie showed
  at 16:9 with bars at the sides.
- Before the release, on development builds of the same code, Europe disc at 21:9
  and 32:9 in a window of that shape: logos, intro movie, title screen, main menu,
  Level Select, loading screens, the opening cutscene of The Daimyo's Story with
  subtitles, an in-level dialogue scene, the first fight, the pause menu,
  quitting back to the title screen, the in-game options and the launcher in all
  four languages. USA disc: start-up, title screen and one level at 21:9. With
  the default 16:9 the picture is the same as in 0.3.4.
- 32:9 on one AMD GPU (Radeon RX 9060 XT): a steady 60 FPS at 1x internal
  resolution (2560x720), 53 to 60 FPS at 2x (5120x1440) while walking.
- The checks in `checks/` pass.

## Known limits

- In cutscenes the wider view can show where a set ends (small dark areas at
  the edges of some shots).
- The loading screen's white background does not reach the sides at 32:9.
- The dark backdrop of the pause menu's quit question covers only the 16:9
  middle.
- 21:9 is 64:27 (2560x1080); on 3440x1440 it leaves thin bars. **Auto** uses the
  screen's exact shape.

## Still not claimed

- Not checked on a real ultrawide monitor: the wide pictures were checked in a
  window of that shape on a 16:9 screen.
- Checked on one AMD GPU (Radeon RX 9060 XT) only.
- RecompLauncher's own settings page was not opened with this release; its
  manifest has the new setting with the same keys as the launcher.
- The USA disc was not played through.
- No install time is guaranteed on HDD, SSD, or NVMe.
