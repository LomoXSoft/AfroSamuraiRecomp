# Draws the replacement button prompt icons (original art for this project,
# GPL v3 like the rest of the repository) into tools\artwork\glyphs\<family>\.
#
# File names are the Xbox button POSITION the game asks for. On Nintendo the
# label printed is the one found at that position: A.png shows B, and so on.
# Run from anywhere:  powershell -NoProfile -File tools\make_glyphs.ps1
param([string]$Out = (Join-Path $PSScriptRoot 'artwork\glyphs'))
Add-Type -AssemblyName System.Drawing

$Dark = [Drawing.Color]::FromArgb(255, 28, 28, 32)
$Rim = [Drawing.Color]::FromArgb(255, 200, 200, 205)

function New-Canvas($w, $h) {
  $b = New-Object Drawing.Bitmap $w, $h, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [Drawing.Graphics]::FromImage($b)
  $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
  $g.Clear([Drawing.Color]::Transparent)
  return $b, $g
}
function Save-Png($b, $g, $path) {
  $g.Dispose()
  New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
  $b.Save($path, [Drawing.Imaging.ImageFormat]::Png); $b.Dispose()
}
function New-Pen($color, $width) {
  $p = New-Object Drawing.Pen $color, $width
  $p.StartCap = 'Round'; $p.EndCap = 'Round'; $p.LineJoin = 'Round'; return $p
}
function Rounded($x, $y, $w, $h, $r) {
  $gp = New-Object Drawing.Drawing2D.GraphicsPath
  $gp.AddArc($x, $y, $r, $r, 180, 90); $gp.AddArc($x + $w - $r, $y, $r, $r, 270, 90)
  $gp.AddArc($x + $w - $r, $y + $h - $r, $r, $r, 0, 90); $gp.AddArc($x, $y + $h - $r, $r, $r, 90, 90)
  $gp.CloseFigure(); return $gp
}
function Draw-Label($g, $text, $px, $w, $h, $dy = 3) {
  $f = New-Object Drawing.Font 'Arial', $px, ([Drawing.FontStyle]::Bold), ([Drawing.GraphicsUnit]::Pixel)
  $sf = New-Object Drawing.StringFormat; $sf.Alignment = 'Center'; $sf.LineAlignment = 'Center'; $sf.FormatFlags = 'NoWrap'
  $g.DrawString($text, $f, [Drawing.Brushes]::White, (New-Object Drawing.RectangleF 0, $dy, $w, $h), $sf)
}
# Round face button: dark disc with a light rim.
function Disc($g) {
  $g.FillEllipse((New-Object Drawing.SolidBrush $Dark), 6, 6, 116, 116)
  $g.DrawEllipse((New-Pen $Rim 6), 6, 6, 116, 116)
}
function Letter-Disc($path, $text, $px = 64) {
  $b, $g = New-Canvas 128 128; Disc $g; Draw-Label $g $text $px 128 128; Save-Png $b $g $path
}
# Shoulder button: wide rounded bar.
function Shoulder($path, $text) {
  $b, $g = New-Canvas 128 128; $gp = Rounded 8 30 112 68 30
  $g.FillPath((New-Object Drawing.SolidBrush $Dark), $gp); $g.DrawPath((New-Pen $Rim 6), $gp)
  Draw-Label $g $text 44 128 128; Save-Png $b $g $path
}
# Trigger: tall rounded bar (the game's trigger prompt is 64x128).
function Trigger($path, $text) {
  $b, $g = New-Canvas 128 256; $gp = Rounded 14 14 100 228 40
  $g.FillPath((New-Object Drawing.SolidBrush $Dark), $gp); $g.DrawPath((New-Pen $Rim 6), $gp)
  Draw-Label $g $text 50 128 256; Save-Png $b $g $path
}
# Small menu button (Share/Options, View/Menu): pill with a short label.
function Pill($path, $text, $px = 30) {
  $b, $g = New-Canvas 128 128; $gp = Rounded 6 36 116 56 54
  $g.FillPath((New-Object Drawing.SolidBrush $Dark), $gp); $g.DrawPath((New-Pen $Rim 5), $gp)
  Draw-Label $g $text $px 128 128 2; Save-Png $b $g $path
}

