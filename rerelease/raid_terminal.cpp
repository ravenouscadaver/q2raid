#include "g_local.h"
#include "raid_director.h"
#include "raid_downed.h"
#include "raid_terminal.h"
#include "raid_ui.h"

void InitTrigger(edict_t *self);

namespace
{
constexpr spawnflags_t RAID_TERMINAL_LEGACY_DIRECT_OPEN = 1_spawnflag;

edict_t *NamedEntity(const char *targetname)
{
    return targetname && *targetname ?
        G_FindByString<&edict_t::targetname>(nullptr, targetname) : nullptr;
}

bool RequiresExplicitInteract(edict_t *self)
{
    return self && self->pathtarget && !Q_strcasecmp(self->pathtarget, "interact");
}

bool ValidInteractor(edict_t *player)
{
    return player && player->client && player->health > 0 && !player->deadflag &&
        !player->client->resp.spectator && !RaidDowned_IsDown(player);
}

bool ActivateInteraction(edict_t *self, edict_t *other)
{
    if (!self || !ValidInteractor(other) || level.time < self->touch_debounce_time)
        return false;

    self->touch_debounce_time = level.time +
        gtime_t::from_sec(self->wait > 0.0f ? self->wait : 0.5f);
    RaidDirector_NotifyEntityEvent(self, "interact", other);

    if (self->spawnflags.has(RAID_TERMINAL_LEGACY_DIRECT_OPEN))
        RaidUI_Open(other, NamedEntity(self->target));
    return true;
}

TOUCH(raid_interaction_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!ValidInteractor(other) || RequiresExplicitInteract(self))
        return;
    ActivateInteraction(self, other);
}

TOUCH(raid_terminal_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!ValidInteractor(other) || RequiresExplicitInteract(self))
        return;
    if (ActivateInteraction(self, other) && !RaidUI_IsActive(other))
        RaidUI_Open(other, NamedEntity(self->target));
}
}

void SP_trigger_raid_interaction(edict_t *ent)
{
    InitTrigger(ent);
    ent->touch = raid_interaction_touch;
    gi.linkentity(ent);
}

void SP_trigger_raid_terminal(edict_t *ent)
{
    InitTrigger(ent);
    ent->touch = raid_terminal_touch;
    gi.linkentity(ent);
}

bool RaidTerminal_Open(edict_t *player, edict_t *terminal)
{
    return RaidUI_Open(player, terminal);
}

bool RaidTerminal_IsActive(edict_t *player)
{
    return RaidUI_IsActive(player);
}

bool RaidTerminal_Interact(edict_t *player, bool pressed)
{
    if (!pressed || !ValidInteractor(player) || RaidUI_IsActive(player))
        return false;

    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *trigger = &g_edicts[i];
        if (!trigger->inuse || !trigger->classname || !RequiresExplicitInteract(trigger) ||
            (Q_strcasecmp(trigger->classname, "trigger_raid_interaction") &&
             Q_strcasecmp(trigger->classname, "trigger_raid_terminal")) ||
            !boxes_intersect(player->absmin, player->absmax, trigger->absmin, trigger->absmax))
            continue;

        if (!ActivateInteraction(trigger, player))
            return false;
        if (!Q_strcasecmp(trigger->classname, "trigger_raid_terminal") && !RaidUI_IsActive(player))
            RaidUI_Open(player, NamedEntity(trigger->target));
        return true;
    }
    return false;
}

void RaidTerminal_UpdateHUD(edict_t *player)
{
    RaidUI_UpdateHUD(player);
}

bool RaidTerminal_HandleInput(edict_t *player, usercmd_t *cmd)
{
    return RaidUI_HandleInput(player, cmd);
}

bool RaidTerminal_Cancel(edict_t *player)
{
    if (!RaidUI_IsActive(player))
        return false;
    RaidUI_Close(player);
    return true;
}

void RaidTerminal_Disconnect(edict_t *player)
{
    RaidUI_Disconnect(player);
}

void RaidTerminal_Reset()
{
    RaidUI_Reset();
}
