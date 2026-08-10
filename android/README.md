# IVAN for Android (unofficial port)

This directory contains an Android SDL2/CMake host for the existing IVAN game
sources. The port targets Android 8.0 (API 26) or newer and packages native
libraries for `arm64-v8a` devices and `x86_64` emulators.

This fork is not an official Attnam release. Its public Android application ID
is `io.github.harminoff.ivan`; see `../ANDROID_PORT_NOTICE.md` for provenance,
modification, source, and licensing details.

## Build and install

Set `JAVA_HOME` to Android Studio's JBR and `ANDROID_HOME` to the Android SDK,
then run from this directory:

```powershell
.\gradlew.bat :app:assembleDebug
adb install -r app\build\outputs\apk\debug\app-debug.apk
adb shell am start -n io.github.harminoff.ivan/.IvanActivity
```

The installable debug APK is written to
`app/build/outputs/apk/debug/app-debug.apk`. Use `:app:assembleRelease` to create
an unsigned release APK when no local release key is configured.

For production distribution, create `signing/keystore.properties` beside this
README with values in the following form. The entire `signing` directory is
ignored by Git and must be backed up securely; losing the key prevents future
updates from replacing an installed release.

For a new port identity, `generate-release-key.ps1` creates a strong local key,
the properties file, and a recovery note without printing the password:

```powershell
.\generate-release-key.ps1
```

```properties
storeFile=signing/ivan-android-release.p12
storePassword=replace-with-secret
keyAlias=ivan-android
keyPassword=replace-with-secret
```

With that file and keystore present, `:app:assembleRelease` automatically signs
`app/build/outputs/apk/release/app-release.apk`. Never commit the keystore or
password file.

The checked-in Gradle wrapper expects Android SDK 35, NDK 28.2.13676358,
CMake 3.22.1, and Java 17. The build vendors SDL 2.32.10, SDL_mixer 2.8.2,
PCRE 8.45, libpng 1.6.50, and zlib 1.3.2, so it does not download native
source dependencies.

## Screen layout and controls

The mobile console uses a framed, zoomed game viewport with extracted stats,
readable messages, and large touch controls. Portrait and landscape layouts
respect Android system bars and display cutouts and recalculate during live
rotation.

- The movement pad preserves IVAN's eight directions plus wait in the center;
  holding any direction repeats movement after a short initial delay.
- Tapping the control header cycles DIRECTIONS, CONTEXT, ITEMS, CHARACTER,
  MOVE, and SYSTEM sections. Holding it opens a direct section chooser.
- Contextual command filtering keeps actions relevant to the current game
  state, while menu, inventory, map, question, and text-prompt modes expose
  touch-specific controls.
- Controller handedness can be changed from the mobile settings.
- Named commands resolve through IVAN's active command table, preserving its
  direction layout and custom bindings.
- Select and Back retain controller semantics in menus and prompts; Android
  Back maps to the same Back action.
- Tapping inside the game viewport is translated to IVAN's 800x600 mouse space.
- Text questions start Android's software keyboard and accept normal text input.

## Data, saves, and audio

Packaged game content is copied on first launch to the versioned app-internal
`files/data/<content-version>` directory. Saves and configuration use the
separate app-internal `files/user` directory, so they survive content upgrades
but are removed when the app is uninstalled. No broad storage permission is
requested.

Android music uses the checked-in Ogg Vorbis renders in `../Music`; the native
audio adapter substitutes `.ogg` whenever the game requests an original MIDI
track. See `../Music/ANDROID_AUDIO.md` for conversion and licensing details.

## Verification

Useful final checks are:

```powershell
.\gradlew.bat :app:lintDebug :app:assembleDebug :app:assembleRelease
adb shell pm clear io.github.harminoff.ivan
adb install -r app\build\outputs\apk\debug\app-debug.apk
adb shell am start -n io.github.harminoff.ivan/.IvanActivity
```

Runtime acceptance requires more than a successful build: launch on a real or
emulated Android device, tap `A` to enter the name prompt, enter text, start a
game, exercise all nine movement cells and all three action pages, rotate in both
directions while the game is running, background/resume it, and confirm that a
saved game can be continued after relaunch.
