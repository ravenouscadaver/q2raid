// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "raid_director.h"
#include "raid_items.h"
#include "raid_monsters.h"
#include "raid_hats.h"
#include "raid_downed.h"
#include "raid_reconstruction.h"
#include "raid_bots.h"
#include "raid_ui.h"

#include "json/json.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <deque>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct raid_director_runtime_t
{
    bool        initialized = false;
    bool        loaded = false;
    std::string mapname;
    std::string script_path;
    std::string encounter_name;
    std::string state;
    Json::Value document;
    bool        wipe_pending = false;
    gtime_t     wipe_reset_at;
};

struct raid_color_cycle_t
{
    std::vector<std::string> targets;
    std::vector<rgba_t>      colors;
    gtime_t                  started_at;
    float                    step_seconds = 0.5f;
    float                    transition_seconds = 0.5f;
};

struct raid_player_status_t
{
    uint32_t    entity_number = 0;
    int32_t     spawn_count = 0;
    std::string name;
    gtime_t     expires_at;
    std::string expire_message;
    Json::Value on_expire;
};

struct raid_entity_snapshot_t
{
    uint32_t entity_number = 0;
    int32_t spawn_count = 0;
    entity_state_t state = {};
    std::string classname;
    std::string model;
    std::string target;
    std::string targetname;
    std::string team;
    std::string message;
    item_id_t item_id = IT_NULL;
    bool map_pickup = false;
    std::bitset<MAX_CLIENTS> item_picked_up_by;
    solid_t solid = SOLID_NOT;
    save_touch_t touch;
    save_use_t use;
    float wait = 0.0f;
    float moveinfo_wait = 0.0f;
    move_state_t moveinfo_state = STATE_BOTTOM;
    bool moveinfo_reversing = false;
    vec3_t moveinfo_dir, moveinfo_dest;
    float moveinfo_current_speed = 0.0f;
    float moveinfo_move_speed = 0.0f;
    float moveinfo_next_speed = 0.0f;
    float moveinfo_remaining_distance = 0.0f;
    float moveinfo_decel_distance = 0.0f;
    save_moveinfo_endfunc_t moveinfo_endfunc;
    int32_t skinnum = 0;
    int32_t frame = 0;
    int32_t modelindex = 0;
    float alpha = 1.0f;
    vec3_t origin, angles, velocity, avelocity;
    vec3_t mins, maxs;
    svflags_t svflags;
    contents_t clipmask;
    movetype_t movetype = MOVETYPE_NONE;
    ent_flags_t flags;
    spawnflags_t spawnflags = SPAWNFLAG_NONE;
    gtime_t nextthink;
    save_think_t think;
    int32_t health = 0, max_health = 0, count = 0;
    item_id_t power_armor_type = IT_NULL;
    int32_t power_armor_power = 0;
    item_id_t initial_power_armor_type = IT_NULL;
    int32_t max_power_armor_power = 0;
    bool deadflag = false, takedamage = false;
    effects_t effects;
    renderfx_t renderfx;
    int32_t sound = 0;
    std::array<uint8_t, sizeof(edict_t)> baseline = {};
    gtime_t timestamp;
    int32_t sounds = 0;
};

struct raid_player_snapshot_t
{
    uint32_t entity_number = 0;
    int32_t spawn_count = 0;
    client_persistant_t pers = {};
};

raid_director_runtime_t director;
std::vector<raid_color_cycle_t> color_cycles;
std::vector<raid_player_status_t> player_statuses;
std::vector<raid_entity_snapshot_t> entity_snapshots;
std::vector<raid_player_snapshot_t> player_snapshots;
cvar_t *raid_script = nullptr;
cvar_t *raid_script_root = nullptr;
cvar_t *raid_autoload = nullptr;
cvar_t *raid_game_dir = nullptr;
gtime_t raid_message_expires;
int raid_message_priority = 0;

struct queued_raid_event_t
{
    std::string source;
    std::string signal;
    uint32_t activator_number = 0;
    int32_t activator_spawn_count = 0;
};
std::deque<queued_raid_event_t> queued_events;
bool dispatching_events = false;

void RaidDirector_ExecuteOperations(const Json::Value &operations, const std::string &context, edict_t *activator);
void RaidDirector_ClearStatusHUD(edict_t *player);

void RaidDirector_SnapshotEntity(edict_t *entity)
{
    if (!entity || std::any_of(entity_snapshots.begin(), entity_snapshots.end(), [entity](const raid_entity_snapshot_t &snapshot) {
        return snapshot.entity_number == entity->s.number && snapshot.spawn_count == entity->spawn_count;
    }))
        return;
    raid_entity_snapshot_t snapshot;
    snapshot.entity_number = entity->s.number;
    snapshot.spawn_count = entity->spawn_count;
    snapshot.state = entity->s;
    snapshot.classname = entity->classname ? entity->classname : "";
    snapshot.model = entity->model ? entity->model : "";
    snapshot.target = entity->target ? entity->target : "";
    snapshot.targetname = entity->targetname ? entity->targetname : "";
    snapshot.team = entity->team ? entity->team : "";
    snapshot.message = entity->message ? entity->message : "";
    snapshot.item_id = entity->item ? entity->item->id : IT_NULL;
    snapshot.map_pickup = entity->item &&
        !entity->spawnflags.has(SPAWNFLAG_ITEM_DROPPED | SPAWNFLAG_ITEM_DROPPED_PLAYER);
    snapshot.item_picked_up_by = entity->item_picked_up_by;
    snapshot.solid = entity->solid;
    snapshot.touch = entity->touch;
    snapshot.use = entity->use;
    snapshot.wait = entity->wait;
    snapshot.moveinfo_wait = entity->moveinfo.wait;
    snapshot.moveinfo_state = entity->moveinfo.state;
    snapshot.moveinfo_reversing = entity->moveinfo.reversing;
    snapshot.moveinfo_dir = entity->moveinfo.dir;
    snapshot.moveinfo_dest = entity->moveinfo.dest;
    snapshot.moveinfo_current_speed = entity->moveinfo.current_speed;
    snapshot.moveinfo_move_speed = entity->moveinfo.move_speed;
    snapshot.moveinfo_next_speed = entity->moveinfo.next_speed;
    snapshot.moveinfo_remaining_distance = entity->moveinfo.remaining_distance;
    snapshot.moveinfo_decel_distance = entity->moveinfo.decel_distance;
    snapshot.moveinfo_endfunc = entity->moveinfo.endfunc;
    snapshot.skinnum = entity->s.skinnum;
    snapshot.frame = entity->s.frame;
    snapshot.modelindex = entity->s.modelindex;
    snapshot.alpha = entity->s.alpha;
    snapshot.origin = entity->s.origin;
    snapshot.angles = entity->s.angles;
    snapshot.velocity = entity->velocity;
    snapshot.avelocity = entity->avelocity;
    snapshot.mins = entity->mins;
    snapshot.maxs = entity->maxs;
    snapshot.svflags = entity->svflags;
    snapshot.clipmask = entity->clipmask;
    snapshot.movetype = entity->movetype;
    snapshot.flags = entity->flags;
    snapshot.spawnflags = entity->spawnflags;
    snapshot.nextthink = entity->nextthink;
    snapshot.think = entity->think;
    snapshot.health = entity->health;
    snapshot.max_health = entity->max_health;
    snapshot.count = entity->count;
    snapshot.power_armor_type = entity->monsterinfo.power_armor_type;
    snapshot.power_armor_power = entity->monsterinfo.power_armor_power;
    snapshot.initial_power_armor_type = entity->monsterinfo.initial_power_armor_type;
    snapshot.max_power_armor_power = entity->monsterinfo.max_power_armor_power;
    snapshot.deadflag = entity->deadflag;
    snapshot.takedamage = entity->takedamage;
    snapshot.effects = entity->s.effects;
    snapshot.renderfx = entity->s.renderfx;
    snapshot.sound = entity->s.sound;
    std::memcpy(snapshot.baseline.data(), entity, sizeof(edict_t));
    snapshot.timestamp = entity->timestamp;
    snapshot.sounds = entity->sounds;
    entity_snapshots.push_back(std::move(snapshot));
}

