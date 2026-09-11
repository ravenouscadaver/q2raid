#include "g_local.h"
#include "raid_items.h"
#include "raid_thirdperson.h"

namespace
{
struct raid_thirdperson_state_t
{
    bool enabled = false;
    bool carrying = false;
    uint32_t avatar_number = 0;
    int32_t avatar_spawn_count = 0;
    uint32_t held_model_number = 0;
    int32_t held_model_spawn_count = 0;
    std::string held_model_name;
    float held_model_scale = 0.0f;
    bool held_item_charged = false;
    uint8_t saved_instance_bits = 0;
    vec3_t previous_camera;
    bool previous_camera_valid = false;
    bool presentation = false;
    bool presentation_restore_enabled = false;
    std::string presentation_model;
    int presentation_frame = 0;
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

edict_t *Resolve(uint32_t number, int32_t spawn_count)
{
    if (!number || number >= globals.num_edicts) return nullptr;
    edict_t *entity = &g_edicts[number];
    return entity->inuse && entity->spawn_count == spawn_count ? entity : nullptr;
}

void DestroyAvatar(edict_t *player)
{
    auto &state = StateFor(player);
    if (edict_t *avatar = Resolve(state.avatar_number, state.avatar_spawn_count))
        G_FreeEdict(avatar);
    state.avatar_number = 0;
    state.avatar_spawn_count = 0;
}

void DestroyHeldModel(edict_t *player)
{
    auto &state = StateFor(player);
    if (edict_t *held = Resolve(state.held_model_number, state.held_model_spawn_count))
        G_FreeEdict(held);
    state.held_model_number = 0;
    state.held_model_spawn_count = 0;
}

void RestoreFirstPersonWeapon(edict_t *player)
{
    if (!player || !player->client)
        return;
    player->client->ps.gunindex = player->client->pers.weapon && player->client->pers.weapon->view_model
        ? gi.modelindex(player->client->pers.weapon->view_model)
        : 0;
    player->client->ps.gunskin = 0;
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
    if (StateFor(player).presentation && !StateFor(player).presentation_model.empty())
    {
        avatar->s.modelindex = gi.modelindex(StateFor(player).presentation_model.c_str());
        avatar->s.frame = StateFor(player).presentation_frame;
        // old_frame was copied from the player model and may not exist in the
        // replacement MD2. KEX renders/interpolates both and asserts on an
        // out-of-range frame, so replacement presentations own the pair.
        avatar->s.old_frame = avatar->s.frame;
        avatar->s.modelindex2 = 0;
        avatar->s.modelindex3 = 0;
        avatar->s.modelindex4 = 0;
        // Player skinnum also packs client/vwep data and is not a valid skin
        // index for an arbitrary replacement model.
        avatar->s.skinnum = 0;
    }
    else
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

void SetPresentationInternal(edict_t *player, bool enabled, const char *model, int frame)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;
    auto &state = StateFor(player);
    if (enabled)
    {
        if (!state.presentation)
        {
            state.presentation_restore_enabled = state.enabled;
            if (!state.enabled)
                state.saved_instance_bits = player->s.instance_bits;
        }
        state.presentation = true;
        state.enabled = true;
        state.presentation_model = model ? model : "";
        state.presentation_frame = frame;
    }
    else if (state.presentation)
    {
        state.presentation = false;
        state.presentation_model.clear();
        state.presentation_frame = 0;
        if (!state.presentation_restore_enabled && !state.carrying)
        {
            state.enabled = false;
            player->s.instance_bits = state.saved_instance_bits;
            DestroyAvatar(player);
            state.previous_camera_valid = false;
            RestoreFirstPersonWeapon(player);
        }
    }
}

void UpdateHeldModel(edict_t *player)
{
    auto &state = StateFor(player);
    edict_t *held = Resolve(state.held_model_number, state.held_model_spawn_count);
    if (!held)
    {
        held = G_Spawn();
        state.held_model_number = held->s.number;
        state.held_model_spawn_count = held->spawn_count;
        held->classname = "raid_held_item_model";
        held->owner = player;
        held->solid = SOLID_NOT;
        held->movetype = MOVETYPE_NONE;
        gi.setmodel(held, state.held_model_name.c_str());
        held->s.renderfx = RF_GLOW | RF_NO_LOD;
        held->s.scale = state.held_model_scale;
    }

    held->s.sound = state.held_item_charged ? gi.soundindex("weapons/rg_hum.wav") : 0;
    held->s.loop_attenuation = ATTN_NORM;
    held->s.loop_volume = state.held_item_charged ? 0.7f : 0.0f;
    held->s.origin = RaidCarry_HeldOrigin(player);
    held->s.angles = { 0, player->client->v_angle[YAW] + 90.0f, 0 };
    gi.linkentity(held);
}
}

void RaidThirdPerson_SetPresentation(edict_t *player, bool enabled, const char *model, int frame)
{
    SetPresentationInternal(player, enabled, model, frame);
}

void RaidThirdPerson_SetCarry(edict_t *player, bool carrying, const char *model, float scale)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;
    auto &state = StateFor(player);
    state.carrying = carrying;
    if (carrying)
    {
        state.held_model_name = model ? model : "models/items/keys/power/tris.md2";
        state.held_model_scale = scale;
        if (!state.enabled) state.saved_instance_bits = player->s.instance_bits;
        state.enabled = true;
    }
    else
    {
        state.enabled = state.presentation;
        if (!state.presentation)
        {
            player->s.instance_bits = state.saved_instance_bits;
            DestroyAvatar(player);
        }
        DestroyHeldModel(player);
        state.held_model_name.clear();
        state.held_model_scale = 0.0f;
        state.held_item_charged = false;
        state.previous_camera_valid = false;
        RestoreFirstPersonWeapon(player);
    }
}

