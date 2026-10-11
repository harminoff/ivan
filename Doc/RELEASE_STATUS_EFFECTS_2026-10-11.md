# Android Status Effects release - 2026-10-11

## Scope

- Main-screen Android visible conditions, fed by the existing game state.
- Severity colors, urgent-first order and turn-free overflow pagination.
- Permanently reserved space under stats: portrait top-left, landscape top-right
  of the map column, without covering tiles or the controller rail.
- Map, controls, stats, toggle and log geometry remain independent of condition
  count, wrapping, paging or clearing. Long labels cannot spill out of their chip.
- Shared severity colors also update the existing enhanced desktop equipment
  conditions; this release does not change desktop HUD layout or ship a Windows build.

## Verification

- All five Windows CTests passed for the final HUD code.
- Android signed preview built for arm64-v8a and x86_64 and was installed on the
  dedicated API 36 emulator, preserving the existing release certificate.
- Actual gameplay screenshots captured a separate Burdened preview character
  in both orientations. No physical phone deployment was performed.
- Regression tests cover severity, colored pixels, fixed geometry across empty,
  single, wrapped and 32-effect paged states, both landscape controller sides,
  minimum sizes, cutouts, text containment and input without game commands.

## Release artifacts and publication

Release version: `0.59-android.9`, version code `9`.
Package: `io.github.harminoff.ivan`.
Signing certificate SHA-256:
`966db1ce3ec261589d8699192d371a8908a3a31959cb4a65e25ed9f71ba539cb`.

Artifacts are preserved under `.codex-build-tmp/release-2026-10-11/artifacts/`:

| File | SHA-256 |
| --- | --- |
| `ivan-android-0.59-android.9.apk` | `E4CBDCD435E2AC4DE88C22C17C2106EF3F31E36676FCF5F7075C2B0D00A67A0F` |
| `ivan-android-0.59-android.9.aab` | `524C91D602A0D579677B0BB47512746CFE9CE95D59C66E2ACA51BFB023615253` |
| `mapping.txt` | `FC62A8932271E1FE08C0B74A317BB3956734DE8BF79880E2576F1F3FA973A73E` |
| `native-debug-symbols.zip` | `EBBFACECA574AD3E2AE390894914ACE7375AAD84249658604C50E0B99AFCE305` |

The version-9 APK/AAB build and release lint passed. APK signature and bundle
signature verified. Both packaged native gameplay libraries are byte-identical
to the approved emulator-tested HUD preview. The bundle embeds its ReTrace map
and native debug metadata.

Live Console inspection confirmed that the requesting user's separate tester
list is on **Closed testing - Layout Review**, serving version 8 before this
update. This is the requested testing destination, not the separate Open testing
track (also serving version 8).

Production releases, tester lists, country availability, pricing and store
screenshots are out of scope and must remain unchanged.

## Publication

- itch: `.9` is publicly visible and marked Android at
  https://harminoff.itch.io/ivan-android-unofficial. The previous `.8` file is
  retained but hidden; older files remain hidden. No files were deleted.
- The feedback-focused devlog is **Published**, with comments enabled, the `.9`
  APK attached, and both portrait and landscape gameplay screenshots:
  https://harminoff.itch.io/ivan-android-unofficial/devlog/1702564/status-effects-update-tell-us-what-you-think.
  Verified the public body, feedback questions, attachments, and download list.
- Google Play: accepted the `.9` bundle with ReTrace mapping and native debug
  symbols. Supported-device counts are unchanged. Saved a 100% rollout and
  submitted exactly **one** change: **Closed testing - Layout Review**, release
  `9 (0.59-android.9)`. Publishing overview confirms **Changes in review**, with
  quick checks running and managed publishing off. This is submitted, not yet
  approved or live; do not claim that testers can already install it.
- Production `.7`, Alpha `.7`, Open testing `.8`, and all tester lists and country
  settings were left unchanged.
- Browser screenshots are retained as `play-layout-review-submitted.jpg` and
  `itch-status-effects-devlog-published.jpg` under the release evidence directory.
- GitHub destination is the verified `master` branch of `harminoff/ivan`, not
  upstream `Attnam/ivan`. Only this update's source, tests, version, notes and
  gameplay screenshots are included; unrelated local files remain untouched.

Player devlog: `release/itch/devlog-status-effects.md`.
Play notes: `release/itch/play-status-effects-release-notes.txt`.