void RaidDirector_CaptureNewPlayers()
{
    for (edict_t *player : active_players())
    {
        auto captured = std::find_if(player_snapshots.begin(), player_snapshots.end(), [player](const raid_player_snapshot_t &snapshot) {
            return snapshot.entity_number == player->s.number;
        });
        if (captured == player_snapshots.end())
            player_snapshots.push_back({ player->s.number, player->spawn_count, player->client->pers });
    }
}

void RaidDirector_RemovePlayerSnapshot(edict_t *player)
{
    if (!player)
        return;
    player_snapshots.erase(std::remove_if(player_snapshots.begin(), player_snapshots.end(), [player](const raid_player_snapshot_t &snapshot) {
        return snapshot.entity_number == player->s.number;
    }), player_snapshots.end());
}

void RaidDirector_RestorePlayers()
{
    for (const raid_player_snapshot_t &snapshot : player_snapshots)
    {
        if (!snapshot.entity_number || snapshot.entity_number >= globals.num_edicts)
            continue;
        edict_t *player = &g_edicts[snapshot.entity_number];
        if (!player->inuse || !player->client)
            continue;
        player->client->pers = snapshot.pers;
        player->client->newweapon = snapshot.pers.weapon;
        ChangeWeapon(player);
    }
}

void RaidDirector_CaptureEntityBaseline()
{
    entity_snapshots.clear();
    player_snapshots.clear();
    // Player corpses live in Quake II's fixed body queue. Leave that queue out
    // of encounter restoration so failed fireteams can remain as world history;
    // the engine naturally cycles the oldest corpse after BODY_QUEUE_SIZE deaths.
    for (uint32_t i = game.maxclients + BODY_QUEUE_SIZE + 1; i < globals.num_edicts; ++i)
        if (g_edicts[i].inuse)
            RaidDirector_SnapshotEntity(&g_edicts[i]);
    gi.Com_PrintFmt("[raid] Captured reset baseline for {} map entities\n", entity_snapshots.size());
}

void RaidDirector_RestoreEntitySnapshots()
{
    // Anything created after the map baseline is encounter debris, regardless
    // of whether its creator remembered to set a conventional dropped/gib flag.
    for (uint32_t i = game.maxclients + BODY_QUEUE_SIZE + 1; i < globals.num_edicts; ++i)
    {
        edict_t *entity = &g_edicts[i];
        if (!entity->inuse)
            continue;
        const bool baseline_entity = std::any_of(entity_snapshots.begin(), entity_snapshots.end(), [entity](const raid_entity_snapshot_t &snapshot) {
            return snapshot.entity_number == entity->s.number && snapshot.spawn_count == entity->spawn_count;
        });
        if (!baseline_entity)
            G_FreeEdict(entity);
    }

    for (raid_entity_snapshot_t &snapshot : entity_snapshots)
    {
        edict_t *entity = snapshot.entity_number < globals.num_edicts ? &g_edicts[snapshot.entity_number] : nullptr;
        if (!entity || !entity->inuse || entity->spawn_count != snapshot.spawn_count)
        {
            entity = G_Spawn();
            const int32_t entity_number = entity->s.number;
            const int32_t spawn_count = entity->spawn_count;
            std::memcpy(entity, snapshot.baseline.data(), sizeof(edict_t));
            entity->s.number = entity_number;
            entity->spawn_count = spawn_count;
            snapshot.entity_number = entity->s.number;
            snapshot.spawn_count = entity->spawn_count;
            gi.linkentity(entity);
            continue;
        }
        const int32_t entity_number = entity->s.number;
        entity->s = snapshot.state;
        entity->s.number = entity_number;
        entity->solid = snapshot.solid;
        entity->touch = snapshot.touch;
        entity->use = snapshot.use;
        entity->wait = snapshot.wait;
        entity->moveinfo.wait = snapshot.moveinfo_wait;
        entity->moveinfo.state = snapshot.moveinfo_state;
        entity->moveinfo.reversing = snapshot.moveinfo_reversing;
        entity->moveinfo.dir = snapshot.moveinfo_dir;
        entity->moveinfo.dest = snapshot.moveinfo_dest;
        entity->moveinfo.current_speed = snapshot.moveinfo_current_speed;
        entity->moveinfo.move_speed = snapshot.moveinfo_move_speed;
        entity->moveinfo.next_speed = snapshot.moveinfo_next_speed;
        entity->moveinfo.remaining_distance = snapshot.moveinfo_remaining_distance;
        entity->moveinfo.decel_distance = snapshot.moveinfo_decel_distance;
        entity->moveinfo.endfunc = snapshot.moveinfo_endfunc;
        entity->velocity = snapshot.velocity;
        entity->avelocity = snapshot.avelocity;
        entity->mins = snapshot.mins;
        entity->maxs = snapshot.maxs;
        entity->svflags = snapshot.svflags;
        entity->clipmask = snapshot.clipmask;
        entity->movetype = snapshot.movetype;
        entity->flags = snapshot.flags;
        entity->spawnflags = snapshot.spawnflags;
        entity->nextthink = snapshot.nextthink;
        entity->think = snapshot.think;
        entity->health = snapshot.health;
        entity->max_health = snapshot.max_health;
        entity->count = snapshot.count;
        entity->monsterinfo.power_armor_type = snapshot.power_armor_type;
        entity->monsterinfo.power_armor_power = snapshot.power_armor_power;
        entity->monsterinfo.initial_power_armor_type = snapshot.initial_power_armor_type;
        entity->monsterinfo.max_power_armor_power = snapshot.max_power_armor_power;
        entity->item_picked_up_by = snapshot.item_picked_up_by;
        entity->deadflag = snapshot.deadflag;
        entity->takedamage = snapshot.takedamage;
        entity->timestamp = snapshot.timestamp;
        entity->sounds = snapshot.sounds;
        gi.linkentity(entity);
    }
}

