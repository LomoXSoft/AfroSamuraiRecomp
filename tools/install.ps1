<#
AfroSamuraiRecomp one-click installer (Windows 10/11 x64).

Asks for an install folder and your Afro Samurai (Europe) ISO, then downloads a
portable toolchain from official sources into <root>\tools (no admin rights, no
PATH/registry changes, no Visual Studio installer), gets the ReXGlue SDK, clones
this repository, extracts your disc and builds afrorecomp.exe.

Safe to re-run: finished steps are remembered in <root>\.install and skipped.
Nothing is ever uploaded. Log: <root>\install.log

Usage (normally just double-click install.bat):
  powershell -ExecutionPolicy Bypass -File install.ps1 [options]
Options (all optional; anything missing is asked interactively):
  -Root <dir>            install folder
  -Iso <file>            your Afro Samurai (Europe) ISO
  -AcceptMsvcLicense     accept the Microsoft Visual Studio Build Tools license
                         without the interactive prompt
  -Shortcut yes|no       create desktop + Start menu shortcuts
  -Ref <branch|tag>      repository revision to build (default: main)
  -RepoUrl <url|path>    repository to clone (default: the GitHub repo)
Advanced:
  -GameDataDir <dir>     use an already extracted disc folder (must contain
                         default.xex) instead of an ISO; it is copied into
                         <root>\gamedata
