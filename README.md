# Twilight Princess HD on the Switch (experimental)

**IMPORTANT**
I have zero coding knowledge, seriously. This "port" is entirely vibecoded by Claude. The bulk of this is adapted from SwitchWakerHD and would not be possible without their work.
What's more, this is my first time ever putting something on GitHub. This project is largely proof of concept at this point and I have not manually tested anything past Ordon. If you run into any issues, feel free to report them on the issue tab of the GitHub, *however* I make no promises on when I will get to those issues or how long this repo will be maintained.


This is a fork of [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) v1.0.0. It runs the Wii U game
**The Legend of Zelda: Twilight Princess HD** natively on a Switch with custom firmware. It keeps SwitchWakerHD's
Switch side (deko3d renderer, libnx host, settings menu, build scripts) and points it at Twilight Princess HD: the
game's own code is recompiled from your dump on your PC. The system calls TP HD makes that Wind Waker HD never
did (Cafe OS, GX2, sound, files, amiibo) and several of its graphics fixes were adapted from
[twilight-princess-hd-recomp](https://github.com/AsadSi/twilight-princess-hd-recomp) (macOS).

**THIS IS VERY WIP.** On my Switch it boots, is able to start a new game and plays the
opening at Ordon at a steady 30 fps, on one screen with the touch screen as the GamePad's. Past the opening it is
untested.

**No game data is included.** This repository contains no code, asset or file of the game: you need your own dump,
and every build recompiles the game from it.

**No prepared shaders are included either (for now).** The console compiles each
of the game's shaders the first time it is needed, in the background while you play. So the first time you reach an area or see something new, you might
see the scene being drawn in: (objects and effects appearing a moment later than the rest). The game keeps running
smoothly while this happens. Compiled shaders are saved on the SD card, so it only happens once per shader and
settles down the more you play. Prepared shaders require me (or a tester) to actually make it to the areas in the game to be prepared and for those shaders to be used as template.

## What you need

- **Your own dump** of Twilight Princess HD, the **USA disc, title 00050000-1019E500, version 81 (Rev 2)**, as an
  extracted folder (`code/Zelda.rpx`, `content/`, `meta/`). The update and DLC are not supported.
- A Switch with Atmosphère and the Homebrew Menu, and about 5 GB free on the SD card.
- A Windows PC with about 10 GB free for the build.
- **Python 3** from [python.org](https://www.python.org/downloads/) (tick "Add python.exe to PATH" when
  installing). Optional: `pip install pillow` gives the homebrew the game's own icon.
- **devkitPro** from its [Windows installer](https://github.com/devkitPro/installer/releases) (choose the Switch
  development components), then in devkitPro's MSYS2 (Start menu: devkitPro > MSYS2) run:
  `pacman -S --needed switch-dev deko3d uam switch-lz4 switch-zlib switch-cmake cmake make`

## Build (Windows)

> **There is no installer or graphical front end.** Everything is done by running Python scripts in a terminal
> (PowerShell on Windows). SwitchWakerHD's `SwitchWakerHD-Installer.exe` does not work for this project: it
> downloads Wind Waker HD's release.

1. Download this project's source (**Source code (zip)** on the Releases page) and extract it to a folder with
   **no spaces** in its path, for example `C:\TPHD`: the build fails when the path has a space in it.
2. Open PowerShell in that folder (the one with `README.md` in it), for example with `cd C:\TPHD`.
3. Run, with the path to your dump:

```
python tools\switch\make_sd.py --game-dir "C:\path\to\your\Twilight Princess HD dump"
```

It checks the game version, translates `Zelda.rpx` to C (~5 minutes), builds the homebrew (20-40 minutes the first
time) and writes `build\sd\switch\tphd\`.

## Install

Copy the contents of `build\sd\` to the root of the SD card. You get `sdmc:/switch/tphd/tphd.nro` and
`sdmc:/switch/tphd/game/`. It is separate from SwitchWakerHD (`sdmc:/switch/wwhd/`), so both can be installed.
Boot into Atmosphère, hold **R** while starting any game (title mode), and pick **Twilight Princess HD**.

## HOME-screen icon (optional)

A forwarder is a small installable title that puts a **Twilight Princess HD** icon on the Switch HOME screen and
starts `sdmc:/switch/tphd/tphd.nro`, so you don't have to hold R over another game. The game works the same either
way.

What it needs, besides the build above:

- **Your own console's `prod.keys`**
- **Git** and **Pillow** (`pip install pillow`), and **gcc** in devkitPro's MSYS2 (`pacman -S gcc`).

Build it together with the game by adding `--keys` to the build command:

```
python tools\switch\make_sd.py --game-dir "C:\path\to\your\Twilight Princess HD dump" --keys "C:\path\to\prod.keys"
```

or on its own, after a build:

```
python tools\switch\forwarder\build_forwarder_windows.py --keys "C:\path\to\prod.keys" --sd build\sd
```

Either way it writes `build\sd\NSP\tphd_forwarder.nsp`. Copy it to the SD card and install it once with DBI or
Goldleaf. You only need to install it again to change its icon or name:
new builds of the game don't need a new forwarder. Its title ID is `01FF545048440000`, so it does not replace
Wind Waker HD's.

## What changed from SwitchWakerHD

- `tools/recomp`: the canonical build is TP HD (Zelda.rpx SHA-256 `28922198…12a9`). Wind Waker HD's hook lists are
  in `tools/recomp/ww/` and are not used. TP HD's own game hooks are in `tools/recomp/hooks.txt` (the sun's depth
  checks, the controller picker, Off-TV Play, Back to the title); the game runs at its own 30 fps.
- `runtime`: TP HD's extra imports (semaphores, condition variables, async FS, tiling apertures, depth-to-texture
  copies, nn_nfp, zlib, MCP, KPAD and its connect callback, …), the OSTime epoch, the sound DSP's TV/GamePad output.
- Off for TP HD: the story system, the update check, Prepare graphics and the first-start shader list (all
  Wind Waker HD specific). The settings menu has no Warp or Mods tab: those, the built-in mods and the cheats are
  Wind Waker HD's.
- Removed: SwitchWakerHD's Android app, Windows installer, guest-mod examples and GitHub workflows.
- Renderer: TP HD's rect lists drawn as quads, the shader decompiler's strict multiply (TP HD's terrain lighting
  is black without it), the R4_G4 texture format's channel order.
- One screen, as SwitchWakerHD (`runtime/src/tp_screen.cpp`): the controller acts as the Wii U GamePad, the game
  plays in Off-TV Play on the GamePad picture (at full resolution), and the console's touch screen is the GamePad's.
  The file select takes the GamePad without asking which controller you use, and Back on the Quest Log goes to the
  title screen (on the Wii U it goes back to that controller question, which has no way back). The settings menu's
  Dev tab (Show developer options) can switch to the Wii U's own GamePad or Pro Controller play instead.
- Shader prep reads TP HD's `content/Shaders.pack.gz`. There is no ready-made shader template: without
  `make_sd.py --shaders`, the console compiles the graphics as you meet them.
- SwitchWakerHD's own documentation (about Wind Waker HD) is not included here; see
  [its repository](https://github.com/centollOS/SwitchWakerHD) for how the build system and renderer work.

## Reporting a problem

Open an issue on this repository's **Issues** page. Say what you were doing and where in the game, and attach:

- the newest log file from `sdmc:/switch/tphd/logs/` on your SD card
- after a crash, the newest file in `sdmc:/atmosphere/crash_reports/`

## Credits

This port stands on other people's work:

- [SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD) by centollOS: the Switch port of Wind Waker HD this is
  forked from (deko3d renderer, Switch host, menus, build and forwarder tools).
- [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp) by Lukas S and contributors: the static
  recompiler and runtime both projects are built on.
- [twilight-princess-hd-recomp](https://github.com/AsadSi/twilight-princess-hd-recomp) by AsadSi: the Twilight
  Princess HD support (OS, graphics, sound, file and amiibo calls) ported here.
- [Cemu](https://github.com/cemu-project/Cemu), devkitPro, libnx and the other projects listed in
  [THIRD_PARTY.md](THIRD_PARTY.md).

The Legend of Zelda: Twilight Princess HD is © Nintendo. This project is not affiliated with or endorsed by Nintendo.

## License

[MPL-2.0](LICENSE), like the projects it is based on. Third-party code keeps its own license
([THIRD_PARTY.md](THIRD_PARTY.md)).
