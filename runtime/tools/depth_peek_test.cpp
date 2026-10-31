#include "gfx/depth_peek.h"
#include <cassert>
#include <limits>

int main() {
    // (TP HD fork) guest screen coordinates -> 1/65536ths -> image pixels
    using gfx::depth_peek::pixel;
    using gfx::depth_peek::to_unit;
    // The centre of TP HD's 640x456 screen is the centre of the depth buffer (the pixel's centre, so within one).
    auto near = [](uint32_t a, uint32_t b) { return a + 1 >= b && a <= b + 1; };
    assert(near(pixel(to_unit(320, 640), 1920), 960));
    assert(near(pixel(to_unit(228, 456), 1080), 540));
    for (auto guest : {640u, 456u}) {
        for (auto host : {1u, 720u, 1080u, 1920u, 2560u}) {
            assert(pixel(to_unit(0, guest), host) <= host / guest);
            assert(pixel(to_unit(-1, guest), host) == 0);
            assert(pixel(to_unit(-32768, guest), host) == 0);
            assert(pixel(to_unit(int(guest), guest), host) == host - 1);
            assert(pixel(to_unit(int(guest) * 2, guest), host) == host - 1);
            assert(pixel(to_unit(int(guest) - 1, guest), host) <= host - 1);
        }
    }
    assert(pixel(65535, 0) == 0);
    using gfx::depth_peek::from_float;
    assert(from_float(0) == 0);
    assert(from_float(0.5f) == 0x7FFFFF);
    assert(from_float(1) == 0xFFFFFF);
    assert(from_float(-1) == 0);
    assert(from_float(2) == 0xFFFFFF);
    assert(from_float(std::numeric_limits<float>::quiet_NaN()) == 0);
}
