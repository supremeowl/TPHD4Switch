#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Depth peeks (peekz.cpp): the renderer copies single pixels of a depth image and publishes them as 24-bit depths.
namespace gfx::depth_peek {
inline uint32_t from_float(float z) {
    if (!std::isfinite(z)) return 0;
    return z >= 1.0f ? 0xFFFFFFu : uint32_t(std::clamp(z, 0.0f, 1.0f) * 16777215.0f);
}
// (TP HD fork) a coordinate in a guest screen of `guestExtent` pixels as 1/65536ths of the screen (pixel centres)
inline uint32_t to_unit(int coordinate, uint32_t guestExtent) {
    return uint32_t(std::clamp((coordinate + 0.5) * 65536.0 / guestExtent, 0.0, 65535.0));
}
// ... and that as a pixel of an image `extent` pixels wide or tall
inline uint32_t pixel(uint32_t unit, uint32_t extent) {
    return std::min<uint32_t>(uint32_t((uint64_t(unit) * extent) >> 16), extent ? extent - 1 : 0);
}
uint64_t next_ticket();
void publish(uint64_t ticket, const std::vector<uint32_t>& destinations, const std::vector<uint32_t>& depths);
bool latest(uint32_t destination, uint32_t& depth);  // the newest published answer for that result address
} // namespace gfx::depth_peek
