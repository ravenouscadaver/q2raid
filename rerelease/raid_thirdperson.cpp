#include "g_local.h"
#include "raid_thirdperson.h"

namespace
{
struct raid_thirdperson_state_t
{
    bool enabled = false;
    bool carrying = false;
    edict_t *avatar = nullptr;
    edict_t *held_model = nullptr;
    const char *held_model_name = nullptr;
    float held_model_scale = 0.0f;
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

void DestroyHeldModel(edict_t *player)
{
    auto &state = StateFor(player);
    if (state.held_model && state.held_model->inuse)
        G_FreeEdict(state.held_model);
    state.held_model = nullptr;
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

    if (StateFor(player).carrying)
    {
        player_skinnum_t packed;
        packed.skinnum = avatar->s.skinnum;
        packed.vwep_index = 0;
        avatar->s.skinnum = packed.skinnum;
    }

    // Only the owner sees this duplicate; everyone else sees the real player.
    avatar->s.instance_bits = static_cast<uint8_t>(~ClientBit(player));
    avatar->svflags &= ~SVF_NOCLIENT;
    gi.linkentity(avatar);
}

void UpdateHeldModel(edict_t *player)
{
    auto &state = StateFor(player);
    if (!state.held_model || !state.held_model->inuse)
    {
        state.held_model = G_Spawn();
        state.held_model->classname = "raid_held_item_model";
        state.held_model->owner = player;
        state.held_model->solid = SOLID_NOT;
        state.held_model->movetype = MOVETYPE_NONE;
        gi.setmodel(state.held_model, state.held_model_name);
        state.held_model->s.renderfx = RF_GLOW | RF_NO_LOD;
        state.held_model->s.scale = state.held_model_scale;
    }

    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    state.held_model->s.origin = player->s.origin + (forward * 13.0f) + vec3_t{ 0, 0, 20.0f };
    state.held_model->s.angles = { 0, player->client->v_angle[YAW] + 90.0f, 0 };
    gi.linkentity(state.held_model);
}
}

void RaidThirdPerson_SetCarry(edict_t *player, bool carrying, const char *model, float scale)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;
    auto &state = StateFor(player);
    state.carrying = carrying;
    if (carrying)
    {
        state.held_model_name = model;
        state.held_model_scale = scale;
        if (!state.enabled) state.saved_instance_bits = player->s.instance_bits;
        state.enabled = true;
    }
    else
    {
        state.enabled = false;
        player->s.instance_bits = state.saved_instance_bits;
        DestroyAvatar(player);
        DestroyHeldModel(player);
        state.held_model_name = nullptr;
        state.held_model_scale = 0.0f;
        if (player->client->pers.weapon && player->client->pers.weapon->view_model)
            player->client->ps.gunindex = gi.modelindex(player->client->pers.weapon->view_model);
    }
}

void RaidThirdPerson_Toggle(edict_t *player)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;

    auto &state = StateFor(player);
    if (state.carrying)
    {
        gi.LocClient_Print(player, PRINT_HIGH, "Third person is locked while carrying a raid item\n");
        return;
    }
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
    if (state.carrying)
    {
        player_skinnum_t packed;
        packed.skinnum = player->s.skinnum;
        packed.vwep_index = 0;
        player->s.skinnum = packed.skinnum;
        UpdateHeldModel(player);
    }
    UpdateAvatar(player, state.avatar);

    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    const vec3_t eye = player->s.origin + vec3_t{ 0, 0, static_cast<float>(player->viewheight) };
    const vec3_t desired = eye - (forward * 88.0f) + vec3_t{ 0, 0, 18.0f };
    const trace_t trace = gi.trace(eye, { -4, -4, -4 }, { 4, 4, 4 }, desired, player, MASK_SOLID);
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
    DestroyHeldModel(player);
    state = {};
}
