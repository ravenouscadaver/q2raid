#include "g_local.h"
#include "raid_downed.h"
#include "raid_director.h"
#include "raid_terminal.h"
#include "raid_terminal_shared.h"

#include <array>

void InitTrigger(edict_t *self);

namespace
{
constexpr spawnflags_t RAID_TERMINAL_LEGACY_DIRECT_OPEN = 1_spawnflag;

struct terminal_state_t
{
    bool active = false;
    uint32_t player_number = 0;
    int32_t player_spawn_count = 0;
    uint32_t gadget_number = 0;
    int32_t gadget_spawn_count = 0;
    vec3_t return_origin;
    vec3_t return_angles;
    gvec3_t last_cmd_angles;
    button_t previous_buttons = BUTTON_NONE;
    float cursor_x = 0.5f;
    float cursor_y = 0.5f;
    int puzzle_progress = 0;
    int last_key = -1;
    gtime_t key_feedback_until;
};

std::array<terminal_state_t, MAX_CLIENTS> states;

edict_t *Resolve(uint32_t number, int32_t spawn_count)
{
    if (!number || number >= globals.num_edicts)
        return nullptr;
    edict_t *entity = &g_edicts[number];
    return entity->inuse && entity->spawn_count == spawn_count ? entity : nullptr;
}

edict_t *NamedEntity(const char *targetname)
{
    return targetname && *targetname ? G_FindByString<&edict_t::targetname>(nullptr, targetname) : nullptr;
}

terminal_state_t &State(edict_t *player)
{
    return states[player->s.number - 1];
}

void ClearHUD(edict_t *player)
{
    if (!player || !player->client)
        return;
    player->client->ps.stats[STAT_RAID_TERMINAL_MODE] = 0;
    player->client->ps.stats[STAT_RAID_TERMINAL_CURSOR_X] = 0;
    player->client->ps.stats[STAT_RAID_TERMINAL_CURSOR_Y] = 0;
    player->client->ps.stats[STAT_RAID_TERMINAL_STATE] = 0;
}

void Close(edict_t *player, bool completed)
{
    terminal_state_t &state = State(player);
    edict_t *gadget = Resolve(state.gadget_number, state.gadget_spawn_count);
    player->s.origin = state.return_origin;
    player->client->ps.pmove.origin = state.return_origin;
    player->client->v_angle = state.return_angles;
    player->client->ps.viewangles = state.return_angles;
    player->solid = SOLID_BBOX;
    gi.linkentity(player);
    ClearHUD(player);
    state = {};
    if (completed && gadget)
    {
        gadget->timestamp = level.time + 1000000_sec;
        RaidDirector_NotifyEntityEvent(gadget, "terminal_complete", player);
    }
}

void SuppressGameplayInput(edict_t *player, usercmd_t *cmd)
{
    cmd->buttons = BUTTON_NONE;
    cmd->forwardmove = 0.0f;
    cmd->sidemove = 0.0f;
    cmd->upmove = 0.0f;
    player->client->buttons = BUTTON_NONE;
    player->client->latched_buttons = BUTTON_NONE;
    player->client->weapon_fire_buffered = false;
}

void OpenTerminal(edict_t *player, edict_t *gadget)
{
    terminal_state_t &state = State(player);
    if (state.active || gadget->timestamp > level.time)
        return;
    state.active = true;
    state.player_number = player->s.number;
    state.player_spawn_count = player->spawn_count;
    state.gadget_number = gadget->s.number;
    state.gadget_spawn_count = gadget->spawn_count;
    state.return_origin = player->s.origin;
    state.return_angles = player->client->v_angle;
    state.last_cmd_angles = player->client->cmd.angles;
    state.previous_buttons = player->client->buttons;

    if (edict_t *camera = NamedEntity(gadget->combattarget))
    {
        player->s.origin = camera->s.origin;
        player->client->ps.pmove.origin = camera->s.origin;
        const vec3_t angles = vectoangles(gadget->s.origin - camera->s.origin);
        player->client->v_angle = angles;
        player->client->ps.viewangles = angles;
    }
    player->solid = SOLID_NOT;
    player->velocity = {};
    gi.linkentity(player);
    RaidDirector_NotifyEntityEvent(gadget, "terminal_open", player);
}

TOUCH(raid_interaction_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || other->health <= 0 || RaidDowned_IsDown(other) ||
        level.time < self->touch_debounce_time ||
        !(other->client->buttons & BUTTON_USE) ||
        (other->client->oldbuttons & BUTTON_USE))
        return;
    self->touch_debounce_time = level.time + gtime_t::from_sec(self->wait > 0.0f ? self->wait : 0.5f);
    // Compatibility wrapper only. Canonical terminals emit an interaction and
    // let encounter JSON decide whether/which terminal should open.
    RaidDirector_NotifyEntityEvent(self, "interact", other);

    if (!self->spawnflags.has(RAID_TERMINAL_LEGACY_DIRECT_OPEN))
        return;

    RaidTerminal_Open(other, NamedEntity(self->target));
}
}