void RaidDirector_ClearTransientState()
{
    for (edict_t *player : active_players())
    {
        RaidDirector_ClearStatusHUD(player);
        player->client->raid_flash_fade_at = 0_ms;
        player->client->raid_flash_end = 0_ms;
        player->client->raid_flash_alpha = 0.0f;
    }
    color_cycles.clear();
    player_statuses.clear();
    queued_events.clear();
    dispatching_events = false;
    director.wipe_pending = false;
    director.wipe_reset_at = 0_ms;
    raid_message_expires = 0_ms;
    raid_message_priority = 0;
    if (director.initialized)
        gi.configstring(CONFIG_RAID_MESSAGE, "");
}

constexpr std::array<player_stat_t, 4> raid_status_stats = {
    STAT_RAID_STATUS, STAT_RAID_STATUS_2, STAT_RAID_STATUS_3, STAT_RAID_STATUS_4
};
constexpr std::array<player_stat_t, 4> raid_status_time_stats = {
    STAT_RAID_STATUS_TIME, STAT_RAID_STATUS_TIME_2, STAT_RAID_STATUS_TIME_3, STAT_RAID_STATUS_TIME_4
};

void RaidDirector_ClearStatusHUD(edict_t *player)
{
    if (!player || !player->client)
        return;
    for (size_t slot = 0; slot < raid_status_stats.size(); ++slot)
    {
        player->client->ps.stats[raid_status_stats[slot]] = 0;
        player->client->ps.stats[raid_status_time_stats[slot]] = 0;
    }
}

void RaidDirector_ClearDocument()
{
    if (director.loaded && director.document["states"].isMember(director.state))
        RaidDirector_ExecuteOperations(director.document["states"][director.state]["exit"], fmt::format("state '{}' exit", director.state), nullptr);

    RaidDirector_RestoreEntitySnapshots();
    RaidCarry_ResetAll();
    RaidHover_Reset();
    RaidMonsters_Reset();
    RaidHats_Reset();
    RaidDowned_ResetAll();
    RaidReconstruction_Reset();
    RaidBots_Reset();
    RaidUI_Reset();
    RaidDirector_ClearTransientState();

    director.loaded = false;
    director.script_path.clear();
    director.encounter_name.clear();
    director.state.clear();
    director.document = Json::Value();
}

void RaidDirector_PostMessage(edict_t *activator, const std::string &message, bool broadcast)
{
    if (message.empty())
        return;

    if (broadcast)
    {
        for (edict_t *player : active_players())
            gi.LocCenter_Print(player, "{}", message.c_str());
    }
    else if (activator && activator->client)
    {
        gi.LocCenter_Print(activator, "{}", message.c_str());
    }
}

void RaidDirector_PostEncounterMessage(const Json::Value &operation)
{
    const std::string text = operation.get("text", "").asString();
    const int priority = operation.get("priority", 0).asInt();
    if (text.empty() || (raid_message_expires > level.time && priority < raid_message_priority))
        return;

    gi.configstring(CONFIG_RAID_MESSAGE, text.c_str());
    raid_message_priority = priority;
    raid_message_expires = level.time + gtime_t::from_sec(std::max(0.1f, operation.get("duration", 5.0f).asFloat()));
}

int RaidDirector_StatusCode(const std::string &name)
{
    if (Q_strcasecmp(name.c_str(), "volatile") == 0)
        return 1;
    if (Q_strcasecmp(name.c_str(), "doomsday") == 0)
        return 2;
    return 0;
}

void RaidDirector_ClearPlayerStatus(edict_t *player, const std::string &name)
{
    if (!player || !player->client)
        return;

    player_statuses.erase(std::remove_if(player_statuses.begin(), player_statuses.end(),
        [player, &name](const raid_player_status_t &status) {
            return status.entity_number == player->s.number && (name.empty() || Q_strcasecmp(status.name.c_str(), name.c_str()) == 0);
        }), player_statuses.end());
}

void RaidDirector_ApplyPlayerStatus(edict_t *player, const Json::Value &operation)
{
    if (!player || !player->client)
    {
        gi.Com_Print("[raid] apply_status requires a player activator\n");
        return;
    }

    const std::string name = operation.get("status", "").asString();
    const int code = RaidDirector_StatusCode(name);
    if (!code)
    {
        gi.Com_PrintFmt("[raid] unknown status '{}'\n", name);
        return;
    }

    const float duration = std::max(0.1f, operation.get("duration", 15.0f).asFloat());
    const std::string policy = operation.get("stack_policy", "refresh").asString();
    auto existing = std::find_if(player_statuses.begin(), player_statuses.end(), [player, &name](const raid_player_status_t &status) {
        return status.entity_number == player->s.number && Q_strcasecmp(status.name.c_str(), name.c_str()) == 0;
    });
    if (policy == "ignore" && existing != player_statuses.end())
        return;
    if (policy == "extend" && existing != player_statuses.end())
    {
        existing->expires_at += gtime_t::from_sec(duration);
        existing->expire_message = operation.get("expire_message", existing->expire_message).asString();
        existing->on_expire = operation["on_expire"];
        return;
    }
    if (policy == "replace")
        RaidDirector_ClearPlayerStatus(player, "");
    else if (policy != "stack")
        RaidDirector_ClearPlayerStatus(player, name);

    player_statuses.push_back({ player->s.number, player->spawn_count, name,
        level.time + gtime_t::from_sec(duration), operation.get("expire_message", "").asString(), operation["on_expire"] });
}

void ApplyStatusFromDefinition(edict_t *player, const char *status, float duration, const char *stack_policy)
{
    Json::Value operation = status && director.document["statuses"][status].isObject()
        ? director.document["statuses"][status]
        : Json::Value(Json::objectValue);
    operation["status"] = status ? status : "";
    if (duration > 0.0f)
        operation["duration"] = duration;
    operation["stack_policy"] = stack_policy && *stack_policy ? stack_policy : "refresh";
    RaidDirector_ApplyPlayerStatus(player, operation);
}

