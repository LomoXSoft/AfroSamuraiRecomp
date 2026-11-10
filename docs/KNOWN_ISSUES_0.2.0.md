# Known issues for 0.2.0

`app/VERSION` is `0.2.0`. These come from the 0.2.0 test runs, the release notes
and earlier reports, not from an independent full playthrough. Earlier lists:
`docs/KNOWN_ISSUES_0.1.1.md`, `docs/KNOWN_ISSUES_0.1.0.md`.

- At 60 FPS, scenes that show two camera views at once can still drop below 60.
- The headband looked correct in play of 0.1.1. Other black textures are not
  claimed fixed.
- Occasional odd audio.
- If two tutorial prompts appear at once, the first one can stay stuck on
  screen (original game behaviour; restarting the level clears it).
- Invulnerability (Settings → Gameplay) works in the tested build. It is not a
  guarantee for the whole game. It is off by default, lasts for that launch
  only, and is not saved.
- The options in **Settings → Experimental** are not fully tested.
- Button prompts: only A, B, X, Y, LB, LT, RT and the D-pad prompt are replaced.
  RB, Start, Back and the stick-click prompts keep the Xbox icon. Checked in game
  with Xbox, DualSense, Switch and generic controllers; Steam icons were not
  checked in game. Keyboard icons show the default keys, not keys you rebind.
- No install time is guaranteed on HDD, SSD, or NVMe; SSD/NVMe recommended.
  **Update** rebuilds the whole game even when little changed.
