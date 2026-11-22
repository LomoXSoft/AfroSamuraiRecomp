<#
Build afrosetup-v<app\VERSION>.exe the way releases are built.

1. Export one commit with `git archive` (CRLF text files, as a Windows checkout).
2. In that export: make_setup_art.py, llvm-rc, clang++ (MSVC target, static CRT).
3. Append the export as the setup payload with pack_setup_payload.py.

The GitHub Actions workflow .github/workflows/setup.yml runs this script.
It can also run locally once clang++, llvm-rc, git and python are on PATH
(for example after tools\env_win.bat). Uncommitted changes are not built.
This script does not upload or replace a GitHub release asset.
#>
param(
  [string]$Ref = 'HEAD',
  [string]$OutDir = '',
  # Git checkout to export from. Default: the one this script is in. The
  # workflow passes a second checkout, so older tags without this script build too.
  [string]$Repo = ''
)

$ErrorActionPreference = 'Stop'
if ($Repo) { $repo = (Resolve-Path $Repo).Path } else { $repo = Split-Path -Parent $PSScriptRoot }
if (-not $OutDir) { $OutDir = $PSScriptRoot }

function Invoke-Tool {
  param([string]$Exe, [string[]]$ToolArgs)
  # Native stderr (compiler warnings) must not stop the script; the exit code decides.
  $ErrorActionPreference = 'Continue'
  & $Exe @ToolArgs
  if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

foreach ($tool in 'git', 'python', 'clang++', 'llvm-rc') {
  if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool was not found on PATH" }
}
$tar = Join-Path $env:SystemRoot 'System32\tar.exe'
if (-not (Test-Path $tar)) { throw "$tar was not found" }

$commit = (& git -C $repo rev-parse --verify "$Ref^{commit}")
if ($LASTEXITCODE -ne 0 -or -not $commit) { throw "unknown ref: $Ref" }
$commit = $commit.Trim()

$work = Join-Path ([IO.Path]::GetTempPath()) ('afrosetup-build-' + $commit.Substring(0, 12))
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
$src = Join-Path $work 'src'
New-Item -ItemType Directory -Force $src | Out-Null
$archive = Join-Path $work 'src.tar'

try {
  # core.autocrlf=true gives the same line endings as the local release builds,
  # whatever the machine's own Git setting is. .gitattributes still wins per file.
  Invoke-Tool git @('-C', $repo, '-c', 'core.autocrlf=true', 'archive', '--format=tar', '-o', $archive, $commit)
  Invoke-Tool $tar @('-xf', $archive, '-C', $src)
  Remove-Item -Force $archive

  $version = (Get-Content (Join-Path $src 'app\VERSION') -TotalCount 1).Trim()
  if ($version -notmatch '^\d+(\.\d+)+$') { throw "app\VERSION is not a dotted number: $version" }
  $name = "afrosetup-v$version.exe"

  Push-Location (Join-Path $src 'tools')
  try {
    Invoke-Tool python @('make_setup_art.py')
    Invoke-Tool llvm-rc @('/fo', 'afrosetup.res', 'afrosetup.rc')
    Invoke-Tool clang++ @(
      '-std=c++17', '-O2', '-Wall', '-Wextra',
      '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00',
      '-fms-runtime-lib=static',
      '-o', $name, 'afrosetup.cpp', 'afrosetup.res',
      '-lcomdlg32', '-lshell32', '-lole32', '-luuid', '-lgdi32', '-lcomctl32', '-luser32', '-ladvapi32',
      '-Wl,/SUBSYSTEM:WINDOWS', '-Wl,/ENTRY:wWinMainCRTStartup')
    Invoke-Tool python @('pack_setup_payload.py', '--exe', $name, '--repo', $src, '--overlay', $src)
  } finally {
    Pop-Location
  }

  New-Item -ItemType Directory -Force $OutDir | Out-Null
  $out = Join-Path (Resolve-Path $OutDir) $name
  Copy-Item -Force (Join-Path $src "tools\$name") $out
  $hash = (Get-FileHash -Algorithm SHA256 $out).Hash.ToLowerInvariant()
  $clang = (& clang++ --version | Select-Object -First 1)
  Write-Host "commit $commit"
  Write-Host "compiler $clang"
  Write-Host "built $out"
  Write-Host "bytes $((Get-Item $out).Length)"
  Write-Host "sha256 $hash"
} finally {
  if (Test-Path $work) { Remove-Item -Recurse -Force $work }
}
