#include "g_local.h"
#include "raid_director.h"
#include "raid_hats.h"

#include <algorithm>
#include <vector>

namespace
{
struct entity_ref_t
{
    uint32_t number = 0;
    int32_t spawn_count = 0;
};

struct applied_hat_t
{
    entity_ref_t hat;
    entity_ref_t monster;
    entity_ref_t attachment;
    bool watched = false;
    bool observation_initialized = false;
    bool moving = true;
    gtime_t freeze_at;
    gtime_t release_at;
    gtime_t moving_since;
    gtime_t next_twitch;
};

std::vector<entity_ref_t> hats;
std::vector<applied_hat_t> applied_hats;
constexpr spawnflags_t RAID_HAT_START_DISABLED = 1_spawnflag;

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

bool SameRef(const entity_ref_t &ref, edict_t *entity)
{
    return entity && ref.number == entity->s.number && ref.spawn_count == entity->spawn_count;
}

bool Matches(edict_t *hat, edict_t *monster)
{
    return hat && monster && monster->inuse && (monster->svflags & SVF_MONSTER) && monster->health > 0 &&
        hat->target && ((monster->map && !Q_strcasecmp(hat->target, monster->map)) ||
            (monster->targetname && !Q_strcasecmp(hat->target, monster->targetname)));
}

THINK(raid_hat_attachment_think) (edict_t *self) -> void
{
    edict_t *monster = self->owner;
    if (!monster || !monster->inuse || monster->health <= 0 || monster->deadflag)
    {
        G_FreeEdict(self);
        return;
    }

    vec3_t forward, right, up;
    AngleVectors(monster->s.angles, forward, right, up);
    self->s.origin = monster->s.origin + forward * self->move_origin[0] +
        right * self->move_origin[1] + up * self->move_origin[2];
    self->s.angles = monster->s.angles + self->move_angles;
    gi.linkentity(self);
    self->nextthink = level.time + FRAME_TIME_S;
}

edict_t *CreateAttachment(edict_t *hat, edict_t *monster)
{
    if (!hat->model || !*hat->model)
        return nullptr;

    edict_t *attachment = G_Spawn();
    attachment->classname = "raid_hat_attachment";
    attachment->owner = monster;
    attachment->movetype = MOVETYPE_NONE;
    attachment->solid = SOLID_NOT;
    attachment->model = G_CopyString(hat->model, TAG_LEVEL);
    attachment->move_origin = hat->move_origin;
    if (attachment->move_origin.lengthSquared() == 0.0f)
        attachment->move_origin[2] = monster->maxs[2] + 8.0f;
    attachment->move_angles = hat->move_angles;
    attachment->s.scale = hat->s.scale > 0.0f ? hat->s.scale : 1.0f;
    attachment->s.renderfx = RF_MINLIGHT;
    gi.setmodel(attachment, attachment->model);
    attachment->think = raid_hat_attachment_think;
    attachment->nextthink = level.time + FRAME_TIME_S;
    raid_hat_attachment_think(attachment);
    return attachment;
}

void ApplyModifiers(edict_t *hat, edict_t *monster)
{
    const float health_multiplier = hat->speed > 0.0f ? hat->speed : 1.0f;
    if (hat->health > 0)
        monster->health = monster->max_health = hat->health;
    else if (health_multiplier != 1.0f)
    {
        monster->health = std::max(1, static_cast<int>(monster->health * health_multiplier));
        monster->max_health = monster->health;
    }

    if (hat->accel > 0.0f && hat->accel != 1.0f)
    {
        const float old_scale = monster->s.scale > 0.0f ? monster->s.scale : 1.0f;
        const float ratio = hat->accel / old_scale;
        monster->s.scale = hat->accel;
        monster->monsterinfo.scale *= ratio;
        monster->mins *= ratio;
        monster->maxs *= ratio;
        monster->mass = std::max(1, static_cast<int>(monster->mass * ratio));
        gi.linkentity(monster);
    }

    if (hat->monsterinfo.power_armor_power > 0)
    {
        monster->monsterinfo.power_armor_type = hat->monsterinfo.power_armor_type != IT_NULL
            ? hat->monsterinfo.power_armor_type
            : IT_ITEM_POWER_SHIELD;
        monster->monsterinfo.power_armor_power = hat->monsterinfo.power_armor_power;
        monster->monsterinfo.max_power_armor_power = hat->monsterinfo.power_armor_power;
        monster->monsterinfo.initial_power_armor_type = monster->monsterinfo.power_armor_type;
    }

    if (hat->style == 1)
    {
        monster->s.effects |= EF_COLOR_SHELL;
        monster->s.renderfx |= RF_SHELL_RED;
    }
    else if (hat->style == 2)
    {
        monster->s.effects |= EF_COLOR_SHELL;
        monster->s.renderfx |= RF_SHELL_BLUE;
    }
    else if (hat->style >= 3)
    {
        monster->s.effects |= EF_COLOR_SHELL;
        monster->s.renderfx |= RF_SHELL_RED | RF_SHELL_BLUE;
    }

    if (hat->mass >= 1 && hat->mass != 3)
        monster->monsterinfo.aiflags |= AI_GOOD_GUY;
    if (hat->mass == 2)
        monster->monsterinfo.aiflags |= AI_STAND_GROUND | AI_HOLD_FRAME;
    if (hat->mass >= 2 && monster->monsterinfo.active_move)
        monster->s.frame = std::clamp(monster->monsterinfo.active_move->firstframe + std::max(0, hat->dmg),
            monster->monsterinfo.active_move->firstframe, monster->monsterinfo.active_move->lastframe);
}

void ApplyHat(edict_t *hat, edict_t *monster)
{
    for (const applied_hat_t &applied : applied_hats)
        if (SameRef(applied.hat, hat) && SameRef(applied.monster, monster))
            return;

    ApplyModifiers(hat, monster);
    edict_t *attachment = CreateAttachment(hat, monster);
    applied_hats.push_back({ Ref(hat), Ref(monster), attachment ? Ref(attachment) : entity_ref_t {} });
    RaidDirector_NotifyEntityEvent(hat, "applied", monster);
}

bool PlayerWatches(edict_t *player, edict_t *monster, float range)
{
    if (!player || !player->client || player->deadflag || player->client->resp.spectator)
        return false;
    const vec3_t eye = player->s.origin + vec3_t{ 0, 0, static_cast<float>(player->viewheight) };
    const vec3_t center = monster->s.origin + (monster->mins + monster->maxs) * 0.5f;
    const vec3_t delta = center - eye;
    const float distance = delta.length();
    if (distance <= 0.0f || distance > range)
        return false;
    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    if (forward.dot(delta / distance) < 0.72f)
        return false;
    const trace_t sight = gi.traceline(eye, center, player, MASK_SHOT);
    return sight.fraction == 1.0f || sight.ent == monster;
}

void UpdateObserverLock(edict_t *hat, applied_hat_t &applied)
{
    if (!hat || hat->mass != 3)
        return;
    edict_t *monster = Resolve(applied.monster);
    if (!monster || monster->health <= 0)
        return;
    const float range = hat->dmg_radius > 0.0f ? hat->dmg_radius : 2048.0f;
    bool watched = false;
    for (edict_t *player : active_players())
        if (PlayerWatches(player, monster, range)) { watched = true; break; }

    const bool should_move = hat->sounds ? watched : !watched;
    if (!applied.observation_initialized || watched != applied.watched)
    {
        applied.observation_initialized = true;
        applied.watched = watched;
        if (should_move)
        {
            applied.release_at = level.time + gtime_t::from_sec(std::max(0.0f, hat->wait));
            applied.freeze_at = 0_ms;
        }
        else
        {
            applied.release_at = 0_ms;
            const gtime_t minimum_move_end = applied.moving_since + gtime_t::from_sec(std::max(0.0f, hat->decel));
            applied.freeze_at = std::max(level.time + gtime_t::from_sec(std::max(0.0f, hat->delay)), minimum_move_end);
        }
        RaidDirector_NotifyEntityEvent(hat, watched ? "watched" : "unwatched", monster);
    }

    if (should_move)
    {
        if (level.time < applied.release_at)
            return;
        if (!applied.moving)
        {
            applied.moving = true;
            applied.moving_since = level.time;
            monster->monsterinfo.aiflags &= ~(AI_HOLD_FRAME | AI_STAND_GROUND);
            monster->nextthink = level.time + FRAME_TIME_S;
            RaidDirector_NotifyEntityEvent(hat, "movement_released", monster);
        }
        return;
    }

    if (level.time < applied.freeze_at)
        return;

    monster->velocity = {};
    monster->avelocity = {};
    monster->monsterinfo.aiflags |= AI_HOLD_FRAME | AI_STAND_GROUND;
    monster->nextthink = level.time + 200_ms;
    if (applied.moving)
    {
        applied.moving = false;
        RaidDirector_NotifyEntityEvent(hat, "movement_frozen", monster);
    }
    if (hat->random > 0.0f && (!applied.next_twitch || level.time >= applied.next_twitch))
    {
        applied.next_twitch = level.time + 1_sec;
        if (frandom() < hat->random && monster->monsterinfo.active_move)
        {
            monster->s.frame = monster->s.frame >= monster->monsterinfo.active_move->lastframe ?
                monster->monsterinfo.active_move->firstframe : monster->s.frame + 1;
            RaidDirector_NotifyEntityEvent(hat, "twitch", monster);
        }
    }
}

THINK(raid_hat_think) (edict_t *self) -> void
{
    if (self->count)
        for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
            if (Matches(self, &g_edicts[i]))
                ApplyHat(self, &g_edicts[i]);

    applied_hats.erase(std::remove_if(applied_hats.begin(), applied_hats.end(), [](const applied_hat_t &applied) {
        return !Resolve(applied.hat) || !Resolve(applied.monster);
    }), applied_hats.end());
    for (applied_hat_t &applied : applied_hats)
        if (SameRef(applied.hat, self))
            UpdateObserverLock(self, applied);
    self->nextthink = level.time + (self->mass == 3 ? 50_ms : 500_ms);
}

USE(raid_hat_use) (edict_t *self, edict_t *, edict_t *activator) -> void
{
    if (self->count && self->mass == 3)
    {
        RaidHats_SetObserverInverted(self, !self->sounds, activator);
        return;
    }
    if (self->count)
        return;
    self->count = 1;
    RaidDirector_NotifyEntityEvent(self, "activated", activator);
}
}

