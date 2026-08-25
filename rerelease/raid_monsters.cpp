#include "g_local.h"
#include "raid_director.h"
#include "raid_monsters.h"

namespace
{
struct entity_ref_t
{
    uint32_t number = 0;
    int32_t spawn_count = 0;
};

struct leash_state_t
{
    entity_ref_t monster;
    gtime_t outside_since;
    bool returning = false;
};

struct roster_entry_t
{
    std::string classname;
    std::string targetname;
    std::string target;
    std::string pathtarget;
    std::string deathtarget;
    std::string healthtarget;
    std::string itemtarget;
    std::string combattarget;
    std::string message;
    std::string model;
    std::string item;
    vec3_t origin;
    vec3_t angles;
    spawnflags_t spawnflags;
    float speed = 0.0f;
    float accel = 0.0f;
    float decel = 0.0f;
    float wait = 0.0f;
    float delay = 0.0f;
    float random = 0.0f;
    int style = 0;
    int count = 0;
    int health = 0;
    int sounds = 0;
    int dmg = 0;
    int mass = 0;
};

struct door_runtime_t
{
    entity_ref_t controller;
    bool active = false;
    bool prepared = false;
    bool deployed_any = false;
    int release_remaining = 0;
    gtime_t next_release;
    gtime_t replenish_at;
    std::vector<leash_state_t> leashes;
    std::vector<roster_entry_t> roster;
};

std::vector<door_runtime_t> door_runtimes;
std::vector<roster_entry_t> roster_templates;

std::string CopyKey(const char *value)
{
    return value ? value : "";
}

const char *RestoreKey(const std::string &value)
{
    return value.empty() ? nullptr : G_CopyString(value.c_str(), TAG_LEVEL);
}

entity_ref_t Ref(edict_t *entity)
{
    return { entity->s.number, entity->spawn_count };
}

edict_t *Resolve(const entity_ref_t &ref)
{
    if (!ref.number || ref.number >= globals.num_edicts)
        return nullptr;
    edict_t *entity = &g_edicts[ref.number];
    return entity->inuse && entity->spawn_count == ref.spawn_count ? entity : nullptr;
}

door_runtime_t &Runtime(edict_t *controller)
{
    auto found = std::find_if(door_runtimes.begin(), door_runtimes.end(), [controller](const door_runtime_t &runtime) {
        return runtime.controller.number == controller->s.number && runtime.controller.spawn_count == controller->spawn_count;
    });
    if (found != door_runtimes.end())
        return *found;
    door_runtimes.push_back({ Ref(controller) });
    return door_runtimes.back();
}

bool IsRosterMember(edict_t *controller, edict_t *monster)
{
    return monster->inuse && monster->classname && !strncmp(monster->classname, "monster_", 8) &&
        monster->targetname && controller->target && !Q_strcasecmp(monster->targetname, controller->target);
}

int CountRoster(edict_t *controller, bool hidden)
{
    int count = 0;
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *monster = &g_edicts[i];
        if (!IsRosterMember(controller, monster) || monster->health <= 0)
            continue;
        if (!!(monster->svflags & SVF_NOCLIENT) == hidden)
            ++count;
    }
    return count;
}

void QueueInitialRelease(edict_t *controller, door_runtime_t &runtime)
{
    const int hidden = CountRoster(controller, true);
    const int requested = controller->count > 0 ? controller->count : (controller->health > 0 ? controller->health : hidden);
    runtime.release_remaining = std::min(hidden, requested);
    runtime.next_release = level.time;
}

