# Known issues for 0.1.1

`app/VERSION` is `0.1.1`. These come from the 0.1.1 test runs and earlier
reports, not from an independent full playthrough. For 0.1.0, see
`docs/KNOWN_ISSUES_0.1.0.md`.

- Frame rate dips, especially above native internal resolution. The cause is
  not proven.
- The headband looked correct in play of 0.1.1 (quad depth offset, half by
  default). Other black textures are not claimed fixed.
- Occasional odd audio.
- If two tutorial prompts appear at once, the first one can stay stuck on
  screen (original game behaviour; restarting the level clears it).
- Experimental (60) can still run cutscenes too fast and drop frames.
- Invulnerability (experimental) works in the tested build. It is not a
  guarantee for the whole game. It is off by default, lasts for that launch
  only, and is not saved.
- HDD installation can take a long time; SSD/NVMe recommended. No install
  time is guaranteed.
