#pragma once

#include <array>

namespace raid_terminal_ui
{
struct key_t
{
    const char *label;
    char value;
    float x;
    float y;
    float width;
    float height;
};

// Provisional normalized rectangles aligned to the current terminal art. The
// production manifest will replace these values after the per-key alpha-mask
// surgery without changing terminal puzzle logic.
inline constexpr std::array<key_t, 4> onboarding_keys = {{
    { "P", 'P', 147.0f / 1224.0f, 872.0f / 1285.0f, 54.0f / 1224.0f, 49.0f / 1285.0f },
    { "A", 'A', 205.0f / 1224.0f, 872.0f / 1285.0f, 51.0f / 1224.0f, 49.0f / 1285.0f },
    { "H", 'H', 260.0f / 1224.0f, 872.0f / 1285.0f, 51.0f / 1224.0f, 49.0f / 1285.0f },
    { "L", 'L', 316.0f / 1224.0f, 872.0f / 1285.0f, 51.0f / 1224.0f, 49.0f / 1285.0f }
}};

inline constexpr key_t clear_key = {
    "CLEAR", 0, 1072.0f / 1224.0f, 872.0f / 1285.0f, 61.0f / 1224.0f, 127.0f / 1285.0f
};

inline constexpr key_t submit_key = {
    "ENTER", 0, 1072.0f / 1224.0f, 1010.0f / 1285.0f, 61.0f / 1224.0f, 127.0f / 1285.0f
};

inline constexpr char onboarding_answer[] = "ALPHA";
inline constexpr int onboarding_answer_length = 5;

inline constexpr bool contains(const key_t &key, float x, float y)
{
    return x >= key.x && x <= key.x + key.width &&
        y >= key.y && y <= key.y + key.height;
}
}