void CaptureRoster(edict_t *controller, door_runtime_t &runtime)
{
    runtime.roster.clear();
    runtime.leashes.clear();
    runtime.deployed_any = false;
    runtime.release_remaining = 0;
    runtime.replenish_at = 0_ms;

    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *monster = &g_edicts[i];
        if (!IsRosterMember(controller, monster) || monster->health <= 0)
            continue;
        runtime.roster.push_back({});
        roster_entry_t &entry = runtime.roster.back();
        entry.classname = CopyKey(monster->classname);
        entry.targetname = CopyKey(monster->targetname);
        entry.target = CopyKey(monster->target);
        entry.pathtarget = CopyKey(monster->pathtarget);
        entry.deathtarget = CopyKey(monster->deathtarget);
        entry.healthtarget = CopyKey(monster->healthtarget);
        entry.itemtarget = CopyKey(monster->itemtarget);
        entry.combattarget = CopyKey(monster->combattarget);
        entry.message = CopyKey(monster->message);
        entry.model = CopyKey(monster->model);
        entry.item = monster->item ? CopyKey(monster->item->classname) : "";
        entry.origin = monster->s.origin;
        entry.angles = monster->s.angles;
        entry.spawnflags = monster->spawnflags | SPAWNFLAG_MONSTER_TRIGGER_SPAWN;
        entry.speed = monster->speed;
        entry.accel = monster->accel;
        entry.decel = monster->decel;
        entry.wait = monster->wait;
        entry.delay = monster->delay;
        entry.random = monster->random;
        entry.style = monster->style;
        entry.count = monster->count;
        entry.health = monster->max_health > 0 ? monster->max_health : monster->health;
        entry.sounds = monster->sounds;
        entry.dmg = monster->dmg;
        entry.mass = monster->mass;

        if (!monster->spawnflags.has(SPAWNFLAG_MONSTER_TRIGGER_SPAWN))
            gi.Com_PrintFmt("[raid] roster monster '{}' lacked Trigger Spawn; Director will safely reconstruct it dormant\n",
                monster->targetname ? monster->targetname : monster->classname);
    }

    runtime.prepared = false;
    gi.Com_PrintFmt("[raid] monster door '{}' captured {} roster members for dormancy\n",
        controller->targetname ? controller->targetname : "<unnamed>", runtime.roster.size());
}

edict_t *SpawnRosterMember(const roster_entry_t &entry)
{
    edict_t *monster = G_Spawn();
    monster->classname = RestoreKey(entry.classname);
    monster->targetname = RestoreKey(entry.targetname);
    monster->target = RestoreKey(entry.target);
    monster->pathtarget = RestoreKey(entry.pathtarget);
    monster->deathtarget = RestoreKey(entry.deathtarget);
    monster->healthtarget = RestoreKey(entry.healthtarget);
    monster->itemtarget = RestoreKey(entry.itemtarget);
    monster->combattarget = RestoreKey(entry.combattarget);
    monster->message = RestoreKey(entry.message);
    monster->model = RestoreKey(entry.model);
    monster->s.origin = entry.origin;
    monster->s.angles = entry.angles;
    monster->spawnflags = entry.spawnflags | SPAWNFLAG_MONSTER_TRIGGER_SPAWN;
    monster->speed = entry.speed;
    monster->accel = entry.accel;
    monster->decel = entry.decel;
    monster->wait = entry.wait;
    monster->delay = entry.delay;
    monster->random = entry.random;
    monster->style = entry.style;
    monster->count = entry.count;
    monster->health = entry.health;
    monster->sounds = entry.sounds;
    monster->dmg = entry.dmg;
    monster->mass = entry.mass;

    st = {};
    st.item = RestoreKey(entry.item);
    ED_CallSpawn(monster);
    return monster->inuse ? monster : nullptr;
}

void RebuildRosters()
{
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *monster = &g_edicts[i];
        if (!monster->inuse || !monster->classname || strncmp(monster->classname, "monster_", 8) || !monster->targetname)
            continue;
        const bool roster_member = std::any_of(roster_templates.begin(), roster_templates.end(), [monster](const roster_entry_t &entry) {
            return !Q_strcasecmp(entry.targetname.c_str(), monster->targetname);
        });
        if (roster_member)
            G_FreeEdict(monster);
    }

    int restored = 0;
    for (const roster_entry_t &entry : roster_templates)
        if (SpawnRosterMember(entry))
            ++restored;
    gi.Com_PrintFmt("[raid] restored {} roster monsters to mapper deployment positions\n", restored);
}

