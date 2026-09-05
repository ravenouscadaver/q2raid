#include "g_local.h"
#include "raid_downed.h"
#include "raid_director.h"
#include "raid_ui.h"
#include "raid_ui_shared.h"

#include "json/json.h"

#include <array>
#include <string>

namespace
{
struct raid_ui_session_t
{
    bool active = false;
    bool completing = false;
    uint32_t player_number = 0;
    int32_t player_spawn_count = 0;
    uint32_t source_number = 0;
    int32_t source_spawn_count = 0;
    raid_ui::screen_t screen = raid_ui::SCREEN_NONE;
    gvec3_t last_cmd_angles;
    float cursor_x = 0.5f;
    float cursor_y = 0.5f;
    int puzzle_progress = 0;
    int last_key = -1;
    gtime_t key_feedback_until;
};

struct carnage_runtime_t
{
    bool director_loaded = false;
    bool all_dead = false;
    gtime_t updated_at;
    int monster_baseline = 0;
    int deaths = 0;
    int wipes = 0;
    int snapshot_kills = 0;
    int snapshot_deaths = 0;
    int snapshot_wipes = 0;
    std::array<bool, MAX_CLIENTS> was_dead = {};
    std::array<bool, MAX_CLIENTS> report_visible = {};
    std::array<gtime_t, MAX_CLIENTS> dismiss_after = {};
};

std::array<raid_ui_session_t, MAX_CLIENTS> sessions;
std::array<gtime_t, MAX_CLIENTS> terminal_reentry_after;
carnage_runtime_t carnage;

void MenuSelect(edict_t *player, pmenuhnd_t *menu);

constexpr pmenu_t raid_ui_menu[] = {
    { "", PMENU_ALIGN_LEFT, MenuSelect, "" }
};

bool ValidPlayer(edict_t *player)
{
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS;
}

raid_ui_session_t &Session(edict_t *player)
{
    return sessions[player->s.number - 1];
}

edict_t *Resolve(uint32_t number, int32_t spawn_count)
{
    if (!number || number >= globals.num_edicts)
        return nullptr;
    edict_t *entity = &g_edicts[number];
    return entity->inuse && entity->spawn_count == spawn_count ? entity : nullptr;
}

void ClearHUD(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    player->client->ps.stats[STAT_RAID_UI_SCREEN] = 0;
    player->client->ps.stats[STAT_RAID_UI_CURSOR_X] = 0;
    player->client->ps.stats[STAT_RAID_UI_CURSOR_Y] = 0;
    player->client->ps.stats[STAT_RAID_UI_STATE] = 0;
}

void ResetCarnageLiveAttempt()
{
    carnage.monster_baseline = level.killed_monsters;
    carnage.deaths = 0;
    carnage.was_dead.fill(false);
    for (edict_t *player : active_players())
        if (ValidPlayer(player) && !player->client->resp.spectator)
            carnage.was_dead[player->s.number - 1] = player->deadflag || player->health <= 0;
}

void ClearCarnageAll()
{
    carnage = {};
}

void UpdateCarnageTracker()
{
    if (carnage.updated_at == level.time)
        return;
    carnage.updated_at = level.time;

    Json::Value director_state;
    RaidDirector_WriteSave(director_state);
    const bool loaded = director_state.get("loaded", false).asBool();

    if (!loaded)
    {
        if (carnage.director_loaded)
            ClearCarnageAll();
        return;
    }

    if (!carnage.director_loaded)
    {
        carnage.director_loaded = true;
        ResetCarnageLiveAttempt();
    }

    bool any_participant = false;
    bool all_dead = true;
    for (edict_t *player : active_players())
    {
        if (!ValidPlayer(player) || player->client->resp.spectator)
            continue;
        any_participant = true;
        const size_t index = player->s.number - 1;
        const bool dead = player->deadflag || player->health <= 0;
        if (dead && !carnage.was_dead[index])
            ++carnage.deaths;
        carnage.was_dead[index] = dead;
        if (!dead)
            all_dead = false;
    }
    if (!any_participant)
        all_dead = false;

    if (carnage.all_dead && !all_dead)
        ResetCarnageLiveAttempt();

    if (all_dead && !carnage.all_dead)
    {
        carnage.snapshot_kills = std::max(0, level.killed_monsters - carnage.monster_baseline);
        carnage.snapshot_deaths = carnage.deaths;
        carnage.snapshot_wipes = ++carnage.wipes;

        for (edict_t *player : active_players())
        {
            if (!ValidPlayer(player) || player->client->resp.spectator)
                continue;
            const size_t index = player->s.number - 1;
            carnage.report_visible[index] = true;
            // Do not let the input that caused the final death instantly dismiss
            // the report on the same frame it appears.
            carnage.dismiss_after[index] = level.time + 500_ms;
        }
    }

    carnage.all_dead = all_dead;
}

bool CarnageReportVisible(edict_t *player)
{
    return ValidPlayer(player) && carnage.director_loaded &&
        carnage.report_visible[player->s.number - 1];
}

void UpdateCarnageHUD(edict_t *player)
{
    if (!CarnageReportVisible(player))
        return;

    const size_t index = player->s.number - 1;
    const button_t dismiss_buttons = BUTTON_ATTACK | BUTTON_USE | BUTTON_JUMP;
    if (level.time >= carnage.dismiss_after[index] &&
        (player->client->latched_buttons & dismiss_buttons))
    {
        player->client->latched_buttons &= ~dismiss_buttons;
        carnage.report_visible[index] = false;
        ClearHUD(player);
        return;
    }

    player->client->ps.stats[STAT_RAID_UI_SCREEN] = raid_ui::SCREEN_CARNAGE_REPORT;
    player->client->ps.stats[STAT_RAID_UI_CURSOR_X] = static_cast<int16_t>(
        std::clamp(carnage.snapshot_kills, 0, 32767));
    player->client->ps.stats[STAT_RAID_UI_CURSOR_Y] = static_cast<int16_t>(
        std::clamp(carnage.snapshot_deaths, 0, 32767));
    const int packed = std::clamp(carnage.snapshot_wipes, 0, 127) << 8;
    player->client->ps.stats[STAT_RAID_UI_STATE] = static_cast<int16_t>(packed);
}

void FinishSession(edict_t *player, pmenuhnd_t *menu)
{
    if (!ValidPlayer(player))
        return;
    raid_ui_session_t &session = Session(player);
    if (!session.active || (menu && menu->owner != &session))
        return;

    edict_t *source = Resolve(session.source_number, session.source_spawn_count);
    const bool completed = session.completing;
    ClearHUD(player);
    session = {};

    if (completed && source)
    {
        source->timestamp = level.time + 1000000_sec;
        RaidDirector_NotifyEntityEvent(source, "terminal_complete", player);
    }
    else
    {
        terminal_reentry_after[player->s.number - 1] = level.time + 500_ms;
    }
}

void MenuClosed(edict_t *player, pmenuhnd_t *menu)
{
    FinishSession(player, menu);
}

void CloseSession(edict_t *player, bool completed)
{
    if (!ValidPlayer(player))
        return;
    raid_ui_session_t &session = Session(player);
    if (!session.active)
        return;
    session.completing = completed;
    if (player->client->menu && player->client->menu->owner == &session)
        PMenu_Close(player);
    else
        FinishSession(player, nullptr);
}

void ConsumeInput(edict_t *player, usercmd_t *cmd)
{
    cmd->forwardmove = 0;
    cmd->sidemove = 0;
    cmd->buttons = BUTTON_NONE;
    player->client->latched_buttons = BUTTON_NONE;
    player->velocity = {};
    player->client->ps.pmove.pm_type = PM_FREEZE;
}

float AngleDelta(float current, float previous)
{
    float delta = current - previous;
    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;
    return delta;
}

void SelectAtCursor(edict_t *player, raid_ui_session_t &session)
{
    for (size_t key_index = 0; key_index < raid_ui::onboarding_keys.size(); ++key_index)
    {
        const raid_ui::key_t &key = raid_ui::onboarding_keys[key_index];
        if (!raid_ui::contains(key, session.cursor_x, session.cursor_y))
            continue;

        session.last_key = static_cast<int>(key_index);
        session.key_feedback_until = level.time + 180_ms;
        if (session.puzzle_progress < raid_ui::onboarding_answer_length)
        {
            if (key.value == raid_ui::onboarding_answer[session.puzzle_progress])
                session.puzzle_progress++;
            else
                session.puzzle_progress = key.value == raid_ui::onboarding_answer[0] ? 1 : 0;
        }
        return;
    }

    if (raid_ui::contains(raid_ui::clear_key, session.cursor_x, session.cursor_y))
    {
        session.puzzle_progress = 0;
        session.last_key = 4;
        session.key_feedback_until = level.time + 180_ms;
    }
    else if (raid_ui::contains(raid_ui::submit_key, session.cursor_x, session.cursor_y))
    {
        session.last_key = 5;
        session.key_feedback_until = level.time + 180_ms;
        if (session.puzzle_progress == raid_ui::onboarding_answer_length)
            CloseSession(player, true);
        else
            session.puzzle_progress = 0;
    }
}

void MenuSelect(edict_t *player, pmenuhnd_t *menu)
{
    if (!ValidPlayer(player) || !menu || menu->owner != &Session(player))
        return;
    SelectAtCursor(player, Session(player));
}
}

