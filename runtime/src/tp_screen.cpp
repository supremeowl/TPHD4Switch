// (TP HD fork) The single-screen GamePad experience for Twilight Princess HD (screen_mode::single_screen, the
// default controller choice; Wind Waker HD's version, screen_mode.cpp, uses Wind Waker's addresses): the file
// select takes the GamePad without showing its controller picker, and play runs in Off-TV Play, the game on the
// GamePad picture, which the window shows. The Wii U GamePad and Pro Controller choices leave the game as it is.
//
// The controller picker (dFile_select_c; zelda_file_select_controller.blo, built at 0293B6CC, buttons "cont_1"
// and "cont_2" at +0x468/+0x46C). The file select's state is the byte at +0x2D2 (its functions in the table at
// 1011DFA8, 8 bytes each). 0293BE34 (at the file select's start) and 02944B50 (Back from the save list) start
// the picker's open animation (0293BDDC(this, from 10, to 19); frame 10 is closed) and set state 8, 02943528,
// which waits for the animations and then state 9, 029435EC, which waits for the pick (+0x479: 0 GamePad,
// 1 Pro Controller). The GamePad's pick: 02008990(controller manager *100D3B94, 3), the window closed
// (19 -> 10) and state 0x0B, 02943CEC, which waits for that and returns to state 0, the save list. The hook on
// state 8 does the same at once: the mode, the window left closed (10 -> 10) and state 0x0B.
//
// Off-TV Play: the frame's end (02BDE770) copies the TV color buffer (102816D8, 1080p) to the GamePad alone
// while the byte at 1012C8D9 is set (unless the frame drew the GamePad's own screen), set from 1012C8D8 at the
// end of the frame by 02BDE9A4(on). The game's own toggle (02AF0C58, controller mode 3 only) calls 02BDE9A4(on)
// and 02B06D38(on) (the GamePad's side of it); the mode setter 02008990 calls both with 0 when leaving mode 3.
// Here, entering mode 3 calls both with 1 and the window shows the GamePad picture (which the renderer takes from
// the TV buffer it was copied from, so at full resolution); outside the mode setter, a 0 becomes 1, so the
// game's toggle can't leave Off-TV Play (one screen: the TV picture isn't drawn there).
#include <cstdint>

#include "input.h"
#include "runtime.h"
#include "screen_mode.h"
#ifdef __SWITCH__
#include "gfx/switch_renderer.h"
#endif

namespace {
constexpr uint32_t kControllerMgr = 0x100D3B94;  // the controller manager (HD's input sources), *
constexpr uint32_t kSetMode = 0x02008990, kModeGamePad = 3;
constexpr uint32_t kSetOffTv = 0x02BDE9A4, kOffTvSide = 0x02B06D38;
constexpr uint32_t kFileSelectAnims = 0x0293D100, kPickerAnim = 0x0293BDDC;

bool gamepad_mode() {
#ifdef __SWITCH__  // the desktop builds show the GamePad screen beside the TV picture and keep the game's choice
    return screen_mode::single_screen() && !input::pro_controller();
#else
    return false;
#endif
}
bool g_in_set_mode = false;  // inside the mode setter: its own Off-TV off when leaving mode 3 goes through
uint32_t g_back_fs = 0;       // the file select whose Back is taking the game to the title (done with it)
bool g_back_to_title = false; // asked for, not yet passed to the scene
}  // namespace

// file select state 8: the picker opening
extern "C" void f_02943528_orig(Cpu* c);
extern "C" void hook_02943528(Cpu* c) {
    g_back_fs = 0;  // a new file select (one may be made again at the same address)
    if (!gamepad_mode()) { f_02943528_orig(c); return; }
    const Cpu saved = *c;
    const uint32_t fs = c->r[3];
    const bool ready = guest_call(c, kFileSelectAnims, {fs}) != 0;  // the file select's own animations, as state 8 waits for
    if (ready) {
        guest_call(c, kSetMode, {ld32(kControllerMgr), kModeGamePad});
        st8(fs + 0x478, 1);
        st8(fs + 0x479, 0);                        // the GamePad
        guest_call(c, kPickerAnim, {fs, 10, 10});  // the window stays closed
        st8(fs + 0x2D2, 0x0B);                     // closes (at once) and returns to the save list
        LOG("[screen] file select: the GamePad taken as chosen (no controller picker)");
    }
    *c = saved;
}

