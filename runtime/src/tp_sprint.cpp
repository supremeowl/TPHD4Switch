// (TP HD fork) Hold A to sprint: an option of the Switch tab's Controls, off by default.
//
// A tap of A while Link runs still rolls. Each press of A starts a countdown in Link (+0x3166: 3, one less each frame,
// -1 when idle; 02063468), and the roll (0204D41C, when the A button's prompt, the do status 1014B62D, says Roll) starts
// when 0204D06C finds it at 0, three frames after the press. With this on, a press still held at that point doesn't
// roll yet: let go soon and it rolls then; keep holding (0.3 s from the press) and Link sprints until A or the stick
// lets go. A quick tap has let go by then, so it rolls as soon as it does without this. With "keep sprinting" on, the
// sprint goes on after A lets go until Link stops moving (procWait 0208D9D8), and a tap of A during it rolls.
//
// The sprint is made of the game's own parts. The animation is its scripted scenes' run: demo mode 38 (+0x618) makes
// setBlendMoveAnime (0202358C) pick animation 0x1C, dashb.bck, a loop with a longer stride, for the run's 0x13 (dashs +
// dasha). The speed follows the top speed +0x598, which checkNextAction (020419D0) sets each frame after using it: 23
// for a run, other values for heavy boots, shallow water and the like, which don't sprint. All of it runs in procMove
// (0208DBE8, daAlink_c's mProcID +0x305C = 4), Link on foot in control: Wolf Link, Epona and targeting have their own.
// A sword in hand goes away as the sprint starts, or when it goes on after a swing, with the game's own sheathing:
// 02024E74 plays waitatos on the upper body, and 0206E810 takes the sword away with its sound at frame 9.5.
#include <atomic>
#include <cstdint>

#include "interp.h"
#include "runtime.h"
#include "tp_sprint.h"

namespace {
constexpr uint32_t kProcId = 0x305C, kPressCountdown = 0x3166, kTopSpeed = 0x598, kStick = 0x3468, kEquipItem = 0x304C;
constexpr uint32_t kUpperAnime = 0x1FCC;
constexpr uint32_t kDemoType = 0x608, kDemoMode = 0x618;
constexpr uint16_t kProcMove = 4;
constexpr uint32_t kDemoSprintRun = 38;
constexpr uint16_t kEquipSword = 0x103;
constexpr uint16_t kSwordAnime = 597, kSwordAnimeHorse = 609;  // waitatos, waithatohs: the sword going away or coming out
constexpr uint32_t kPutAwayItem = 0x02024E74;  // (Link, 0)
constexpr uint32_t kEventRunning = 0x1014A795, kDoStatus = 0x1014B62D;
constexpr uint8_t kDoRoll = 0x79;
// "non ZR interact", the A button itself (InputMap_game.binput): {action, name, manager index s16, generation s16},
// registered by 02014878. The action's state byte +8: bit 0 held, bit 1 pressed, bit 2 released.
constexpr uint32_t kActionA = 0x10133ED4, kInputManagers = 0x1012D554;
constexpr double kRunSpeed = 23.0;
// the choices of the Controls tab; the stride alone gives 30 (thighs 77 degrees in dashb, 57 in dashs)
constexpr double kSprintSpeeds[tp_sprint::kSpeedChoices] = {30.0, 33.0, 34.5};
constexpr const char* kSpeedNames[tp_sprint::kSpeedChoices] = {"30  (1.3x)", "33  (1.43x)", "34.5  (1.5x)"};
constexpr int kHoldSteps = 9;          // logic steps (30 a second) of A held while running before the sprint
constexpr uint64_t kKeepSteps = 60;    // a sprint held through this long out of procMove (a hop, a turn) goes on
constexpr double kStartStick = 0.7, kKeepStick = 0.3;

std::atomic<bool> g_enabled{false}, g_keep{false};
std::atomic<int> g_speed_choice{1};
bool g_sprinting = false;
bool g_pending = false;   // a press that matured while held: rolls if A lets go before the sprint
int g_held_steps = 0;
uint64_t g_last_step = 0;  // the last logic step in procMove

bool a_held() {
    const uint32_t action = ld32(kActionA);
    if (action < 0x10000000u || action >= 0x50000000u) return false;
    const uint32_t manager = ld32(kInputManagers + 4 * uint32_t(int32_t(int16_t(ld16(kActionA + 8)))));
    if (!manager || ld32(manager + 0x10) != uint32_t(int32_t(int16_t(ld16(kActionA + 0xA))))) return false;  // not bound yet
    return ld8(action + 8) & 1;
}
// 02016794: in a scene, Link follows its script
bool in_event(uint32_t link) { return ld8(kEventRunning) || ld8(link + kDemoType); }

double sprint_speed() { return kSprintSpeeds[g_speed_choice.load(std::memory_order_relaxed)]; }

void stop(const char* why) {
    if (g_sprinting) LOG("[sprint] off (%s)", why);
    g_sprinting = false;
}
}  // namespace