void ClearStatusByName(edict_t *player, const char *status)
{
    RaidDirector_ClearPlayerStatus(player, status ? status : "");
}

float StatusDurationFromDefinition(const char *status, float fallback)
{
    if (status && director.document["statuses"][status]["duration"].isNumeric())
        return std::max(0.1f, director.document["statuses"][status]["duration"].asFloat());
    return std::max(0.1f, fallback);
}

int32_t RaidDirector_PackColor(const rgba_t &color)
{
    return color.a | (color.b << 8) | (color.g << 16) | (color.r << 24);
}

bool RaidDirector_ReadColor(const Json::Value &value, rgba_t &color)
{
    if (!value.isArray() || value.size() < 3 || value.size() > 4)
        return false;

    for (Json::ArrayIndex i = 0; i < value.size(); ++i)
        if (!value[i].isNumeric() || value[i].asInt() < 0 || value[i].asInt() > 255)
            return false;

    color = {
        static_cast<uint8_t>(value[0].asInt()),
        static_cast<uint8_t>(value[1].asInt()),
        static_cast<uint8_t>(value[2].asInt()),
        static_cast<uint8_t>(value.size() == 4 ? value[3].asInt() : 255)
    };
    return true;
}

void RaidDirector_SetNamedColor(const std::string &targetname, const rgba_t &color)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        RaidDirector_SnapshotEntity(entity);
        entity->s.skinnum = RaidDirector_PackColor(color);
        ++matches;
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] color target '{}' not found\n", targetname);
}

void RaidDirector_FireTarget(const std::string &targetname, edict_t *activator)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        ++matches;
        if (entity->use)
            entity->use(entity, activator ? activator : entity, activator ? activator : entity);
        else
            gi.Com_PrintFmt("[raid] target '{}' ({}) has no use function\n", targetname, entity->classname);
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] fire target '{}' not found\n", targetname);
}

void RaidDirector_DisableEntity(const std::string &targetname)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        RaidDirector_SnapshotEntity(entity);
        entity->solid = SOLID_NOT;
        entity->touch = nullptr;
        entity->use = nullptr;
        gi.linkentity(entity);
        ++matches;
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] disable target '{}' not found\n", targetname);
}

void RaidDirector_SetField(const std::string &targetname, const std::string &field, const Json::Value &value)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        RaidDirector_SnapshotEntity(entity);
        if (field == "wait" && value.isNumeric())
        {
            entity->wait = value.asFloat();
            entity->moveinfo.wait = entity->wait;
            ++matches;
        }
        else if (field == "hover_distance" && value.isNumeric() && entity->classname && !Q_strcasecmp(entity->classname, "raid_hovertext"))
        {
            entity->speed = std::max(1.0f, value.asFloat());
            ++matches;
        }
        else if (field == "hover_radius" && value.isNumeric() && entity->classname && !Q_strcasecmp(entity->classname, "raid_hovertext"))
        {
            entity->dmg_radius = std::max(1.0f, value.asFloat());
            ++matches;
        }
        else if (field == "require_los" && value.isBool() && entity->classname && !Q_strcasecmp(entity->classname, "raid_hovertext"))
        {
            entity->sounds = value.asBool() ? 1 : 0;
            ++matches;
        }
        else if (field == "observer_inverted" && value.isBool() && entity->classname && !Q_strcasecmp(entity->classname, "raid_hat"))
        {
            if (RaidHats_SetObserverInverted(entity, value.asBool()))
                ++matches;
        }
        else
        {
            gi.Com_PrintFmt("[raid] unsupported field write '{}.{}'\n", targetname, field);
            return;
        }
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] field target '{}' not found\n", targetname);
}