bool RaidHats_SetObserverInverted(edict_t *hat, bool inverted, edict_t *activator)
{
    if (!hat || !hat->classname || Q_strcasecmp(hat->classname, "raid_hat") || hat->mass != 3)
        return false;
    hat->sounds = inverted ? 1 : 0;
    for (applied_hat_t &applied : applied_hats)
        if (SameRef(applied.hat, hat))
            applied.observation_initialized = false;
    RaidDirector_NotifyEntityEvent(hat, inverted ? "observation_inverted" : "observation_normal", activator);
    return true;
}

void SP_raid_hat(edict_t *ent)
{
    if (!ent->target || !*ent->target)
        gi.Com_PrintFmt("[raid] raid_hat '{}' has no monster target group\n",
            ent->targetname ? ent->targetname : "<unnamed>");
    ent->solid = SOLID_NOT;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    ent->count = !ent->spawnflags.has(RAID_HAT_START_DISABLED);
    ent->use = raid_hat_use;
    ent->think = raid_hat_think;
    ent->nextthink = level.time + 200_ms;
    if (hats.size() < 32)
    {
        ent->noise_index = static_cast<int>(hats.size());
        gi.configstring(CONFIG_RAID_HAT_NAME + ent->noise_index,
            ent->message && *ent->message ? ent->message : "RAID TARGET");
        hats.push_back(Ref(ent));
    }
    else
        gi.Com_PrintFmt("[raid] raid_hat '{}' exceeds the 32 presentation-name limit\n",
            ent->targetname ? ent->targetname : "<unnamed>");
    gi.linkentity(ent);
}

