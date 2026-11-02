# Changelog

What changed, newest first. A change to the game hooks (`tools/recomp/hooks.txt`) needs the full
`make_sd.py` build; the others only need the homebrew rebuilt.

## 0.2.0 (2026-11-01)

- **Hold A to sprint** (Controls tab, off by default): a tap still rolls. Link puts his sword away as he starts
  running. Options to keep sprinting after letting go of A, and a choice of three speeds.
- **Docked item menu**: when docked, + opens the item screen that works with buttons, since there's no touch screen
  on the TV. 
- **Capture button video** (Controls tab, off by default): when the game is started from the HOME screen icon.
- **Clearer defaults for each screen**: docked now starts at 1x (1920x1080), handheld at 0.67x (1280x720). The Picture
  tab shows Twilight Princess HD's real sizes, resolution is a dropdown with a new 0.5x choice, and dynamic resolution
  can go down to 0.5x.
- **FPS counter**: counts only the frames actually shown on screen.
- `make_sd.py --wua` finds the game, and `--help` no longer lists Wind Waker's options.
- Rebuilds pick up header changes, and a source zip unzipped over an older one rebuilds cleanly.






## 0.1.0 (2026-10-30)

- Twilight Princess HD (USA, v81) on SwitchWakerHD v1.0.0's Switch runtime
- TP HD's extra system calls (Cafe OS, GX2, sound, files, amiibo) and several graphics fixes, adapted from
  [twilight-princess-hd-recomp](https://github.com/AsadSi/twilight-princess-hd-recomp).
- Graphics: terrain lighting (strict multiply), rect lists drawn as quads, the R4_G4 texture format (water
  ripples), and the sun's lens flare no longer darkening the screen (its depth checks answered from the GPU).
- **One-screen GamePad play**, as SwitchWakerHD: the controller acts as the Wii U GamePad and the game plays in
  Off-TV Play, its picture filling the screen at full resolution, with the console's touch screen as the GamePad's.
- **No controller question**: the file select takes the GamePad without asking which controller you use.
- **Back to the title**: Back on the Quest Log goes to the title screen. (On the Wii U it goes back to the
  controller question, which has no way back.)
- Shader prep reads TP HD's own shader archive.