// Q2RAID MODIFIED
// RAID_STATUS: IMPLEMENTED_UNBUILT
// RAID_STATUS_DATE: 2026-08-29
// RAID_FEATURE: terminal_asset_free_layout
// RAID_EVIDENCE: user runtime screenshot after build 88
// RAID_PROTECTION: READ_ONLY_UNLESS_USER_AUTHORIZED

#pragma once

#include <array>

namespace raid_ui
{
enum screen_t : int
{
    SCREEN_NONE = 0,
    SCREEN_ONBOARDING_TERMINAL = 1,
    SCREEN_CARNAGE_REPORT = 2
};

struct key_t
{
    const char *label;
    char value;
    float x;
    float y;
    float width;
    float height;
};

inline constexpr std::array<key_t, 4> onboarding_keys = {{
    { "P", 'P', 0.25f, 0.66f, 0.09f, 0.08f },
    { "A", 'A', 0.37f, 0.66f, 0.09f, 0.08f },
    { "H", 'H', 0.49f, 0.66f, 0.09f, 0.08f },
    { "L", 'L', 0.61f, 0.66f, 0.09f, 0.08f }
}};

inline constexpr key_t clear_key = {
    "CLEAR", 0, 0.25f, 0.79f, 0.20f, 0.08f
};

inline constexpr key_t submit_key = {
    "ENTER", 0, 0.55f, 0.79f, 0.20f, 0.08f
};

inline constexpr char onboarding_answer[] = "ALPHA";
inline constexpr int onboarding_answer_length = 5;

inline constexpr bool contains(const key_t &key, float x, float y)
{
    return x >= key.x && x <= key.x + key.width &&
        y >= key.y && y <= key.y + key.height;
}
}
