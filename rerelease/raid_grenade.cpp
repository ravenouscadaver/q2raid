#include "g_local.h"
#include "raid_downed.h"
#include "raid_grenade.h"
#include "raid_items.h"

namespace
{
struct raid_grenade_state_t
{
    bool active = false;
    bool key_held = false;
    bool native_primed = false;
    bool native_completed = false;
    gitem_t *previous_weapon = nullptr;
};

raid_grenade_state_t states[MAX_CLIENTS];

bool ValidPlayer(edict_t *player)
{
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS;
}

raid_grenade_state_t &State(edict_t *player)
{
    return states[player->s.number - 1];
}

gitem_t *Grenades()
{
    return &itemlist[IT_AMMO_GRENADES];
}

bool CanRestore(edict_t *player, gitem_t *weapon)
{
    return weapon && weapon != Grenades() && weapon->weaponthink &&
        player->client->pers.inventory[weapon->id] > 0;
}

void Clear(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    State(player) = {};
}

void RestoreOrFinish(edict_t *player)
{
    raid_grenade_state_t &state = State(player);
    if (CanRestore(player, state.previous_weapon) &&
        player->client->pers.weapon != state.previous_weapon)
        player->client->newweapon = state.previous_weapon;
    state = {};
}
}

void RaidGrenade_Press(edict_t *player)
{
    if (!ValidPlayer(player))
        return;

    raid_grenade_state_t &state = State(player);
    if (state.active || player->deadflag || player->health <= 0 ||
        player->client->resp.spectator || RaidDowned_IsDown(player))
        return;

    gitem_t *grenades = Grenades();
    if (player->client->pers.inventory[IT_AMMO_GRENADES] <= 0 ||
        player->client->grenade_time || player->client->grenade_finished_time ||
        (player->client->pers.weapon == grenades && player->client->weaponstate == WEAPON_FIRING))
        return;

    state.active = true;
    state.key_held = true;
    state.previous_weapon = player->client->pers.weapon;

    if (RaidCarry_BlocksWeapons(player))
        RaidCarry_Drop(player);

    if (player->client->pers.weapon != grenades)
        player->client->newweapon = grenades;
}

void RaidGrenade_Release(edict_t *player)
{
    if (ValidPlayer(player) && State(player).active)
        State(player).key_held = false;
}

void RaidGrenade_FilterCommand(edict_t *player, usercmd_t &cmd)
{
    if (!ValidPlayer(player) || !State(player).active)
        return;

    raid_grenade_state_t &state = State(player);
    if (player->deadflag || player->health <= 0 || player->client->resp.spectator ||
        RaidDowned_IsDown(player))
    {
        state = {};
        cmd.buttons &= ~BUTTON_ATTACK;
        return;
    }

    if (!state.native_primed || state.key_held)
        cmd.buttons |= BUTTON_ATTACK;
    else
        cmd.buttons &= ~BUTTON_ATTACK;
}

void RaidGrenade_Update(edict_t *player)
{
    if (!ValidPlayer(player) || !State(player).active)
        return;

    raid_grenade_state_t &state = State(player);
    if (player->deadflag || player->health <= 0 || player->client->resp.spectator ||
        RaidDowned_IsDown(player))
    {
        state = {};
        return;
    }

    if (player->client->grenade_time || player->client->grenade_blew_up)
        state.native_primed = true;
    if (state.native_primed && !player->client->grenade_time)
        state.native_completed = true;

    // A last-grenade throw can run native auto-selection before this update,
    // so completion must be observed before testing the current weapon.
    if (state.native_completed && !player->client->grenade_finished_time &&
        player->client->weaponstate != WEAPON_FIRING)
    {
        RestoreOrFinish(player);
        return;
    }

    if (player->client->pers.weapon != Grenades() && player->client->newweapon != Grenades())
        RestoreOrFinish(player);
}

void RaidGrenade_OnDeath(edict_t *player)
{
    Clear(player);
}

void RaidGrenade_Disconnect(edict_t *player)
{
    Clear(player);
}

void RaidGrenade_ResetAll()
{
    for (raid_grenade_state_t &state : states)
        state = {};
}
