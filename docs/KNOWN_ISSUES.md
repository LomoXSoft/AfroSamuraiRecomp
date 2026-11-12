# Known issues

For the current version (`app/VERSION`). These come from test runs on the
author's PC and from reports, not from an independent full playthrough. Older
lists are in the Git history and the release notes in `docs/releases/`.

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

## Controls and button prompts
- The game's prompts for A, B, X, Y, LB, LT, RT and the D-pad follow your device.
  RB, Start, Back and the stick clicks have not been found yet and keep the Xbox
  icon.
- Checked in game with Xbox, DualSense, Switch and generic controllers. Steam
  icons were not checked in game. Steam Deck is not tested.
- Keyboard icons show the first key of each binding.

## Install
- Supported discs: Europe (Redump 17884, ISO SHA-1
  `29394DB1854DFC59C767D768C8D35E0A08ACB73D`) and USA.
- USA disc: new in 0.3.0. Checked with a controller and the keyboard (English and
  French): menus, Europe saves, a new game, loading screens, the in-game options
  menu, 60 FPS, Game Journalist, and the prompt icons (A, B, X, Y and RT confirmed
  to use the same images as Europe). Not played through.
- The USA disc has English and French text only: with Spanish or Italian chosen,
  the launcher keeps that language and the game runs in English.
- No USA ISO SHA-1 is recorded yet: a USA ISO shows the "not verified" note, and
  its game files are checked after extraction.
- Windows x64 only (D3D12).
- No install time is guaranteed on HDD, SSD or NVMe; SSD/NVMe recommended. A
  fresh install took about 13 minutes on the author's PC.