void RaidThirdPerson_SetCarryCharged(edict_t *player, bool charged)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;
    StateFor(player).held_item_charged = charged;
}

void RaidThirdPerson_Toggle(edict_t *player)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;

    auto &state = StateFor(player);
    if (state.presentation)
    {
        gi.LocClient_Print(player, PRINT_HIGH, "Third person is locked by the current presentation\n");
        return;
    }
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
        state.previous_camera_valid = false;
        RestoreFirstPersonWeapon(player);
        gi.LocClient_Print(player, PRINT_HIGH, "Raid third-person carry test OFF\n");
    }
}

void RaidThirdPerson_Update(edict_t *player)
{
    if (!player || !player->client || player->s.number < 1 || player->s.number > MAX_CLIENTS)
        return;

    auto &state = StateFor(player);
    if (!state.enabled)
        return;
    if (player->deadflag || player->client->resp.spectator)
    {
        player->s.instance_bits = state.saved_instance_bits;
        DestroyAvatar(player);
        DestroyHeldModel(player);
        state.previous_camera_valid = false;
        return;
    }

    edict_t *avatar = Resolve(state.avatar_number, state.avatar_spawn_count);
    if (!avatar)
    {
        avatar = CreateAvatar(player);
        state.avatar_number = avatar->s.number;
        state.avatar_spawn_count = avatar->spawn_count;
    }

    player->s.instance_bits = static_cast<uint8_t>(state.saved_instance_bits | ClientBit(player));
    if (state.carrying)
    {
        player_skinnum_t packed;
        packed.skinnum = player->s.skinnum;
        packed.vwep_index = 0;
        player->s.skinnum = packed.skinnum;
        UpdateHeldModel(player);
    }
    UpdateAvatar(player, avatar);

    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    const vec3_t eye = player->s.origin + vec3_t{ 0, 0, static_cast<float>(player->viewheight) };
    const vec3_t desired = eye - (forward * 88.0f) + vec3_t{ 0, 0, 18.0f };
    trace_t trace = gi.trace(eye, { -6, -6, -8 }, { 6, 6, 8 }, desired, player, MASK_SOLID);
    vec3_t camera = trace.endpos;
    if (trace.startsolid || trace.allsolid)
        camera = eye;
    else if (trace.fraction < 1.0f)
        camera += trace.plane.normal * 4.0f;
    if (state.previous_camera_valid && (state.previous_camera - camera).length() < 256.0f)
    {
        const trace_t sweep = gi.trace(state.previous_camera, { -6, -6, -8 }, { 6, 6, 8 }, camera, player, MASK_SOLID);
        if (!sweep.startsolid && !sweep.allsolid)
            camera = sweep.endpos + (sweep.fraction < 1.0f ? sweep.plane.normal * 4.0f : vec3_origin);
    }
    state.previous_camera = camera;
    state.previous_camera_valid = true;
    player->client->ps.viewoffset = camera - player->s.origin;
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
