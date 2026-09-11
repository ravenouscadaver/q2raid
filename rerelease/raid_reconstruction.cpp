#include "g_local.h"
#include "raid_director.h"
#include "raid_reconstruction.h"

#include <algorithm>
#include <vector>

namespace
{
struct entity_ref_t
{
    uint32_t number = 0;
    int32_t spawn_count = 0;
};

struct chamber_state_t
{
    entity_ref_t chamber;
    entity_ref_t player;
};

struct reconstruction_player_state_t
{
    entity_ref_t player;
    bool viewing_chamber = false;
};

std::vector<chamber_state_t> chambers;
std::vector<reconstruction_player_state_t> waiting_players;

entity_ref_t Ref(edict_t *entity)
{
    return entity ? entity_ref_t{ entity->s.number, entity->spawn_count } : entity_ref_t{};
}

edict_t *Resolve(const entity_ref_t &ref)
{
    if (!ref.number || ref.number >= globals.num_edicts)
        return nullptr;
    edict_t *entity = &g_edicts[ref.number];
    return entity->inuse && entity->spawn_count == ref.spawn_count ? entity : nullptr;
}

bool Same(const entity_ref_t &ref, edict_t *entity)
{
    return entity && ref.number == entity->s.number && ref.spawn_count == entity->spawn_count;
}

bool IsChamber(edict_t *entity)
{
    return entity && entity->inuse && entity->classname && !Q_strcasecmp(entity->classname, "raid_gadget") &&
        entity->message && !Q_strcasecmp(entity->message, "reconstruction_chamber");
}

void DiscoverChambers()
{
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *entity = &g_edicts[i];
        if (!IsChamber(entity))
            continue;
        const bool known = std::any_of(chambers.begin(), chambers.end(), [entity](const chamber_state_t &state) {
            return Same(state.chamber, entity);
        });
        if (!known)
            chambers.push_back({ Ref(entity), {} });
    }
    std::sort(chambers.begin(), chambers.end(), [](const chamber_state_t &a, const chamber_state_t &b) {
        edict_t *left = Resolve(a.chamber);
        edict_t *right = Resolve(b.chamber);
        return (left ? left->count : 0) < (right ? right->count : 0);
    });
}

reconstruction_player_state_t *FindPlayerState(edict_t *player)
{
    auto found = std::find_if(waiting_players.begin(), waiting_players.end(), [player](const reconstruction_player_state_t &state) {
        return Same(state.player, player);
    });
    return found == waiting_players.end() ? nullptr : &*found;
}

chamber_state_t *FindAssignedChamber(edict_t *player)
{
    auto found = std::find_if(chambers.begin(), chambers.end(), [player](const chamber_state_t &state) {
        return Same(state.player, player);
    });
    return found == chambers.end() ? nullptr : &*found;
}

edict_t *NamedEntity(const char *targetname)
{
    return targetname && *targetname ? G_FindByString<&edict_t::targetname>(nullptr, targetname) : nullptr;
}

edict_t *SpawnMarker(edict_t *chamber)
{
    return chamber ? NamedEntity(chamber->deathtarget) : nullptr;
}

edict_t *CameraMarker(edict_t *chamber)
{
    return chamber ? NamedEntity(chamber->combattarget) : nullptr;
}

void AssignWaitingPlayers()
{
    DiscoverChambers();
    chambers.erase(std::remove_if(chambers.begin(), chambers.end(), [](const chamber_state_t &state) {
        return !IsChamber(Resolve(state.chamber));
    }), chambers.end());
    waiting_players.erase(std::remove_if(waiting_players.begin(), waiting_players.end(), [](const reconstruction_player_state_t &state) {
        edict_t *player = Resolve(state.player);
        return !player || !player->client || (player->health > 0 && !player->client->resp.spectator);
    }), waiting_players.end());

    for (chamber_state_t &state : chambers)
        if (edict_t *player = Resolve(state.player); !player || !FindPlayerState(player))
            state.player = {};

    for (reconstruction_player_state_t &waiting : waiting_players)
    {
        edict_t *player = Resolve(waiting.player);
        if (!player || FindAssignedChamber(player))
            continue;
        auto available = std::find_if(chambers.begin(), chambers.end(), [](const chamber_state_t &state) {
            return !Resolve(state.player);
        });
        if (available == chambers.end())
            break;
        available->player = waiting.player;
        if (edict_t *chamber = Resolve(available->chamber))
            RaidDirector_NotifyEntityEvent(chamber, "player_assigned", player);
    }
}

