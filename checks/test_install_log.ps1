# Dynamic checks for tools/logwrite.ps1. A log IO error must not throw.
# A later real failure must still throw. No install is started.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. (Join-Path $root 'tools/logwrite.ps1')

function Assert($cond, [string]$msg) {
  if (-not $cond) { throw "FAIL $msg" }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ('afro-logwrite-' + [guid]::NewGuid().ToString('n'))
New-Item -ItemType Directory -Path $work | Out-Null
try {
  $script:LOG = Join-Path $work 'install.log'
  $script:LogFallback = $null
  $script:LogNoted = $false
  $script:LogBusy = $false
  $script:LogRetryAt = [datetime]::MinValue

  Write-InstallLog 'first line'
  $body = [IO.File]::ReadAllText($script:LOG)
  Assert ($body -match 'first line') 'writable log received the line'

  # Destination that cannot be opened for append. A directory fails on
  # Windows PowerShell 5.1 and on PowerShell 7, without changing ACLs.
  $blocked = Join-Path $work 'blocked-dir'
  New-Item -ItemType Directory -Path $blocked | Out-Null
  $script:LOG = $blocked
  $script:LogFallback = $null
  $script:LogNoted = $false
  Write-InstallLog 'kept after io error'
  Assert ($script:LogFallback) 'unwritable destination switched to a fallback'
  $fb = [IO.File]::ReadAllText($script:LogFallback)
  Assert ($fb -match 'kept after io error') 'fallback kept the line'
  Assert ($fb -match [regex]::Escape($blocked)) 'fallback names the unwritable path'
  Assert ($script:LogNoted) 'the switch is announced once'

  # Nested call while a write is in progress must return instead of recursing.
  $script:LogBusy = $true
  Write-InstallLog 'must not nest'
  $script:LogBusy = $false
  $fb2 = [IO.File]::ReadAllText($script:LogFallback)
  Assert ($fb2 -eq $fb) 'busy writer did not recurse'

  # Closing the lock (here: pointing back at a real file) resumes the primary.
  $script:LOG = Join-Path $work 'install.log'
  $script:LogRetryAt = [datetime]::MinValue
  Write-InstallLog 'back on primary'
  $primary = [IO.File]::ReadAllText($script:LOG)
  Assert ($primary -match 'back on primary') 'primary log resumed after it became writable'
  Assert (-not $script:LogFallback) 'fallback was cleared after recovery'

  # A real installation error after a log failure still propagates.
  $script:LOG = $blocked
  $script:LogFallback = $null
  $script:LogNoted = $false
  $saw = $false
  try {
    Write-InstallLog 'before real failure'
    throw 'REAL_INSTALL_FAILURE'
  } catch {
    $saw = $_.Exception.Message -eq 'REAL_INSTALL_FAILURE'
  }
  Assert $saw 'real failure is not replaced by the log error'
  Assert ($script:LogFallback) 'evidence of the real step was kept in the fallback'

  # Share mode. Classic Notepad opens with read sharing, which denies a writer.
  # Linux does not enforce Windows byte-range share modes; record which one this host did.
  $held = Join-Path $work 'held.log'
  [IO.File]::WriteAllText($held, 'existing')
  $script:LOG = $held
  $script:LogFallback = $null
  $script:LogNoted = $false
  $share = [IO.File]::Open($held, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
  try {
    Write-InstallLog 'while held'
    if ($script:LogFallback) {
      Write-Output 'share-lock: this host denied the writer; fallback used'
      Assert (([IO.File]::ReadAllText($script:LogFallback)) -match 'while held') 'held-file line reached the fallback'
    } else {
      Write-Output 'share-lock: this host did not enforce FileShare.Read (not a Windows lock)'
      Assert (([IO.File]::ReadAllText($held)) -match 'while held') 'unlocked host still wrote the primary'
    }
  } finally { $share.Dispose() }

  # The cmdlet that failed on the user's machine still throws when the path cannot be opened.
  $script:LOG = $blocked
  $script:LogFallback = $null
  $script:LogNoted = $false
  $threw = $false
  try { 'x' | Add-Content -LiteralPath $blocked -ErrorAction Stop } catch { $threw = $true }
  Assert $threw 'Add-Content still throws on an unwritable path'
  Write-InstallLog 'after cmdlet throw'
  Assert ($script:LogFallback) 'our writer survived the same path'

  # Parent directory missing: sibling fallback cannot be created, so the temp file is used.
  $script:LOG = Join-Path $work 'no-such-dir/install.log'
  $script:LogFallback = $null
  $script:LogNoted = $false
  Write-InstallLog 'temp evidence'
  Assert ($script:LogFallback -like ("*" + [IO.Path]::DirectorySeparatorChar + 'AfroSamuraiRecomp-install.log')) 'temp fallback when the install directory cannot hold a log'
  Assert (([IO.File]::ReadAllText($script:LogFallback)) -match 'temp evidence') 'temp fallback kept the line'

  Write-Output 'install log writer: ok'
} finally {
  Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
