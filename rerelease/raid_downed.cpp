#include "g_local.h"
#include "m_insane.h"
#include "raid_downed.h"
#include "raid_thirdperson.h"

namespace
{
struct downed_state_t
{
    bool downed = false;
    int saved_health = 1;
    int frame = FRAME_crawl1;
    gtime_t next_frame;
};

downed_state_t states[MAX_CLIENTS];

downed_state_t &State(edict_t *player)
{
    return states[player->s.number - 1];
}

bool ValidPlayer(edict_t *player)
{
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS;
}

void EnterDowned(edict_t *player)
{
    downed_state_t &state = State(player);
    if (state.downed || player->deadflag)
        return;
    state.downed = true;
    state.saved_health = std::max(1, player->max_health / 4);
    state.frame = FRAME_crawl1;
    state.next_frame = level.time + 100_ms;
    player->health = 1;
    player->client->buttons &= ~BUTTON_ATTACK;
    player->client->latched_buttons &= ~BUTTON_ATTACK;
    player->client->ps.gunindex = 0;
    RaidThirdPerson_SetPresentation(player, true, "models/monsters/insane/tris.md2", state.frame);
    gi.LocClient_Print(player, PRINT_HIGH, "MARINE DOWN - CRAWL TO COVER\n");
}

void LeaveDowned(edict_t *player, bool restore_health)
{
    downed_state_t &state = State(player);
    if (!state.downed)
        return;
    state.downed = false;
    RaidThirdPerson_SetPresentation(player, false, nullptr, 0);
    if (restore_health)
        player->health = std::max(1, state.saved_health);
    state = {};
}
}

bool RaidDowned_IsDown(edict_t *player)
{
    return ValidPlayer(player) && State(player).downed;
}

bool RaidDowned_InterceptFatalDamage(edict_t *player)
{
    if (!coop->integer || !ValidPlayer(player) || State(player).downed || player->deadflag)
        return false;
    if (player->health <= -25)
        return false;
    EnterDowned(player);
    return true;
}

void RaidDowned_ToggleTest(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    if (State(player).downed)
    {
        LeaveDowned(player, true);
        gi.LocClient_Print(player, PRINT_HIGH, "DOWNED TEST REVIVED\n");
    }
    else
        EnterDowned(player);
}

void RaidDowned_FilterCommand(edict_t *player, usercmd_t &cmd)
{
    if (!RaidDowned_IsDown(player))
        return;
    cmd.forwardmove *= 0.2f;
    cmd.sidemove *= 0.2f;
    cmd.buttons &= ~BUTTON_ATTACK;
    cmd.buttons |= BUTTON_CROUCH;
}

void RaidDowned_Update(edict_t *player)
{
    if (!RaidDowned_IsDown(player))
        return;
    downed_state_t &state = State(player);
    if (level.time >= state.next_frame)
    {
        state.frame = state.frame >= FRAME_crawl9 ? FRAME_crawl1 : state.frame + 1;
        state.next_frame = level.time + 100_ms;
    }
    player->client->buttons &= ~BUTTON_ATTACK;
    player->client->latched_buttons &= ~BUTTON_ATTACK;
    player->client->ps.gunindex = 0;
    RaidThirdPerson_SetPresentation(player, true, "models/monsters/insane/tris.md2", state.frame);
}

void RaidDowned_OnDeath(edict_t *player)
{
    if (ValidPlayer(player))
        LeaveDowned(player, false);
}

void RaidDowned_Disconnect(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    LeaveDowned(player, false);
    State(player) = {};
}

void RaidDowned_ResetAll()
{
    for (edict_t *player : active_players())
        if (RaidDowned_IsDown(player))
            LeaveDowned(player, true);
}