void SetCameraView(edict_t *player, chamber_state_t *assignment)
{
    if (!player || !assignment)
        return;
    edict_t *chamber = Resolve(assignment->chamber);
    edict_t *camera = CameraMarker(chamber);
    edict_t *look = SpawnMarker(chamber);
    if (!chamber || !camera)
        return;
    const vec3_t target = look ? look->s.origin : chamber->s.origin;
    const vec3_t angles = vectoangles(target - camera->s.origin);
    player->client->chase_target = nullptr;
    player->s.origin = camera->s.origin;
    player->client->ps.pmove.origin = camera->s.origin;
    player->client->v_angle = angles;
    player->client->ps.viewangles = angles;
    player->client->ps.pmove.pm_type = PM_FREEZE;
    player->velocity = {};
    gi.linkentity(player);
}
}

bool RaidReconstruction_OnDeath(edict_t *player)
{
    if (!player || !player->client || FindPlayerState(player))
        return false;
    DiscoverChambers();
    if (chambers.empty())
        return false;
    waiting_players.push_back({ Ref(player), true });
    AssignWaitingPlayers();
    return true;
}

void RaidReconstruction_OnDisconnect(edict_t *player)
{
    waiting_players.erase(std::remove_if(waiting_players.begin(), waiting_players.end(), [player](const reconstruction_player_state_t &state) {
        return Same(state.player, player);
    }), waiting_players.end());
    for (chamber_state_t &state : chambers)
        if (Same(state.player, player))
            state.player = {};
    AssignWaitingPlayers();
}

bool RaidReconstruction_IsQueued(edict_t *player)
{
    return player && FindPlayerState(player);
}

void RaidReconstruction_EnterSpectator(edict_t *player)
{
    reconstruction_player_state_t *state = FindPlayerState(player);
    if (!state)
        return;
    state->viewing_chamber = true;
    SetCameraView(player, FindAssignedChamber(player));
    gi.LocClient_Print(player, PRINT_HIGH, "RECONSTRUCTION PENDING - USE TO VIEW CHAMBER\n");
}

bool RaidReconstruction_HandleSpectatorInput(edict_t *player, usercmd_t *cmd)
{
    reconstruction_player_state_t *state = FindPlayerState(player);
    if (!state || !player->client->resp.spectator)
        return false;
    const bool attack_pressed = (cmd->buttons & BUTTON_ATTACK) && !(player->client->oldbuttons & BUTTON_ATTACK);
    const bool jump_pressed = (cmd->buttons & BUTTON_JUMP) && !(player->client->oldbuttons & BUTTON_JUMP);
    const bool use_pressed = (cmd->buttons & BUTTON_USE) && !(player->client->oldbuttons & BUTTON_USE);
    if (state->viewing_chamber && (attack_pressed || jump_pressed))
    {
        state->viewing_chamber = false;
        return false;
    }
    if (!state->viewing_chamber && use_pressed)
        state->viewing_chamber = true;
    if (!state->viewing_chamber)
        return false;
    SetCameraView(player, FindAssignedChamber(player));
    return true;
}

bool RaidReconstruction_CanDeposit(edict_t *chamber)
{
    AssignWaitingPlayers();
    if (!IsChamber(chamber))
        return false;
    auto found = std::find_if(chambers.begin(), chambers.end(), [chamber](const chamber_state_t &state) {
        return Same(state.chamber, chamber) && Resolve(state.player);
    });
    return found != chambers.end();
}

bool RaidReconstruction_Complete(edict_t *chamber, edict_t *rescuer)
{
    AssignWaitingPlayers();
    auto assignment = std::find_if(chambers.begin(), chambers.end(), [chamber](const chamber_state_t &state) {
        return Same(state.chamber, chamber);
    });
    if (assignment == chambers.end())
        return false;
    edict_t *player = Resolve(assignment->player);
    edict_t *spawn = SpawnMarker(chamber);
    if (!player || !spawn)
    {
        gi.Com_PrintFmt("[raid] reconstruction chamber '{}' lacks an assigned player or reconstruct_spawn\n",
            chamber->targetname ? chamber->targetname : "<unnamed>");
        return false;
    }

    assignment->player = {};
    waiting_players.erase(std::remove_if(waiting_players.begin(), waiting_players.end(), [player](const reconstruction_player_state_t &state) {
        return Same(state.player, player);
    }), waiting_players.end());
    RaidRespawnAt(player, spawn->s.origin, spawn->s.angles);
    RaidDirector_NotifyEntityEvent(chamber, "reconstructed", rescuer);
    AssignWaitingPlayers();
    return true;
}

void RaidReconstruction_RunFrame()
{
    AssignWaitingPlayers();
    for (const chamber_state_t &state : chambers)
    {
        edict_t *chamber = Resolve(state.chamber);
        edict_t *player = Resolve(state.player);
        edict_t *marker = SpawnMarker(chamber);
        if (!chamber || !player || !marker)
            continue;
        gi.Draw_OrientedWorldText(marker->s.origin + vec3_t{ 0, 0, 32 }, player->client->pers.netname,
            rgba_cyan, 0.2f, FRAME_TIME_MS.seconds(), true);
    }
}

void RaidReconstruction_Reset()
{
    waiting_players.clear();
    chambers.clear();
}