void RaidDirector_ExecuteOperations(const Json::Value &operations, const std::string &context, edict_t *activator)
{
    if (!operations.isArray())
        return;

    for (const Json::Value &operation : operations)
    {
        const std::string op = operation.get("op", "").asString();
        if (op == "fire_target")
        {
            RaidDirector_FireTarget(operation.get("target", "").asString(), activator);
        }
        else if (op == "disable_entity")
        {
            RaidDirector_DisableEntity(operation.get("target", "").asString());
        }
        else if (op == "set_field")
        {
            RaidDirector_SetField(operation.get("target", "").asString(), operation.get("field", "").asString(), operation["value"]);
        }
        else if (op == "set_monster_door")
        {
            const std::string target = operation.get("target", "").asString();
            if (!RaidMonsters_SetEnabled(target.c_str(), operation.get("enabled", true).asBool()))
                gi.Com_PrintFmt("[raid] monster door target '{}' not found\n", target);
        }
        else if (op == "open_terminal")
        {
            const std::string target = operation.get("target", "").asString();
            edict_t *terminal = target.empty()
                ? nullptr
                : G_FindByString<&edict_t::targetname>(nullptr, target.c_str());
            if (!RaidUI_Open(activator, terminal))
                gi.Com_PrintFmt("[raid] open_terminal could not open '{}' for its activator\n", target);
        }
        else if (op == "bot_move_to")
        {
            if (!RaidBots_MoveTo(operation.get("bot", "first").asString().c_str(),
                operation.get("target", "").asString().c_str(), operation.get("tolerance", 0.0f).asFloat()))
                gi.Com_PrintFmt("[raid] bot_move_to could not resolve bot '{}' or goal '{}'\n",
                    operation.get("bot", "first").asString(), operation.get("target", "").asString());
        }
        else if (op == "bot_follow_activator")
        {
            if (!RaidBots_Follow(operation.get("bot", "first").asString().c_str(), activator))
                gi.Com_Print("[raid] bot_follow_activator could not resolve bot or activator\n");
        }
        else if (op == "bot_operate_gadget")
        {
            if (!RaidBots_Operate(operation.get("bot", "first").asString().c_str(),
                operation.get("goal", "").asString().c_str(), operation.get("target", "").asString().c_str(),
                operation.get("tolerance", 0.0f).asFloat(), operation.get("duration", 0.0f).asFloat(),
                operation.get("hold", true).asBool()))
                gi.Com_Print("[raid] bot_operate_gadget could not resolve its bot, goal, or gadget\n");
        }
        else if (op == "post_message")
        {
            RaidDirector_PostMessage(activator, operation.get("text", "").asString(), operation.get("scope", "activator").asString() == "all");
        }
        else if (op == "post_encounter_message")
        {
            RaidDirector_PostEncounterMessage(operation);
        }
        else if (op == "apply_status")
        {
            if (operation.get("scope", "activator").asString() == "all")
                for (edict_t *player : active_players())
                    RaidDirector_ApplyPlayerStatus(player, operation);
            else
                RaidDirector_ApplyPlayerStatus(activator, operation);
        }
        else if (op == "clear_status")
        {
            if (operation.get("scope", "activator").asString() == "all")
                for (edict_t *player : active_players())
                    RaidDirector_ClearPlayerStatus(player, operation.get("status", "").asString());
            else
                RaidDirector_ClearPlayerStatus(activator, operation.get("status", "").asString());
        }
        else if (op == "damage_player")
        {
            if (activator && activator->client)
            {
                const int damage = std::max(0, operation.get("amount", 0).asInt());
                T_Damage(activator, activator, activator, vec3_origin, activator->s.origin, vec3_origin,
                    damage, 0, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
            }
        }
        else if (op == "kill_player")
        {
            if (activator && activator->client)
                T_Damage(activator, activator, activator, vec3_origin, activator->s.origin, vec3_origin,
                    100000, 0, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
        }
        else if (op == "screen_shake")
        {
            const float duration = std::max(0.0f, operation.get("duration", 1.0f).asFloat());
            const float intensity = std::clamp(operation.get("intensity", 2.5f).asFloat(), 0.0f, 12.0f);
            const bool all = operation.get("scope", "activator").asString() == "all";
            for (edict_t *player : active_players())
                if (all || player == activator)
                {
                    player->client->quake_time = std::max(player->client->quake_time, level.time + gtime_t::from_sec(duration));
                    player->client->raid_shake_intensity = std::max(player->client->raid_shake_intensity, intensity);
                }
        }
        else if (op == "screen_flash")
        {
            rgba_t color = rgba_white;
            RaidDirector_ReadColor(operation["color"], color);
            const float hold = std::max(0.0f, operation.get("duration", 0.15f).asFloat());
            const float fade = std::max(0.0f, operation.get("fade", 1.0f).asFloat());
            const float alpha = std::clamp(operation.get("intensity", color.a / 255.0f).asFloat(), 0.0f, 1.0f);
            const bool all = operation.get("scope", "activator").asString() == "all";
            for (edict_t *player : active_players())
                if (all || player == activator)
                {
                    player->client->raid_flash_color = {
                        color.r / 255.0f, color.g / 255.0f, color.b / 255.0f
                    };
                    player->client->raid_flash_alpha = alpha;
                    player->client->raid_flash_fade_at = level.time + gtime_t::from_sec(hold);
                    player->client->raid_flash_end = player->client->raid_flash_fade_at + gtime_t::from_sec(fade);
                }
        }
        else if (op == "play_sound")
        {
            const std::string sound = operation.get("sound", "").asString();
            const bool all = operation.get("scope", "activator").asString() == "all";
            if (!sound.empty())
            {
                const int sound_index = gi.soundindex(sound.c_str());
                for (edict_t *player : active_players())
                    if (all || player == activator)
                        gi.sound(player, CHAN_AUTO, sound_index, 1.0f, ATTN_NONE, 0.0f);
            }
        }
        else if (op == "color_cycle")
        {
            raid_color_cycle_t cycle;
            cycle.started_at = level.time;
            cycle.step_seconds = std::max(0.05f, operation.get("step_seconds", 0.5f).asFloat());
            cycle.transition_seconds = std::clamp(operation.get("transition_seconds", cycle.step_seconds).asFloat(), 0.0f, cycle.step_seconds);

            for (const Json::Value &target : operation["targets"])
                cycle.targets.push_back(target.asString());
            for (const Json::Value &value : operation["colors"])
            {
                rgba_t color;
                if (RaidDirector_ReadColor(value, color))
                    cycle.colors.push_back(color);
            }

            if (cycle.targets.empty() || cycle.colors.size() < 2)
            {
                gi.Com_Print("[raid] color_cycle requires targets and at least two colors\n");
                continue;
            }

            color_cycles.push_back(std::move(cycle));
        }
        else
        {
            gi.Com_PrintFmt("[raid] Unknown operation '{}' in {}\n", op, context);
        }
    }
}

void RaidDirector_ExecuteEnter(const std::string &state_name, edict_t *activator)
{
    RaidDirector_ExecuteOperations(director.document["states"][state_name]["enter"], fmt::format("state '{}'", state_name), activator);
}

bool RaidDirector_ValidateOperations(const Json::Value &operations, const std::string &context, std::string &error)
{
    if (!operations.isArray())
    {
        error = fmt::format("{} must be an array", context);
        return false;
    }

    static const std::vector<std::string> known_ops = {
        "fire_target", "disable_entity", "set_field", "post_message", "post_encounter_message",
        "apply_status", "clear_status", "damage_player", "kill_player", "screen_shake",
        "screen_flash", "play_sound", "color_cycle", "set_monster_door", "bot_move_to",
        "bot_follow_activator", "bot_operate_gadget", "open_terminal"
    };

    for (Json::ArrayIndex i = 0; i < operations.size(); ++i)
    {
        const Json::Value &operation = operations[i];
        if (!operation.isObject() || !operation["op"].isString())
        {
            error = fmt::format("{}[{}] requires a string op", context, i);
            return false;
        }

        const std::string op = operation["op"].asString();
        if (std::find(known_ops.begin(), known_ops.end(), op) == known_ops.end())
        {
            error = fmt::format("{}[{}] uses unknown op '{}'", context, i, op);
            return false;
        }

        if ((op == "fire_target" || op == "disable_entity" || op == "set_field" || op == "set_monster_door" ||
            op == "bot_move_to" || op == "bot_operate_gadget" || op == "open_terminal") &&
            (!operation["target"].isString() || operation["target"].asString().empty()))
        {
            error = fmt::format("{}[{}] op '{}' requires target", context, i, op);
            return false;
        }
        if (op == "set_monster_door" && operation.isMember("enabled") && !operation["enabled"].isBool())
        {
            error = fmt::format("{}[{}] set_monster_door enabled must be boolean", context, i);
            return false;
        }
        if (op == "bot_operate_gadget" && (!operation["goal"].isString() || operation["goal"].asString().empty()))
        {
            error = fmt::format("{}[{}] bot_operate_gadget requires goal", context, i);
            return false;
        }
        if ((op == "post_message" || op == "post_encounter_message") && !operation["text"].isString())
        {
            error = fmt::format("{}[{}] op '{}' requires text", context, i, op);
            return false;
        }
        if ((op == "apply_status" || op == "clear_status") && !operation["status"].isString())
        {
            error = fmt::format("{}[{}] op '{}' requires status", context, i, op);
            return false;
        }
        if (op == "play_sound" && !operation["sound"].isString())
        {
            error = fmt::format("{}[{}] play_sound requires sound", context, i);
            return false;
        }
        if (op == "screen_flash" && operation.isMember("color"))
        {
            rgba_t color;
            if (!RaidDirector_ReadColor(operation["color"], color))
            {
                error = fmt::format("{}[{}] screen_flash color must be [r,g,b] or [r,g,b,a] bytes", context, i);
                return false;
            }
        }
        if (op == "color_cycle" && (!operation["targets"].isArray() || !operation["colors"].isArray()))
        {
            error = fmt::format("{}[{}] color_cycle requires targets and colors arrays", context, i);
            return false;
        }
        if (op == "apply_status" && operation.isMember("on_expire") &&
            !RaidDirector_ValidateOperations(operation["on_expire"], fmt::format("{}[{}].on_expire", context, i), error))
            return false;
    }
    return true;
}

bool RaidDirector_Validate(const Json::Value &root, std::string &error)
{
    if (!root.isObject())
    {
        error = "root must be an object";
        return false;
    }

    if (!root.isMember("initial_state") || !root["initial_state"].isString() || root["initial_state"].asString().empty())
    {
        error = "initial_state must be a non-empty string";
        return false;
    }

    if (!root.isMember("states") || !root["states"].isObject() || root["states"].empty())
    {
        error = "states must be a non-empty object";
        return false;
    }

    if (root.isMember("reset") && !root["reset"].isArray())
    {
        error = "reset must be an operation array";
        return false;
    }
    if (root.isMember("reset") && !RaidDirector_ValidateOperations(root["reset"], "reset", error))
        return false;

    if (root.isMember("statuses") && !root["statuses"].isObject())
    {
        error = "statuses must be an object";
        return false;
    }
    for (const std::string &name : root["statuses"].getMemberNames())
    {
        const Json::Value &status = root["statuses"][name];
        if (!status.isObject())
        {
            error = fmt::format("status '{}' must be an object", name);
            return false;
        }
        if (status.isMember("duration") && !status["duration"].isNumeric())
        {
            error = fmt::format("status '{}'.duration must be numeric", name);
            return false;
        }
        if (status.isMember("on_expire") &&
            !RaidDirector_ValidateOperations(status["on_expire"], fmt::format("status '{}'.on_expire", name), error))
            return false;
    }

    const std::string initial_state = root["initial_state"].asString();
    if (!root["states"].isMember(initial_state))
    {
        error = fmt::format("initial_state '{}' is not defined in states", initial_state);
        return false;
    }

    for (const std::string &name : root["states"].getMemberNames())
    {
        const Json::Value &state = root["states"][name];
        if (!state.isObject())
        {
            error = fmt::format("state '{}' must be an object", name);
            return false;
        }

        if (state.isMember("enter") && !state["enter"].isArray())
        {
            error = fmt::format("state '{}'.enter must be an array", name);
            return false;
        }

        if (state.isMember("enter") && !RaidDirector_ValidateOperations(state["enter"], fmt::format("state '{}'.enter", name), error))
            return false;

        if (state.isMember("exit") && !state["exit"].isArray())
        {
            error = fmt::format("state '{}'.exit must be an array", name);
            return false;
        }
        if (state.isMember("exit") && !RaidDirector_ValidateOperations(state["exit"], fmt::format("state '{}'.exit", name), error))
            return false;
    }

    if (root.isMember("events") && !root["events"].isArray())
    {
        error = "events must be an array";
        return false;
    }

    if (root.isMember("events"))
    {
        for (Json::ArrayIndex i = 0; i < root["events"].size(); ++i)
        {
            const Json::Value &event = root["events"][i];
            if (!event.isObject() || !event["source"].isString() || event["source"].asString().empty())
            {
                error = fmt::format("events[{}].source must be a non-empty string", i);
                return false;
            }
            if (event.isMember("signal") && !event["signal"].isString())
            {
                error = fmt::format("events[{}].signal must be a string", i);
                return false;
            }
            if (event.isMember("do") && !event["do"].isArray())
            {
                error = fmt::format("events[{}].do must be an array", i);
                return false;
            }
            if (event.isMember("do") && !RaidDirector_ValidateOperations(event["do"], fmt::format("events[{}].do", i), error))
                return false;
            if (event.isMember("set_state") && (!event["set_state"].isString() || !root["states"].isMember(event["set_state"].asString())))
            {
                error = fmt::format("events[{}].set_state references an unknown state", i);
                return false;
            }
        }
    }

    return true;
}

std::filesystem::path RaidDirector_ResolvePath(const char *path)
{
    std::filesystem::path requested(path ? path : "");
    if (requested.is_absolute())
        return requested;
    if (raid_script_root && *raid_script_root->string)
        return std::filesystem::path(raid_script_root->string) / requested;
    if (raid_game_dir && *raid_game_dir->string)
        return std::filesystem::path(raid_game_dir->string) / requested;
    return requested;
}
}

void RaidDirector_OnClientDisconnect(edict_t *player)
{
    RaidDirector_RemovePlayerSnapshot(player);
}

bool RaidDirector_OnPartyWipe()
{
    if (!director.loaded)
        return false;
    if (director.wipe_pending)
        return true;

    director.wipe_pending = true;
    director.wipe_reset_at = level.time + 2500_ms;
    if (director.document["states"].isMember("wipe"))
        RaidDirector_SetState("wipe");
    else
        gi.Com_PrintFmt("[raid] Party wipe detected in encounter '{}'\n", director.encounter_name);
    return true;
}

void RaidDirector_ApplyStatus(edict_t *player, const char *status, float duration, const char *stack_policy)
{
    ApplyStatusFromDefinition(player, status, duration, stack_policy);
}

void RaidDirector_ClearStatus(edict_t *player, const char *status)
{
    ClearStatusByName(player, status);
}

float RaidDirector_StatusDuration(const char *status, float fallback)
{
    return StatusDurationFromDefinition(status, fallback);
}

void RaidDirector_Init()
{
    raid_script = gi.cvar("raid_script", "", CVAR_NOFLAGS);
    raid_script_root = gi.cvar("raid_script_root", "", CVAR_NOFLAGS);
    raid_autoload = gi.cvar("raid_autoload", "1", CVAR_NOFLAGS);
    raid_game_dir = gi.cvar("game", "", CVAR_NOFLAGS);

    director = {};
    director.initialized = true;
    gi.Com_Print("[raid] Director initialized (single server authority)\n");
}

void RaidDirector_Shutdown()
{
    RaidDirector_ClearDocument();
    director.initialized = false;
}

void RaidDirector_ResetForMap(const char *mapname)
{
    // SpawnEntities has already discarded the old level edicts. Never run an
    // in-place snapshot restore here or the previous BSP can contaminate the
    // freshly cleared entity array during a coop map transition.
    RaidCarry_ResetAll();
    RaidHover_Reset();
    RaidMonsters_ClearMap();
    RaidHats_ClearMap();
    RaidDowned_ResetAll();
    RaidReconstruction_Reset();
    RaidBots_Reset();
    RaidUI_Reset();
    RaidDirector_ClearTransientState();
    entity_snapshots.clear();
    player_snapshots.clear();
    director.loaded = false;
    director.script_path.clear();
    director.encounter_name.clear();
    director.state.clear();
    director.document = Json::Value();
    director.mapname = mapname ? mapname : "";
}

void RaidDirector_OnMapReady()
{
    RaidItems_OnMapReady();
    RaidDirector_CaptureEntityBaseline();
    if (!director.initialized || !raid_autoload || !raid_autoload->integer)
        return;
    if (raid_script && *raid_script->string)
    {
        RaidDirector_Load(raid_script->string);
        return;
    }

    const std::filesystem::path matched = std::filesystem::path("raid/encounters") / (director.mapname + ".json");
    const std::filesystem::path resolved = RaidDirector_ResolvePath(matched.string().c_str());
    if (std::filesystem::exists(resolved))
    {
        gi.Com_PrintFmt("[raid] Found map-matched encounter '{}'\n", matched.string());
        RaidDirector_Load(matched.string().c_str());
    }
}

void RaidDirector_RunFrame()
{
    RaidHover_RunFrame();
    RaidItems_RunFrame();
    if (director.loaded)
        RaidDirector_CaptureNewPlayers();
    if (director.wipe_pending && level.time >= director.wipe_reset_at)
    {
        gi.Com_PrintFmt("[raid] Completing in-place wipe reset for encounter '{}'\n", director.encounter_name);
        RaidDirector_ResetEncounter();
    }
    if (raid_message_expires && raid_message_expires <= level.time)
    {
        gi.configstring(CONFIG_RAID_MESSAGE, "");
        raid_message_expires = 0_ms;
        raid_message_priority = 0;
    }

    for (raid_color_cycle_t &cycle : color_cycles)
    {
        const float elapsed = std::max(0.0f, (level.time - cycle.started_at).seconds<float>());
        const int step = static_cast<int>(std::floor(elapsed / cycle.step_seconds));
        const float within_step = std::fmod(elapsed, cycle.step_seconds);
        float fraction = cycle.transition_seconds > 0.0f ? std::min(1.0f, within_step / cycle.transition_seconds) : 1.0f;
        fraction = fraction * fraction * (3.0f - (2.0f * fraction));

        for (size_t i = 0; i < cycle.targets.size(); ++i)
        {
            const rgba_t &from = cycle.colors[(i + step) % cycle.colors.size()];
            const rgba_t &to = cycle.colors[(i + step + 1) % cycle.colors.size()];
            const auto channel = [fraction](uint8_t a, uint8_t b) {
                return static_cast<uint8_t>(std::clamp(a + ((b - a) * fraction), 0.0f, 255.0f));
            };
            RaidDirector_SetNamedColor(cycle.targets[i], {
                channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)
            });
        }
    }

    for (edict_t *player : active_players())
        RaidDirector_ClearStatusHUD(player);

    struct expired_status_t
    {
        edict_t *player;
        std::string message;
        Json::Value operations;
    };
    std::vector<expired_status_t> expired_statuses;

    for (auto it = player_statuses.begin(); it != player_statuses.end();)
    {
        edict_t *player = (it->entity_number > 0 && it->entity_number < globals.num_edicts) ? &g_edicts[it->entity_number] : nullptr;
        if (!player || !player->inuse || !player->client || player->spawn_count != it->spawn_count || player->health <= 0)
        {
            it = player_statuses.erase(it);
            continue;
        }

        const float remaining = (it->expires_at - level.time).seconds<float>();
        if (remaining <= 0.0f)
        {
            expired_statuses.push_back({ player, it->expire_message, it->on_expire });
            it = player_statuses.erase(it);
            continue;
        }
        ++it;
    }

    for (const expired_status_t &expired : expired_statuses)
    {
        RaidDirector_PostMessage(expired.player, expired.message, false);
        RaidDirector_ExecuteOperations(expired.operations, "status expiry", expired.player);
    }

    for (const raid_player_status_t &status : player_statuses)
    {
        edict_t *player = &g_edicts[status.entity_number];
        for (size_t slot = 0; slot < raid_status_stats.size(); ++slot)
        {
            if (player->client->ps.stats[raid_status_stats[slot]])
                continue;
            player->client->ps.stats[raid_status_stats[slot]] = RaidDirector_StatusCode(status.name);
            player->client->ps.stats[raid_status_time_stats[slot]] = static_cast<int16_t>(
                std::ceil(std::max(0.0f, (status.expires_at - level.time).seconds<float>())));
            break;
        }
    }
}

