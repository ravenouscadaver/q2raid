#include "cg_local.h"
#include "cg_raid_ui.h"
#include "raid_ui_shared.h"

namespace
{
void DrawFallbackChassis(float x, float y, float width, float height)
{
    const float aperture_x = x + width * (180.0f / 1224.0f);
    const float aperture_y = y + height * (160.0f / 1285.0f);
    const float aperture_w = width * ((1050.0f - 180.0f) / 1224.0f);
    const float aperture_h = height * ((720.0f - 160.0f) / 1285.0f);
    const rgba_t color{ 18, 20, 18, 245 };
    cgi.SCR_DrawColorPic(x, y, width, aperture_y - y, "_white", color);
    cgi.SCR_DrawColorPic(x, aperture_y, aperture_x - x, aperture_h, "_white", color);
    cgi.SCR_DrawColorPic(aperture_x + aperture_w, aperture_y,
        x + width - (aperture_x + aperture_w), aperture_h, "_white", color);
    cgi.SCR_DrawColorPic(x, aperture_y + aperture_h, width,
        y + height - (aperture_y + aperture_h), "_white", color);
}

void DrawCarnageReport(const player_state_t *ps, const vrect_t &hud_vrect, int32_t scale)
{
    const float x = hud_vrect.x * scale;
    const float y = hud_vrect.y * scale;
    const float width = hud_vrect.width * scale;
    const float height = hud_vrect.height * scale;
    const float center_x = x + width * 0.5f;
    const uint16_t packed = static_cast<uint16_t>(ps->stats[STAT_RAID_UI_STATE]);
    const int kills = std::max<int>(0, ps->stats[STAT_RAID_UI_CURSOR_X]);
    const int deaths = std::max<int>(0, ps->stats[STAT_RAID_UI_CURSOR_Y]);
    const int mechanics = packed & 0xff;
    const int wipes = (packed >> 8) & 0x7f;

    cgi.SCR_DrawColorPic(x, y, width, height, "_white", rgba_t{ 16, 5, 4, 235 });
    cgi.SCR_DrawColorPic(x + width * 0.08f, y + height * 0.12f,
        width * 0.84f, height * 0.76f, "_white", rgba_t{ 8, 5, 4, 225 });

    cgi.SCR_DrawFontString("THE DARKNESS CONSUMES YOU", center_x, y + height * 0.19f,
        scale, rgba_t{ 205, 55, 42, 255 }, true, text_align_t::CENTER);
    cgi.SCR_DrawFontString("CARNAGE REPORT", center_x, y + height * 0.28f,
        scale, rgba_t{ 230, 205, 185, 255 }, true, text_align_t::CENTER);

    const float left = x + width * 0.28f;
    const float value_x = x + width * 0.70f;
    const float line = cgi.SCR_FontLineHeight(scale) * 2.0f;
    float row = y + height * 0.40f;
    const rgba_t label{ 165, 145, 130, 255 };
    const rgba_t value{ 235, 220, 205, 255 };

    const auto draw_stat = [&](const char *name, int number) {
        const std::string text = fmt::format("{}", number);
        cgi.SCR_DrawFontString(name, left, row, scale, label, true, text_align_t::LEFT);
        cgi.SCR_DrawFontString(text.c_str(), value_x, row, scale, value, true, text_align_t::RIGHT);
        row += line;
    };

    draw_stat("HOSTILES ELIMINATED", kills);
    draw_stat("MARINE DEATHS", deaths);
    draw_stat("MECHANIC PROGRESS", mechanics);
    draw_stat("WIPE", wipes);

    cgi.SCR_DrawFontString("FIRE / USE / JUMP TO DISMISS", center_x, y + height * 0.78f,
        scale, rgba_t{ 130, 112, 100, 255 }, true, text_align_t::CENTER);
}
}

