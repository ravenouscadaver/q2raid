#include "g_local.h"
#include "raid_director.h"
#include "raid_items.h"
#include "raid_thirdperson.h"

void InitTrigger(edict_t *self);

namespace
{
enum raid_carry_mode_t { RAID_CARRY_AUTO = 0, RAID_CARRY_HEAVY = 1, RAID_CARRY_WEAPON = 2 };

struct raid_carry_state_t
{
    edict_t *item = nullptr;
    gitem_t *previous_weapon = nullptr;
    int previous_relic_inventory = 0;
};
raid_carry_state_t carry_states[MAX_CLIENTS];

struct raid_hover_state_t
{
    uint32_t entity_number = 0;
    int32_t spawn_count = 0;
    gtime_t next_pulse;
};
raid_hover_state_t hover_states[MAX_CLIENTS];

raid_carry_state_t &CarryState(edict_t *player) { return carry_states[player->s.number - 1]; }
bool IsSupportedRaidItem(edict_t *item)
{
    return item && item->message &&
        (!Q_strcasecmp(item->message, "power_core") || !Q_strcasecmp(item->message, "bfg_relic"));
}

bool IsWeaponRelic(edict_t *item)
{
    return item && (item->style == RAID_CARRY_WEAPON ||
        (item->style == RAID_CARRY_AUTO && item->message && !Q_strcasecmp(item->message, "bfg_relic")));
}

gitem_t *RelicWeapon(edict_t *item)
{
    const char *classname = item && item->pathtarget ? item->pathtarget : "weapon_bfg";
    return FindItemByClassname(classname);
}

void FinishCarry(edict_t *player)
{
    auto &state = CarryState(player);
    if (state.item && state.item->deathtarget && state.item->sounds)
        RaidDirector_ClearStatus(player, state.item->deathtarget);
    if (IsWeaponRelic(state.item))
    {
        gitem_t *weapon = RelicWeapon(state.item);
        if (weapon)
            player->client->pers.inventory[weapon->id] = state.previous_relic_inventory;
        player->client->newweapon = state.previous_weapon;
        ChangeWeapon(player);
    }
    else
        RaidThirdPerson_SetCarry(player, false);
    state = {};
}

void RestoreRaidItem(edict_t *item)
{
    item->s.origin = item->move_origin;
    item->s.angles = item->move_angles;
    item->velocity = {};
    item->avelocity = {};
    item->solid = SOLID_TRIGGER;
    item->movetype = MOVETYPE_TOSS;
    item->svflags &= ~SVF_NOCLIENT;
    item->s.effects = EF_ROTATE | EF_BOB;
    item->touch_debounce_time = 0_ms;
    gi.linkentity(item);
}

TOUCH(raid_item_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || other->deadflag || level.time < self->touch_debounce_time ||
        RaidCarry_IsCarrying(other) || !IsSupportedRaidItem(self))
        return;

    auto &state = CarryState(other);
    state.item = self;
    self->solid = SOLID_NOT;
    self->movetype = MOVETYPE_NONE;
    self->svflags |= SVF_NOCLIENT;
    gi.unlinkentity(self);
    if (IsWeaponRelic(self))
    {
        gitem_t *weapon = RelicWeapon(self);
        if (!weapon)
        {
            gi.Com_PrintFmt("[raid] raid_item '{}' weapon '{}' not found\n",
                self->targetname ? self->targetname : "<unnamed>", self->pathtarget ? self->pathtarget : "weapon_bfg");
            RestoreRaidItem(self);
            state = {};
            return;
        }
        state.previous_weapon = other->client->pers.weapon;
        state.previous_relic_inventory = other->client->pers.inventory[weapon->id];
        other->client->pers.inventory[weapon->id] = std::max(1, state.previous_relic_inventory);
        other->client->newweapon = weapon;
        ChangeWeapon(other);
    }
    else
        RaidThirdPerson_SetCarry(other, true, self->model, self->s.scale);
    if (self->deathtarget && *self->deathtarget)
    {
        const float configured_duration = self->delay > 0.0f
            ? self->delay
            : RaidDirector_StatusDuration(self->deathtarget, 15.0f);
        if (!self->timestamp || self->timestamp <= level.time)
            self->timestamp = level.time + gtime_t::from_sec(configured_duration);
        const float remaining = std::max(0.1f, (self->timestamp - level.time).seconds());
        RaidDirector_ApplyStatus(other, self->deathtarget, remaining, self->healthtarget);
    }
    RaidDirector_NotifyEntityEvent(self, "pickup", other);
    gi.LocClient_Print(other, PRINT_HIGH, IsWeaponRelic(self) ? "BFG RELIC ACQUIRED\n" : "POWER CORE ACQUIRED\n");
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
    if (!ent->model) ent->model = IsWeaponRelic(ent) ? "models/weapons/g_bfg/tris.md2" : "models/items/keys/power/tris.md2";
    if (!ent->speed) ent->speed = 0.45f;
    if (!ent->healthtarget) ent->healthtarget = "refresh";
    if (!st.was_key_specified("clear_status_on_drop")) ent->sounds = 1;
    ent->timestamp = 0_ms;
    ent->move_origin = ent->s.origin;
    ent->move_angles = ent->s.angles;
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

void SP_raid_hovertext(edict_t *ent)
{
    if (ent->dmg_radius <= 0.0f) ent->dmg_radius = 24.0f;
    if (ent->speed <= 0.0f) ent->speed = 256.0f;
    if (!st.was_key_specified("require_los")) ent->sounds = 1;
    ent->solid = SOLID_NOT;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    gi.linkentity(ent);
}

bool RaidCarry_IsCarrying(edict_t *player)
{
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS && CarryState(player).item;
}

bool RaidCarry_BlocksWeapons(edict_t *player)
{
    return RaidCarry_IsCarrying(player) && !IsWeaponRelic(CarryState(player).item);
}

bool RaidCarry_IsWeaponRelic(edict_t *player)
{
    return RaidCarry_IsCarrying(player) && IsWeaponRelic(CarryState(player).item);
}

float RaidCarry_MovementScale(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player)) return 1.0f;
    return std::clamp(CarryState(player).item->speed, 0.1f, 1.0f);
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
    if (RaidCarry_BlocksWeapons(player))
    {
        player->client->weapon_fire_buffered = false;
        player->client->latched_buttons &= ~BUTTON_ATTACK;
    }
}

