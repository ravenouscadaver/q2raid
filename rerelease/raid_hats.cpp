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
};

std::vector<entity_ref_t> hats;
std::vector<applied_hat_t> applied_hats;

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
        hat->target && monster->targetname && !Q_strcasecmp(hat->target, monster->targetname);
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

    if (hat->mass >= 1)
        monster->monsterinfo.aiflags |= AI_GOOD_GUY;
    if (hat->mass >= 2)
        monster->monsterinfo.aiflags |= AI_STAND_GROUND | AI_HOLD_FRAME;
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

THINK(raid_hat_think) (edict_t *self) -> void
{
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
        if (Matches(self, &g_edicts[i]))
            ApplyHat(self, &g_edicts[i]);

    applied_hats.erase(std::remove_if(applied_hats.begin(), applied_hats.end(), [](const applied_hat_t &applied) {
        return !Resolve(applied.hat) || !Resolve(applied.monster);
    }), applied_hats.end());
    self->nextthink = level.time + 500_ms;
}
}

void SP_raid_hat(edict_t *ent)
{
    if (!ent->target || !*ent->target)
        gi.Com_PrintFmt("[raid] raid_hat '{}' has no monster target group\n",
            ent->targetname ? ent->targetname : "<unnamed>");
    ent->solid = SOLID_NOT;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    ent->think = raid_hat_think;
    ent->nextthink = level.time + 200_ms;
    hats.push_back(Ref(ent));
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
