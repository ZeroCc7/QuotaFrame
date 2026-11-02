#pragma once

namespace usage_panel::amoled {

struct ReelFrame {
    int digit;
    int next;
    int phase;  // 0..1023, fraction of a glyph height.
    int direction;
};

constexpr ReelFrame reel_frame(int from, int target, int place, int progress, bool extra_turn)
{
    const int start = from / place % 10;
    const int end = target / place % 10;
    const bool descending = from >= target;
    int steps = descending ? (start - end + 10) % 10 : (end - start + 10) % 10;
    if (extra_turn && from != target) steps += 10;
    const int travel = steps * progress;
    const int digit = (start + (descending ? -travel / 1024 : travel / 1024) + 30) % 10;
    return {digit, (digit + (descending ? 9 : 1)) % 10,
            travel % 1024, descending ? -1 : 1};
}

constexpr bool reel_endpoints_valid()
{
    for (int target = 0; target <= 1000; ++target) {
        for (int place = 1; place <= 1000; place *= 10) {
            const auto start = reel_frame(1000, target, place, 0, true);
            const auto end = reel_frame(1000, target, place, 1024, true);
            if (start.digit != 1000 / place % 10 || start.phase != 0 ||
                end.digit != target / place % 10 || end.phase != 0) return false;
        }
    }
    return true;
}

static_assert(reel_endpoints_valid());
static_assert(reel_frame(270, 260, 10, 512, false).phase == 512);
static_assert(reel_frame(0, 10, 10, 1024, false).digit == 1);

}  // namespace usage_panel::amoled
