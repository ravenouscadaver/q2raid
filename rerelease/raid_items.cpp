#include "g_local.h"
#include "raid_director.h"
#include "raid_items.h"
#include "raid_thirdperson.h"

void InitTrigger(edict_t *self);

namespace
{
struct raid_carry_state_t { edict_t *item = nullptr; };
raid_carry_state_t carry_states[MAX_CLIENTS];

raid_carry_state_t &CarryState(edict_t *player) { return carry_states[player->s.number - 1]; }
bool IsPowerCore(edict_t *item) { return item && item->message && !Q_strcasecmp(item->message, "power_core"); }

void FinishCarry(edict_t *player)
{
    CarryState(player).item = nullptr;
    RaidThirdPerson_SetCarry(player, false);
}

TOUCH(raid_item_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || other->deadflag || level.time < self->touch_debounce_time ||
        RaidCarry_IsCarrying(other) || !IsPowerCore(self))
        return;

    CarryState(other).item = self;
    self->solid = SOLID_NOT;
    self->movetype = MOVETYPE_NONE;
    self->svflags |= SVF_NOCLIENT;
    gi.unlinkentity(self);
    RaidThirdPerson_SetCarry(other, true);
    RaidDirector_NotifyEntityEvent(self, "pickup", other);
    gi.LocClient_Print(other, PRINT_HIGH, "POWER CORE ACQUIRED\n");
}

edict_t *FindSocket(edict_t *trigger, edict_t *item)
{
    edict_t *best = nullptr;
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *gadget = &g_edicts[i];
        if (!gadget->inuse || !gadget->classname || Q_strcasecmp(gadget->classname, "raid_gadget") || gadget->health)
            continue;
        if (!gadget->message || Q_strcasecmp(gadget->message, "core_socket"))
            continue;
        if (gadget->target && item->message && Q_strcasecmp(gadget->target, item->message))
            continue;
        if (trigger->target && (!gadget->team || Q_strcasecmp(trigger->target, gadget->team)))
            continue;
        if (!best || gadget->count < best->count)
            best = gadget;
    }
    return best;
}

TOUCH(raid_deposit_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || !RaidCarry_IsCarrying(other))
        return;
    edict_t *item = CarryState(other).item;
    edict_t *socket = FindSocket(self, item);
    if (!socket)
        return;

    socket->health = 1;
    item->s.origin = socket->s.origin;
    item->s.angles = socket->s.angles;
    item->solid = SOLID_NOT;
    item->movetype = MOVETYPE_NONE;
    item->svflags &= ~SVF_NOCLIENT;
    item->s.effects = EF_NONE;
    gi.linkentity(item);
    FinishCarry(other);
    RaidDirector_NotifyEntityEvent(socket, "deposit", other);
    gi.LocClient_Print(other, PRINT_HIGH, "POWER CORE DEPOSITED\n");
}
}

void SP_raid_item(edict_t *ent)
{
    if (!ent->message) ent->message = "power_core";
    if (!ent->model) ent->model = "models/items/n64/power_core/tris.md2";
    gi.setmodel(ent, ent->model);
    ent->mins = { -16, -16, -16 };
    ent->maxs = { 16, 16, 16 };
    ent->solid = SOLID_TRIGGER;
    ent->movetype = MOVETYPE_TOSS;
    ent->touch = raid_item_touch;
    ent->s.effects = EF_ROTATE | EF_BOB;
    ent->s.renderfx = RF_GLOW | RF_NO_LOD;
    gi.linkentity(ent);
}

void SP_raid_gadget(edict_t *ent)
{
    if (!ent->message) ent->message = "core_socket";
    if (!ent->target) ent->target = "power_core";
    ent->solid = SOLID_NOT;
    ent->movetype = MOVETYPE_NONE;
    gi.linkentity(ent);
}

void SP_trigger_raid_deposit(edict_t *ent)
{
    InitTrigger(ent);
    ent->touch = raid_deposit_touch;
    gi.linkentity(ent);
}

bool RaidCarry_IsCarrying(edict_t *player)
{
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS && CarryState(player).item;
}

bool RaidCarry_Drop(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player)) return false;
    edict_t *item = CarryState(player).item;
    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    item->s.origin = player->s.origin + (forward * 28.0f) + vec3_t{ 0, 0, 8 };
    item->velocity = forward * 180.0f + vec3_t{ 0, 0, 100 };
    item->solid = SOLID_TRIGGER;
    item->movetype = MOVETYPE_TOSS;
    item->svflags &= ~SVF_NOCLIENT;
    item->s.effects = EF_ROTATE | EF_BOB;
    item->touch_debounce_time = level.time + 1_sec;
    gi.linkentity(item);
    FinishCarry(player);
    RaidDirector_NotifyEntityEvent(item, "drop", player);
    return true;
}

void RaidCarry_Update(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player)) return;
    if (player->deadflag || player->health <= 0) { RaidCarry_Drop(player); return; }
    player->client->weapon_fire_buffered = false;
    player->client->latched_buttons &= ~BUTTON_ATTACK;
}