void RaidDirector_NotifyEntityEvent(edict_t *source, const char *signal, edict_t *activator)
{
    if (!director.loaded || !source || !source->targetname || !signal)
        return;

    queued_events.push_back({ source->targetname, signal,
        activator ? activator->s.number : 0, activator ? activator->spawn_count : 0 });
    if (dispatching_events)
        return;

    dispatching_events = true;
    int budget = 1024;
    while (!queued_events.empty() && budget-- > 0)
    {
        queued_raid_event_t queued = std::move(queued_events.front());
        queued_events.pop_front();
        edict_t *event_activator = nullptr;
        if (queued.activator_number && queued.activator_number < globals.num_edicts)
        {
            edict_t *candidate = &g_edicts[queued.activator_number];
            if (candidate->inuse && candidate->spawn_count == queued.activator_spawn_count)
                event_activator = candidate;
        }

        const Json::Value &events = director.document["events"];
        if (!events.isArray())
            continue;

        for (const Json::Value &event : events)
        {
            if (event.get("source", "").asString() != queued.source || event.get("signal", "activate").asString() != queued.signal)
                continue;

            const std::string from_state = event.get("from_state", "").asString();
            if (!from_state.empty() && from_state != director.state)
                continue;

            const std::string next_state = event.get("set_state", "").asString();
            RaidDirector_ExecuteOperations(event["do"], fmt::format("event '{}:{}'", queued.source, queued.signal), event_activator);
            if (!next_state.empty())
            {
                if (!director.document["states"].isMember(next_state))
                {
                    gi.Com_PrintFmt("[raid] Event from '{}' references unknown state '{}'\n", queued.source, next_state);
                    continue;
                }

                const std::string previous = director.state;
                RaidDirector_ExecuteOperations(director.document["states"][previous]["exit"], fmt::format("state '{}' exit", previous), event_activator);
                director.state = next_state;
                color_cycles.clear();
                gi.Com_PrintFmt("[raid] Event '{}:{}': '{}' -> '{}'\n", queued.source, queued.signal, previous, director.state);
                RaidDirector_ExecuteEnter(director.state, event_activator);
            }
        }
    }
    if (!queued_events.empty())
    {
        gi.Com_PrintFmt("[raid] Event budget exhausted; discarded {} recursively generated events\n", queued_events.size());
        queued_events.clear();
    }
    dispatching_events = false;
}

