# Android Back, Activity reentry, and description paragraphs

## Changes

- Android 13+ registers the platform `OnBackInvokedCallback` at default priority; older Android uses the Activity Back override. Both send the existing game cancel command instead of directly finishing the Activity. Native Android Back maps to Escape, which is understood by legacy numeric prompts as well as inventory and other menus.
- Display-density and font-scale changes are handled in place, refreshing safe insets/density rather than restarting the game.
- OS Activity shutdown no longer enters the interactive save/quit handler. `SDL_QUIT`/`SDL_APP_TERMINATING` unwind to `SDL_main`, clean up the game and renderer on the native thread, and return normally. This leaves existing checkpoints intact; it does not create an emergency save of an interrupted turn or award a score.
- The SDL Java bridge now serializes native-session ownership. A replacement Activity retires the previous native thread before resetting SDL's shared state. Late destroy, surface, focus, configuration, and queued-window callbacks from an obsolete Activity cannot tear down the new session. The SDL runner retains its owning Activity rather than finishing whichever instance happens to be current.
- Session cleanup tolerates no game/partial initialization/repeated cleanup, clears released pointers, zero-initializes session pointer arrays, and clears interrupted menu/question state.
- Long description prose receives blank-line paragraph breaks at sentence boundaries. Existing authored breaks, short text, wording, abbreviations, initials, decimal points, and closing quotes are preserved. Formatting occurs after crafting-description metadata is parsed so additional paragraphs cannot become false crafting requirements.
- God previews show separate alignment, prayer-history, and optional remembered-response sections without the duplicated fixed-width table row. The existing extra-information option and hidden-relation behavior are unchanged.
- Item-card titles have a height budget so a compact screen retains a scrollable description viewport.

## Verification

- Windows release build and all four CTests pass: adaptive layout, actual mobile-card rendering/input, start-mode model, and crash regression.
- Paragraph tests cover actual encrypted-scroll prose, authored line breaks, formatting idempotence, short text, initials/abbreviations, decimals/quotes, preserved content, and blank wrapped lines. Crafting tests keep the final description and requirements in their correct sections. God tests verify section layout and option-controlled remembered responses.
- Mobile-card renderer/input tests pass at 720x1600, 1600x720, and 480x800, including minimum body height, scrolling to the final text line, retained title/action pixels, and no accidental selection/prayer while scrolling.
- Signed Android arm64-v8a/x86_64 APK/AAB builds and lint pass: 0 errors, 51 warnings. The additional warning recommends `RequiresApi` instead of the guarded `TargetApi(33)` annotation; the project already uses the latter pattern.
- API 36 emulator, final candidate: existing save loaded, paragraph spacing inspected, system Back returned from inventory to gameplay without finishing the Activity, numeric-setting Back returned to Options, and Options Back returned to the main menu.
- Forced Activity finish/relaunch while inventory was open (`am start -R 2 -W`) passed in the same process (PID 5328). The log shows old native thread 5359 completing shutdown before new native thread 5423 starts; the old Activity's later `onDestroy` did not stop the new game. The existing checkpoint was subsequently loaded again.
- Repeated forced replacement (`am start -R 3 -W`) passed, exercising both populated and empty session cleanup. Main-menu Back followed its existing no-exit behavior, and a subsequent launcher intent brought the same responsive menu to the front without creating another instance.
- In-place size/density changes retained the active game. At 480x800, the final description line and weight were reached by swiping; title/actions stayed present. Earlier 1600x720 inspection confirmed the two description paragraphs fit in landscape. Font-scale changes also retained the session.
- Home/resume was tested separately. No IVAN ANR is recorded in the emulator's final `dumpsys activity lastanr` snapshot. Test emulator font-scale and developer settings were restored (1.0 and no always-finish override).
- No physical device was attached. Legacy Android Back and a real altar prayer remain manual acceptance checks. Synthetic edge swipes did not reliably invoke system navigation in this emulator; actual gesture-device acceptance remains separate from the verified Back key path and platform callback registration.

## Evidence

Screenshots and lifecycle reports are under `.codex-build-tmp/`:

- `back-complete-inventory.png`: final portrait description paragraphs.
- `back-final-landscape.png`: paragraph spacing in landscape (same paragraph logic, before final ownership/title refinements).
- `back-complete-return-gameplay.png`: system Back returned from inventory.
- `back-complete-number-prompt.png`, `back-complete-number-cancelled.png`: numeric prompt and Back result.
- `back-complete-recreated.png`: main menu after forced Activity replacement.
- `back-complete-compact-top.png`, `back-complete-compact-bottom.png`: small-screen detail scrolling and visible final text/weight.
- `back-complete-home-resume.png`: resumed gameplay.
- `back-complete-root-reopen.png`: responsive main menu after repeated replacement, root Back, and a subsequent launcher intent.
- `back-complete-lifecycle.log`, `back-complete-anr.txt`: Activity/thread ordering and ANR snapshot.

## Unpublished candidate

Version remains `0.59-android.8` / code 8. Earlier staged candidates were preserved. Final files are in `.codex-build-tmp/back-paragraph-release/`:

- `ivan-android-0.59-android.8-back-text-fixes.apk`
  - SHA-256: `F0AA19A1FFA50A6CC89C3E148FCE292C299A45B42D576339C4D7611C7BCD4E64`
- `ivan-android-0.59-android.8-back-text-fixes.aab`
  - SHA-256: `02A7B3E900B176BFCA5CC6D126461BBCA5C63DC992FDF3278A282390B0CB238F`
- Matching `native-debug-symbols.zip` and `mapping.txt`.
- APK signing certificate verified unchanged: `966db1ce3ec261589d8699192d371a8908a3a31959cb4a65e25ed9f71ba539cb`.

No commit, GitHub push, Play Console upload, or itch publication was performed.
The dedicated, headless test emulator was stopped after verification.

## Documentation used

Context7 was queried for Android Back/lifecycle guidance, but did not return the specific callback documentation. Implementation was checked against the bundled SDL2 bridge and the official Android references:

- https://developer.android.com/guide/navigation/custom-back/predictive-back-gesture
- https://developer.android.com/reference/android/window/OnBackInvokedDispatcher
- https://developer.android.com/guide/topics/resources/runtime-changes
