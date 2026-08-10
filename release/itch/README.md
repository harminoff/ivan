# itch.io release materials

The Android port's itch.io project is maintained as a private draft at:

https://harminoff.itch.io/ivan-android-unofficial

Do not change its visibility to Public until the signed artifact, public source
commit, page copy, screenshots, and download flow have all been checked.

## Current release candidate

- Version: `0.59-android.1` (`versionCode` 1)
- Application ID: `io.github.harminoff.ivan`
- Minimum Android version: Android 8.0 / API 26
- Architectures: `arm64-v8a` and `x86_64`
- APK: `builds/ivan-android-0.59-android.1.apk` (ignored by Git)
- APK SHA-256: `D7461EF693EDA87F889A8E79B6FAE8BF6ECC567DD27DAD1D59F5DCDD18334DFB`
- Signing certificate SHA-256: `966DB1CE3EC261589D8699192D371A8908A3A31959CB4A65E25ED9F71BA539CB`

The release keystore and recovery information live in the ignored
`android/signing` directory. Back up that directory securely before publishing;
losing it prevents future APKs from updating existing installations.

## Page presentation

- Cover: `cover-630x500.png`
- Screenshots: portrait and landscape menu, portrait and landscape gameplay,
  and the reformatted story screen from the signed release APK
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
