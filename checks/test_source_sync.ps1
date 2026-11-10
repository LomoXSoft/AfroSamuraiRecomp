# Dynamic checks for Sync-SourceTree in tools/install.ps1 (the update source
# sync). Only temporary folders are used. No install is started.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$p = Join-Path $root 'tools/install.ps1'
$tok = $null; $err = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($p, [ref]$tok, [ref]$err)
foreach ($f in $ast.FindAll({ $args[0] -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
  if ($f.Name -in 'Get-TreeFiles', 'Test-SameBytes', 'Sync-SourceTree') { . ([scriptblock]::Create($f.Extent.Text)) }
}
$script:emits = 0
function Emit-Step { $script:emits++ }
function Assert($c, $m) { if (-not $c) { throw "FAIL $m" } else { "ok  $m" } }
$w = Join-Path $env:TEMP ('afro-sync-' + [guid]::NewGuid().ToString('n'))
$src = "$w\src"; $dst = "$w\dst"
New-Item -ItemType Directory -Force "$src\app\src", "$src\tools", "$src\.git", "$dst\app\src", "$dst\app\out\build", "$dst\app\generated\default", "$dst\app\assets", "$dst\.git\objects", "$dst\tools\__pycache__" | Out-Null
Set-Content "$src\app\src\same.cpp" 'same'; Set-Content "$dst\app\src\same.cpp" 'same'
Set-Content "$src\app\src\changed.cpp" 'new'; Set-Content "$dst\app\src\changed.cpp" 'old'
Set-Content "$src\app\src\added.h" 'added'
Set-Content "$src\tools\a.xex" 'disc'
Set-Content "$src\.git\HEAD" 'ref'
Set-Content "$src\app\.git" 'gitdir: elsewhere'
Set-Content "$src\app\afro_hooks.toml" 'hooks'; Set-Content "$dst\app\afro_hooks.toml" 'hooks'
Set-Content "$dst\app\src\stale.cpp" 'stale'
Set-Content "$dst\tools\__pycache__\x.pyc" 'pyc'
Set-Content "$dst\app\out\build\obj.o" 'obj'
Set-Content "$dst\app\generated\default\f.cpp" 'gen'
Set-Content "$dst\app\assets\default.xex" 'xex'
Set-Content "$dst\app\afro_switch_tables.toml" 'tables'
Set-Content "$dst\.git\objects\o" 'o'; (Get-Item "$dst\.git\objects\o").IsReadOnly = $true
$old = [datetime]'2020-01-01'
Get-ChildItem $dst -Recurse -File -Force | ForEach-Object { $_.IsReadOnly = $false; $_.LastWriteTime = $old }
(Get-Item "$dst\.git\objects\o").IsReadOnly = $true
# snapshot timestamps OLDER than the objects: a changed file must still be newer
Get-ChildItem $src -Recurse -File -Force | ForEach-Object { $_.LastWriteTime = [datetime]'2019-01-01' }
$r = Sync-SourceTree $src $dst @('app\out', 'app\generated', 'app\assets', 'app\afro_switch_tables.toml', 'app\afro_icon.ico')
"changed=$($r.Changed) same=$($r.Same) removed=$($r.Removed) :: $($r.ChangedFiles -join ',') :: $($r.RemovedFiles -join ',')"
Assert ($r.Changed -eq 2) 'two files written (changed + added)'
Assert ($r.Same -eq 2) 'two equal files left alone'
Assert ((Get-Item "$dst\app\src\same.cpp").LastWriteTime -eq $old) 'unchanged file keeps its old time'
Assert ((Get-Item "$dst\app\afro_hooks.toml").LastWriteTime -eq $old) 'unchanged hooks keeps its old time'
Assert ((Get-Item "$dst\app\src\changed.cpp").LastWriteTime -gt (Get-Date).AddMinutes(-1)) 'changed file is newer than objects'
Assert ((Get-Content "$dst\app\src\changed.cpp") -eq 'new') 'changed content copied'
Assert (Test-Path "$dst\app\src\added.h") 'new file added'
Assert (-not (Test-Path "$dst\app\src\stale.cpp")) 'stale source removed'
Assert (-not (Test-Path "$dst\tools\__pycache__\x.pyc")) 'stale pycache removed'
Assert (-not (Test-Path "$dst\.git")) 'old clone .git removed'
Assert (-not (Test-Path "$dst\tools\a.xex")) 'disc image not copied'
Assert (-not (Test-Path "$dst\.git\HEAD")) 'source .git not copied'
Assert (-not (Test-Path "$dst\app\.git")) 'a worktree .git file is not copied'
Assert ((Get-Item "$dst\app\out\build\obj.o").LastWriteTime -eq $old) 'build folder kept untouched'
Assert (Test-Path "$dst\app\generated\default\f.cpp") 'generated kept'
Assert (Test-Path "$dst\app\assets\default.xex") 'assets kept'
Assert (Test-Path "$dst\app\afro_switch_tables.toml") 'switch tables kept'
$r2 = Sync-SourceTree $src $dst @('app\out', 'app\generated', 'app\assets', 'app\afro_switch_tables.toml', 'app\afro_icon.ico')
Assert ($r2.Changed -eq 0 -and $r2.Removed -eq 0 -and $r2.Same -eq 4) 'second sync is a no-op'
$r3 = Sync-SourceTree $src "$w\fresh" @('app\out')
Assert ($r3.Changed -eq 4 -and (Test-Path "$w\fresh\app\src\same.cpp")) 'fresh destination gets every file'
Remove-Item $w -Recurse -Force
'source sync checks: ok'