bool RaidDirector_Load(const char *path)
{
    if (!director.initialized)
    {
        gi.Com_Print("[raid] Director is not initialized\n");
        return false;
    }
    if (!path || !*path)
    {
        gi.Com_Print("[raid] No encounter JSON path supplied\n");
        return false;
    }

    const std::filesystem::path resolved = RaidDirector_ResolvePath(path);
    std::ifstream stream(resolved, std::ios::in | std::ios::binary);
    if (!stream)
    {
        gi.Com_PrintFmt("[raid] Couldn't open encounter JSON '{}'\n", resolved.string());
        return false;
    }

    Json::CharReaderBuilder reader;
    reader["collectComments"] = false;
    Json::Value root;
    JSONCPP_STRING parse_error;
    if (!Json::parseFromStream(reader, stream, &root, &parse_error))
    {
        gi.Com_PrintFmt("[raid] Couldn't parse encounter JSON '{}': {}\n", resolved.string(), parse_error);
        return false;
    }

    std::string validation_error;
    if (!RaidDirector_Validate(root, validation_error))
    {
        gi.Com_PrintFmt("[raid] Invalid encounter JSON '{}': {}\n", resolved.string(), validation_error);
        return false;
    }

    RaidDirector_ClearDocument();
    director.document = std::move(root);
    director.script_path = path;
    director.encounter_name = director.document.get("encounter", director.mapname).asString();
    director.state = director.document["initial_state"].asString();
    director.loaded = true;

    RaidMonsters_PrepareRosters();
    RaidDirector_ExecuteEnter(director.state, nullptr);

    gi.Com_PrintFmt("[raid] Loaded encounter '{}' from '{}' (initial state '{}', {} states)\n",
        director.encounter_name,
        resolved.string(),
        director.state,
        director.document["states"].size());
    return true;
}

