#include "g_local.h"
#include "raid_thirdperson.h"

namespace
{
struct raid_thirdperson_state_t
{
    bool enabled = false;
    edict_t *avatar = nullptr;
    uint8_t saved_instance_bits = 0;
};

raid_thirdperson_state_t states[MAX_CLIENTS];

raid_thirdperson_state_t &StateFor(edict_t *player)
{
    return states[player->s.number - 1];
}

uint8_t ClientBit(edict_t *player)
{
    return static_cast<uint8_t>(1u << (player->s.number - 1));
}

void DestroyAvatar(edict_t *player)
{
    auto &state = StateFor(player);
    if (state.avatar && state.avatar->inuse)
        G_FreeEdict(state.avatar);
    state.avatar = nullptr;
}

edict_t *CreateAvatar(edict_t *player)
{
    edict_t *avatar = G_Spawn();
    avatar->classname = "raid_thirdperson_avatar";
    avatar->owner = player;
    avatar->solid = SOLID_NOT;
    avatar->movetype = MOVETYPE_NONE;
    return avatar;
}

void UpdateAvatar(edict_t *player, edict_t *avatar)
{
    const uint32_t avatar_number = avatar->s.number;
    avatar->s = player->s;
    avatar->s.number = avatar_number;
    avatar->s.modelindex = MODELINDEX_PLAYER;

    // Animation-compatible zero-asset stand-in for a two-handed power core.
    player_skinnum_t packed;
    packed.skinnum = avatar->s.skinnum;
    const gitem_t *bfg = GetItemByIndex(IT_WEAPON_BFG);
    if (bfg)
        packed.vwep_index = bfg->vwep_index - level.vwep_offset + 1;
    avatar->s.skinnum = packed.skinnum;

    // Only the owner sees this duplicate; everyone else sees the real player.
    avatar->s.instance_bits = static_cast<uint8_t>(~ClientBit(player));
    avatar->svflags &= ~SVF_NOCLIENT;
    gi.linkentity(avatar);
}
}

void RaidThirdPerson_Toggle(edict_t *player)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;

    auto &state = StateFor(player);
    state.enabled = !state.enabled;

    if (state.enabled)
    {
        state.saved_instance_bits = player->s.instance_bits;
        gi.LocClient_Print(player, PRINT_HIGH, "Raid third-person carry test ON\n");
    }
    else
    {
        player->s.instance_bits = state.saved_instance_bits;
        DestroyAvatar(player);
        gi.LocClient_Print(player, PRINT_HIGH, "Raid third-person carry test OFF\n");
    }
}

void RaidThirdPerson_Update(edict_t *player)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;

    auto &state = StateFor(player);
    if (!state.enabled || player->deadflag || player->client->resp.spectator)
        return;

    if (!state.avatar || !state.avatar->inuse)
        state.avatar = CreateAvatar(player);

    player->s.instance_bits = static_cast<uint8_t>(state.saved_instance_bits | ClientBit(player));
    UpdateAvatar(player, state.avatar);

    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    const vec3_t eye = player->s.origin + vec3_t{ 0, 0, static_cast<float>(player->viewheight) };
    const vec3_t desired = eye - (forward * 72.0f) + vec3_t{ 0, 0, 18.0f };
    const trace_t trace = gi.traceline(eye, desired, player, MASK_SOLID);
    player->client->ps.viewoffset = trace.endpos - player->s.origin;
    player->client->ps.gunindex = 0;
}

void RaidThirdPerson_Disconnect(edict_t *player)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;

    auto &state = StateFor(player);
    player->s.instance_bits = state.saved_instance_bits;
    DestroyAvatar(player);
    state = {};
}

