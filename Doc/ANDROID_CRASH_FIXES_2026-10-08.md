# Android crash fixes - 0.59-android.8

Implemented against the existing working tree without resetting or discarding
tracked or untracked user changes. Not committed, pushed, or published.

## Play Console issues addressed

The reviewed production reports were from version code 7 / 0.59-android.7.

| Cluster | Observed failure | Fix |
| --- | --- | --- |
| `39a65995361e9c57cda89434fe34111f` | Inventory quick-equip reaches `gearslot::PutInItem` through a missing body part. | Shared slot validation checks the body part, restrictions, and incoming item before removing either item. |
| `f8478c933fbb7bbba37962314eb22aa2` | Pickup quick-equip takes the same invalid equipment path. | Uses the shared validator and chooses an available limb before treating an empty slot as usable. |
| `f9a54314a57ca8c4def6cd7ffd551235` | Duplicate configuration registration aborts startup; exit then reaches SDL EGL texture cleanup. | Rebuild the configuration registry each session; initialize process-owned tables only once; perform Android graphics/audio cleanup explicitly on the SDL thread. |

The equipment crash instruction in the matching arm64 binary was the first
store in `gearslot::PutInItem` (`str x1, [x0, #8]` at `0x5cec60`). This supports
the invalid destination-slot diagnosis, rather than a bad item payload.

The startup fix also clears renderer-owned snapshots, mobile touch timers,
controller handles, and queued input. Graphics shutdown is repeat-safe and
nulls released pointers. CPU-side scaled-view regions retain their stable IDs
but are rebound to the next session's framebuffer. The latter was added after
the first emulator gameplay-after-relaunch test caught a stale-buffer abort.

SDL documentation cautions against `atexit(SDL_Quit)` with a dynamically loaded
SDL library, and states that destroying a renderer frees its associated
textures. These informed the explicit cleanup and cache lifetime changes:
[SDL_Quit](https://wiki.libsdl.org/SDL2/SDL_Quit),
[SDL_DestroyRenderer](https://wiki.libsdl.org/SDL2/SDL_DestroyRenderer).

## Verification

- Windows release build succeeded. CTest: 3/3 passed (`adaptiveui_layout_test`,
  `startmode_model_test`, `crash_regression_test`).
- New regression coverage: missing right/left/both limbs for weapons, rings,
  gloves and boots; missing head; invalid and forbidden slots; rejected incoming
  equipment; cursed outgoing equipment; unchanged items on failed validation;
  configuration re-registration and serialization; three graphics
  initialization/cleanup cycles; snapshot cleanup; scaled-region rebinding;
  stale queued-input cleanup.
- Android `assembleRelease`, `bundleRelease`, and `lintRelease` succeeded.
  Lint reports 0 errors and 50 warnings; this is not a warning-free build.
- Final signed APK installed on a dedicated Android 16 / API 36 x86_64 emulator.
  Three native `SDL_main` sessions ran in the same process (PID 9280), with two
  successful main-menu Quit / Activity-finish cycles. After the first relaunch,
  character creation, story, gameplay, inventory, and save-and-quit succeeded.
  Final runtime log contains no IVAN abort, native fatal signal, or Java crash.
- No physical Redmi Pad 2 or OnePlus 15 was attached. Their original production
  failures have not been reproduced and retested on those devices. Missing-limb
  cases are automated native regression tests, not an Android gameplay replay.
- APK signature verifies with the existing release certificate, SHA-256
  `966db1ce3ec261589d8699192d371a8908a3a31959cb4a65e25ed9f71ba539cb`.
  Package `io.github.harminoff.ivan`, version code 8, min SDK 26, target SDK 36.

Evidence is under `.codex-build-tmp/`: `crash-fix-android-verified.log`,
`crash-fix-windows-verified.log`, `crash-fix-android-runtime.log`, and
`crash-verified-{menu,gameplay,inventory,after-save,third-menu}.png`.
CTest details are in `build-win32-native-release/Testing/Temporary/LastTest.log`.

## Staged release artifacts

These are local staging files, not published itch.io or Play Console uploads.

| Artifact | Relative path | SHA-256 |
| --- | --- | --- |
| Signed APK | `release/itch/builds/ivan-android-0.59-android.8.apk` | `84E0AD49E5E6352493779EC36EC20FB05407009D4C2362DA2573663D30E22D76` |
| Play bundle | `release/itch/upload/0.59-android.8/ivan-android-0.59-android.8.aab` | `54067ACA4F0F7DAB7E172A330B541595344F8A2D40D6AC02FA25BD29990AEBB6` |
| Native symbols | `release/itch/upload/0.59-android.8/native-debug-symbols.zip` | `6557DD94631AA8C0FF1D891C114625E09308A21756A13D926E82DC4790F1DED9` |
| R8 mapping | `release/itch/upload/0.59-android.8/mapping.txt` | `50322B871814D3D7D3D94C86E85C8EA08BF7A536D5DF3939FC9CB655C0FB5C61` |

Before publication, spot-check a physical arm64 device, especially equipment
after limb loss and Quit/reopen after gameplay. Upload the matching symbols and
mapping with this bundle. Confirm declining event rates in Play Console after
distribution; local tests cannot establish that production clusters are gone.

## Suggested player release notes

- Fixed crashes when equipping items after losing a limb.
- Improved stability when closing and reopening the game.
