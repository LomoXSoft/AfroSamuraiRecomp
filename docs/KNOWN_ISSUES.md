# Known issues

For the current version (`app/VERSION`). These come from test runs on one PC
and from reports, not from an independent full playthrough. Older lists are in
the Git history and the release notes in `docs/releases/`.

## Game
- At 60 FPS, scenes that show two camera views at once can still drop below 60.
- The headband looked correct in play since 0.1.1. Other black textures are not
  claimed fixed.
- Occasional odd audio.
- If two tutorial prompts appear at once, the first one can stay stuck on screen
  (original game behaviour; restarting the level clears it).
- **Game Journalist** difficulty (you can't die) works in the tested levels. It is
  not a guarantee for the whole game. It is off by default and saved like the
  other settings.
- Loading screens: only the Justice and Okiku stories were checked one by one;
  the others follow the same two rules (play in full, any button continues once
  the level is loaded).
- The options under **Experimental** (launcher or the in-game menu) are not fully
  tested.

## Graphics
- Upscaling (AMD FSR 1, AMD CAS) and anti-aliasing (FXAA) are new. They were
  checked on one PC with an AMD Radeon RX 9060 XT, in borderless fullscreen at
  2560x1440: the intro logos and movie of the Europe and USA discs, and one level
  of the Europe disc loaded from a save. FSR gives sharper edges than bilinear.
  Not checked on NVIDIA or Intel GPUs.
- Changes in the in-game options were checked on the Europe disc: upscaling and
  both sharpness sliders change the picture at once (also the paused picture
  behind the menu), FXAA from the next frame, so once the menu is closed. The
  launcher shows the values afterwards. RecompLauncher was checked separately: it
  shows the four settings and writes their keys.
- FXAA changed little in the scenes checked (dark rooms, low-contrast edges).
- FSR only upscales when the screen or window is larger than the game's picture
  (1280x720 at 1x internal resolution, 2560x1440 at 2x). At the same size it
  sharpens like CAS; then CAS sharpness applies and FSR sharpness does not.
- The in-game options, the start-up hint and the F3 overlay are drawn over the
  upscaled picture; this setting does not change them.
- Only the spatial FSR 1 is included. The runtime's temporal FSR 2 / 3 path is
  not built.
- Aspect ratio (21:9, 32:9, Auto; not released yet): checked on one PC with a
  16:9 screen (AMD Radeon RX 9060 XT), in a window of the wide shape, on the
  Europe disc: start-up logos, intro movie, title screen, main menu, loading
  screen, one level (walking and turning), pause menu, quitting back to the
  title screen and the in-game options, Level Select, the Daimyo level's opening
  cutscene (with subtitles), an in-level dialogue scene and the first fight in its
  open street; on the USA disc: start-up, title screen and one level at 21:9.
  Not checked on a real ultrawide monitor.
- With a wide aspect, the start-up logos, full-screen movies, loading screens and
  the menus over the title screen's movie show at 16:9 with black bars at the
  sides: they are drawn for a 16:9 screen.
- 21:9 is 64:27 (2560x1080). On a 3440x1440 screen it leaves thin bars (under 1 %
  of the width); **Auto** uses the screen's exact shape.
- A wider picture costs GPU time: 32:9 renders twice the pixels of 16:9 at the
  same internal resolution (2560x720 at 1x, 5120x1440 at 2x). On the test PC 32:9
  at 1x kept 60 FPS; 5120x1440 dipped to about 53 FPS while walking.
- At 32:9 the sides of the 3D scene look stretched, as with any very wide field
  of view.
- Effects that blur over a fixed number of the game's pixels (bloom, depth of
  field) spread a little more sideways on a wide picture.
- In-engine cutscenes are wide too. Their sets were built for the 16:9 frame, so
  at the sides the wider view can show where a set ends (small black areas were
  seen at the edges of two shots of the Daimyo level's opening cutscene).
- A few 2D overlays cover only the 16:9 middle on a wide picture: the dark
  backdrop of the pause menu's quit question leaves the game visible at the sides.
- In a window, the window takes the picture's shape (2560x720 for 32:9 at 100 %
  display scaling), which can be wider than a 16:9 screen.

## Controls and button prompts
- The game's prompts for A, B, X, Y, LB, LT, RT and the D-pad follow your device.
  RB, Start, Back and the stick clicks have not been found yet and keep the Xbox
  icon.
- Checked in game with Xbox, DualSense, Switch and generic controllers. Steam
  icons were not checked in game. Steam Deck is not tested.
- Keyboard icons show the first key of each binding.
- Camera invert: on the USA disc the game's own Camera menu has the Y axis only.
  X invert works there from the launcher or the F1 menu, but the USA save does
  not store it; it is kept in `afro_language.cfg`.

## Install
- Supported discs: Europe (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) and USA.
- USA disc: new in 0.3.0. Installed from a USA ISO and from extracted files, and
  checked with a controller and the keyboard (English and French): menus, Europe
  saves, a new game, loading screens, the in-game options menu, 60 FPS, Game
  Journalist, and the prompt icons (A, B, X, Y and RT confirmed to use the same
  images as Europe). Not played through.
- The USA disc has English and French text only: with Spanish or Italian chosen,
  the launcher keeps that language and the game runs in English.
- No USA ISO SHA-1 is recorded yet: a USA ISO shows the "not verified" note, and
  its game files are checked after extraction.
- Windows x64 only (D3D12).
- No install time is guaranteed on HDD, SSD or NVMe; SSD/NVMe recommended. A
  fresh install took 13 to 16 minutes on the test PC.
