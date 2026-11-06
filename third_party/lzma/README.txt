LZMA SDK (7z decoder + LZMA encoder) - C sources
================================================

Source : 7-Zip 23.01 source package, folder "C/"
         https://www.7-zip.org/a/7z2301-src.tar.xz
         SHA-256 356071007360e5a1824d9904993e8b2480b51b570e8c9faf7c0f58ebe4bf9f74
Author : Igor Pavlov
License: Public domain (see the header of every file: "Public domain").

Only the files needed to READ 7z archives are included (7zArcIn, 7zDec,
LzmaDec, Lzma2Dec, BCJ/BCJ2/Delta filters, CRC). They are not modified.

Why: X-Plane 11 stores its DSF scenery files as 7z archives. The plugin
reads the installed DSF files itself to draw the ground (src/data/SevenZip.*).

Added 2026-11-06 (terrain stage 1), same package, same version, not modified:
  LzmaEnc.c, LzmaEnc.h  - LZMA encoder
  LzFind.c, LzFind.h    - match finder used by the encoder
  LzFindOpt.c           - optimized match finder helpers (used by LzFind.c)
  LzHash.h              - hash constants for the match finder

Why: the terrain cache keeps the full-resolution DSF elevation (DEM, 1201 x
1201 int16 per tile) compressed on disk (row delta + LZMA, src/data/
ElevationCodec.*). The encoder is built single-threaded: CMake defines Z7_ST
for lzma_sdk, so the multi-threaded match finder (LzFindMt, Threads) is not
needed and not included.
