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

TOUCH(raid_interaction_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || other->health <= 0 || RaidDowned_IsDown(other) ||
        level.time < self->touch_debounce_time)
        return;
    self->touch_debounce_time = level.time +
        gtime_t::from_sec(self->wait > 0.0f ? self->wait : 0.5f);
    RaidDirector_NotifyEntityEvent(self, "interact", other);

    if (self->spawnflags.has(RAID_TERMINAL_LEGACY_DIRECT_OPEN))
        RaidUI_Open(other, NamedEntity(self->target));
}

TOUCH(raid_terminal_touch) (edict_t *self, edict_t *other, const trace_t &trace,
    bool other_touching_self) -> void
{
    raid_interaction_touch(self, other, trace, other_touching_self);
    if (other && other->client && !RaidUI_IsActive(other))
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
