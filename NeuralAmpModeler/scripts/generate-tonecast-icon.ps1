param(
  [string]$OutputPath = (Join-Path $PSScriptRoot '..\resources\ToneCast.ico')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-GradientBrush {
  param(
    [System.Drawing.RectangleF]$Bounds,
    [string[]]$Colors
  )

  if ($Bounds.Width -lt 1) { $Bounds.Width = 1 }
  if ($Bounds.Height -lt 1) { $Bounds.Height = 1 }
  $brush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
    $Bounds,
    [System.Drawing.ColorTranslator]::FromHtml($Colors[0]),
    [System.Drawing.ColorTranslator]::FromHtml($Colors[$Colors.Count - 1]),
    45.0
  )
  $blend = [System.Drawing.Drawing2D.ColorBlend]::new($Colors.Count)
  $blend.Positions = [single[]](0.0, 0.16, 0.34, 0.52, 0.72, 1.0)
  $blend.Colors = [System.Drawing.Color[]]($Colors | ForEach-Object {
    [System.Drawing.ColorTranslator]::FromHtml($_)
  })
  $brush.InterpolationColors = $blend
  return $brush
}

function New-ToneCastPng {
  param([int]$Size)

  $renderSize = [Math]::Max(256, $Size * 4)
  $source = [System.Drawing.Bitmap]::new(
    $renderSize,
    $renderSize,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
  )
  $graphics = [System.Drawing.Graphics]::FromImage($source)
  try {
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $scale = $renderSize / 200.0
    $center = $renderSize / 2.0
    $basePoints = @(
      @(6.0, -78.0),
      @(74.0, -39.0),
      @(48.0, -24.0),
      @(6.0, -48.0)
    )
    $silver = @('#615d57', '#f5f0e9', '#9c968e', '#fffaf2', '#837d75', '#ded8d0')
    $brass = @('#6d3514', '#f3b269', '#a65321', '#ffd092', '#8f431c', '#e58a32')
    $pen = [System.Drawing.Pen]::new(
      [System.Drawing.ColorTranslator]::FromHtml('#17120e'),
      [single][Math]::Max(1.0, 1.8 * $scale)
    )
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    try {
      for ($segment = 0; $segment -lt 6; $segment++) {
        $angle = $segment * [Math]::PI / 3.0
        $cos = [Math]::Cos($angle)
        $sin = [Math]::Sin($angle)
        [System.Drawing.PointF[]]$points = $basePoints | ForEach-Object {
          $x = $_[0]
          $y = $_[1]
          [System.Drawing.PointF]::new(
            [single]($center + (($x * $cos) - ($y * $sin)) * $scale),
            [single]($center + (($x * $sin) + ($y * $cos)) * $scale)
          )
        }
        $minX = ($points | Measure-Object X -Minimum).Minimum
        $minY = ($points | Measure-Object Y -Minimum).Minimum
        $maxX = ($points | Measure-Object X -Maximum).Maximum
        $maxY = ($points | Measure-Object Y -Maximum).Maximum
        $bounds = [System.Drawing.RectangleF]::new(
          [single]$minX,
          [single]$minY,
          [single]($maxX - $minX),
          [single]($maxY - $minY)
        )
        $colors = if (($segment % 2) -eq 0) { $silver } else { $brass }
        $brush = New-GradientBrush -Bounds $bounds -Colors $colors
        try {
          $graphics.FillPolygon($brush, $points)
          $graphics.DrawPolygon($pen, $points)
        }
        finally {
          $brush.Dispose()
        }
      }
    }
    finally {
      $pen.Dispose()
    }
  }
  finally {
    $graphics.Dispose()
  }

  $iconBitmap = [System.Drawing.Bitmap]::new(
    $Size,
    $Size,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
  )
  $downsample = [System.Drawing.Graphics]::FromImage($iconBitmap)
  try {
    $downsample.Clear([System.Drawing.Color]::Transparent)
    $downsample.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $downsample.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $downsample.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $downsample.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $downsample.DrawImage($source, 0, 0, $Size, $Size)
  }
  finally {
    $downsample.Dispose()
    $source.Dispose()
  }

  $stream = [System.IO.MemoryStream]::new()
  try {
    $iconBitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    return ,$stream.ToArray()
  }
  finally {
    $stream.Dispose()
    $iconBitmap.Dispose()
  }
}

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$images = [System.Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
  $images.Add([byte[]](New-ToneCastPng -Size $size))
}
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($resolvedOutput)) | Out-Null
$file = [System.IO.File]::Open($resolvedOutput, [System.IO.FileMode]::Create)
$writer = [System.IO.BinaryWriter]::new($file)
try {
  $writer.Write([uint16]0)
  $writer.Write([uint16]1)
  $writer.Write([uint16]$sizes.Count)
  $offset = 6 + (16 * $sizes.Count)
  for ($index = 0; $index -lt $sizes.Count; $index++) {
    $dimension = if ($sizes[$index] -ge 256) { 0 } else { $sizes[$index] }
    $writer.Write([byte]$dimension)
    $writer.Write([byte]$dimension)
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    $writer.Write([uint32]$images[$index].Length)
    $writer.Write([uint32]$offset)
    $offset += $images[$index].Length
  }
  foreach ($image in $images) {
    $writer.Write([byte[]]$image)
  }
}
finally {
  $writer.Dispose()
  $file.Dispose()
}

Write-Host "Generated ToneCast icon: $resolvedOutput"
Write-Host "Embedded sizes: $($sizes -join ', ')"
