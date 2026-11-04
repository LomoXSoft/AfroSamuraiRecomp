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
  -InstallDir <dir>      install folder (alias -Root); skips the folder picker
  -Iso <file>            your Afro Samurai (Europe) ISO
  -AcceptMsvcLicense     accept the Microsoft Visual Studio Build Tools license
                         without the interactive prompt
  -Shortcut yes|no       create desktop + Start menu shortcuts
  -Ref <branch|tag>      repository revision to build (default: main)
  -RepoUrl <url|path>    repository to clone (default: the GitHub repo)
  -CleanupAfterInstall   at the end, delete everything not needed to play (no question)
  -KeepBuildFiles        at the end, keep the toolchain and build files (no question)
Advanced:
  -GameDataDir <dir>     use an already extracted disc folder (must contain
                         default.xex) instead of an ISO; it is copied into
                         <root>\gamedata
#>
param(
  [Alias('Root')][string]$InstallDir, [string]$Iso, [switch]$AcceptMsvcLicense,
  [ValidateSet('', 'yes', 'no')][string]$Shortcut = '',
  [string]$Ref = 'main',
  [string]$RepoUrl = 'https://github.com/LomoXSoft/AfroSamuraiRecomp.git',
  [string]$GameDataDir,
  [switch]$CleanupAfterInstall, [switch]$KeepBuildFiles
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
$TAR = Join-Path $env:SystemRoot 'System32\tar.exe'
function Extract($arc, $dest, [int]$strip = 0, [switch]$Into) {
  # Extracts with Windows' built-in tar (bsdtar: zip, tar.xz, ...), much faster than Expand-Archive.
  # Into a temporary folder that is renamed at the end, so a cancelled run never leaves a
  # half-extracted folder behind (-Into: add files to an existing folder instead).
  Prog $script:DLSHARE ("extracting {0}..." -f (Split-Path $arc -Leaf)) -Force
  $tmp = if ($Into) { $dest } else { "$dest.partial" }
  if (-not $Into -and (Test-Path -LiteralPath $tmp)) { Remove-Item -LiteralPath $tmp -Recurse -Force }
  New-Item -ItemType Directory -Force -Path $tmp | Out-Null
  $ok = $false
  if (Test-Path -LiteralPath $TAR) {
    $a = @('-xf', $arc, '-C', $tmp); if ($strip) { $a += @('--strip-components', "$strip") }
    & $TAR @a 2>&1 | ForEach-Object { Add-Content -LiteralPath $LOG "$_" }
    $ok = ($LASTEXITCODE -eq 0)
    if (-not $ok) { Say "    tar failed ($LASTEXITCODE), falling back to .NET unzip" Yellow }
  }
  if (-not $ok) {
    if ($strip -or $arc -notmatch '\.(zip|whl)$') { throw "cannot extract $arc (tar.exe missing or failed)" }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [IO.Compression.ZipFile]::OpenRead($arc)
    try { foreach ($e in $z.Entries) { $t = Join-Path $tmp $e.FullName; if ($e.Name -eq '') { New-Item -ItemType Directory -Force -Path $t | Out-Null; continue }; New-Item -ItemType Directory -Force -Path (Split-Path $t -Parent) | Out-Null; [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $t, $true) } } finally { $z.Dispose() }
  }
  if (-not $Into) {
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Rename-Item -LiteralPath $tmp (Split-Path $dest -Leaf)
  }
  Prog 1 -Force
}
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
function PickFolder($start) {
  try {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.Application]::EnableVisualStyles()
    $f = New-Object System.Windows.Forms.FolderBrowserDialog
    $f.Description = 'Choose WHERE to install AfroSamuraiRecomp (a new folder is created inside it)'
    $f.ShowNewFolderButton = $true
    if (Test-Path -LiteralPath $start) { $f.SelectedPath = $start }
    $top = New-Object System.Windows.Forms.Form -Property @{ TopMost = $true }
    if ($f.ShowDialog($top) -eq 'OK') { return $f.SelectedPath }
    return $null
  } catch { return (Ask 'Folder to install into (a new folder is created inside it)' $start) }
}
function CheckPath($p) {
  $w = @()
  if ($p -match '[^\x20-\x7E]') { $w += 'the path contains non-ASCII characters (accents, etc.), which can break the toolchain' }
  if ($p.Length -gt 80) { $w += "the path is long ($($p.Length) characters); the build creates deep folders, prefer something short like C:\Games" }
  try { $fr = (New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($p))).AvailableFreeSpace / 1GB; if ($fr -lt $NEED_GB) { $w += ("only {0:n1} GB free on that drive, about {1} GB are needed" -f $fr, $NEED_GB) } } catch { }
  $w | ForEach-Object { Write-Host "WARNING: $_" -ForegroundColor Yellow }
}
$NAME = if ($inRepo) { 'AfroSamuraiRecomp-install' } else { 'AfroSamuraiRecomp' }
$Root = $InstallDir
while (-not $Root) {
  $start = if ($inRepo) { Split-Path (Split-Path $here -Parent) -Parent } else { $here }
  Write-Host 'Choose the folder to install into (a window opens; a new folder is created inside it)...'
  $parent = PickFolder $start
  if (-not $parent) { $parent = Ask 'No folder chosen. Type a folder to install into' $start }
  $cand = [IO.Path]::GetFullPath((Join-Path $parent $NAME))
  if ($inRepo -and $cand -eq (Split-Path $here -Parent)) { $cand = "$cand-install" }
  Write-Host ''; Write-Host "Install folder: $cand" -ForegroundColor Cyan
  CheckPath $cand
  if (YesNo 'Install there?' 'y') { $Root = $cand }
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
  # ------------------------------------------------------- questions -------
  # Every question is asked here, before any download, so the rest runs unattended.
  Write-Host ''; Write-Host 'All questions are asked now; after that the installation runs to the end on its own.' -ForegroundColor Cyan
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

  # Microsoft license for the portable MSVC compiler + Windows SDK (only if not installed yet).
  $script:STEP = 'Microsoft license'
  $needMsvc = -not (Done 'msvc')
  if ($needMsvc -and -not $AcceptMsvcLicense) {
    $lic = 'https://go.microsoft.com/fwlink/?LinkId=2327714'
    try {
      $m = Invoke-RestMethod -UseBasicParsing -Uri 'https://aka.ms/vs/stable/channel'
      $bt = $m.channelItems | Where-Object { $_.id -eq 'Microsoft.VisualStudio.Product.BuildTools' } | Select-Object -First 1
      $u = ($bt.localizedResources | Where-Object { $_.language -eq 'en-us' } | Select-Object -First 1).license
      if ($u) { $lic = $u }
    } catch { }
    Write-Host ''
    Write-Host 'The Microsoft C++ compiler and Windows SDK are downloaded from Microsoft''s official'
    Write-Host 'Visual Studio servers. They are covered by the Visual Studio Build Tools license:'
    Write-Host "  $lic" -ForegroundColor Cyan
    Say "MSVC license shown: $lic"
    if (-not (YesNo 'Do you accept the Visual Studio Build Tools license?' 'n')) { throw 'The Microsoft license was not accepted; the compiler cannot be downloaded.' }
    $AcceptMsvcLicense = $true; Say 'MSVC license accepted by the user'
  }

  # Shortcuts (answer remembered for re-runs).
  $script:STEP = 'shortcuts question'
  $scFile = Join-Path $STAMPS 'shortcut.answer'
  if (-not $Shortcut -and (Test-Path -LiteralPath $scFile)) { $Shortcut = (Get-Content -LiteralPath $scFile -Raw).Trim() }
  if (-not $Shortcut) { $Shortcut = if (YesNo 'Create desktop and Start menu shortcuts when finished?' 'y') { 'yes' } else { 'no' } }
  Set-Content -LiteralPath $scFile -Value $Shortcut

  Write-Host ''
  Write-Host 'Summary' -ForegroundColor Cyan
  Say "  install folder: $Root"
  Say ("  game data:      {0}" -f $(if ($GameDataDir) { "$GameDataDir (copied)" } elseif ($Iso) { $Iso } else { 'already extracted' }))
  Say ("  Microsoft license: {0}" -f $(if ($needMsvc) { 'accepted' } else { 'MSVC already installed' }))
  Say "  shortcuts:      $Shortcut"
  Say '  Starting now; no more questions. This takes about 30-60 minutes.'
  Write-Host ''

  # ------------------------------------------------------- toolchain -------
  Step 'git' 'MinGit (portable Git)' { Extract (Fetch 'mingit') (Join-Path $TOOLS 'git') }
  Step 'cmake' 'CMake (portable)' { Extract (Fetch 'cmake') (Join-Path $TOOLS 'cmake') 1 }
  Step 'ninja' 'Ninja' { Extract (Fetch 'ninja') (Join-Path $TOOLS 'ninja') }
  Step 'python' 'Python 3.12 (embeddable) + pycryptodome' {
    $p = Join-Path $TOOLS 'python'; Extract (Fetch 'python') $p
    Extract (Fetch 'pycryptodome') $p -Into
    Run (Join-Path $p 'python.exe') @('-c', 'import Crypto.Cipher.AES')
  }
  Step 'llvm' 'LLVM/Clang 20.1.8 (about 940 MB download)' {
    $d = Join-Path $TOOLS 'llvm'; Extract (Fetch 'llvm') $d 1
    if (-not (Test-Path -LiteralPath (Join-Path $d 'bin\clang++.exe'))) { throw 'clang++.exe missing after extraction' }
  }
  Step 'msvc' 'MSVC compiler + Windows SDK (portable, from Microsoft)' {
    Copy-Item -LiteralPath (Fetch 'pmsvc') (Join-Path $TOOLS 'portable-msvc.py') -Force
    $py = Join-Path $TOOLS 'python\python.exe'
    Say '    The Microsoft C++ compiler and Windows SDK are downloaded from Microsoft''s official'
    Say '    Visual Studio servers and are covered by the Visual Studio Build Tools license.'
    if (-not $AcceptMsvcLicense) { throw 'Microsoft license not accepted' }   # asked at the start
    Run $py @('portable-msvc.py', '--accept-license') $TOOLS
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
    Extract (Fetch 'rexsdk') (Join-Path $Root 'sdk')
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
      # blobless clone: full history (bd833a2 is reachable) but file contents only for the checkout
      Git @('-c', 'core.longpaths=true', 'clone', '--filter=blob:none', '--no-checkout', $REXGLUE_GIT, $s)
    }
    Git @('-C', $s, '-c', 'advice.detachedHead=false', '-c', 'core.longpaths=true', 'checkout', '-f', $REXGLUE_COMMIT)
    # submodules: shallow at the recorded commits (much smaller), full fetch as a fallback
    try { Git @('-C', $s, '-c', 'core.longpaths=true', 'submodule', 'update', '--init', '--recursive', '--depth', '1', '--jobs', '8') }
    catch { Say '    shallow submodule fetch failed, retrying with full history' Yellow; Git @('-C', $s, '-c', 'core.longpaths=true', 'submodule', 'update', '--init', '--recursive', '--jobs', '8') }
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

  # ------------------------------------------------------- game folder -----
  # Self-contained copy of what is needed to play: <root>\game (exe, DLLs, launcher) + Play.bat.
  # Shortcuts point here, so it keeps working if the build files are deleted later.
  $script:STEP = 'game folder'
  $GAME = Join-Path $Root 'game'
  New-Item -ItemType Directory -Force -Path $GAME | Out-Null
  Get-ChildItem -LiteralPath $out -File | Where-Object {
    $_.Name -notmatch '\.(stock|pdb|ilk|lib|exp|ninja|cmake|log)$' -and $_.Name -notmatch '^(CMakeCache\.txt|\.ninja_.*|compile_commands\.json)$'
  } | ForEach-Object { Copy-Item -LiteralPath $_.FullName (Join-Path $GAME $_.Name) -Force }
  $ico = Join-Path $REPO 'app\afro_icon.ico'; if (Test-Path -LiteralPath $ico) { Copy-Item -LiteralPath $ico (Join-Path $GAME 'afro_icon.ico') -Force }
  # Same options as tools\run_win.bat.
  @('@echo off', 'rem Start Afro Samurai Recomp. Log: ..\afro.log. Hold Shift at start for the launcher.', 'cd /d "%~dp0"',
    'afrorecomp.exe --game_data_root="%~dp0..\gamedata" --gpu_plugin=xenos --readback_resolve=fast --log_file="%~dp0..\afro.log" %*') |
    Set-Content -LiteralPath (Join-Path $GAME 'Play.bat') -Encoding ASCII
  foreach ($n in 'afrorecomp.exe', 'rexruntime.dll', 'rexgpu-xenos.dll') { if (-not (Test-Path -LiteralPath (Join-Path $GAME $n))) { throw "$n missing in $GAME" } }
  Say "    game folder: $GAME"

  # ------------------------------------------------------- shortcut --------
  $script:STEP = 'shortcuts'
  if ($Shortcut -eq 'yes') {
    $ws = New-Object -ComObject WScript.Shell
    foreach ($dir in @([Environment]::GetFolderPath('Desktop'), (Join-Path ([Environment]::GetFolderPath('Programs')) ''))) {
      $l = $ws.CreateShortcut((Join-Path $dir 'Afro Samurai Recomp.lnk'))
      $l.TargetPath = Join-Path $GAME 'Play.bat'; $l.WorkingDirectory = $GAME
      if (Test-Path -LiteralPath (Join-Path $GAME 'afro_icon.ico')) { $l.IconLocation = Join-Path $GAME 'afro_icon.ico' }
      $l.Save(); Say "    shortcut: $($l.FullName)"
    }
  }

  $mins = ((Get-Date) - $t0).TotalMinutes
  Write-Progress -Activity 'AfroSamuraiRecomp installer' -Completed
  try { $Host.UI.RawUI.WindowTitle = 'AfroSamuraiRecomp installer [100%] done' } catch { }
  Say '[100%] All steps finished.' Green
  Say ''
  Say ("INSTALL OK in {0:n0} min." -f $mins) Green
  Say "Play:  $GAME\Play.bat   (hold Shift at start to show the launcher again)"
  Say "Saves: $([Environment]::GetFolderPath('MyDocuments'))\afrorecomp"
  Say "Log:   $Root\afro.log (game), $LOG (installer)"

  # ------------------------------------------------------- optional cleanup ---
  # Only folders INSIDE the install root; never the ISO, gamedata, game, logs or saves.
  $script:STEP = 'cleanup'
  $junk = @('tools', 'downloads', 'rexglue-sdk', 'sdk', 'afrorecomp', '.install') | ForEach-Object { Join-Path $Root $_ } |
    Where-Object { (Test-Path -LiteralPath $_) -and ([IO.Path]::GetFullPath($_)).StartsWith($Root + '\', [StringComparison]::OrdinalIgnoreCase) }
  if ($inRepo) { $junk = @($junk | Where-Object { $_ -ne $here2 }) }   # never delete the checkout the installer runs from
  $isoFull = if ($Iso) { [IO.Path]::GetFullPath($Iso) } else { '' }
  if ($isoFull) { $junk = @($junk | Where-Object { -not $isoFull.StartsWith($_ + '\', [StringComparison]::OrdinalIgnoreCase) }) }
  $bytes = 0; foreach ($j in $junk) { $bytes += (Get-ChildItem -LiteralPath $j -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum }
  $gb = $bytes / 1GB
  if ($junk.Count -gt 0 -and -not $KeepBuildFiles) {
    $doIt = [bool]$CleanupAfterInstall
    if (-not $CleanupAfterInstall) {
      $msg = ("Installation finished.`n`nDelete the files that are only needed to build the game and keep just what is needed to play?`n`n" +
        "This frees about {0:n1} GB (toolchain, ReXGlue SDK and source, build files, downloads).`n" +
        "Kept: the game folder ({1}), the extracted game data, the logs. Your ISO and your saves are never touched.`n`n" +
        "Note: updating or rebuilding later needs a full new install (everything is downloaded again).") -f $gb, $GAME
      try {
        Add-Type -AssemblyName System.Windows.Forms
        $r = [System.Windows.Forms.MessageBox]::Show($msg, 'AfroSamuraiRecomp - free disk space?', 'YesNo', 'Question', 'Button2')
        $doIt = ($r -eq 'Yes')
      } catch { Write-Host $msg; $doIt = YesNo 'Delete them?' 'n' }
    }
    if ($doIt) {
      Say ("Deleting build files ({0:n1} GB)..." -f $gb)
      foreach ($j in $junk) { Remove-Item -LiteralPath $j -Recurse -Force -ErrorAction Continue; Say "    deleted $j" }
      Say ("Freed about {0:n1} GB. To update later, run the installer again (full download)." -f $gb) Green
    } else {
      Say ("Build files kept ({0:n1} GB in tools, sdk, rexglue-sdk, afrorecomp, downloads)." -f $gb)
    }
  }
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
