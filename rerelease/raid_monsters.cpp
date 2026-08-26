#include "g_local.h"
#include "raid_director.h"
#include "raid_monsters.h"
#include "raid_hats.h"

#include <deque>

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
    vec3_t anchor;
    entity_ref_t return_goal;
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
    std::string raid_hat;
    std::string item;
    vec3_t origin;
    vec3_t angles;
    spawnflags_t spawnflags = SPAWNFLAG_NONE;
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
    bool deployed_any = false;
    int release_remaining = 0;
    size_t next_template = 0;
    gtime_t next_release;
    gtime_t replenish_at;
    std::vector<leash_state_t> leashes;
    std::string last_status = "idle";
    entity_ref_t pending;
    bool pending_woken = false;
};

std::deque<door_runtime_t> door_runtimes;
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

std::vector<std::string> RosterSlots(edict_t *controller)
{
    std::vector<std::string> slots;
    if (!controller || !controller->target)
        return slots;
    std::string value = controller->target;
    std::replace(value.begin(), value.end(), ';', ',');
    size_t start = 0;
    while (start <= value.size())
    {
        const size_t end = value.find(',', start);
        std::string slot = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const size_t first = slot.find_first_not_of(" \t");
        const size_t last = slot.find_last_not_of(" \t");
        if (first != std::string::npos)
            slots.push_back(slot.substr(first, last - first + 1));
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return slots;
}

bool AcceptsRoster(edict_t *controller, const char *roster)
{
    if (!roster)
        return false;
    const std::vector<std::string> slots = RosterSlots(controller);
    return std::any_of(slots.begin(), slots.end(), [roster](const std::string &slot) {
        return !Q_strcasecmp(slot.c_str(), roster);
    });
}

bool IsRosterMember(edict_t *controller, edict_t *monster)
{
    return monster->inuse && monster->classname && !strncmp(monster->classname, "monster_", 8) &&
        monster->targetname && AcceptsRoster(controller, monster->targetname);
}

bool IsDoorInstance(edict_t *controller, edict_t *monster)
{
    return IsRosterMember(controller, monster) && monster->owner == controller;
}

int CountDoorInstances(edict_t *controller, bool hidden)
{
    int count = 0;
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *monster = &g_edicts[i];
        if (!IsDoorInstance(controller, monster) || monster->health <= 0)
            continue;
        if (!!(monster->svflags & SVF_NOCLIENT) == hidden)
            ++count;
    }
    return count;
}

int CountDoorTotal(edict_t *controller)
{
    return CountDoorInstances(controller, false) + CountDoorInstances(controller, true);
}

void QueueInitialRelease(edict_t *controller, door_runtime_t &runtime)
{
    if (edict_t *pending = Resolve(runtime.pending))
        G_FreeEdict(pending);
    runtime.leashes.clear();
    runtime.deployed_any = false;
    runtime.replenish_at = 0_ms;
    runtime.pending = {};
    runtime.pending_woken = false;
    const int active = CountDoorInstances(controller, false);
    const int max_active = controller->health > 0 ? controller->health : std::numeric_limits<int>::max();
    const int requested = controller->count > 0 ? controller->count :
        (controller->radius_dmg > 0 ? controller->radius_dmg : (controller->health > 0 ? controller->health : 1));
    runtime.release_remaining = std::max(0, std::min(requested, max_active - active));
    runtime.next_release = level.time;
    runtime.last_status = runtime.release_remaining > 0 ? "initial deployment queued" : "initial deployment capped";
}

void CaptureTemplate(edict_t *monster)
{
        roster_templates.push_back({});
        roster_entry_t &entry = roster_templates.back();
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
        entry.raid_hat = CopyKey(monster->map);
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

}

edict_t *SpawnRosterMember(const roster_entry_t &entry, edict_t *controller)
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
    monster->map = RestoreKey(entry.raid_hat);
    monster->s.origin = controller->s.origin;
    monster->s.angles = controller->s.angles;
    monster->pos1 = entry.origin;
    monster->owner = controller;
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
    if (monster->inuse)
    {
        monster->pos1 = entry.origin;
        RaidHats_ApplyMonster(monster);
    }
    return monster->inuse ? monster : nullptr;
}

void ClearRosterInstances()
{
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *monster = &g_edicts[i];
        if (!monster->inuse || !monster->classname || strncmp(monster->classname, "monster_", 8) || !monster->targetname)
            continue;
        if (monster->owner && monster->owner->inuse && monster->owner->classname &&
            !Q_strcasecmp(monster->owner->classname, "raid_monster_door"))
            G_FreeEdict(monster);
    }
}

