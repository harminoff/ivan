[CmdletBinding()]
param(
  [string]$StageDirectory = ".codex-build-tmp/release-2026-10-08/windows-stage/ivan",
  [string]$OutputDirectory = ".codex-build-tmp/release-2026-10-08/artifacts"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$stagePath = (Resolve-Path (Join-Path $repoRoot $StageDirectory)).Path
$outputPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$packageName = "ivan-windows-0.59-readability-stability"
$packagePath = Join-Path $outputPath $packageName
$zipPath = Join-Path $outputPath "$packageName.zip"
if((Test-Path -LiteralPath $packagePath) -or (Test-Path -LiteralPath $zipPath))
{
  throw "Package output already exists; choose a new output directory to preserve it."
}
$gamePath = Join-Path $packagePath "ivan"
New-Item -ItemType Directory -Path $gamePath -Force | Out-Null

# Only installed assets and binaries are packaged, never local settings/saves.
foreach($entry in @("Graphics", "Script", "Music", "Sound", "ivan.exe",
                    "COPYING", "INSTALL", "LICENSING", "NEWS", "MANUAL", "README.md"))
{
  Copy-Item -LiteralPath (Join-Path $stagePath $entry) -Destination $gamePath -Recurse
}
$runtimeFiles = @(Get-ChildItem -LiteralPath $stagePath -Filter "*.dll" -File)
foreach($required in @("SDL2.dll", "SDL2_mixer.dll", "libpng16-16.dll", "zlib1.dll",
                       "libgcc_s_dw2-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"))
{
  if($required -notin $runtimeFiles.Name) { throw "Runtime DLL missing: $required" }
}
$runtimeFiles | Copy-Item -Destination $gamePath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "windows-update-readme.txt") `
  -Destination (Join-Path $packagePath "READ-ME-FIRST.txt")

$licensePath = Join-Path $gamePath "licenses"
New-Item -ItemType Directory -Path $licensePath | Out-Null
foreach($component in @("SDL2", "SDL2_mixer", "libpng", "zlib", "flac", "libogg",
                        "opus", "opusfile", "mpg123", "wavpack", "gcc-libs", "winpthreads"))
{
  $installedLicense = "C:/msys64/mingw32/share/licenses/$component"
  if(Test-Path -LiteralPath $installedLicense)
  {
    Copy-Item -LiteralPath $installedLicense -Destination $licensePath -Recurse
  }
}
foreach($license in @("xbrzscale/License.txt", "fantasyname/UNLICENSE",
                      "android/vendor/pcre/LICENCE", "android/licenses/FASTNOISE-MIT.txt",
                      "android/licenses/FEAUDIO-MIT.txt"))
{
  $licenseName = $license.Replace("/", "-")
  Copy-Item -LiteralPath (Join-Path $repoRoot $license) `
    -Destination (Join-Path $licensePath $licenseName)
}

Compress-Archive -LiteralPath $packagePath -DestinationPath $zipPath -CompressionLevel Optimal
Copy-Item -LiteralPath (Join-Path $repoRoot ".codex-build-tmp/back-paragraph-release/ivan-android-0.59-android.8-back-text-fixes.apk") `
  -Destination (Join-Path $outputPath "ivan-android-0.59-android.8.apk")
Copy-Item -LiteralPath (Join-Path $repoRoot ".codex-build-tmp/back-paragraph-release/ivan-android-0.59-android.8-back-text-fixes.aab") `
  -Destination (Join-Path $outputPath "ivan-android-0.59-android.8.aab")
foreach($debugFile in @("mapping.txt", "native-debug-symbols.zip"))
{
  Copy-Item -LiteralPath (Join-Path $repoRoot ".codex-build-tmp/back-paragraph-release/$debugFile") `
    -Destination $outputPath
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "play-beta-release-notes.txt") -Destination $outputPath
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "devlog-readability-and-stability.md") -Destination $outputPath

Get-FileHash -LiteralPath $zipPath, (Join-Path $outputPath "ivan-android-0.59-android.8.apk"), `
  (Join-Path $outputPath "ivan-android-0.59-android.8.aab") -Algorithm SHA256 |
  Select-Object @{Name="File"; Expression={Split-Path $_.Path -Leaf}}, Hash
