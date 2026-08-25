param(
    [string]$KeyAlias = "ivan-android"
)

$ErrorActionPreference = "Stop"

$signingDirectory = Join-Path $PSScriptRoot "signing"
$keystorePath = Join-Path $signingDirectory "ivan-android-release.p12"
$propertiesPath = Join-Path $signingDirectory "keystore.properties"
$recoveryPath = Join-Path $signingDirectory "RECOVERY.txt"

if ((Test-Path -LiteralPath $keystorePath) -or
    (Test-Path -LiteralPath $propertiesPath) -or
    (Test-Path -LiteralPath $recoveryPath)) {
    throw "Release signing files already exist. Refusing to overwrite them."
}

$keytool = Get-Command keytool -ErrorAction Stop
$randomBytes = New-Object byte[] 32
$randomNumberGenerator = [System.Security.Cryptography.RandomNumberGenerator]::Create()
$randomNumberGenerator.GetBytes($randomBytes)
$randomNumberGenerator.Dispose()
$password = [Convert]::ToBase64String($randomBytes).TrimEnd("=").Replace("+", "-").Replace("/", "_")

New-Item -ItemType Directory -Path $signingDirectory -Force | Out-Null

$keytoolArguments = @(
    "-genkeypair",
    "-storetype", "PKCS12",
    "-keystore", $keystorePath,
    "-storepass", $password,
    "-keypass", $password,
    "-alias", $KeyAlias,
    "-keyalg", "RSA",
    "-keysize", "4096",
    "-validity", "10000",
    "-dname", "CN=IVAN Android Unofficial, OU=Community Port, O=harminoff, C=US"
)

& $keytool.Source @keytoolArguments

if ($LASTEXITCODE -ne 0) {
    throw "keytool failed with exit code $LASTEXITCODE"
}

@"
storeFile=signing/ivan-android-release.p12
storePassword=$password
keyAlias=$KeyAlias
keyPassword=$password
"@ | Set-Content -LiteralPath $propertiesPath -Encoding ASCII

@"
IVAN Android release signing recovery information

Keystore: ivan-android-release.p12
Alias: $KeyAlias
Password: $password

Back up this entire signing directory securely. Anyone with these files can
sign updates as this Android application. Losing them prevents future releases
from updating existing installations.
"@ | Set-Content -LiteralPath $recoveryPath -Encoding UTF8

Write-Host "Created the release keystore and local recovery file in $signingDirectory"
Write-Host "Back up that directory before publishing the first release."
