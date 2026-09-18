# PURE WARDRIVER logo background generator.
# Converts pictures/pure-wardriver-logo.png into a dimmed, dithered
# 240x320 BGR565 C array (V8 panel wants BGR order, pushImage swaps nothing).
# Run from the repo root after changing the logo:
#   powershell -ExecutionPolicy Bypass -File tools/GenerateWardriverBg.ps1

param(
  [string]$InFile = "pictures/pure-wardriver-logo.png",
  [string]$Output = "esp32_marauder/PureWardriverBg.h",
  [string]$Preview = "",
  [double]$Dim = 0.55,
  [int]$Width = 240,
  [int]$Height = 320,
  [switch]$Gray
)

Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Bitmap]::FromFile((Join-Path (Get-Location) $InFile))
$small = New-Object System.Drawing.Bitmap $Width, $Height
$g = [System.Drawing.Graphics]::FromImage($small)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($src, 0, 0, $Width, $Height)
$g.Dispose()

# Bayer 4x4 ordered dithering against RGB565 banding.
$bayer = @(0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)
$clamp = { param($v) if ($v -lt 0) { 0 } elseif ($v -gt 255) { 255 } else { [int]$v } }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("#define wardriver_bg_width $Width")
[void]$sb.AppendLine("#define wardriver_bg_height $Height")
[void]$sb.AppendLine("static const uint16_t wardriver_bg_bits[] PROGMEM = {")
for ($py = 0; $py -lt $Height; $py++) {
  $line = @()
  for ($px = 0; $px -lt $Width; $px++) {
    $c = $small.GetPixel($px, $py)
    if ($Gray) {
      $gl = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
      $cr = $gl; $cg = $gl; $cb = $gl
    } else {
      $cr = $c.R; $cg = $c.G; $cb = $c.B
    }
    $d = ($bayer[(($py % 4) * 4) + ($px % 4)] - 8) / 16.0
    $r = &$clamp ([math]::Round($cr * $Dim + $d))
    $gg = &$clamp ([math]::Round($cg * $Dim + $d))
    $b = &$clamp ([math]::Round($cb * $Dim + $d))
    $r5 = $r -shr 3
    $g6 = $gg -shr 2
    $b5 = $b -shr 3
    # BGR565: panel wants swapped order (TFT_RGB_ORDER TFT_BGR).
    $v = ($b5 -shl 11) -bor ($g6 -shl 5) -bor $r5
    $line += ("0x{0:X4}" -f $v)
  }
  [void]$sb.AppendLine("  " + ($line -join ", ") + ",")
}
[void]$sb.AppendLine("};")
Set-Content -LiteralPath $Output -Value $sb.ToString() -Encoding Ascii

if ($Preview -ne "") {
  # Preview in plain RGB (the panel renders the stored BGR565 in original colors).
  $prev = New-Object System.Drawing.Bitmap $Width, $Height
  for ($py = 0; $py -lt $Height; $py++) {
    for ($px = 0; $px -lt $Width; $px++) {
      $c = $small.GetPixel($px, $py)
      if ($Gray) {
        $gl2 = 0.299 * $c.R + 0.587 * $c.G + 0.114 * $c.B
        $rv = $gl2; $gv = $gl2; $bv = $gl2
      } else {
        $rv = $c.R; $gv = $c.G; $bv = $c.B
      }
      $prev.SetPixel($px, $py, [System.Drawing.Color]::FromArgb(
        [math]::Max(0, [math]::Min(255, [int]($rv * $Dim))),
        [math]::Max(0, [math]::Min(255, [int]($gv * $Dim))),
        [math]::Max(0, [math]::Min(255, [int]($bv * $Dim)))))
    }
  }
  $prev.Save($Preview)
  $prev.Dispose()
}

$src.Dispose()
$small.Dispose()
Write-Output "done"