bool RaidUI_Open(edict_t *player, edict_t *source)
{
    if (!ValidPlayer(player) || player->health <= 0 || player->deadflag ||
        player->client->resp.spectator || RaidDowned_IsDown(player) ||
        !source || !source->inuse ||
        !source->classname || Q_strcasecmp(source->classname, "raid_gadget") ||
        !source->message || Q_strcasecmp(source->message, "terminal") ||
        source->timestamp > level.time ||
        terminal_reentry_after[player->s.number - 1] > level.time || player->client->menu)
        return false;

    raid_ui_session_t &session = Session(player);
    if (session.active)
        return false;

    session.active = true;
    session.player_number = player->s.number;
    session.player_spawn_count = player->spawn_count;
    session.source_number = source->s.number;
    session.source_spawn_count = source->spawn_count;
    session.screen = raid_ui::SCREEN_ONBOARDING_TERMINAL;
    session.last_cmd_angles = player->client->cmd.angles;

    if (!PMenu_Open(player, raid_ui_menu, -1, static_cast<int>(std::size(raid_ui_menu)),
        nullptr, nullptr, &session, MenuClosed, false))
    {
        session = {};
        return false;
    }

    RaidDirector_NotifyEntityEvent(source, "terminal_open", player);
    return true;
}

bool RaidUI_IsActive(edict_t *player)
{
    if (!ValidPlayer(player))
        return false;
    raid_ui_session_t &session = Session(player);
    return session.active && player->client->menu && player->client->menu->owner == &session;
}

