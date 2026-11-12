# AfroSamuraiRecomp v0.2.1 – Controls and loading screens

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe disc. No game executable and no game
files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.2.1.exe`

SHA-256 `ea941b8bbca7561b9e09111ece003d49a20ae3ee53fe26b2f39b327599a899ed`

1845049 bytes.

Already on 0.1.x or 0.2.0: run it, choose **Update** and pick your install folder.
Your extracted game, saves and launcher settings stay. **Update** now rebuilds only
what changed.

## Fixes

- **Fresh installs work again.** Microsoft's newest compiler libraries (MSVC 14.52)
  refuse the setup's Clang 20, so a new 0.2.0 install stopped at the game build.
  The setup now uses MSVC 14.51 and Windows SDK 28000. An install that already got
  14.52 is repaired the next time you run the setup.
- Switching between keyboard and controller no longer leaves the mouse cursor on
  screen.

## Improvements

### Controls
- New **Controls** tab in Settings: control mode, mouse sliders and **Key bindings...**.
- Every keyboard and mouse action can be rebound. A key already in use moves to the
  action you changed. The keyboard icons in game show your keys.
- The same window remaps controllers, one layout per family: **Xbox, generic pads
  and Steam Deck**, **PlayStation** and **Nintendo Switch**. Buttons are named as
  printed on each pad, and the icons in game follow the remap.
- Action names follow the original manual (Light/Heavy attack, Guard, Overfocus,
  Dash, Center camera, Pause, Skill menu).

### Gameplay
- **Difficulty**: **Normal** (default) or **Game Journalist**, where you can't die.
  It is saved like the other settings.
- **Loading screens**: the lore shown while a level loads plays in full again, as
  on the Xbox 360 (on PC the level loads in about a second and the story was cut).
  Once the level is loaded the screen reads **Press any button to continue**, and
  any button skips it; otherwise it goes on by itself when the story ends. **Fast**
  restores the old behaviour.

### Display
- **Borderless fullscreen** (default, as before), **Window** or **Exclusive
  fullscreen**.

### RecompLauncher
- Afro Samurai can be played from [RecompLauncher](https://recomplauncher.com).
  Installing it there runs this setup. Its settings (language, display, resolution,
  frame rate, VSync, difficulty, loading screens, controls, mouse) show in
  RecompLauncher, and it backs up your saves. Started from RecompLauncher, the
  game's own launcher window stays closed (hold **Shift** to open it).

## Tested

- Fresh install into an empty folder with this setup (11 min): MSVC 14.51, the game
  built and started; a RecompLauncher-style start (no arguments) found the game data
  and skipped the launcher window.
- Repair of a fresh 0.2.0 install that had stopped at the game build (MSVC 14.52):
  the setup swapped in 14.51 and the game built.
- Played on one PC: key bindings and controller remapping (keyboard, Xbox
  pad), Difficulty (Game Journalist), Exclusive fullscreen, the cursor when switching
  between keyboard and controller, and the loading screens of Justice (full pass,
  44 s) and Okiku at the start of the Daimyo level (39 s), with and without a button.
- The whole patch stack applies to a clean ReXGlue `bd833a2`.

## Still not claimed

- At 60, scenes that show two camera views at once can still drop below 60.
- The "Afro!" voice on the loading screen's eyes starts slightly cut (as before).
- Only the Justice and Okiku loading stories were checked one by one; others follow
  the same two rules.
- No install time is guaranteed on HDD, SSD, or NVMe.
