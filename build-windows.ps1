[CmdletBinding()]
param(
  [string]$BuildDirectory = "build-win32-native-release",
  [string]$OutputDirectory = "playable",
  [string]$MinGwRoot = "C:\msys64\mingw32",
  [ValidateRange(1, 64)]
  [int]$Jobs = 4,
  [switch]$Configure,
  [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
$mingwBin = Join-Path $MinGwRoot "bin"
$gcc = Join-Path $mingwBin "gcc.exe"
$gxx = Join-Path $mingwBin "g++.exe"
$runtime = Join-Path $mingwBin "libwinpthread-1.dll"
$buildPath = Join-Path $repoRoot $BuildDirectory
$outputPath = Join-Path $repoRoot $OutputDirectory
$pcreRoot = Join-Path $repoRoot "third_party\pcre-i686"

foreach($required in @($gcc, $gxx, $runtime))
{
  if(!(Test-Path -LiteralPath $required))
  {
    throw "Required MINGW32 toolchain file was not found: $required"
  }
}

# cc1plus.exe lives below lib\gcc rather than beside the runtime DLLs. Its
# child process therefore depends on MINGW32\bin being ahead of unrelated
# MinGW distributions in PATH. In particular, Lua for Windows ships an
# incompatible libwinpthread-1.dll that makes MSYS2 cc1plus hang silently.
$pathEntries = $env:PATH -split ";" | Where-Object {
  $_ -and $_.TrimEnd("\") -ine $mingwBin.TrimEnd("\")
}
$env:PATH = (@($mingwBin) + $pathEntries) -join ";"

$tempPath = Join-Path $repoRoot ".codex-build-tmp"
New-Item -ItemType Directory -Path $tempPath -Force | Out-Null
$env:TEMP = $tempPath
$env:TMP = $tempPath

$pcreLibrary = Join-Path $pcreRoot "lib\libpcre.a"
$pcreInclude = Join-Path $pcreRoot "include"
if(!(Test-Path -LiteralPath $pcreLibrary) -or
   !(Test-Path -LiteralPath $pcreInclude))
{
  throw "The i686 PCRE dependency is missing below $pcreRoot"
}

# Configure on every invocation so an older cache cannot silently leave the
# executable non-portable or install it below Program Files. PORTABLE_BUILD
# also mirrors game data beside the build-tree executable for direct testing.
& cmake -S $repoRoot -B $buildPath -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_C_COMPILER=$gcc" `
  "-DCMAKE_CXX_COMPILER=$gxx" `
  -DCMAKE_CXX_FLAGS=-DPCRE_STATIC `
  "-DCMAKE_INSTALL_PREFIX=$outputPath" `
  -DPORTABLE_BUILD=ON `
  "-DPCRE_LIBRARY=$pcreLibrary" `
  "-DPCRE_INCLUDE_DIR=$pcreInclude"
if($LASTEXITCODE -ne 0)
{
  throw "CMake configuration failed with exit code $LASTEXITCODE"
}

& cmake --build $buildPath --config Release --parallel $Jobs
if($LASTEXITCODE -ne 0)
{
  throw "Windows build failed with exit code $LASTEXITCODE"
}

if(!$SkipTests)
{
  & ctest --test-dir $buildPath --output-on-failure
  if($LASTEXITCODE -ne 0)
  {
    throw "Windows tests failed with exit code $LASTEXITCODE"
  }
}

& cmake --install $buildPath --config Release
if($LASTEXITCODE -ne 0)
{
  throw "Windows package installation failed with exit code $LASTEXITCODE"
}

$playableExecutable = Join-Path $outputPath "ivan\ivan.exe"
# Resolve from the installed executable as well as the build-tree copy. Some
# older MinGW install rules omit runtime DLLs when Windows paths use backslashes.
& cmake "-DIVAN_EXECUTABLE=$playableExecutable" `
  "-DIVAN_DESTINATION=$(Join-Path $outputPath 'ivan')" `
  "-DIVAN_DEPENDENCY_ROOT=$MinGwRoot" `
  -P (Join-Path $repoRoot "cmake\copy_runtime_dependencies.cmake")
if($LASTEXITCODE -ne 0)
{
  throw "Windows runtime dependency staging failed with exit code $LASTEXITCODE"
}

Write-Host "Windows Release build completed: $playableExecutable"
