# Android music renders

The Android build uses the `.ogg` files in this directory because Android does
not provide the desktop MIDI output API used by IVAN. They were rendered from
the adjacent, original IVAN `.mid` files at 44.1 kHz with FluidSynth 2.5.7 and
encoded as Vorbis quality 5 with FFmpeg.

`render-android-audio.ps1` records the conversion commands. Run it with the
GeneralUser GS 2.0.3 SoundFont path to reproduce all Android music assets:

```powershell
.\render-android-audio.ps1 -SoundFont C:\path\to\GeneralUser-GS.sf2
```

The render instrument bank was GeneralUser GS 2.0.3 by S. Christian Collins.
Its license permits unrestricted use for music creation and full use of its
samples in music production, including commercial recordings. The SoundFont is
used only during the release preparation process and is not shipped in the APK.

- GeneralUser GS: https://github.com/mrbumpy409/GeneralUser-GS
- FluidSynth: https://github.com/FluidSynth/fluidsynth
- FFmpeg: https://ffmpeg.org/

FluidSynth substituted the closest available kit for a few missing percussion
bank programs while rendering. All eleven output files were then decoded with
`ffprobe`/FFmpeg as part of Android artifact verification.