bool RaidDirector_Reload()
{
    if (director.script_path.empty())
    {
        if (raid_script && *raid_script->string)
            return RaidDirector_Load(raid_script->string);

        gi.Com_Print("[raid] No encounter JSON has been selected\n");
        return false;
    }

    const std::string path = director.script_path;
    return RaidDirector_Load(path.c_str());
}

bool RaidDirector_ResetEncounter()
{
    if (!director.loaded)
    {
        gi.Com_Print("[raid] No encounter JSON is loaded\n");
        return false;
    }

    if (director.document["states"].isMember(director.state))
        RaidDirector_ExecuteOperations(director.document["states"][director.state]["exit"],
            fmt::format("state '{}' exit", director.state), nullptr);

    RaidDirector_RestoreEntitySnapshots();
    RaidCarry_ResetAll();
    RaidHover_Reset();
    RaidMonsters_Reset();
    RaidHats_Reset();
    RaidDowned_ResetAll();
    RaidReconstruction_Reset();
    RaidBots_Reset();
    RaidUI_Reset();
    RaidDirector_RestorePlayers();
    RaidDirector_ClearTransientState();
    director.state = director.document["initial_state"].asString();
    RaidDirector_ExecuteOperations(director.document["reset"], "encounter reset", nullptr);
    RaidDirector_ExecuteEnter(director.state, nullptr);
    gi.Com_PrintFmt("[raid] Encounter '{}' reset to initial state '{}'\n", director.encounter_name, director.state);
    return true;
}

bool RaidDirector_SetState(const char *state_name)
{
    if (!director.loaded)
    {
        gi.Com_Print("[raid] No encounter JSON is loaded\n");
        return false;
    }
    if (!state_name || !*state_name || !director.document["states"].isMember(state_name))
    {
        gi.Com_PrintFmt("[raid] Unknown Director state '{}'\n", state_name ? state_name : "");
        return false;
    }

    const std::string previous = director.state;
    RaidDirector_ExecuteOperations(director.document["states"][previous]["exit"], fmt::format("state '{}' exit", previous), nullptr);
    director.state = state_name;
    color_cycles.clear();
    gi.Com_PrintFmt("[raid] State: '{}' -> '{}'\n", previous, director.state);
    RaidDirector_ExecuteEnter(director.state, nullptr);
    return true;
}

void RaidDirector_TestFlash(bool dark)
{
    for (edict_t *player : active_players())
    {
        player->client->raid_flash_color = dark ? vec3_t{ 0, 0, 0 } : vec3_t{ 1, 1, 1 };
        player->client->raid_flash_alpha = dark ? 0.9f : 1.0f;
        player->client->raid_flash_fade_at = level.time + 150_ms;
        player->client->raid_flash_end = player->client->raid_flash_fade_at +
            gtime_t::from_sec(dark ? 1.2f : 1.0f);
    }
}

void RaidDirector_Dump()
{
    gi.Com_PrintFmt("[raid] Director: initialized={}, loaded={}, map='{}', encounter='{}', state='{}', script='{}'\n",
        director.initialized,
        director.loaded,
        director.mapname,
        director.encounter_name,
        director.state,
        director.script_path);

    if (!director.loaded)
        return;

    gi.Com_Print("[raid] States:");
    for (const std::string &name : director.document["states"].getMemberNames())
        gi.Com_PrintFmt(" {}{}", name, name == director.state ? "*" : "");
    gi.Com_Print("\n");
}

void RaidDirector_WriteSave(Json::Value &output)
{
    output = Json::Value(Json::objectValue);
    output["loaded"] = director.loaded;
    output["script"] = director.script_path;
    output["state"] = director.state;
}

void RaidDirector_ReadSave(const Json::Value &input)
{
    if (!input.isObject() || !input.get("loaded", false).asBool())
        return;

    const std::string script = input.get("script", "").asString();
    const std::string state = input.get("state", "").asString();
    if (!script.empty() && RaidDirector_Load(script.c_str()) && !state.empty())
        RaidDirector_SetState(state.c_str());
}
