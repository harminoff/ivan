# itch.io release materials

The Android port's itch.io project is published at:

https://harminoff.itch.io/ivan-android-unofficial

## Readability and Stability update candidate

- Version: `0.59-android.8` (`versionCode` 8)
- Application ID: `io.github.harminoff.ivan`
- Minimum Android version: Android 8.0 / API 26
- Architectures: `arm64-v8a` and `x86_64`
- APK: `.codex-build-tmp/release-2026-10-08/artifacts/ivan-android-0.59-android.8.apk` (local only)
- APK SHA-256: `F0AA19A1FFA50A6CC89C3E148FCE292C299A45B42D576339C4D7611C7BCD4E64`
- Play bundle: the adjacent `ivan-android-0.59-android.8.aab`
- AAB SHA-256: `02A7B3E900B176BFCA5CC6D126461BBCA5C63DC992FDF3278A282390B0CB238F`
- Signing certificate SHA-256: `966DB1CE3EC261589D8699192D371A8908A3A31959CB4A65E25ED9F71BA539CB`

The matching Windows desktop package is
`.codex-build-tmp/release-2026-10-08/artifacts/ivan-windows-0.59-readability-stability.zip`,
with SHA-256 `64BC2C1C053A0856CDDD03882AE4786BDE9660A720A884434AE03D358EF5C7FB`.

The Windows project is https://harminoff.itch.io/ivan-desktop-enhanced.
`package-update.ps1` packages a fresh Windows installation with its runtime DLLs
and license notices, omitting local saves, settings, and runtime logs. The
player-facing devlog is `devlog-readability-and-stability.md`; the Play beta
notes are `play-beta-release-notes.txt`. Publication status is recorded separately
in `Doc/RELEASE_READABILITY_STABILITY_2026-10-08.md`.

The release keystore and recovery information live in the ignored
`android/signing` directory. Back up that directory securely before publishing;
losing it prevents future APKs from updating existing installations.

## Page presentation

- Cover: `cover-630x500.png`
- Screenshots: current portrait and landscape menu/gameplay layouts and the
  reformatted story screen from the signed release APK
- Theme: background `#07090d`, content `#12100f`, text `#f4eddd`, links
  `#d83a32`, Pixel font, Large text, Sidebar screenshots
- Classification: Role Playing; Android downloadable; released; $0 or donate
- Tags: roguelike, turn-based, pixel-art, open-source, singleplayer, retro,
  fantasy
- AI disclosure: Graphics (store cover) and Code (Android port assistance)

The cover was produced with the built-in image generation tool using the real
landscape menu and gameplay captures as references. Its prompt requested an
original 315:250 IVAN-style storefront tile, exact `IVAN` and
`ANDROID • UNOFFICIAL PORT` text, black/brass/bone/red colors, and the real
blue-water/green-island gameplay vignette. `prepare-cover.ps1` creates the exact
630x500 upload. `prepare-screenshots.ps1` caps emulator captures at itch.io's
3840x2160 screenshot limit without changing their aspect ratio.
