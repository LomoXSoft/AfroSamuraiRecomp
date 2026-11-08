# Append-only install log. A failure to write the log is not an installation failure.
# Retries a short lock, then uses one fallback file. Never throws and never
# calls itself while already inside a write.

$script:LogFallback = $null
$script:LogNoted = $false
$script:LogBusy = $false
$script:LogRetryAt = [datetime]::MinValue

function Write-LogConsole([string]$Text) {
  try {
    [Console]::Out.WriteLine($Text)
    [Console]::Out.Flush()
  } catch {
    try { Write-Host $Text } catch { }
  }
}

function Add-LogLine([string]$Path, [string]$Line) {
  if (-not $Path) { return $false }
  # Default matches Add-Content on this host: ANSI on Windows PowerShell 5.1, UTF-8 on PowerShell 7.
  $bytes = [Text.Encoding]::Default.GetBytes($Line + "`r`n")
  for ($i = 0; $i -lt 2; $i++) {
    $fs = $null
    try {
      $fs = New-Object System.IO.FileStream($Path, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
      $fs.Write($bytes, 0, $bytes.Length)
      $fs.Flush()
      return $true
    } catch {
      if ($i -eq 0) { Start-Sleep -Milliseconds 40 }
    } finally {
      if ($fs) { $fs.Dispose() }
    }
  }
  return $false
}

function Test-LogPath([string]$Path) {
  $fs = $null
  try {
    $fs = New-Object System.IO.FileStream($Path, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
    return $true
  } catch {
    return $false
  } finally {
    if ($fs) { $fs.Dispose() }
  }
}

function Get-LogFallback([string]$Primary) {
  $dir = Split-Path -Parent $Primary
  if ($dir) {
    $beside = Join-Path $dir 'install-fallback.log'
    if (Test-LogPath $beside) { return $beside }
  }
  $temp = Join-Path ([System.IO.Path]::GetTempPath()) 'AfroSamuraiRecomp-install.log'
  if (Test-LogPath $temp) { return $temp }
  return $null
}

function Write-InstallLog([string]$Text, [switch]$Bare) {
  if ($script:LogBusy) { return }
  $script:LogBusy = $true
  try {
    $line = if ($Bare) { $Text } else { (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ' + $Text }
    $primary = $script:LOG
    if ($script:LogFallback) {
      if ($primary -and (Get-Date) -gt $script:LogRetryAt) {
        if (Add-LogLine $primary $line) {
          $back = $script:LogFallback
          $script:LogFallback = $null
          Write-LogConsole "install.log accepts writes again (fallback was $back)."
          return
        }
        $script:LogRetryAt = (Get-Date).AddSeconds(2)
      }
      [void](Add-LogLine $script:LogFallback $line)
      return
    }
    if (-not $primary) { return }
    if (Add-LogLine $primary $line) { return }
    $fb = Get-LogFallback $primary
    if (-not $fb) {
      if (-not $script:LogNoted) {
        $script:LogNoted = $true
        Write-LogConsole "Could not write $primary. Installation continues. No fallback log could be created."
      }
      return
    }
    $script:LogFallback = $fb
    $script:LogRetryAt = (Get-Date).AddSeconds(2)
    [void](Add-LogLine $fb $line)
    if (-not $script:LogNoted) {
      $script:LogNoted = $true
      $note = "Could not write $primary. Installation continues. Fallback log: $fb"
      [void](Add-LogLine $fb $note)
      Write-LogConsole $note
    }
  } finally {
    $script:LogBusy = $false
  }
}