// Back on the save list (the Quest Log). The game goes back to the controller picker (state 0x15, 02944B50: its
// animations, then 02008990(mgr, 5), the title's mode, and state 8), which has no Back of its own: on the Wii U
// the title can't be reached again from the file select. Without the picker, Back goes to the title instead:
// after the same animations and mode, the file select scene (below) asks for the title, as a reset does.
extern "C" void f_02944B50_orig(Cpu* c);
extern "C" void hook_02944B50(Cpu* c) {
    if (!gamepad_mode()) { f_02944B50_orig(c); return; }
    const uint32_t fs = c->r[3];
    if (fs == g_back_fs) return;  // already on its way to the title: nothing more until the scene changes
    const Cpu saved = *c;
    const bool ready = guest_call(c, kFileSelectAnims, {fs}) != 0 && guest_call(c, 0x0293D55C, {fs}) != 0;
    if (ready) {
        guest_call(c, kSetMode, {ld32(kControllerMgr), 5});
        g_back_fs = fs;
        g_back_to_title = true;
        LOG("[screen] file select: Back on the save list goes to the title");
    }
    *c = saved;
}

// the file select scene (dScnName), each frame while its file select runs (r3 the scene): the title asked for with
// dComIfG_changeOpeningScene(scene, 0xC, 30) (02904CA0: next stage F_SP102, the scene change; as its other
// callers, the game's resets, call it)
extern "C" void f_02AC0390_orig(Cpu* c);
extern "C" void hook_02AC0390(Cpu* c) {
    if (!g_back_to_title) { f_02AC0390_orig(c); return; }
    g_back_to_title = false;
    const Cpu saved = *c;
    guest_call(c, 0x02904CA0, {c->r[3], 0xC, 0x1E});
    *c = saved;
}

// the controller manager's mode setter (mgr, mode): mode 3 is the GamePad's play
extern "C" void f_02008990_orig(Cpu* c);
extern "C" void hook_02008990(Cpu* c) {
    const uint32_t mode = c->r[4];
    g_in_set_mode = true;
    f_02008990_orig(c);
    g_in_set_mode = false;
    if (!gamepad_mode()) return;
    const bool off_tv = mode == kModeGamePad;
#ifdef __SWITCH__
    gfxsw::set_gamepad_view(off_tv);  // the title and the file select (other modes) show the TV picture
#endif
    if (!off_tv) return;
    const Cpu saved = *c;
    guest_call(c, kSetOffTv, {1});
    guest_call(c, kOffTvSide, {1});
    *c = saved;
    LOG("[screen] GamePad play: Off-TV Play");
}

// Off-TV Play on (1) / off (0), and its GamePad side: kept on in GamePad mode, except by the mode setter
extern "C" void f_02BDE9A4_orig(Cpu* c);
extern "C" void hook_02BDE9A4(Cpu* c) {
    if (gamepad_mode() && !g_in_set_mode && c->r[3] == 0 && ld32(ld32(kControllerMgr) + 0x14) == kModeGamePad) c->r[3] = 1;
    f_02BDE9A4_orig(c);
}
extern "C" void f_02B06D38_orig(Cpu* c);
extern "C" void hook_02B06D38(Cpu* c) {
    if (gamepad_mode() && !g_in_set_mode && c->r[3] == 0 && ld32(ld32(kControllerMgr) + 0x14) == kModeGamePad) c->r[3] = 1;
    f_02B06D38_orig(c);
}
