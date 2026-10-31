# Changelog

What changed, newest first. A change to the game hooks (`tools/recomp/hooks.txt`) needs the full
`make_sd.py` build; the others only need the homebrew rebuilt.

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