void RaidCarry_ResetAll()
{
    for (edict_t *player : active_players())
        if (RaidCarry_IsCarrying(player))
            FinishCarry(player);

    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *entity = &g_edicts[i];
        if (!entity->inuse || !entity->classname)
            continue;
        if (!Q_strcasecmp(entity->classname, "raid_item"))
        {
            entity->timestamp = 0_ms;
            RestoreRaidItem(entity);
        }
        else if (!Q_strcasecmp(entity->classname, "raid_gadget"))
            entity->health = 0;
    }
}

void RaidHover_Reset()
{
    for (raid_hover_state_t &state : hover_states)
        state = {};
}

void RaidHover_RunFrame()
{
    for (edict_t *player : active_players())
    {
        if (!player->client || player->deadflag || player->client->resp.spectator)
            continue;

        const vec3_t eye = player->s.origin + vec3_t{ 0, 0, static_cast<float>(player->viewheight) };
        vec3_t forward;
        AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
        edict_t *best = nullptr;
        float best_perpendicular = std::numeric_limits<float>::max();

        for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
        {
            edict_t *marker = &g_edicts[i];
            if (!marker->inuse || !marker->classname || Q_strcasecmp(marker->classname, "raid_hovertext"))
                continue;

            vec3_t marker_origin = marker->s.origin;
            if (marker->target)
            {
                edict_t *follow = G_FindByString<&edict_t::targetname>(nullptr, marker->target);
                if (follow) marker_origin = follow->s.origin;
            }

            const vec3_t delta = marker_origin - eye;
            if (delta.length() > marker->speed)
                continue;
            const float along = delta.dot(forward);
            if (along <= 0.0f || along > marker->speed)
                continue;
            const float perpendicular = (delta - (forward * along)).length();
            if (perpendicular > marker->dmg_radius || perpendicular >= best_perpendicular)
                continue;
            if (marker->sounds)
            {
                const trace_t sight = gi.traceline(eye, marker_origin, player, MASK_SOLID);
                if (sight.fraction < 1.0f)
                    continue;
            }
            best = marker;
            best_perpendicular = perpendicular;
        }

        raid_hover_state_t &state = hover_states[player->s.number - 1];
        edict_t *previous = state.entity_number && state.entity_number < globals.num_edicts ? &g_edicts[state.entity_number] : nullptr;
        if (previous && (!previous->inuse || previous->spawn_count != state.spawn_count))
            previous = nullptr;

        if (best != previous)
        {
            if (previous)
                RaidDirector_NotifyEntityEvent(previous, "hover_exit", player);
            state = {};
            if (best)
            {
                state.entity_number = best->s.number;
                state.spawn_count = best->spawn_count;
                state.next_pulse = level.time + 1_sec;
                if (best->message && *best->message)
                    gi.LocCenter_Print(player, "{}", best->message);
                RaidDirector_NotifyEntityEvent(best, "hover_enter", player);
            }
        }
        else if (best && level.time >= state.next_pulse)
        {
            state.next_pulse = level.time + 1_sec;
            RaidDirector_NotifyEntityEvent(best, "hover", player);
        }
    }
}