# PlayStation: A position = cross, B = circle, X = square, Y = triangle.
$ps = Join-Path $Out 'playstation'
$b, $g = New-Canvas 128 128; Disc $g; $p = New-Pen ([Drawing.Color]::FromArgb(255, 124, 178, 232)) 11
$g.DrawLine($p, 40, 40, 88, 88); $g.DrawLine($p, 88, 40, 40, 88); Save-Png $b $g "$ps\A.png"
$b, $g = New-Canvas 128 128; Disc $g; $g.DrawEllipse((New-Pen ([Drawing.Color]::FromArgb(255, 255, 102, 102)) 11), 38, 38, 52, 52); Save-Png $b $g "$ps\B.png"
$b, $g = New-Canvas 128 128; Disc $g; $g.DrawRectangle((New-Pen ([Drawing.Color]::FromArgb(255, 230, 153, 204)) 11), 40, 40, 48, 48); Save-Png $b $g "$ps\X.png"
$b, $g = New-Canvas 128 128; Disc $g
$g.DrawPolygon((New-Pen ([Drawing.Color]::FromArgb(255, 64, 226, 160)) 11), [Drawing.Point[]]@((New-Object Drawing.Point 64, 34), (New-Object Drawing.Point 94, 86), (New-Object Drawing.Point 34, 86)))
Save-Png $b $g "$ps\Y.png"
Shoulder "$ps\LB.png" 'L1'; Shoulder "$ps\RB.png" 'R1'
Trigger "$ps\LT.png" 'L2'; Trigger "$ps\RT.png" 'R2'
Pill "$ps\BACK.png" 'SHARE' 26; Pill "$ps\START.png" 'OPTIONS' 21
Letter-Disc "$ps\LS.png" 'L3' 48; Letter-Disc "$ps\RS.png" 'R3' 48

# Nintendo: the label at the Xbox position (A<->B, X<->Y).
$nx = Join-Path $Out 'nintendo'
Letter-Disc "$nx\A.png" 'B'; Letter-Disc "$nx\B.png" 'A'; Letter-Disc "$nx\X.png" 'Y'; Letter-Disc "$nx\Y.png" 'X'
Shoulder "$nx\LB.png" 'L'; Shoulder "$nx\RB.png" 'R'
Trigger "$nx\LT.png" 'ZL'; Trigger "$nx\RT.png" 'ZR'
Letter-Disc "$nx\BACK.png" ([string][char]0x2212) 72; Letter-Disc "$nx\START.png" '+' 72
Letter-Disc "$nx\LS.png" 'LS' 44; Letter-Disc "$nx\RS.png" 'RS' 44

# Steam Deck / Steam Controller: same letters as Xbox, monochrome.
$st = Join-Path $Out 'steam'
Letter-Disc "$st\A.png" 'A'; Letter-Disc "$st\B.png" 'B'; Letter-Disc "$st\X.png" 'X'; Letter-Disc "$st\Y.png" 'Y'
Shoulder "$st\LB.png" 'L1'; Shoulder "$st\RB.png" 'R1'
Trigger "$st\LT.png" 'L2'; Trigger "$st\RT.png" 'R2'
Pill "$st\BACK.png" 'VIEW' 28; Pill "$st\START.png" 'MENU' 28
Letter-Disc "$st\LS.png" 'L3' 48; Letter-Disc "$st\RS.png" 'R3' 48

# Keyboard (mnk_mode): key caps for the default bindings of the ReXGlue
# keyboard driver (keybind_* cvars). Rebinding keys does not redraw these.
function KeyCap($path, $text, $px = 56) {
  $b, $g = New-Canvas 128 128
  $base = Rounded 8 14 112 106 26
  $g.FillPath((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 14, 14, 16))), $base)
  $top = Rounded 8 8 112 100 26
  $g.FillPath((New-Object Drawing.SolidBrush $Dark), $top); $g.DrawPath((New-Pen $Rim 5), $top)
  Draw-Label $g $text $px 128 116 0; Save-Png $b $g $path
}
# Mouse with the left or right button lit.
function Mouse($path, $right) {
  $b, $g = New-Canvas 128 128
  $body = Rounded 28 6 72 116 36
  $g.FillPath((New-Object Drawing.SolidBrush $Dark), $body)
  $g.SetClip($body)
  $x = if ($right) { 64 } else { 28 }
  $g.FillRectangle((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 235, 235, 240))), $x, 6, 36, 50)
  $g.ResetClip()
  $pen = New-Pen $Rim 5
  $g.DrawLine($pen, 64, 8, 64, 56); $g.DrawLine($pen, 30, 56, 98, 56)
  $g.DrawPath($pen, $body); Save-Png $b $g $path
}
$kb = Join-Path $Out 'keyboard'
KeyCap "$kb\A.png" 'SPACE' 31; KeyCap "$kb\B.png" 'E'; Mouse "$kb\X.png" $false; Mouse "$kb\Y.png" $true
KeyCap "$kb\LB.png" '1'; KeyCap "$kb\RB.png" '3'; KeyCap "$kb\LT.png" 'R'; KeyCap "$kb\RT.png" 'Q'
KeyCap "$kb\BACK.png" 'ESC' 44; KeyCap "$kb\START.png" 'ENTER' 31; KeyCap "$kb\LS.png" 'F'; KeyCap "$kb\RS.png" 'K'
