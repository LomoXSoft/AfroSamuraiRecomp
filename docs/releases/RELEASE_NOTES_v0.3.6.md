# AfroSamuraiRecomp v0.3.6 – Setup antivirus fix

Source-only release. The downloadable program is the setup installer. It builds
the game on your PC from your own Europe or USA disc. No game executable and no
game files are included.

HDD installation can take a long time; SSD/NVMe recommended.

## Download

`afrosetup-v0.3.6.exe`

SHA-256 `f2bc2b64bb90abfc7045a5519d3b69e361bf559e3585faa59a0960097c1677aa`

2000243 bytes.

Already on 0.1.x, 0.2.x or 0.3.x: run it, choose **Update** and pick your install
folder. Your extracted game, saves and launcher settings stay. **Update** rebuilds
only what changed (under a minute from 0.3.5 on the test PC).

## New

### Fewer antivirus warnings for the setup
- VirusTotal flagged the v0.3.5 setup on 19 of 74 engines (v0.3.4: 3 of 71), so
  RecompLauncher held it back. Most of the 19 came from one BitDefender signature
  shared by other engines. The rest were machine-learning guesses: the setup
  looked like a "dropper".
- The setup now carries version information: product, company, description and
  version show in the file's **Properties → Details**.
- The setup no longer starts PowerShell paused and then resumes it. That pattern
  is what antivirus engines look for in process injection. PowerShell still runs
  inside the setup's job, so **Cancel** and closing the setup still stop the
  whole build.
- A test build with these changes scanned 4 of 71 engines on VirusTotal. This
  release's setup scanned 5 of 71 before release: Bkav, CrowdStrike, SecureAge,
  Symantec and Microsoft (`Trojan:Win32/Wacatac.C!ml`, a machine-learning guess).
  The Microsoft detection has been reported to Microsoft as a false positive.
- If an antivirus still warns: the setup is not signed and starts PowerShell to
  download the build tools. Its source is `tools/afrosetup.cpp`, and the SHA-256
  above lets you check your download.

The game is the same as in v0.3.5.

## Tested

- Europe, fresh install of this release through the setup's own window, from
  the ISO (Spanish): the ISO matched Redump 17884, 18 minutes. Started past the
  launcher: the intro movie played.
- Update of a 0.3.5 Europe install to 0.3.6: 0.4 minutes, code generation
  skipped, launcher settings kept. The launcher showed `afrorecomp_v0.3.6`, and
  the game started.
- On a test build with the same setup code: closing the setup by force while it
  was building also ended PowerShell and the tools it had started.
- The checks in `checks/` pass, including the setup command test.

## Still not claimed

- The USA disc was not installed with this release (no USA files on the test
  PC). The setup passes the same options for both discs, and the game is
  unchanged from v0.3.5.
- Antivirus results can change after release, and other engines may still flag
  the setup.
- RecompLauncher was not run with this release.
- No install time is guaranteed on HDD, SSD, or NVMe.
