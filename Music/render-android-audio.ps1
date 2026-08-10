param(
    [Parameter(Mandatory = $true)]
    [string]$SoundFont,
    [string]$FluidSynth = "fluidsynth",
    [string]$FFmpeg = "ffmpeg"
)

$ErrorActionPreference = "Stop"
$tracks = @(
    "Cathedral", "Dungeon", "Dungeon2", "Dungeon3", "Empty", "Japan",
    "defeat", "mainmenu", "newgame", "victory", "world"
)

foreach ($track in $tracks) {
    $midi = Join-Path $PSScriptRoot "$track.mid"
    $wave = Join-Path $PSScriptRoot "$track.render.wav"
    $vorbis = Join-Path $PSScriptRoot "$track.ogg"

    & $FluidSynth -ni $SoundFont $midi -F $wave -r 44100
    if ($LASTEXITCODE -ne 0) {
        throw "FluidSynth failed while rendering $midi"
    }

    & $FFmpeg -y -i $wave -c:a libvorbis -q:a 5 $vorbis
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg failed while encoding $vorbis"
    }

    Remove-Item -LiteralPath $wave
}