edict_t *NearestPlayer(const vec3_t &origin)
{
    edict_t *best = nullptr;
    float best_distance = std::numeric_limits<float>::max();
    for (edict_t *player : active_players())
    {
        if (!player->client || player->deadflag || player->health <= 0 || player->client->resp.spectator)
            continue;
        const float distance = (player->s.origin - origin).lengthSquared();
        if (distance < best_distance)
        {
            best = player;
            best_distance = distance;
        }
    }
    return best;
}

bool WakeMonster(edict_t *controller, edict_t *monster)
{
    edict_t *player = NearestPlayer(monster->s.origin);
    if (controller->pathtarget && *controller->pathtarget)
        monster->target = controller->pathtarget;

    if (monster->svflags & SVF_NOCLIENT)
    {
        if (!monster->use)
        {
            gi.Com_PrintFmt("[raid] roster monster '{}' is hidden but has no Trigger Spawn use callback\n",
                monster->targetname ? monster->targetname : "<unnamed>");
            return false;
        }
        edict_t *activator = controller->mass == 0 ? controller : (player ? player : controller);
        monster->use(monster, controller, activator);
    }
    else if (controller->mass && player && !(monster->monsterinfo.aiflags & AI_GOOD_GUY))
    {
        monster->enemy = player;
        FoundTarget(monster);
    }
    return true;
}

bool ReleaseOne(edict_t *controller, door_runtime_t &runtime)
{
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *monster = &g_edicts[i];
        if (!IsRosterMember(controller, monster) || monster->health <= 0 || !(monster->svflags & SVF_NOCLIENT))
            continue;
        if (!WakeMonster(controller, monster))
            continue;
        runtime.deployed_any = true;
        runtime.leashes.push_back({ Ref(monster) });
        RaidDirector_NotifyEntityEvent(controller, "deploy", monster);
        return true;
    }
    return false;
}

void UpdateLeashes(edict_t *controller, door_runtime_t &runtime)
{
    if (controller->dmg_radius <= 0.0f)
        return;

    for (leash_state_t &state : runtime.leashes)
    {
        edict_t *monster = Resolve(state.monster);
        if (!monster || monster->health <= 0 || monster->deadflag)
            continue;

        const float distance = (monster->s.origin - controller->s.origin).length();
        if (state.returning)
        {
            const float returned_distance = controller->accel > 0.0f ? controller->accel : 64.0f;
            if (distance <= returned_distance)
            {
                state.returning = false;
                state.outside_since = 0_ms;
                monster->monsterinfo.aiflags &= ~AI_COMBAT_POINT;
                monster->goalentity = monster->movetarget = nullptr;
                if (edict_t *player = NearestPlayer(monster->s.origin))
                {
                    monster->enemy = player;
                    FoundTarget(monster);
                }
            }
            continue;
        }

        if (distance <= controller->dmg_radius)
        {
            state.outside_since = 0_ms;
            continue;
        }
        if (!state.outside_since)
            state.outside_since = level.time;
        if (level.time < state.outside_since + gtime_t::from_sec(std::max(0.0f, controller->speed)))
            continue;

        state.returning = true;
        monster->enemy = monster->oldenemy = nullptr;
        monster->goalentity = monster->movetarget = controller;
        monster->monsterinfo.aiflags |= AI_COMBAT_POINT;
        monster->ideal_yaw = vectoyaw(controller->s.origin - monster->s.origin);
        if (monster->monsterinfo.run)
            monster->monsterinfo.run(monster);
        RaidDirector_NotifyEntityEvent(controller, "leash_return", monster);
    }
}

USE(raid_monster_door_use) (edict_t *self, edict_t *, edict_t *) -> void
{
    door_runtime_t &runtime = Runtime(self);
    runtime.active = !runtime.active;
    runtime.replenish_at = 0_ms;
    if (runtime.active)
    {
        if (!runtime.prepared)
            CaptureRoster(self, runtime);
        if (runtime.prepared)
            QueueInitialRelease(self, runtime);
        RaidDirector_NotifyEntityEvent(self, "activated", nullptr);
    }
    else
    {
        runtime.release_remaining = 0;
        RaidDirector_NotifyEntityEvent(self, "deactivated", nullptr);
    }
}

