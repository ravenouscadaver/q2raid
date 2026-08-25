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

struct door_runtime_t
{
    entity_ref_t controller;
    bool active = false;
    bool deployed_any = false;
    int release_remaining = 0;
    gtime_t next_release;
    gtime_t replenish_at;
    std::vector<leash_state_t> leashes;
};

std::vector<door_runtime_t> door_runtimes;

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
        const int hidden = CountRoster(self, true);
        const int requested = self->count > 0 ? self->count : (self->health > 0 ? self->health : hidden);
        runtime.release_remaining = std::min(hidden, requested);
        runtime.next_release = level.time;
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

void RaidMonsters_Reset()
{
    door_runtimes.clear();
}