edict_t *CreateDoorInstance(edict_t *controller, door_runtime_t &runtime)
{
    std::vector<const roster_entry_t *> choices;
    for (const roster_entry_t &entry : roster_templates)
        if (AcceptsRoster(controller, entry.targetname.c_str()))
            choices.push_back(&entry);
    if (choices.empty())
    {
        runtime.last_status = "no matching roster templates";
        return nullptr;
    }
    const roster_entry_t &entry = *choices[runtime.next_template++ % choices.size()];
    edict_t *monster = SpawnRosterMember(entry, controller);
    runtime.last_status = monster ? "dormant instance created" : "monster spawn failed";
    return monster;
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
    if (controller->mass == 2 && controller->pathtarget && *controller->pathtarget)
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

edict_t *CreateLeashGoal(const vec3_t &origin)
{
    edict_t *goal = G_Spawn();
    goal->classname = "raid_leash_goal";
    goal->s.origin = origin;
    goal->solid = SOLID_NOT;
    goal->movetype = MOVETYPE_NONE;
    goal->svflags |= SVF_NOCLIENT;
    gi.linkentity(goal);
    return goal;
}

void FreeLeashGoal(leash_state_t &state)
{
    if (edict_t *goal = Resolve(state.return_goal))
        G_FreeEdict(goal);
    state.return_goal = {};
}

void ClearLeashGoals()
{
    for (door_runtime_t &runtime : door_runtimes)
        for (leash_state_t &state : runtime.leashes)
            FreeLeashGoal(state);
}

void UpdateLeashes(edict_t *controller, door_runtime_t &runtime)
{
    if (controller->dmg_radius <= 0.0f)
        return;

    for (leash_state_t &state : runtime.leashes)
    {
        edict_t *monster = Resolve(state.monster);
        if (!monster || monster->health <= 0 || monster->deadflag)
        {
            FreeLeashGoal(state);
            continue;
        }

        const float distance = (monster->s.origin - state.anchor).length();
        if (state.returning)
        {
            const float returned_distance = controller->accel > 0.0f ? controller->accel : 64.0f;
            if (distance <= returned_distance)
            {
                state.returning = false;
                state.outside_since = 0_ms;
                FreeLeashGoal(state);
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
        edict_t *goal = CreateLeashGoal(state.anchor);
        state.return_goal = Ref(goal);
        monster->enemy = monster->oldenemy = nullptr;
        monster->goalentity = monster->movetarget = goal;
        monster->monsterinfo.aiflags |= AI_COMBAT_POINT;
        monster->ideal_yaw = vectoyaw(state.anchor - monster->s.origin);
        if (monster->monsterinfo.run)
            monster->monsterinfo.run(monster);
        RaidDirector_NotifyEntityEvent(controller, "leash_return", monster);
    }
}

USE(raid_monster_door_use) (edict_t *self, edict_t *, edict_t *) -> void
{
    door_runtime_t &runtime = Runtime(self);
    if (runtime.active)
    {
        runtime.last_status = "enable ignored: already active";
        return;
    }
    runtime.active = true;
    runtime.replenish_at = 0_ms;
    QueueInitialRelease(self, runtime);
    RaidDirector_NotifyEntityEvent(self, "activated", nullptr);
}

THINK(raid_monster_door_think) (edict_t *self) -> void
{
    door_runtime_t &runtime = Runtime(self);
    const float interval = self->wait > 0.0f ? self->wait : 0.5f;
    const int max_active = self->health > 0 ? self->health : std::numeric_limits<int>::max();
    int active = CountDoorInstances(self, false);

    if (runtime.active && runtime.release_remaining > 0 && active < max_active && level.time >= runtime.next_release)
    {
        edict_t *pending = Resolve(runtime.pending);
        if (!pending)
        {
            runtime.pending = {};
            runtime.pending_woken = false;
            pending = CreateDoorInstance(self, runtime);
            if (pending)
            {
                runtime.pending = Ref(pending);
                runtime.last_status = "instance initializing";
            }
            else
                runtime.release_remaining = 0;
        }
        else if (!runtime.pending_woken)
        {
            if (pending->svflags & SVF_NOCLIENT)
            {
                if (WakeMonster(self, pending))
                {
                    runtime.pending_woken = true;
                    runtime.last_status = "waiting for clear deployment volume";
                }
                else
                {
                    G_FreeEdict(pending);
                    runtime.pending = {};
                    runtime.last_status = "wake failed";
                }
            }
            else
                runtime.last_status = "instance initializing";
        }
        else if (!(pending->svflags & SVF_NOCLIENT))
        {
            runtime.deployed_any = true;
            runtime.leashes.push_back({ Ref(pending), pending->pos1 });
            RaidDirector_NotifyEntityEvent(self, "deploy", pending);
            --runtime.release_remaining;
            ++active;
            runtime.pending = {};
            runtime.pending_woken = false;
            runtime.next_release = level.time + gtime_t::from_sec(interval);
            runtime.last_status = "monster visibly deployed";
        }
    }

    if (runtime.active && runtime.release_remaining == 0)
    {
        int requested = 0;
        if (self->style == 1 && active < self->radius_dmg)
            requested = self->radius_dmg - active;
        else if (self->style == 2 && runtime.deployed_any && active == 0)
            requested = self->dmg > 0 ? self->dmg : (self->health > 0 ? self->health : 1);

        if (requested > 0)
        {
            if (!runtime.replenish_at)
                runtime.replenish_at = level.time + gtime_t::from_sec(std::max(0.0f, self->delay));
            else if (level.time >= runtime.replenish_at)
            {
                runtime.release_remaining = std::max(0, std::min(requested, max_active - CountDoorTotal(self)));
                runtime.next_release = level.time;
                runtime.replenish_at = 0_ms;
                runtime.last_status = runtime.release_remaining > 0 ? "replenishment queued" : "replenishment capped";
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
    if (!st.was_key_specified("wake_mode")) ent->mass = 1;
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
        std::vector<entity_ref_t> captured;
        for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
        {
            edict_t *monster = &g_edicts[i];
            if (!monster->inuse || !monster->classname || strncmp(monster->classname, "monster_", 8) || !monster->targetname)
                continue;
            bool belongs = false;
            for (uint32_t j = game.maxclients + 1; j < globals.num_edicts; ++j)
            {
                edict_t *controller = &g_edicts[j];
                if (controller->inuse && controller->classname && !Q_strcasecmp(controller->classname, "raid_monster_door") &&
                    AcceptsRoster(controller, monster->targetname)) { belongs = true; break; }
            }
            if (!belongs)
                continue;
            CaptureTemplate(monster);
            captured.push_back(Ref(monster));
        }
        for (const entity_ref_t &ref : captured)
            if (edict_t *monster = Resolve(ref)) G_FreeEdict(monster);
        gi.Com_PrintFmt("[raid] captured {} reusable monster roster templates\n", roster_templates.size());
    }
    ClearLeashGoals();
    ClearRosterInstances();
    door_runtimes.clear();
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *controller = &g_edicts[i];
        if (!controller->inuse || !controller->classname || Q_strcasecmp(controller->classname, "raid_monster_door"))
            continue;
        Runtime(controller);
    }
}

void RaidMonsters_Reset()
{
    ClearLeashGoals();
    ClearRosterInstances();
    door_runtimes.clear();
}

void RaidMonsters_ClearMap()
{
    ClearLeashGoals();
    door_runtimes.clear();
    roster_templates.clear();
}

bool RaidMonsters_SetEnabled(const char *targetname, bool enabled)
{
    if (!targetname || !*targetname)
        return false;
    bool matched = false;
    edict_t *controller = nullptr;
    while ((controller = G_FindByString<&edict_t::targetname>(controller, targetname)))
    {
        if (!controller->classname || Q_strcasecmp(controller->classname, "raid_monster_door"))
            continue;
        matched = true;
        door_runtime_t &runtime = Runtime(controller);
        if (enabled)
        {
            if (!runtime.active)
            {
                runtime.active = true;
                runtime.replenish_at = 0_ms;
                QueueInitialRelease(controller, runtime);
                RaidDirector_NotifyEntityEvent(controller, "activated", nullptr);
            }
            else
                runtime.last_status = "enable ignored: already active";
        }
        else
        {
            if (edict_t *pending = Resolve(runtime.pending))
                G_FreeEdict(pending);
            runtime.active = false;
            runtime.release_remaining = 0;
            runtime.replenish_at = 0_ms;
            runtime.pending = {};
            runtime.pending_woken = false;
            runtime.last_status = "disabled";
            RaidDirector_NotifyEntityEvent(controller, "deactivated", nullptr);
        }
    }
    return matched;
}

void RaidMonsters_Dump()
{
    gi.Com_PrintFmt("[raid] Monster doors: {} controllers, {} templates\n", door_runtimes.size(), roster_templates.size());
    for (door_runtime_t &runtime : door_runtimes)
    {
        edict_t *controller = Resolve(runtime.controller);
        if (!controller)
            continue;
        int eligible_templates = 0;
        for (const roster_entry_t &entry : roster_templates)
            if (AcceptsRoster(controller, entry.targetname.c_str()))
                ++eligible_templates;
        gi.Com_PrintFmt("[raid] door='{}' enabled={} rosters='{}' templates={} active={} hidden={} queued={} status='{}'\n",
            controller->targetname ? controller->targetname : "<unnamed>", runtime.active,
            controller->target ? controller->target : "", eligible_templates,
            CountDoorInstances(controller, false), CountDoorInstances(controller, true),
            runtime.release_remaining, runtime.last_status);
    }
}