namespace tp_sprint {
bool enabled() { return g_enabled.load(std::memory_order_relaxed); }
void set_enabled(bool on) {
    if (g_enabled.exchange(on) != on) LOG("[sprint] hold A to sprint %s", on ? "on" : "off");
}
bool keep_after_release() { return g_keep.load(std::memory_order_relaxed); }
void set_keep_after_release(bool on) {
    if (g_keep.exchange(on) != on) LOG("[sprint] keep sprinting after A %s", on ? "on" : "off");
}
int speed_choice() { return g_speed_choice.load(std::memory_order_relaxed); }
void set_speed_choice(int choice) {
    if (choice < 0 || choice >= kSpeedChoices) return;
    if (g_speed_choice.exchange(choice) != choice) LOG("[sprint] speed %.1f", kSprintSpeeds[choice]);
}
const char* speed_name(int choice) { return kSpeedNames[choice]; }
}  // namespace tp_sprint

// The roll's check: 1 when a press's countdown is at 0 (it takes the press). Its callers in the roll's own states and
// for other prompts go on as they are.
extern "C" void f_0204D06C_orig(Cpu* c);
extern "C" void hook_0204D06C(Cpu* c) {
    const uint32_t link = c->r[3];
    if (!tp_sprint::enabled() || ld16(link + kProcId) != kProcMove || ld8(kDoStatus) != kDoRoll || in_event(link)) {
        f_0204D06C_orig(c);
        return;
    }
    const bool held = a_held();
    if (int16_t(ld16(link + kPressCountdown)) == 0) {
        st16(link + kPressCountdown, 0xFFFF);
        g_pending = held;
        c->r[3] = held ? 0 : 1;
        return;
    }
    const bool roll = g_pending && !held;
    if (roll) g_pending = false;
    c->r[3] = roll ? 1 : 0;
}

// daAlink_c::procMove, the state of Link running, once per logic step
extern "C" void f_0208DBE8_orig(Cpu* c);
extern "C" void hook_0208DBE8(Cpu* c) {
    const uint32_t link = c->r[3];
    if (!tp_sprint::enabled()) {
        stop("turned off");
        g_pending = false;
        g_held_steps = 0;
        f_0208DBE8_orig(c);
        return;
    }
    const bool held = a_held(), keep = tp_sprint::keep_after_release();
    const uint64_t step = interp::logic_steps(), gap = step - g_last_step;
    g_last_step = step;
    if (gap > 1) {  // back in this state
        g_pending = false;
        g_held_steps = 0;
        if ((!held && !keep) || gap > kKeepSteps) stop("left the run");
    }
    const double stick = ldf32(link + kStick);
    if (in_event(link)) {
        stop("a scene");
        g_held_steps = 0;
    } else if (g_sprinting) {
        if (!held && !keep) stop("A let go");
        else if (stick < kKeepStick) stop("the stick let go");
        // a press during a kept sprint rolls only if it lets go soon
        else if (!held) g_held_steps = 0;
        else if (g_held_steps < kHoldSteps && ++g_held_steps == kHoldSteps) g_pending = false;
    } else if (held && stick >= kStartStick) {
        if (++g_held_steps >= kHoldSteps) {
            g_sprinting = true;
            g_pending = false;
            LOG("[sprint] on");
        }
    } else {
        g_held_steps = 0;
    }
    const uint16_t upper = ld16(link + kUpperAnime);
    if (g_sprinting && ld16(link + kEquipItem) == kEquipSword && upper != kSwordAnime && upper != kSwordAnimeHorse) {
        const Cpu saved = *c;
        guest_call(c, kPutAwayItem, {link, 0});
        *c = saved;
    }
    // for the speed: checkNextAction sets the run's top speed again after using this one
    const double speed = sprint_speed();
    const bool raised = g_sprinting && ldf32(link + kTopSpeed) == kRunSpeed;
    if (raised) stf32(link + kTopSpeed, speed);
    f_0208DBE8_orig(c);
    // (it can leave this state before it sets it)
    if (raised && ldf32(link + kTopSpeed) == speed) stf32(link + kTopSpeed, kRunSpeed);
    if (!held) g_pending = false;  // the roll's check has had its chance
}

// daAlink_c::setBlendMoveAnime(morf): the sprint's run, whose speed over the top speed picks how far into it the blend
// from walk to run goes. In here the demo mode is only compared with 38; what this calls compares it with 18 and 23.
extern "C" void f_0202358C_orig(Cpu* c);
extern "C" void hook_0202358C(Cpu* c) {
    const uint32_t link = c->r[3];
    if (!g_sprinting || ld16(link + kProcId) != kProcMove || ldf32(link + kTopSpeed) != kRunSpeed) {
        f_0202358C_orig(c);
        return;
    }
    const uint32_t demo_mode = ld32(link + kDemoMode);
    st32(link + kDemoMode, kDemoSprintRun);
    stf32(link + kTopSpeed, sprint_speed());
    f_0202358C_orig(c);
    st32(link + kDemoMode, demo_mode);
    stf32(link + kTopSpeed, kRunSpeed);
}

// daAlink_c::procWait, Link standing: a kept sprint ends here, not on his next run
extern "C" void f_0208D9D8_orig(Cpu* c);
extern "C" void hook_0208D9D8(Cpu* c) {
    if (g_sprinting && ldf32(c->r[3] + kStick) < kKeepStick) stop("Link stopped");
    f_0208D9D8_orig(c);
}
