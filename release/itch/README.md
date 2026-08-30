# itch.io release materials

The Android port's itch.io project is published at:

https://harminoff.itch.io/ivan-android-unofficial

## Current release candidate

- Version: `0.59-android.6` (`versionCode` 6)
- Application ID: `io.github.harminoff.ivan`
- Minimum Android version: Android 8.0 / API 26
- Architectures: `arm64-v8a` and `x86_64`
- APK: `builds/ivan-android-0.59-android.6.apk` (ignored by Git)
- APK SHA-256: `5E2D7535295AC9F20BF5A9C22BA61763EBF7B219281061C943BB5C788C30F7F3`
- Signing certificate SHA-256: `966DB1CE3EC261589D8699192D371A8908A3A31959CB4A65E25ED9F71BA539CB`

The matching Windows desktop package is
`builds/ivan-windows-0.59-crafting-and-tiles.zip` (ignored by Git), with
SHA-256 `D91782CFA6231260AA0955DC1C7AFE4049B61058B557F05659FDB41A36B6B417`.

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
