# Android inventory and prayer information fixes

## Findings and changes

- Item card sizing wrapped descriptions at a wider width than the actual padded text area. The final lines could therefore disappear even when the card appeared to fit. Measurement now uses the same wrapping and padding as drawing.
- Long cards previously shrank to 1x text and still silently stopped drawing at the panel edge. Their body now scrolls at a readable minimum scale, with a swipe hint and position indicator. Descriptions, metrics/comparison, and crafting requirements remain reachable. Titles and action controls stay fixed.
- Detail scrolling is independent of the item grid. Ordinary redraws retain its position; choosing another item/god resets it. Swipes, including those ending over SELECT, never select/equip/pray.
- Prayer rows previously appended remembered responses to a long fixed-height label, but omitted them from the detail panel. The altar-only branch also ignored the extra-information option. Both paths now use the same god-information builder. Enhanced UI uses short god names and previews description, alignment/prayer history, and the optional last known response.
- The existing **Show extra info about gods when praying** setting still controls remembered responses. No hidden current relation is exposed, and previewing information does not change relation or consume a turn. The existing prayer confirmation remains unchanged.

## Verification

- Windows release build and all four CTests passed: `adaptiveui_layout_test`, `mobileui_card_test`, `startmode_model_test`, `crash_regression_test`.
- The new mobile-card test compiles the actual Android renderer/input implementation against a software SDL renderer, with Java Activity calls stubbed. It tests portrait and landscape, long descriptions/requirements/remembered responses, visible last-line pixels, independent scrolling, selection reset, fixed title/action pixels, safe preview, and clip restoration. It does not simulate Android Activity lifecycle or JNI.
- God-information tests verify the option off/on, unknown response wording, prayer history, and no hidden-relation exposure/mutation. The altar and ordinary chooser share the same entry builder.
- Android arm64-v8a/x86_64 signed APK/AAB builds and lint passed. Lint: 0 errors, 50 existing warnings.
- Android API 36 emulator: loaded the existing test save and visually verified the complete encrypted-scroll description in portrait and landscape. The test save is in wilderness, where praying is unavailable; a real altar prayer with extra information remains a manual acceptance check. No physical device was attached.
- Context7 returned SDL3 documentation for the SDL2 clipping queries; SDL2 behavior was verified against the bundled SDL2 source and the official SDL2 wiki instead. In particular, disjoint rectangle intersections must be replaced with an explicit empty clip before drawing.

## Candidate artifacts (not published)

The unpublished version remains `0.59-android.8` / version code 8. Earlier staged crash-fix artifacts were preserved. This candidate includes those baseline changes plus the text fixes:

- `.codex-build-tmp/ui-text-release/ivan-android-0.59-android.8-text-fixes.apk`
  - SHA-256: `3437AA885EA040AFFE90DE4240B5B68D468ADF99DC299C3B1F60E02D0A7D3E12`
- `.codex-build-tmp/ui-text-release/ivan-android-0.59-android.8-text-fixes.aab`
  - SHA-256: `E0223CC2621B3779053C85E92E7B229756D7E7A622CCBCA8C462EA23FDB796C9`
- Matching native debug symbols and R8 mapping are in the same directory.
- Verified APK signing certificate SHA-256: `966db1ce3ec261589d8699192d371a8908a3a31959cb4a65e25ed9f71ba539cb` (existing signing key).

No commit, push, Play Console upload, or itch publication was performed.

## Separate lifecycle observation

Follow-up implementation and verification: [Android Back and paragraph fixes](ANDROID_BACK_PARAGRAPH_FIXES_2026-10-08.md). The observation below records the earlier candidate, before the Back/Activity-ownership fixes.

During emulator testing, Android's system Back button closed the Activity while an inventory was open. Reopening stalled at the splash screen until a cold start. A later emulator density change, which can recreate an Activity, also left a blank screen and then an Android **isn't responding** dialog (captured in `.codex-build-tmp/ui-small-portrait-redraw.png`). This differs from the previously tested in-game/main-menu Quit path. SDLActivity's default system Back destroys the Activity and waits for the native thread; the game handles SDL_QUIT with a confirmation callback. This is a suspected shutdown/confirmation interaction, not a confirmed root cause. It was not changed as part of the two text fixes and needs investigation before publishing. The dedicated test emulator was stopped; no user/physical-device saves were changed.
