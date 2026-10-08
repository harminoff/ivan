# Readability and Stability release - 2026-10-08

## Artifacts

Prepared under `.codex-build-tmp/release-2026-10-08/artifacts/`:

| Artifact | SHA-256 |
| --- | --- |
| `ivan-windows-0.59-readability-stability.zip` | `64BC2C1C053A0856CDDD03882AE4786BDE9660A720A884434AE03D358EF5C7FB` |
| `ivan-android-0.59-android.8.apk` | `F0AA19A1FFA50A6CC89C3E148FCE292C299A45B42D576339C4D7611C7BCD4E64` |
| `ivan-android-0.59-android.8.aab` | `02A7B3E900B176BFCA5CC6D126461BBCA5C63DC992FDF3278A282390B0CB238F` |

The Android APK retains the existing signing certificate SHA-256:
`966db1ce3ec261589d8699192d371a8908a3a31959cb4a65e25ed9f71ba539cb`.
Matching `mapping.txt` and `native-debug-symbols.zip` accompany the bundle.

## Scope

- Shared desktop/Android description paragraphs and clearer god previews.
- Android item and prayer-detail scrolling, compact title layout, Back handling,
  native-session ownership, and Activity shutdown/reentry fixes.
- Shared quick-equip validation for missing limbs and unusable equipment slots.
- Include previously unpublished Origins source and regression tests so the
  repository reproduces the supplied packages.
- Fix Windows runtime DLL staging and package only installed assets/binaries,
  with update instructions and license notices. No user saves/settings included.

## Verification

- Windows release build and all four CTests passed on this release turn.
- Extracted the final Windows ZIP to a fresh verification directory; its 296
  entries contain no Save/Bones/Scrshot folders, local configuration, keystore,
  question history, or runtime logs.
- Extracted executable passed `--version`, `--startmode-test`, and
  `--crash-regression-test` with PATH limited to Windows system directories.
  This verifies the bundled DLLs without relying on the installed MinGW PATH.
- APK signature and APK/AAB hashes reverified this turn. Android runtime and
  lint evidence is documented in `ANDROID_BACK_PARAGRAPH_FIXES_2026-10-08.md`.
- No new desktop visual acceptance capture in this turn. Physical gesture/legacy
  Android and real-altar prayer checks remain separate acceptance gaps.

## Publication

GitHub release branch: `codex/readability-stability-update`, targeting the
`master` branch of `harminoff/ivan` (not upstream `Attnam/ivan`).

At preparation time, itch offered Android `.7` and Windows
`ivan-windows-0.59-crafting-and-tiles.1.zip`. Play production and the existing
Alpha/Layout Review closed tracks were `.7`. Open testing had a paused `.7`
release and its existing country availability. The requested destination is
Open testing (beta), without changing production or existing closed testers.

- GitHub commit `da5a0df` pushed; PR #1 is open at
  https://github.com/harminoff/ivan/pull/1, with the verified fork head/base.
- Play accepted the signed `.8` bundle and confirmed that its ReTrace mapping
  file and native debug symbols are attached. Supported-device counts are
  unchanged from `.7`.
- Saved a 100% Open testing rollout and the resumption of the paused beta track;
  submitted exactly these two changes. Initial publishing overview showed
  **Changes in review** while quick checks ran. Managed publishing remained off.
- Follow-up Console verification on 2026-10-08 confirms the beta track is
  **Active**, latest release **8 (0.59-android.8)**, **Available to unlimited
  testers**, released October 8 at 2:49 PM as displayed by Console. The track
  retains its 173 countries / regions. Publishing overview reports the update
  published; store visibility may take longer to propagate. Evidence retained
  as `play-beta-live.jpg` alongside the release artifacts.
  Production and Alpha/Layout Review were not changed, nor were tester
  membership or country settings.
- itch downloads and player-facing devlogs published on 2026-10-08:
  - Android: `ivan-android-0.59-android.8.apk`, marked Android and publicly
    visible at https://harminoff.itch.io/ivan-android-unofficial.
  - Windows: `ivan-windows-0.59-readability-stability.zip`, marked Windows and
    publicly visible at https://harminoff.itch.io/ivan-desktop-enhanced.
  - Previous downloads remain retained but hidden; no files were deleted and
    project pricing, screenshots, and other listing settings were preserved.
  - Both public download sections show only the new package and link to the
    published Readability and Stability Update. Each devlog attaches its new
    package and includes the player-facing fixes, Windows Origins, and update
    instructions:
    - https://harminoff.itch.io/ivan-android-unofficial/devlog/1697902/readability-and-stability-update
    - https://harminoff.itch.io/ivan-desktop-enhanced/devlog/1697905/readability-and-stability-update
  - Public-page screenshots retained alongside the release artifacts as
    `itch-android-published.jpg` and `itch-windows-published.jpg`.

Player notes: `release/itch/devlog-readability-and-stability.md`.
Play notes: `release/itch/play-beta-release-notes.txt`.
