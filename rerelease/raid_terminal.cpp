#include "g_local.h"
#include "raid_director.h"
#include "raid_terminal.h"

#include <array>

void InitTrigger(edict_t *self);

namespace
{
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
    float cursor_x = 0.5f;
    float cursor_y = 0.5f;
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
    player->client->ps.stats[STAT_RAID_HAT_NAME] = 0;
    player->client->ps.stats[STAT_RAID_HAT_HEALTH] = 0;
    player->client->ps.stats[STAT_RAID_HAT_RANK] = 0;
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

void Open(edict_t *player, edict_t *gadget)
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

TOUCH(raid_terminal_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || other->health <= 0)
        return;
    edict_t *gadget = NamedEntity(self->target);
    if (!gadget || !gadget->classname || Q_strcasecmp(gadget->classname, "raid_gadget"))
        return;
    Open(other, gadget);
}
}

void SP_trigger_raid_terminal(edict_t *ent)
{
    InitTrigger(ent);
    ent->touch = raid_terminal_touch;
    gi.linkentity(ent);
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
    player->client->ps.stats[STAT_RAID_HAT_NAME] = 33;
    player->client->ps.stats[STAT_RAID_HAT_HEALTH] = static_cast<int16_t>(state.cursor_x * 1000.0f);
    player->client->ps.stats[STAT_RAID_HAT_RANK] = static_cast<int16_t>(state.cursor_y * 1000.0f);

    const bool click = (cmd->buttons & BUTTON_ATTACK) && !(player->client->oldbuttons & BUTTON_ATTACK);
    if (click && state.cursor_x >= 0.38f && state.cursor_x <= 0.62f && state.cursor_y >= 0.38f && state.cursor_y <= 0.55f)
    {
        Close(player, true);
        return true;
    }
    if ((cmd->buttons & BUTTON_USE) && !(player->client->oldbuttons & BUTTON_USE))
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
