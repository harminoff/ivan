param(
    [string]$InputDirectory,
    [string]$OutputDirectory,
    [int]$MaximumWidth = 3840,
    [int]$MaximumHeight = 2160
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($InputDirectory)) {
    $InputDirectory = Join-Path $PSScriptRoot "screenshots"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot "upload"
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

Get-ChildItem -LiteralPath $InputDirectory -Filter "*.png" | ForEach-Object {
    $source = [System.Drawing.Image]::FromFile($_.FullName)
    try {
        $scale = [Math]::Min(1.0, [Math]::Min($MaximumWidth / $source.Width, $MaximumHeight / $source.Height))
        $width = [Math]::Max(1, [int][Math]::Round($source.Width * $scale))
        $height = [Math]::Max(1, [int][Math]::Round($source.Height * $scale))
        $outputPath = Join-Path $OutputDirectory $_.Name

        $target = New-Object System.Drawing.Bitmap $width, $height
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($target)
            try {
                $graphics.Clear([System.Drawing.Color]::Black)
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
                $graphics.DrawImage($source, 0, 0, $width, $height)
            }
            finally {
                $graphics.Dispose()
            }

            $target.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $target.Dispose()
        }

        Write-Host "$($_.Name): $($source.Width)x$($source.Height) -> ${width}x${height}"
    }
    finally {
        $source.Dispose()
    }
}