void CG_RaidUI_Draw(const player_state_t *ps, const vrect_t &hud_vrect, int32_t scale)
{
    if (!ps)
        return;

    if (ps->stats[STAT_RAID_UI_SCREEN] == raid_ui::SCREEN_CARNAGE_REPORT)
    {
        DrawCarnageReport(ps, hud_vrect, scale);
        return;
    }

    if (ps->stats[STAT_RAID_UI_SCREEN] != raid_ui::SCREEN_ONBOARDING_TERMINAL)
        return;

    const float terminal_height = hud_vrect.height * 0.94f * scale;
    const float terminal_width = terminal_height * (1224.0f / 1285.0f);
    const float terminal_x = (hud_vrect.x + hud_vrect.width * 0.5f) * scale - terminal_width * 0.5f;
    const float terminal_y = (hud_vrect.y + hud_vrect.height * 0.5f) * scale - terminal_height * 0.5f;
    cgi.SCR_DrawColorPic(hud_vrect.x * scale, hud_vrect.y * scale,
        hud_vrect.width * scale, hud_vrect.height * scale, "_white", rgba_t{ 0, 0, 0, 185 });

    // Build the terminal entirely from engine-native primitives. No terminal
    // PNG is queried or decoded on this path; build #88 proved that merely
    // entering the external-image rendering path can crash inside KEX.
    DrawFallbackChassis(terminal_x, terminal_y, terminal_width, terminal_height);

    const int state = std::max<int>(0, ps->stats[STAT_RAID_UI_STATE]);
    const int puzzle_progress = std::clamp(state & 0x0f, 0, raid_ui::onboarding_answer_length);
    const int pressed_key = ((state >> 4) & 0x0f) - 1;
    const float screen_center_x = terminal_x + terminal_width * 0.5f;
    const float prompt_y = terminal_y + terminal_height * (260.0f / 1285.0f);

    cgi.SCR_DrawFontString("ACCESS TOKEN CORRUPTED", screen_center_x, prompt_y,
        scale, rgba_t{ 114, 255, 148, 255 }, true, text_align_t::CENTER);

    std::string reconstruction;
    for (int index = 0; index < raid_ui::onboarding_answer_length; ++index)
    {
        if (index)
            reconstruction += ' ';
        reconstruction += index < puzzle_progress ? raid_ui::onboarding_answer[index] : '_';
    }
    cgi.SCR_DrawFontString(reconstruction.c_str(), screen_center_x,
        prompt_y + cgi.SCR_FontLineHeight(scale) * 2.0f, scale,
        rgba_t{ 210, 255, 220, 255 }, true, text_align_t::CENTER);

    const auto draw_key = [&](const raid_ui::key_t &key, int key_index,
        const rgba_t &label_color) {
        const float key_x = terminal_x + terminal_width * key.x;
        const float key_y = terminal_y + terminal_height * key.y;
        const float key_width = terminal_width * key.width;
        const float key_height = terminal_height * key.height;
        if (pressed_key == key_index)
            cgi.SCR_DrawColorPic(key_x, key_y, key_width, key_height, "_white",
                rgba_t{ 5, 10, 7, 165 });
        cgi.SCR_DrawFontString(key.label, key_x + 4.0f * scale,
            key_y + 2.0f * scale, scale, label_color, true, text_align_t::LEFT);
    };

    for (size_t key_index = 0; key_index < raid_ui::onboarding_keys.size(); ++key_index)
        draw_key(raid_ui::onboarding_keys[key_index], static_cast<int>(key_index),
            rgba_t{ 175, 230, 185, 255 });
    draw_key(raid_ui::clear_key, 4, rgba_t{ 255, 184, 80, 255 });
    draw_key(raid_ui::submit_key, 5, rgba_t{ 130, 255, 150, 255 });

    const float cursor_x = terminal_x + terminal_width *
        (std::clamp<int>(ps->stats[STAT_RAID_UI_CURSOR_X], 0, 1000) / 1000.0f);
    const float cursor_y = terminal_y + terminal_height *
        (std::clamp<int>(ps->stats[STAT_RAID_UI_CURSOR_Y], 0, 1000) / 1000.0f);
    cgi.SCR_DrawColorPic(cursor_x - 7.0f * scale, cursor_y - 1.0f * scale,
        14.0f * scale, 2.0f * scale, "_white", rgba_t{ 255, 220, 96, 255 });
    cgi.SCR_DrawColorPic(cursor_x - 1.0f * scale, cursor_y - 7.0f * scale,
        2.0f * scale, 14.0f * scale, "_white", rgba_t{ 255, 220, 96, 255 });
}