void RaidHats_ApplyMonster(edict_t *monster)
{
    for (const entity_ref_t &hat_ref : hats)
        if (edict_t *hat = Resolve(hat_ref); Matches(hat, monster))
            ApplyHat(hat, monster);
}

void RaidHats_OnMonsterKilled(edict_t *monster, edict_t *attacker)
{
    for (const applied_hat_t &applied : applied_hats)
    {
        if (!SameRef(applied.monster, monster))
            continue;
        if (edict_t *hat = Resolve(applied.hat))
            RaidDirector_NotifyEntityEvent(hat, "monster_killed", attacker);
        if (edict_t *attachment = Resolve(applied.attachment))
            G_FreeEdict(attachment);
    }
}

void RaidHats_UpdateHUD(edict_t *player)
{
    if (!player || !player->client)
        return;
    player->client->ps.stats[STAT_RAID_HAT_NAME] = 0;
    player->client->ps.stats[STAT_RAID_HAT_HEALTH] = 0;
    player->client->ps.stats[STAT_RAID_HAT_RANK] = 0;
    if (player->deadflag || player->client->resp.spectator)
        return;

    const vec3_t eye = player->s.origin + vec3_t{ 0, 0, static_cast<float>(player->viewheight) };
    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    edict_t *best_hat = nullptr;
    edict_t *best_monster = nullptr;
    float best_perpendicular = std::numeric_limits<float>::max();

    for (const applied_hat_t &applied : applied_hats)
    {
        edict_t *hat = Resolve(applied.hat);
        edict_t *monster = Resolve(applied.monster);
        if (!hat || !monster || monster->health <= 0 || monster->deadflag)
            continue;
        const vec3_t center = monster->s.origin + (monster->mins + monster->maxs) * 0.5f;
        const vec3_t delta = center - eye;
        const float distance = delta.length();
        const float display_distance = hat->dmg_radius > 0.0f ? hat->dmg_radius : 1024.0f;
        if (distance > display_distance)
            continue;
        const float along = delta.dot(forward);
        if (along <= 0.0f)
            continue;
        const float perpendicular = (delta - forward * along).length();
        const float aim_radius = std::max(24.0f, (monster->maxs - monster->mins).length() * 0.35f);
        if (perpendicular > aim_radius || perpendicular >= best_perpendicular)
            continue;
        const trace_t sight = gi.traceline(eye, center, player, MASK_SHOT);
        if (sight.fraction < 1.0f && sight.ent != monster)
            continue;
        best_hat = hat;
        best_monster = monster;
        best_perpendicular = perpendicular;
    }

    if (!best_hat || !best_monster)
        return;
    player->client->ps.stats[STAT_RAID_HAT_NAME] = static_cast<int16_t>(best_hat->noise_index + 1);
    player->client->ps.stats[STAT_RAID_HAT_HEALTH] = static_cast<int16_t>(std::clamp(
        best_monster->health * 1000 / std::max(1, best_monster->max_health), 0, 1000));
    const int shield = std::clamp(
        best_monster->monsterinfo.power_armor_power * 1000 /
            std::max(1, best_monster->monsterinfo.max_power_armor_power), 0, 1000);
    player->client->ps.stats[STAT_RAID_HAT_RANK] = static_cast<int16_t>(
        (shield << 2) | std::clamp(best_hat->style, 0, 3));
}

void RaidHats_Reset()
{
    for (const applied_hat_t &applied : applied_hats)
        if (edict_t *attachment = Resolve(applied.attachment))
            G_FreeEdict(attachment);
    applied_hats.clear();
}

void RaidHats_ClearMap()
{
    RaidHats_Reset();
    hats.clear();
}