#>
param(
  [string]$Root, [string]$Iso, [switch]$AcceptMsvcLicense,
  [ValidateSet('', 'yes', 'no')][string]$Shortcut = '',
  [string]$Ref = 'main',
  [string]$RepoUrl = 'https://github.com/LomoXSoft/AfroSamuraiRecomp.git',
  [string]$GameDataDir
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'Continue'
$script:DLSHARE = 0.9   # share of a download+extract step spent downloading
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$ISO_SHA1 = '29394DB1854DFC59C767D768C8D35E0A08ACB73D'
$NEED_GB = 20
# Pinned downloads (official sources) with SHA-256.
$DL = @{
  mingit = @{ url = 'https://github.com/git-for-windows/git/releases/download/v2.56.0.windows.4/MinGit-2.56.0.4-64-bit.zip'; sha = 'e3bf78351fe777fba47ddee5950082f8a98bb0f02604fdc7ba3ee2cb68ffa877'; file = 'MinGit-2.56.0.4-64-bit.zip' }
  cmake  = @{ url = 'https://github.com/Kitware/CMake/releases/download/v4.4.5/cmake-4.4.5-windows-x86_64.zip'; sha = '326c826486d46a23cf9d5a1c9ee3a567d427bdd02c4a34071937a2a675cff004'; file = 'cmake-4.4.5-windows-x86_64.zip' }
  ninja  = @{ url = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'; sha = '07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65'; file = 'ninja-win-1.13.2.zip' }
  llvm   = @{ url = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/clang%2Bllvm-20.1.8-x86_64-pc-windows-msvc.tar.xz'; sha = 'f229769f11d6a6edc8ada599c0cda964b7dee6ab1a08c6cf9dd7f513e85b107f'; file = 'clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz' }
  python = @{ url = 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip'; sha = '4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3'; file = 'python-3.12.10-embed-amd64.zip' }
  pycryptodome = @{ url = 'https://files.pythonhosted.org/packages/e1/52/7d0830ccb961c9e7224b88a60df1a4eab7fa6c5fd426d7b7c972988a4cac/pycryptodome-4.3.0-cp39-abi3-win_amd64.whl'; sha = '195c9d781905605b3b9de8567c47986a9d0ee513566b5150eb9400692465b610'; file = 'pycryptodome-4.3.0-cp39-abi3-win_amd64.zip' }
  # portable-msvc.py by Martins Mozeiko (public domain): downloads MSVC + Windows SDK
  # directly from Microsoft's official Visual Studio manifests.
  pmsvc  = @{ url = 'https://gist.githubusercontent.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/raw/49b58850aa5e5c1e65f090d4651ee76607883e11/portable-msvc.py'; sha = '1791cccca594ff083ce8d3e0a139e6156d2c312e24332a909e608010f0230a51'; file = 'portable-msvc.py' }
  rexsdk = @{ url = 'https://github.com/rexglue/rexglue-sdk/releases/download/nightly-20261002-bd833a2a/rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip'; sha = '1e19d7ec9be0d9d7e1a0088a34a33b068d4dea520181e81318528ef3b37627e7'; file = 'rexglue-sdk-0.10.0.24-dev.gbd833a2-win-amd64.zip' }
}
$REXGLUE_GIT = 'https://github.com/rexglue/rexglue-sdk.git'
$REXGLUE_COMMIT = 'bd833a2'

function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c; if ($script:LOG) { "$(Get-Date -f 'yyyy-MM-dd HH:mm:ss') $m" | Add-Content -LiteralPath $script:LOG } }
function Ask($q, $def) { $a = Read-Host "$q [$def]"; if ([string]::IsNullOrWhiteSpace($a)) { $def } else { $a.Trim().Trim('"') } }
function YesNo($q, $def = 'y') { (Ask "$q (y/n)" $def).ToLower().StartsWith('y') }
$script:STEP = 'start'; $script:BASE = 0
# Overall progress: each step weighted by its typical duration (minutes).
$PLAN = [ordered]@{ 'iso-hash' = 1; git = 1; cmake = 1; ninja = 0.3; python = 0.5; llvm = 8; msvc = 6; repo = 0.3; sdk = 1; 'rexglue-src' = 4; gamedata = 4; 'build-app' = 30; 'build-gpu' = 15; verify = 0.2 }
$TOTALW = ($PLAN.Values | Measure-Object -Sum).Sum
$script:DONEW = 0; $script:CUR = $null; $script:LASTLOGPCT = -10; $script:LASTDRAW = [DateTime]::MinValue
function Prog([double]$frac, [string]$detail = '', [switch]$Force) {
  $w = if ($script:CUR) { $PLAN[$script:CUR] } else { 0 }
  $pct = [math]::Min(99, [math]::Floor(100 * ($script:DONEW + $w * [math]::Max([double]0, [math]::Min([double]1, $frac))) / $TOTALW))
  $i = if ($script:CUR) { @($PLAN.Keys).IndexOf($script:CUR) + 1 } else { 0 }
  $line = "[{0,3}%] Step {1}/{2}: {3}" -f $pct, $i, $PLAN.Count, $script:STEP
  if ($detail) { $line += " ($detail)" }
  if (-not $Force -and ((Get-Date) - $script:LASTDRAW).TotalMilliseconds -lt 500) { return }
  $script:LASTDRAW = Get-Date
  try { $Host.UI.RawUI.WindowTitle = "AfroSamuraiRecomp installer $line" } catch { }
  Write-Progress -Activity 'AfroSamuraiRecomp installer' -Status $line -PercentComplete $pct
  if ($Force -or $pct -ge $script:LASTLOGPCT + 5) { $script:LASTLOGPCT = $pct; Say $line White }
}
function Finish($n) { $script:DONEW += $PLAN[$n]; $script:CUR = $null }
function Done($n) { Test-Path -LiteralPath (Join-Path $STAMPS "$n.done") }
function Mark($n) { New-Item -ItemType File -Force -Path (Join-Path $STAMPS "$n.done") | Out-Null }
function Step($n, $title, [scriptblock]$body) {
  $script:STEP = $title; $script:CUR = $n
  if (Done $n) { Say "[skip] $title (already done)" DarkGray; Finish $n; return }
  Prog 0 -Force
  $t = Get-Date; & $body; Mark $n; Finish $n
  Say ("    done in {0:n0} s" -f ((Get-Date) - $t).TotalSeconds) Green
}
function Fetch($key) {
  $d = $DL[$key]; $out = Join-Path $DLDIR $d.file
  if ((Test-Path -LiteralPath $out) -and ((Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash -eq $d.sha)) { return $out }
  Say "    downloading $($d.url)"
  $req = [Net.HttpWebRequest]::Create($d.url); $req.UserAgent = 'AfroSamuraiRecomp-installer'
  $resp = $req.GetResponse(); $len = $resp.ContentLength; $in = $resp.GetResponseStream()
  $fs = [IO.File]::Create("$out.part"); $buf = New-Object byte[] 1048576; $got = 0
  try {
    while (($r = $in.Read($buf, 0, $buf.Length)) -gt 0) {
      $fs.Write($buf, 0, $r); $got += $r
      if ($len -gt 0) { Prog ($got / $len * $script:DLSHARE) ("downloading {0}: {1:n0}/{2:n0} MB" -f $d.file, ($got / 1MB), ($len / 1MB)) }
    }
  } finally { $fs.Close(); $in.Close(); $resp.Close() }
  $h = (Get-FileHash -LiteralPath "$out.part" -Algorithm SHA256).Hash
  if ($h -ne $d.sha) { Remove-Item -LiteralPath "$out.part" -Force; throw "SHA-256 mismatch for $($d.file) (got $h, expected $($d.sha))" }
  Move-Item -LiteralPath "$out.part" $out -Force
  Say ("    ok, {0:n1} MB, SHA-256 verified" -f ((Get-Item -LiteralPath $out).Length / 1MB))
  return $out
}
function Unzip($zip, $dest) { $ProgressPreference = 'SilentlyContinue'; Prog $script:DLSHARE 'extracting' -Force; if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }; Expand-Archive -LiteralPath $zip -DestinationPath $dest -Force }
function Run($exe, [string[]]$a, $wd = $null) {
  # Runs a program, streaming its output to the console and the log; throws on failure.
  if ($wd) { Push-Location -LiteralPath $wd }
  try {
    $prev = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    & $exe @a 2>&1 | ForEach-Object {
      $s = "$_"; Add-Content -LiteralPath $LOG $s
      if ($s -match '^\[(\d+)/(\d+)\]') { Prog ($script:BASE + (1.0 - $script:BASE) * [double]$Matches[1] / [double]$Matches[2]) ("compiling {0}/{1}" -f $Matches[1], $Matches[2]) }
      elseif ($s -match 'codegen|CODEGEN') { Write-Host "    $s" }
      elseif ($s -notmatch '^\[') { Write-Host "    $s" }
    }
    $code = $LASTEXITCODE; $ErrorActionPreference = $prev
    if ($code -ne 0) { throw "$exe exited with code $code" }
  } finally { if ($wd) { Pop-Location } }
}
function Git([string[]]$a, $wd = $null) { Run (Join-Path $TOOLS 'git\cmd\git.exe') $a $wd }

# ---------------------------------------------------------------- setup ------
Write-Host ''
Write-Host 'AfroSamuraiRecomp installer' -ForegroundColor Yellow
Write-Host 'Builds the game from YOUR OWN disc image. Nothing is uploaded anywhere.'
Write-Host ''
$here = $PSScriptRoot
$inRepo = Test-Path -LiteralPath (Join-Path $here 'build_win.bat')
if (-not $Root) {
  # From a clone: a sibling folder (keeps toolchain and game files out of the checkout); standalone: next to the script.
  $def = if ($inRepo) { Join-Path (Split-Path (Split-Path $here -Parent) -Parent) 'AfroSamuraiRecomp-install' } else { Join-Path $here 'AfroSamuraiRecomp' }
  $Root = Ask 'Install folder (needs about 20 GB free; avoid paths with spaces or non-ASCII characters if possible)' $def
}
$Root = [IO.Path]::GetFullPath($Root)
New-Item -ItemType Directory -Force -Path $Root | Out-Null
$LOG = Join-Path $Root 'install.log'; $script:LOG = $LOG
$TOOLS = Join-Path $Root 'tools'; $DLDIR = Join-Path $Root 'downloads'; $STAMPS = Join-Path $Root '.install'
$REPO = Join-Path $Root 'afrorecomp'
$here2 = Split-Path $here -Parent
# Started from a clone of this repo: build that checkout (no GitHub access needed for the source).
if ($inRepo -and -not $PSBoundParameters.ContainsKey('RepoUrl')) { $RepoUrl = $here2 }
if ($inRepo -and ((Split-Path $here2 -Parent) -eq $Root)) { $REPO = $here2 }   # run from an existing clone
New-Item -ItemType Directory -Force -Path $TOOLS, $DLDIR, $STAMPS | Out-Null
$t0 = Get-Date
Say "install folder: $Root"
Say "log file:       $LOG"

try {
  # ------------------------------------------------------- inputs ----------
  $script:STEP = 'choose game data'
  $haveGame = Test-Path -LiteralPath (Join-Path $Root 'gamedata\default.xex')
  if (-not $haveGame -and -not $GameDataDir -and -not $Iso) {
    Say 'Select your Afro Samurai (Europe) ISO...'
    try {
      Add-Type -AssemblyName System.Windows.Forms
      $dlg = New-Object System.Windows.Forms.OpenFileDialog
      $dlg.Title = 'Select your Afro Samurai (Europe) ISO'; $dlg.Filter = 'Disc images (*.iso)|*.iso|All files (*.*)|*.*'
      if ($dlg.ShowDialog() -eq 'OK') { $Iso = $dlg.FileName }
    } catch { }
    if (-not $Iso) { $Iso = Ask 'Path to your ISO' '' }
  }
  if ($Iso -and -not (Test-Path -LiteralPath $Iso)) { throw "ISO not found: $Iso" }
  if ($GameDataDir -and -not (Test-Path -LiteralPath (Join-Path $GameDataDir 'default.xex'))) { throw "$GameDataDir does not contain default.xex" }

  # ------------------------------------------------------- checks ----------
  $script:STEP = 'system checks'
  if (-not [Environment]::Is64BitOperatingSystem) { throw 'A 64-bit Windows is required.' }
  $free = (Get-PSDrive -Name ($Root.Substring(0, 1))).Free / 1GB
  Say ("free space on {0}: {1:n1} GB" -f $Root.Substring(0, 2), $free)
  if (-not (Done 'build') -and $free -lt $NEED_GB) { throw ("Not enough disk space: about {0} GB needed, {1:n1} GB free." -f $NEED_GB, $free) }
  $gpus = @(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -notmatch 'Basic|Remote|Virtual|Parsec|Meta' })
  if ($gpus.Count -eq 0) { Say 'WARNING: no hardware GPU found. The game needs a Direct3D 12 GPU with up-to-date drivers.' Yellow }
  else { $gpus | ForEach-Object { Say "GPU: $($_.Name) (driver $($_.DriverVersion))" } ; Say 'The game needs Direct3D 12 (feature level 11_0 or newer, i.e. most GPUs from 2015 on).' }

  $script:STEP = 'verify ISO'; $script:CUR = 'iso-hash'
  if ($Iso -and -not (Done 'iso-hash')) {
    Prog 0.1 'SHA-1 of the ISO' -Force
    Say 'Checking the ISO (SHA-1, takes a minute)...'
    $h = (Get-FileHash -LiteralPath $Iso -Algorithm SHA1).Hash
    if ($h -eq $ISO_SHA1) { Say 'ISO matches Afro Samurai (Europe), Redump 17884.' Green; Mark 'iso-hash' }
    else {
      Say "WARNING: ISO SHA-1 is $h, expected $ISO_SHA1." Yellow
      Say 'Only the Europe disc is supported; another version or a bad dump will NOT work.' Yellow
      if (-not (YesNo 'Continue anyway?' 'n')) { throw 'ISO does not match the supported version.' }
    }
  }
  Finish 'iso-hash'

  # ------------------------------------------------------- toolchain -------
  Step 'git' 'MinGit (portable Git)' { Unzip (Fetch 'mingit') (Join-Path $TOOLS 'git') }
  Step 'cmake' 'CMake (portable)' {
    $z = Fetch 'cmake'; $tmp = Join-Path $TOOLS 'cmake.tmp'; Unzip $z $tmp
    if (Test-Path -LiteralPath (Join-Path $TOOLS 'cmake')) { Remove-Item -LiteralPath (Join-Path $TOOLS 'cmake') -Recurse -Force }
    Move-Item -LiteralPath (Get-ChildItem -LiteralPath $tmp -Directory | Select-Object -First 1).FullName (Join-Path $TOOLS 'cmake')
    Remove-Item -LiteralPath $tmp -Recurse -Force
  }
  Step 'ninja' 'Ninja' { Unzip (Fetch 'ninja') (Join-Path $TOOLS 'ninja') }
  Step 'python' 'Python 3.12 (embeddable) + pycryptodome' {
    $p = Join-Path $TOOLS 'python'; Unzip (Fetch 'python') $p
    Expand-Archive -LiteralPath (Fetch 'pycryptodome') -DestinationPath $p -Force
    Run (Join-Path $p 'python.exe') @('-c', 'import Crypto.Cipher.AES')
  }
  Step 'llvm' 'LLVM/Clang 20.1.8 (about 940 MB download)' {
    $x = Fetch 'llvm'; $d = Join-Path $TOOLS 'llvm'
    if (Test-Path -LiteralPath $d) { Remove-Item -LiteralPath $d -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $d | Out-Null
    Prog 0.9 'extracting (a few minutes)' -Force
    Run "$env:SystemRoot\System32\tar.exe" @('-xf', $x, '-C', $d, '--strip-components', '1')
    if (-not (Test-Path -LiteralPath (Join-Path $d 'bin\clang++.exe'))) { throw 'clang++.exe missing after extraction' }
  }
  Step 'msvc' 'MSVC compiler + Windows SDK (portable, from Microsoft)' {
    Copy-Item -LiteralPath (Fetch 'pmsvc') (Join-Path $TOOLS 'portable-msvc.py') -Force
    $py = Join-Path $TOOLS 'python\python.exe'
    Say '    The Microsoft C++ compiler and Windows SDK are downloaded from Microsoft''s official'
    Say '    Visual Studio servers and are covered by the Visual Studio Build Tools license.'
    if ($AcceptMsvcLicense) {
      Say '    license accepted with -AcceptMsvcLicense'
      Run $py @('portable-msvc.py', '--accept-license') $TOOLS
    } else {
      Push-Location -LiteralPath $TOOLS
      try { & $py portable-msvc.py; $code = $LASTEXITCODE } finally { Pop-Location }
      if ($code -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $TOOLS 'msvc\setup_x64.bat'))) { throw 'MSVC was not installed (license not accepted or download failed).' }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $TOOLS 'msvc\setup_x64.bat'))) { throw 'msvc\setup_x64.bat missing' }
    Remove-Item -LiteralPath (Join-Path $TOOLS 'downloads') -Recurse -Force -ErrorAction SilentlyContinue
  }

  # ------------------------------------------------------- sources ---------
  Step 'repo' "AfroSamuraiRecomp source ($Ref)" {
    if (-not (Test-Path -LiteralPath (Join-Path $REPO '.git'))) {
      if (Test-Path -LiteralPath $REPO) { Remove-Item -LiteralPath $REPO -Recurse -Force }
      Git @('clone', $RepoUrl, $REPO)
    }
    if ($REPO -ne $here2) { Git @('-C', $REPO, 'checkout', $Ref) }
  }
  Step 'sdk' 'ReXGlue SDK nightly 0.10.0.24-dev.gbd833a2' {
    Unzip (Fetch 'rexsdk') (Join-Path $Root 'sdk')
    if (-not (Test-Path -LiteralPath (Join-Path $Root 'sdk\win-amd64\bin\rexglue.exe'))) {
      # zip layouts differ between nightlies: find rexglue.exe and move its parent into sdk\win-amd64
      $exe = Get-ChildItem -LiteralPath (Join-Path $Root 'sdk') -Recurse -Filter rexglue.exe | Select-Object -First 1
      if (-not $exe) { throw 'rexglue.exe not found in the SDK zip' }
      $top = $exe.Directory.Parent.FullName; $tmp = Join-Path $Root 'sdk.tmp'
      Move-Item -LiteralPath $top $tmp; Remove-Item -LiteralPath (Join-Path $Root 'sdk') -Recurse -Force
      New-Item -ItemType Directory -Force -Path (Join-Path $Root 'sdk') | Out-Null
      Move-Item -LiteralPath $tmp (Join-Path $Root 'sdk\win-amd64')
    }
  }
  Step 'rexglue-src' "ReXGlue source at $REXGLUE_COMMIT" {
    $s = Join-Path $Root 'rexglue-sdk'
    if (-not (Test-Path -LiteralPath (Join-Path $s '.git'))) {
      if (Test-Path -LiteralPath $s) { Remove-Item -LiteralPath $s -Recurse -Force }
      Git @('-c', 'core.longpaths=true', 'clone', $REXGLUE_GIT, $s)
    }
    Git @('-C', $s, '-c', 'advice.detachedHead=false', 'checkout', $REXGLUE_COMMIT)
    Git @('-C', $s, '-c', 'core.longpaths=true', 'submodule', 'update', '--init', '--recursive')
  }

  # ------------------------------------------------------- game data -------
  Step 'gamedata' 'Game files' {
    $g = Join-Path $Root 'gamedata'
    if ($GameDataDir) {
      Say "    copying $GameDataDir (advanced -GameDataDir option)"
      & "$env:SystemRoot\System32\robocopy.exe" $GameDataDir $g /E /NFL /NDL /NJH /NP | Out-Null
      if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $g 'default.xex'))) {
      Run (Join-Path $TOOLS 'python\python.exe') @((Join-Path $REPO 'tools\xiso_extract.py'), $Iso, $g, '/')
    }
    if (-not (Test-Path -LiteralPath (Join-Path $g 'default.xex'))) { throw 'gamedata\default.xex missing after extraction' }
  }

  # ------------------------------------------------------- build -----------
  Step 'build-app' 'Build afrorecomp.exe (code generation + compile, 10-40 min)' { $script:BASE = 0.25; Prog 0.02 'code generation, a few minutes' -Force; Run "$env:SystemRoot\System32\cmd.exe" @('/c', 'tools\build_win.bat') $REPO }
  Step 'build-gpu' 'Build patched GPU plugin and runtime' { $script:BASE = 0.1; Run "$env:SystemRoot\System32\cmd.exe" @('/c', 'tools\build_gpu_plugin.bat') $REPO }

  $script:STEP = 'verify output'; $script:CUR = 'verify'; Prog 0 -Force
  $out = Join-Path $REPO 'app\out\build\win-amd64-release'
  foreach ($c in @(@('afrorecomp.exe', $null), @('rexruntime.dll', 'afro_xma_frameless_chain'), @('rexgpu-xenos.dll', 'afro_edram_no_wrap'))) {
    $f = Join-Path $out $c[0]
    if (-not (Test-Path -LiteralPath $f)) { throw "$($c[0]) missing in $out" }
    if ($c[1]) {
      $txt = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($f))
      if (-not $txt.Contains($c[1])) { throw "$($c[0]) is not the patched build (no $($c[1]))" }
    }
    Say "    ok: $($c[0])"
  }
  Mark 'build'; Finish 'verify'

  # ------------------------------------------------------- shortcut --------
  $script:STEP = 'shortcuts'
  if (-not $Shortcut) { $Shortcut = if (YesNo 'Create desktop and Start menu shortcuts?' 'y') { 'yes' } else { 'no' } }
  if ($Shortcut -eq 'yes') {
    $ws = New-Object -ComObject WScript.Shell
    foreach ($dir in @([Environment]::GetFolderPath('Desktop'), (Join-Path ([Environment]::GetFolderPath('Programs')) ''))) {
      $l = $ws.CreateShortcut((Join-Path $dir 'Afro Samurai Recomp.lnk'))
      $l.TargetPath = Join-Path $REPO 'tools\run_win.bat'; $l.WorkingDirectory = $REPO
      $ico = Join-Path $REPO 'app\afro_icon.ico'; if (Test-Path -LiteralPath $ico) { $l.IconLocation = $ico }
      $l.Save(); Say "    shortcut: $($l.FullName)"
    }
  }

  $mins = ((Get-Date) - $t0).TotalMinutes
  Write-Progress -Activity 'AfroSamuraiRecomp installer' -Completed
  try { $Host.UI.RawUI.WindowTitle = 'AfroSamuraiRecomp installer [100%] done' } catch { }
  Say '[100%] All steps finished.' Green
  Say ''
  Say ("INSTALL OK in {0:n0} min." -f $mins) Green
  Say "Play:  $REPO\tools\run_win.bat   (hold Shift at start to show the launcher again)"
  Say "Saves: $([Environment]::GetFolderPath('MyDocuments'))\afrorecomp"
  Say "Log:   $Root\afro.log (game), $LOG (installer)"
  Say "Downloads in $DLDIR can be deleted to free space (about 1 GB)."
  exit 0
} catch {
  Say ''
  Write-Progress -Activity 'AfroSamuraiRecomp installer' -Completed
  try { $Host.UI.RawUI.WindowTitle = "AfroSamuraiRecomp installer FAILED at: $script:STEP" } catch { }
  Say "INSTALL FAILED at step: $script:STEP" Red
  Say "Error: $($_.Exception.Message)" Red
  Say "Full log: $LOG" Red
  Say 'Fix the problem and run the installer again: finished steps are skipped.' Yellow
  exit 1
}
