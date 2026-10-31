// (TP HD fork) TP HD's depth peeks (dPeekZ_c, the GameCube's GXPeekZ), adapted from
// AsadSi/twilight-princess-hd-recomp e5d1bb5. The sun's lens flare fades with them when something is in front of
// it: the game queues screen points, and once a frame peekData (029196A0) reads each point's depth. HD reads it
// from a CPU-linear view (tiling aperture) of the depth buffer converted to a texture (02BD5C50), which this
// renderer never writes back to guest memory, so the game read depths that have nothing to do with the scene.
// The hook answers each point from the latest GPU readback of that pixel instead (the far plane until there is
// one) and has the renderer read the points again (OP_PEEK_Z). An answer is kept by the result address that
// asked (the sun asks with the same ones every frame, from points that move a little), so it is a frame or two
// old, as HD's own aperture read is. It is written here, when that address asks again, never from the GPU's
// completion: an object that asked may be gone by then.
//
// dPeekZ_c: u8 count at +0, then cells of { s16 x, s16 y, u32* result } from +4, in the GameCube's 640x456
// screen; 02BD5C50 maps them to the depth buffer, mirrored in x when the byte at 1012C8E3 is set (Hero Mode).
// OP_PEEK_Z: the converted depth image's address, then (x, y, result address) per point, x and y in 1/65536ths
// of the image (gfx/depth_peek.h). WWHD_PEEKZ=0 keeps the game's own function; WWHD_PEEKZ_LOG=1 logs the answers.
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "gfx/depth_peek.h"
#include "gfx/renderer.h"
#include "gx2/gx2.h"
#include "gx2/gx2_cmd.h"
#include "runtime.h"

namespace {
std::mutex g_mutex;
struct Answer {
    uint32_t depth;   // 24-bit, 0xFFFFFF = far
    uint64_t ticket;  // the request it answers (an older readback never replaces a newer one)
};
std::unordered_map<uint32_t, Answer> g_answers;  // result address -> the latest readback
bool log_on() {
    static const bool on = [] { const char* e = getenv("WWHD_PEEKZ_LOG"); return e && *e && *e != '0'; }();
    return on;
}
}  // namespace

namespace gfx::depth_peek {
uint64_t next_ticket() {
    static std::atomic<uint64_t> serial{0};
    return ++serial;
}
// render thread, once the GPU has copied the pixels
void publish(uint64_t ticket, const std::vector<uint32_t>& destinations, const std::vector<uint32_t>& depths) {
    std::lock_guard lock(g_mutex);
    for (size_t i = 0; i < destinations.size() && i < depths.size(); i++) {
        if (!destinations[i]) continue;
        auto [it, added] = g_answers.try_emplace(destinations[i], Answer{depths[i], ticket});
        if (!added && it->second.ticket < ticket) it->second = Answer{depths[i], ticket};
    }
}
bool latest(uint32_t destination, uint32_t& depth) {
    std::lock_guard lock(g_mutex);
    auto it = g_answers.find(destination);
    if (it == g_answers.end()) return false;
    depth = it->second.depth;
    return true;
}
}  // namespace gfx::depth_peek

extern "C" void f_029196A0_orig(Cpu* c);  // dPeekZ_c::peekData

extern "C" void hook_029196A0(Cpu* c) {
    static const bool off = [] { const char* e = getenv("WWHD_PEEKZ"); return e && atoi(e) == 0; }();
    const uint32_t image = gx2::converted_depth();
    if (off || !image || !render::can_peek_z()) { f_029196A0_orig(c); return; }
    const uint32_t obj = c->r[3], n = std::min<uint32_t>(ld8(obj), 64);
    const bool mirror = ld8(0x1012C8E3) != 0;
    std::vector<uint32_t> req{image};
    req.reserve(1 + n * 3);
    {
        std::lock_guard lock(g_mutex);
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t cell = obj + 4 + i * 8, dst = ld32(cell + 4);
            if (!dst) continue;
            const int x = int16_t(ld16(cell)), y = int16_t(ld16(cell + 2));
            uint32_t u = gfx::depth_peek::to_unit(x, 640), v = gfx::depth_peek::to_unit(y, 456);
            if (mirror) u = 0xFFFF - u;
            auto it = g_answers.find(dst);
            const uint32_t depth = it != g_answers.end() ? it->second.depth : 0xFFFFFF;
            st32(dst, depth);
            req.insert(req.end(), {u, v, dst});
            if (log_on()) LOG("[peekz] (%d,%d) -> %08X: %06X%s", x, y, dst, depth, it == g_answers.end() ? " (no readback yet)" : "");
        }
    }
    st8(obj, 0);  // as the game's function: the cells are answered
    if (req.size() > 1) gx2::emit(gx2::OP_PEEK_Z, req.data(), (uint32_t)req.size());
}