bool RaidUI_HandleInput(edict_t *player, usercmd_t *cmd)
{
    if (!ValidPlayer(player) || !cmd || !RaidUI_IsActive(player))
        return false;

    raid_ui_session_t &session = Session(player);
    if (!Resolve(session.player_number, session.player_spawn_count) ||
        player->health <= 0 || player->deadflag || player->client->resp.spectator ||
        RaidDowned_IsDown(player))
    {
        CloseSession(player, false);
        ConsumeInput(player, cmd);
        return true;
    }

    session.cursor_x = std::clamp(session.cursor_x +
        AngleDelta(cmd->angles[YAW], session.last_cmd_angles[YAW]) / 90.0f, 0.0f, 1.0f);
    session.cursor_y = std::clamp(session.cursor_y +
        AngleDelta(cmd->angles[PITCH], session.last_cmd_angles[PITCH]) / 90.0f, 0.0f, 1.0f);
    session.last_cmd_angles = cmd->angles;

    if (session.key_feedback_until <= level.time)
        session.last_key = -1;

    const button_t select_buttons = BUTTON_ATTACK | BUTTON_JUMP;
    const bool select_pressed = (cmd->buttons & select_buttons) &&
        !(player->client->oldbuttons & select_buttons);
    const bool cancel_pressed = (cmd->buttons & BUTTON_USE) &&
        !(player->client->oldbuttons & BUTTON_USE);

    if (select_pressed)
        PMenu_Select(player);
    if (cancel_pressed && RaidUI_IsActive(player))
        CloseSession(player, false);

    ConsumeInput(player, cmd);
    return true;
}

void RaidUI_UpdateHUD(edict_t *player)
{
    UpdateCarnageTracker();

    if (RaidUI_IsActive(player))
    {
        raid_ui_session_t &session = Session(player);
        player->client->ps.stats[STAT_RAID_UI_SCREEN] = static_cast<int16_t>(session.screen);
        player->client->ps.stats[STAT_RAID_UI_CURSOR_X] = static_cast<int16_t>(session.cursor_x * 1000.0f);
        player->client->ps.stats[STAT_RAID_UI_CURSOR_Y] = static_cast<int16_t>(session.cursor_y * 1000.0f);
        player->client->ps.stats[STAT_RAID_UI_STATE] = static_cast<int16_t>(
            (session.puzzle_progress & 0x0f) | ((session.last_key + 1) << 4));
        return;
    }

    if (CarnageReportVisible(player))
    {
        UpdateCarnageHUD(player);
        return;
    }

    ClearHUD(player);
}

void RaidUI_Close(edict_t *player)
{
    CloseSession(player, false);
}

void RaidUI_Disconnect(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    CloseSession(player, false);
    ClearHUD(player);
    Session(player) = {};
    terminal_reentry_after[player->s.number - 1] = 0_ms;
    carnage.report_visible[player->s.number - 1] = false;
    carnage.was_dead[player->s.number - 1] = false;
}

void RaidUI_Reset()
{
    // Reset terminal sessions only. Carnage wipe snapshots deliberately survive
    // an in-place Director encounter reset so each marine can dismiss the report
    // independently after the next attempt has already been restored.
    for (raid_ui_session_t &session : sessions)
    {
        edict_t *player = Resolve(session.player_number, session.player_spawn_count);
        if (session.active && ValidPlayer(player))
            CloseSession(player, false);
        session = {};
    }
    terminal_reentry_after.fill(0_ms);
}