THINK(raid_monster_door_think) (edict_t *self) -> void
{
    door_runtime_t &runtime = Runtime(self);
    if (!runtime.prepared)
    {
        int total = 0;
        int hidden = 0;
        for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
        {
            edict_t *monster = &g_edicts[i];
            if (!IsRosterMember(self, monster) || monster->health <= 0)
                continue;
            ++total;
            if (monster->svflags & SVF_NOCLIENT)
                ++hidden;
        }
        if (total == 0 || hidden == total)
        {
            runtime.prepared = true;
            if (runtime.active)
                QueueInitialRelease(self, runtime);
            RaidDirector_NotifyEntityEvent(self, "roster_ready", nullptr);
        }
    }
    const float interval = self->wait > 0.0f ? self->wait : 0.5f;
    const int max_active = self->health > 0 ? self->health : std::numeric_limits<int>::max();
    int active = CountRoster(self, false);

    if (runtime.active && runtime.release_remaining > 0 && active < max_active && level.time >= runtime.next_release)
    {
        if (ReleaseOne(self, runtime))
        {
            --runtime.release_remaining;
            runtime.next_release = level.time + gtime_t::from_sec(interval);
            ++active;
        }
        else
            runtime.release_remaining = 0;
    }

    if (runtime.active && runtime.release_remaining == 0 && CountRoster(self, true) > 0)
    {
        int requested = 0;
        if (self->style == 1 && active < self->radius_dmg)
            requested = self->radius_dmg - active;
        else if (self->style == 2 && runtime.deployed_any && active == 0)
            requested = self->dmg > 0 ? self->dmg : max_active;

        if (requested > 0)
        {
            if (!runtime.replenish_at)
                runtime.replenish_at = level.time + gtime_t::from_sec(std::max(0.0f, self->delay));
            else if (level.time >= runtime.replenish_at)
            {
                runtime.release_remaining = std::min({ requested, CountRoster(self, true), max_active - active });
                runtime.next_release = level.time;
                runtime.replenish_at = 0_ms;
                RaidDirector_NotifyEntityEvent(self, "replenish", nullptr);
            }
        }
        else
            runtime.replenish_at = 0_ms;
    }

    UpdateLeashes(self, runtime);
    self->nextthink = level.time + 100_ms;
}
}

void SP_raid_monster_door(edict_t *ent)
{
    if (!ent->target || !*ent->target)
        gi.Com_PrintFmt("[raid] raid_monster_door '{}' has no monster roster target\n",
            ent->targetname ? ent->targetname : "<unnamed>");
    if (ent->wait <= 0.0f) ent->wait = 0.5f;
    if (ent->delay < 0.0f) ent->delay = 0.0f;
    if (ent->accel <= 0.0f) ent->accel = 64.0f;
    if (ent->speed < 0.0f) ent->speed = 0.0f;
    ent->solid = SOLID_NOT;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    ent->use = raid_monster_door_use;
    ent->think = raid_monster_door_think;
    ent->nextthink = level.time + 100_ms;
    gi.linkentity(ent);
}

void RaidMonsters_PrepareRosters()
{
    if (roster_templates.empty())
    {
        for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
        {
            edict_t *controller = &g_edicts[i];
            if (!controller->inuse || !controller->classname || Q_strcasecmp(controller->classname, "raid_monster_door"))
                continue;
            door_runtime_t temporary;
            CaptureRoster(controller, temporary);
            roster_templates.insert(roster_templates.end(), temporary.roster.begin(), temporary.roster.end());
        }
    }

    RebuildRosters();
    door_runtimes.clear();
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *controller = &g_edicts[i];
        if (!controller->inuse || !controller->classname || Q_strcasecmp(controller->classname, "raid_monster_door"))
            continue;
        CaptureRoster(controller, Runtime(controller));
    }
}

void RaidMonsters_Reset()
{
    if (!roster_templates.empty())
        RebuildRosters();
    door_runtimes.clear();
}

void RaidMonsters_ClearMap()
{
    door_runtimes.clear();
    roster_templates.clear();
}
