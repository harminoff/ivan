# Third-party notices for IVAN Android

This file identifies the principal third-party components included in the
Android application. Full license texts are retained beside their source and
are also copied into the APK's `assets/licenses` directory where noted.

## IVAN and xBRZ

- Iter Vehemens ad Necem: Copyright 2001-2004 Timo Kiviluoto and subsequent
  contributors; GNU GPL version 2 or later. See `COPYING` and `LICENSING`.
- xbrzscale and xBRZ: Copyright Przemyslaw Grzywacz, Zenju, and contributors;
  GNU GPL version 3 or later. See `xbrzscale/License.txt` and source headers.
  Inclusion of this code makes the combined Android work GPLv3-or-later.

## Android native dependencies

- SDL 2.32.10: Copyright 1997-2025 Sam Lantinga; zlib license. See
  `android/vendor/SDL/LICENSE.txt`. SDL's bundled HID and YUV implementations
  retain their individual notices below `android/vendor/SDL/src`.
- SDL_mixer 2.8.2: Copyright 1997-2025 Sam Lantinga; zlib license. See
  `android/vendor/SDL_mixer/LICENSE.txt`.
- stb_vorbis: Copyright 2017 Sean Barrett; MIT license or public domain. The
  Android build uses the MIT option. See `android/licenses/STB-VORBIS-MIT.txt`.
- PCRE 8.45: Copyright 1997-2021 University of Cambridge, Philip Hazel,
  Zoltan Herczeg, Google Inc., and contributors; BSD 3-Clause style license.
  See `android/vendor/pcre/LICENCE`.
- libpng 1.6.50: Copyright the PNG Reference Library Authors and contributors;
  PNG Reference Library License version 2. See `android/vendor/libpng/LICENSE`.
- zlib 1.3.2: Copyright 1995-2026 Jean-loup Gailly and Mark Adler; zlib
  license. See `android/vendor/zlib/LICENSE`.
- LLVM libc++ shared runtime supplied by Android NDK 28.2.13676358: LLVM
  contributors; Apache-2.0 with LLVM Exceptions, with legacy libc++ portions
  also available under the included MIT terms. See
  `android/licenses/LLVM-LIBCXX-LICENSE.txt`.

## Other code and content

- FastNoise 0.4.1: Copyright 2017 Jordan Peck; MIT license. See
  `android/licenses/FASTNOISE-MIT.txt` and `FastNoise/FastNoise.h`.
- FeAudio code: Copyright 2004-2016 Adrian M. Gin; MIT license. See
  `android/licenses/FEAUDIO-MIT.txt` and source headers under `audio`.
- fantasyname: dedicated to the public domain under the Unlicense. See
  `fantasyname/UNLICENSE`.
- `Graphics/Font2.png`: derived from work by Neil Roy and distributed under
  Creative Commons Attribution-ShareAlike 3.0. Origin, attribution, and the
  license text are retained in `LICENSING`.
- Android Ogg music files are rendered adaptations of IVAN's original MIDI
  files. GeneralUser GS 2.0.3 was used as a production instrument bank but is
  not shipped. See `Music/ANDROID_AUDIO.md` and
  `Music/render-android-audio.ps1`.

No endorsement by any upstream author or project is implied.
