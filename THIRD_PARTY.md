# Third-party code

This project (Twilight Princess HD on the Switch) contains, builds against, or fetches at build time
the components below. Nothing in this list is game data: the repository contains no code, asset or
data file of the game, and every build recompiles the game from the player's own dump.

The list and its pins come from SwitchWakerHD v1.0.0, whose licenses were checked against each
project's LICENSE file and, for packages of the devkitPro image, the installed package (2026-10-07).

## Where this project's own code comes from

All MPL-2.0 (`LICENSE`), each building on the one before:

| Project | What it gave this project |
|---|---|
| [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) (Lukas S and contributors), up to v0.2.2 | the static recompiler (`tools/recomp`) and the runtime the rest is built on |
| [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) v1.0.0 (centollOS) | the Switch port: the deko3d renderer, the libnx host, the settings menu, the build, shader and forwarder tools |
| [twilight-princess-hd-recomp](https://github.com/AsadSi/twilight-princess-hd-recomp) (AsadSi) | Twilight Princess HD's OS, graphics, sound, file and amiibo calls; the sun's depth peeks (`runtime/src/peekz.cpp`); reading `Shaders.pack.gz` (`tools/shaderprep.py`) |

This project's own changes are marked "(TP HD)" or "(TP HD fork)" in the source.

## In this repository

| Path | Origin | License |
|---|---|---|
| everything not listed below | the projects above and this project's changes | MPL-2.0 (`LICENSE`) |
| `runtime/third_party/cemu/` | [Cemu](https://github.com/cemu-project/Cemu): GPU address library, shader decompiler, reference structures; `runtime/src/espresso_fp.c` and parts of the OS layer follow Cemu as noted in those files | MPL-2.0 (`runtime/third_party/cemu/LICENSE.txt`) |
| `runtime/third_party/uam/` | [uam](https://github.com/devkitPro/uam) 1.1.0 at `5a5afc2` (fincs, devkitPro), with this repository's patches (`PATCHES.md`) | zlib for uam's own files, MIT for `mesa-imported/` (from Mesa) (`runtime/third_party/uam/LICENSE`) |
| `runtime/third_party/imgui/` | [Dear ImGui](https://github.com/ocornut/imgui) v1.92.9b (Omar Cornut and contributors) | MIT |
| `runtime/third_party/fmt/` | [{fmt}](https://github.com/fmtlib/fmt) | MIT |
| `runtime/third_party/xxhash/` | [xxHash](https://github.com/Cyan4973/xxHash) (Yann Collet) | BSD-2-Clause |
| `runtime/third_party/metal-cpp/` | [metal-cpp](https://developer.apple.com/metal/cpp/) (Apple; macOS builds only) | Apache-2.0 |
| `tools/wuaextract.py` | Written from the format of [ZArchive](https://github.com/Exzap/ZArchive) (Exzap); nothing of it is vendored | project's (MPL-2.0); format: MIT No Attribution |
| `tools/switch/forwarder/nx-hbloader-forwarder.patch` | Patch to nx-hbloader | same as nx-hbloader (ISC) |

## Fetched at build time (not in the repository)

| Component | Pin | Used for | License |
|---|---|---|---|
| [devkitA64](https://devkitpro.org), [libnx](https://github.com/switchbrew/libnx) | `devkitpro/devkita64:latest` image (libnx 4.12.0) | Switch toolchain and runtime | ISC (libnx); toolchain runtime libraries under their own licenses |
| [deko3d](https://github.com/devkitPro/deko3d) | the devkitPro image's (0.5.0) | Switch GPU API | Zlib |
| [nx-hbloader](https://github.com/switchbrew/nx-hbloader) | v2.4.5 (`82b9512`) | HOME-menu forwarder (optional) | ISC |
| [hacBrewPack](https://github.com/TooTallNate/hacBrewPack) | v3.05 (`745b16e`) | packs the forwarder NSP (build tool only, not shipped) | GPL-2.0 |
| [hactool](https://github.com/SciresM/hactool) | 1.4.0, devkitPro package | verifies the forwarder NSP (build tool only) | ISC |
| [SDL 3](https://github.com/libsdl-org/SDL) | 3.4.18 where not installed (desktop) | windows, input, audio (Linux, Windows, Android) | Zlib |
| [glslang](https://github.com/KhronosGroup/glslang) | 16.0.0 where not installed (Windows) | Vulkan shader compiler (desktop) | BSD-3-Clause (core), with parts under BSD-2-Clause, MIT and Apache-2.0 (its `LICENSE.txt`) |
| [zlib](https://github.com/madler/zlib), [LZ4](https://github.com/lz4/lz4) | 1.3.1, 1.10.0 where not installed (Windows) | compression | Zlib; BSD-2-Clause |
| [zstd](https://github.com/facebook/zstd) | 1.5.7 (release builds; URL and SHA-256 in `cmake/Zstd.cmake`) | `.wua` archives in the extractor | BSD-3-Clause |

The Switch build links lz4 1.9.3 from the devkitPro portlibs (the library is BSD-2-Clause; only its command-line tools are GPL-2.0).

## Binaries

Builds link only code under the licenses above (MPL-2.0, permissive licenses), no GPL code. A
build contains the game's code recompiled from the player's dump and is for the player's own use
only.