void SP_trigger_raid_interaction(edict_t *ent)
{
    InitTrigger(ent);
    ent->touch = raid_interaction_touch;
    gi.linkentity(ent);
}

void SP_trigger_raid_terminal(edict_t *ent)
{
    SP_trigger_raid_interaction(ent);
}

bool RaidTerminal_Open(edict_t *player, edict_t *terminal)
{
    if (!player || !player->client || player->health <= 0 ||
        !terminal || !terminal->inuse || !terminal->classname ||
        Q_strcasecmp(terminal->classname, "raid_gadget") ||
        !terminal->message || Q_strcasecmp(terminal->message, "terminal"))
        return false;

    terminal_state_t &state = State(player);
    if (state.active || terminal->timestamp > level.time)
        return false;

    OpenTerminal(player, terminal);
    return true;
}

bool RaidTerminal_IsActive(edict_t *player)
{
    return player && player->client && State(player).active;
}

bool RaidTerminal_HandleInput(edict_t *player, usercmd_t *cmd)
{
    if (!player || !player->client)
        return false;
    terminal_state_t &state = State(player);
    if (!state.active)
        return false;
    if (!Resolve(state.player_number, state.player_spawn_count) || player->health <= 0)
    {
        Close(player, false);
        return false;
    }

    const button_t buttons = cmd->buttons;
    const button_t pressed = buttons & ~state.previous_buttons;
    state.previous_buttons = buttons;

    const auto angle_delta = [](float current, float previous) {
        float delta = current - previous;
        while (delta > 180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;
        return delta;
    };
    state.cursor_x = std::clamp(state.cursor_x + angle_delta(cmd->angles[YAW], state.last_cmd_angles[YAW]) / 90.0f, 0.0f, 1.0f);
    state.cursor_y = std::clamp(state.cursor_y + angle_delta(cmd->angles[PITCH], state.last_cmd_angles[PITCH]) / 90.0f, 0.0f, 1.0f);
    state.last_cmd_angles = cmd->angles;

    player->client->ps.pmove.pm_type = PM_FREEZE;
    player->velocity = {};
    player->client->ps.stats[STAT_RAID_TERMINAL_MODE] = 1;
    player->client->ps.stats[STAT_RAID_TERMINAL_CURSOR_X] = static_cast<int16_t>(state.cursor_x * 1000.0f);
    player->client->ps.stats[STAT_RAID_TERMINAL_CURSOR_Y] = static_cast<int16_t>(state.cursor_y * 1000.0f);

    if (state.key_feedback_until <= level.time)
        state.last_key = -1;
    player->client->ps.stats[STAT_RAID_TERMINAL_STATE] = static_cast<int16_t>(
        (state.puzzle_progress & 0x0f) | ((state.last_key + 1) << 4));

    const bool click = !!(pressed & BUTTON_ATTACK);
    if (click)
    {
        for (size_t key_index = 0; key_index < raid_terminal_ui::onboarding_keys.size(); ++key_index)
        {
            const raid_terminal_ui::key_t &key = raid_terminal_ui::onboarding_keys[key_index];
            if (!raid_terminal_ui::contains(key, state.cursor_x, state.cursor_y))
                continue;

            state.last_key = static_cast<int>(key_index);
            state.key_feedback_until = level.time + 180_ms;
            if (state.puzzle_progress >= raid_terminal_ui::onboarding_answer_length)
                break;
            if (key.value == raid_terminal_ui::onboarding_answer[state.puzzle_progress])
                state.puzzle_progress++;
            else
                state.puzzle_progress = key.value == raid_terminal_ui::onboarding_answer[0] ? 1 : 0;
            break;
        }

        if (raid_terminal_ui::contains(raid_terminal_ui::clear_key, state.cursor_x, state.cursor_y))
        {
            state.puzzle_progress = 0;
            state.last_key = 4;
            state.key_feedback_until = level.time + 180_ms;
        }
        else if (raid_terminal_ui::contains(raid_terminal_ui::submit_key, state.cursor_x, state.cursor_y))
        {
            state.last_key = 5;
            state.key_feedback_until = level.time + 180_ms;
            if (state.puzzle_progress == raid_terminal_ui::onboarding_answer_length)
            {
                Close(player, true);
                SuppressGameplayInput(player, cmd);
                return true;
            }
            state.puzzle_progress = 0;
        }
    }
    if (pressed & BUTTON_USE)
        Close(player, false);
    SuppressGameplayInput(player, cmd);
    return true;
}

bool RaidTerminal_Cancel(edict_t *player)
{
    if (!player || !player->client || !State(player).active)
        return false;
    Close(player, false);
    return true;
}

void RaidTerminal_Disconnect(edict_t *player)
{
    if (player && player->client)
        State(player) = {};
}

void RaidTerminal_Reset()
{
    for (terminal_state_t &state : states)
    {
        edict_t *player = Resolve(state.player_number, state.player_spawn_count);
        if (state.active && player && player->client)
            Close(player, false);
        state = {};
    }
}
