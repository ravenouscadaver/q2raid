#include "g_local.h"
#include "raid_director.h"
#include "raid_downed.h"
#include "raid_items.h"
#include "raid_reconstruction.h"
#include "raid_thirdperson.h"

void InitTrigger(edict_t *self);
void SpawnDamage(int type, const vec3_t &origin, const vec3_t &normal, int damage);

namespace
{
enum raid_carry_mode_t { RAID_CARRY_AUTO = 0, RAID_CARRY_HEAVY = 1, RAID_CARRY_WEAPON = 2 };

struct raid_carry_state_t
{
    uint32_t item_number = 0;
    int32_t item_spawn_count = 0;
    gitem_t *previous_weapon = nullptr;
    int previous_relic_inventory = 0;
    uint32_t charge_trigger_number = 0;
    int32_t charge_trigger_spawn_count = 0;
    gtime_t charge_started;
};
raid_carry_state_t carry_states[MAX_CLIENTS];

struct raid_item_state_t
{
    uint32_t entity_number = 0;
    int32_t spawn_count = 0;
    bool initial_charged = false;
    bool charged = false;
    gtime_t next_vfx;
};
std::vector<raid_item_state_t> item_states;

struct raid_hover_state_t
{
    uint32_t entity_number = 0;
    int32_t spawn_count = 0;
    gtime_t next_pulse;
};
raid_hover_state_t hover_states[MAX_CLIENTS];

struct gadget_state_t
{
    uint32_t entity_number = 0;
    int32_t spawn_count = 0;
    bool occupied = false;
    uint32_t item_number = 0;
    int32_t item_spawn_count = 0;
};
std::vector<gadget_state_t> gadget_states;
constexpr spawnflags_t RAID_ITEM_START_CHARGED = 1_spawnflag;

raid_item_state_t &ItemState(edict_t *item)
{
    auto found = std::find_if(item_states.begin(), item_states.end(), [item](const raid_item_state_t &state) {
        return state.entity_number == item->s.number && state.spawn_count == item->spawn_count;
    });
    if (found != item_states.end()) return *found;
    const bool start_charged = item->spawnflags.has(RAID_ITEM_START_CHARGED);
    item_states.push_back({ item->s.number, item->spawn_count,
        start_charged, start_charged, 0_ms });
    return item_states.back();
}

gadget_state_t &GadgetState(edict_t *gadget)
{
    auto found = std::find_if(gadget_states.begin(), gadget_states.end(), [gadget](const gadget_state_t &state) {
        return state.entity_number == gadget->s.number && state.spawn_count == gadget->spawn_count;
    });
    if (found != gadget_states.end()) return *found;
    gadget_states.push_back({ gadget->s.number, gadget->spawn_count, false, 0, 0 });
    return gadget_states.back();
}

void SetGadgetOccupied(edict_t *gadget, edict_t *item)
{
    gadget_state_t &state = GadgetState(gadget);
    state.occupied = item != nullptr;
    state.item_number = item ? item->s.number : 0;
    state.item_spawn_count = item ? item->spawn_count : 0;
}

void ReleaseItemSocket(edict_t *item)
{
    for (gadget_state_t &state : gadget_states)
        if (state.occupied && state.item_number == item->s.number &&
            state.item_spawn_count == item->spawn_count)
        {
            state.occupied = false;
            state.item_number = 0;
            state.item_spawn_count = 0;
        }
}

raid_carry_state_t &CarryState(edict_t *player) { return carry_states[player->s.number - 1]; }
edict_t *CarriedItem(const raid_carry_state_t &state)
{
    if (!state.item_number || state.item_number >= globals.num_edicts)
        return nullptr;
    edict_t *item = &g_edicts[state.item_number];
    return item->inuse && item->spawn_count == state.item_spawn_count ? item : nullptr;
}
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

bool CanBeCharged(edict_t *item)
{
    return item && item->message && !Q_strcasecmp(item->message, "power_core");
}

void ApplyCarryStatus(edict_t *player, edict_t *item)
{
    if (!item->deathtarget || !*item->deathtarget)
        return;
    const float configured_duration = item->delay > 0.0f
        ? item->delay
        : RaidDirector_StatusDuration(item->deathtarget, 15.0f);
    if (!item->timestamp || item->timestamp <= level.time)
        item->timestamp = level.time + gtime_t::from_sec(configured_duration);
    const float remaining = std::max(0.1f, (item->timestamp - level.time).seconds());
    RaidDirector_ApplyStatus(player, item->deathtarget, remaining, item->healthtarget);
}

gitem_t *RelicWeapon(edict_t *item)
{
    const char *classname = item && item->pathtarget ? item->pathtarget : "weapon_bfg";
    return FindItemByClassname(classname);
}

void FinishCarry(edict_t *player)
{
    auto &state = CarryState(player);
    edict_t *item = CarriedItem(state);
    if (item && item->deathtarget && item->sounds)
        RaidDirector_ClearStatus(player, item->deathtarget);
    if (IsWeaponRelic(item))
    {
        gitem_t *weapon = RelicWeapon(item);
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

void ReturnRaidItemToOrigin(edict_t *player, edict_t *item, const char *signal)
{
    FinishCarry(player);
    item->timestamp = 0_ms;
    ItemState(item).charged = ItemState(item).initial_charged;
    RestoreRaidItem(item);
    RaidDirector_NotifyEntityEvent(item, signal, player);
}

TOUCH(raid_item_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || other->deadflag || RaidDowned_IsDown(other) || level.time < self->touch_debounce_time ||
        RaidCarry_IsCarrying(other) || !IsSupportedRaidItem(self))
        return;

    ReleaseItemSocket(self);
    auto &state = CarryState(other);
    state.item_number = self->s.number;
    state.item_spawn_count = self->spawn_count;
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
    {
        RaidThirdPerson_SetCarry(other, true,
            self->combattarget && *self->combattarget ? self->combattarget : self->model, self->s.scale);
        RaidThirdPerson_SetCarryCharged(other, ItemState(self).charged);
    }
    if (!CanBeCharged(self) || ItemState(self).charged)
        ApplyCarryStatus(other, self);
    RaidDirector_NotifyEntityEvent(self, "pickup", other);
    gi.LocClient_Print(other, PRINT_HIGH, IsWeaponRelic(self) ? "BFG RELIC ACQUIRED\n" : "POWER CORE ACQUIRED\n");
}

edict_t *FindSocket(edict_t *trigger, edict_t *item)
{
    edict_t *best = nullptr;
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *gadget = &g_edicts[i];
        if (!gadget->inuse || !gadget->classname || Q_strcasecmp(gadget->classname, "raid_gadget") || GadgetState(gadget).occupied)
            continue;
        const bool core_socket = gadget->message && !Q_strcasecmp(gadget->message, "core_socket");
        const bool reconstruction = gadget->message && !Q_strcasecmp(gadget->message, "reconstruction_chamber");
        if (!core_socket && !reconstruction)
            continue;
        if (reconstruction && !RaidReconstruction_CanDeposit(gadget))
            continue;
        if (gadget->target && item->message && Q_strcasecmp(gadget->target, item->message))
            continue;
        const char *required_state = gadget->pathtarget && *gadget->pathtarget
            ? gadget->pathtarget
            : trigger->pathtarget;
        if (required_state && !Q_strcasecmp(required_state, "charged") && !ItemState(item).charged)
            continue;
        if (trigger->target && (!gadget->team || Q_strcasecmp(trigger->target, gadget->team)))
            continue;
        if (!best || gadget->count < best->count)
            best = gadget;
    }
    return best;
}

bool BoxesOverlap(const vec3_t &mins_a, const vec3_t &maxs_a, const vec3_t &mins_b, const vec3_t &maxs_b)
{
    return mins_a[0] <= maxs_b[0] && maxs_a[0] >= mins_b[0] &&
        mins_a[1] <= maxs_b[1] && maxs_a[1] >= mins_b[1] &&
        mins_a[2] <= maxs_b[2] && maxs_a[2] >= mins_b[2];
}

edict_t *FindChargingTrigger(edict_t *item, const vec3_t &held_origin)
{
    const vec3_t held_mins = held_origin + item->mins;
    const vec3_t held_maxs = held_origin + item->maxs;
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *trigger = &g_edicts[i];
        if (!trigger->inuse || trigger->solid != SOLID_TRIGGER || !trigger->classname ||
            Q_strcasecmp(trigger->classname, "trigger_raid_item"))
            continue;
        if (trigger->target && item->message && Q_strcasecmp(trigger->target, item->message))
            continue;
        if (trigger->pathtarget && Q_strcasecmp(trigger->pathtarget, "uncharged"))
            continue;
        if (BoxesOverlap(held_mins, held_maxs, trigger->absmin, trigger->absmax))
            return trigger;
    }
    return nullptr;
}

void UpdateCharging(edict_t *player, edict_t *item)
{
    raid_carry_state_t &carry = CarryState(player);
    if (!CanBeCharged(item) || ItemState(item).charged)
    {
        carry.charge_trigger_number = 0;
        carry.charge_trigger_spawn_count = 0;
        carry.charge_started = 0_ms;
        return;
    }

    edict_t *trigger = FindChargingTrigger(item, RaidCarry_HeldOrigin(player));
    if (!trigger)
    {
        if (carry.charge_trigger_number)
        {
            RaidDirector_NotifyEntityEvent(item, "charge_cancelled", player);
            gi.LocClient_Print(player, PRINT_HIGH, "CORE CHARGE INTERRUPTED\n");
        }
        carry.charge_trigger_number = 0;
        carry.charge_trigger_spawn_count = 0;
        carry.charge_started = 0_ms;
        return;
    }

    if (carry.charge_trigger_number != trigger->s.number ||
        carry.charge_trigger_spawn_count != trigger->spawn_count)
    {
        carry.charge_trigger_number = trigger->s.number;
        carry.charge_trigger_spawn_count = trigger->spawn_count;
        carry.charge_started = level.time;
        RaidDirector_NotifyEntityEvent(item, "charge_begin", player);
        RaidDirector_NotifyEntityEvent(trigger, "charge_begin", player);
        gi.LocClient_Print(player, PRINT_HIGH, "CORE CHARGING\n");
    }

    const float charge_time = trigger->delay > 0.0f ? trigger->delay : 1.0f;
    if (level.time < carry.charge_started + gtime_t::from_sec(charge_time))
        return;

    ItemState(item).charged = true;
    RaidThirdPerson_SetCarryCharged(player, true);
    carry.charge_trigger_number = 0;
    carry.charge_trigger_spawn_count = 0;
    carry.charge_started = 0_ms;
    ApplyCarryStatus(player, item);
    RaidDirector_NotifyEntityEvent(item, "charge_complete", player);
    RaidDirector_NotifyEntityEvent(trigger, "charge_complete", player);
    gi.LocClient_Print(player, PRINT_HIGH, "POWER CORE CHARGED\n");
}

TOUCH(raid_deposit_touch) (edict_t *self, edict_t *other, const trace_t &, bool) -> void
{
    if (!other->client || !RaidCarry_IsCarrying(other))
        return;
    edict_t *item = CarriedItem(CarryState(other));
    edict_t *socket = FindSocket(self, item);
    if (!socket)
        return;

    const bool reconstruction = socket->message && !Q_strcasecmp(socket->message, "reconstruction_chamber");
    if (reconstruction)
    {
        if (!RaidReconstruction_Complete(socket, other))
            return;
        item->s.origin = socket->s.origin;
        item->solid = SOLID_NOT;
        item->movetype = MOVETYPE_NONE;
        item->svflags |= SVF_NOCLIENT;
        item->s.effects = EF_NONE;
        item->timestamp = 0_ms;
        ItemState(item).charged = false;
        gi.unlinkentity(item);
        FinishCarry(other);
        SetGadgetOccupied(socket, nullptr);
        RaidDirector_NotifyEntityEvent(socket, "deposit", other);
        gi.LocClient_Print(other, PRINT_HIGH, "MARINE RECONSTRUCTED\n");
        return;
    }

    SetGadgetOccupied(socket, item);
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
    if (!ent->model || !*ent->model) ent->model = "models/items/keys/power/tris.md2";
    if (!ent->speed) ent->speed = 0.45f;
    if (ent->deathtarget && !Q_strcasecmp(ent->deathtarget, "none"))
        ent->deathtarget = nullptr;
    else if ((!ent->deathtarget || !*ent->deathtarget) && !Q_strcasecmp(ent->message, "power_core"))
        ent->deathtarget = "volatile";
    if (!ent->healthtarget) ent->healthtarget = "refresh";
    if (!ent->itemtarget && !Q_strcasecmp(ent->message, "power_core")) ent->itemtarget = "electric_sparks";
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
    if ((!Q_strcasecmp(ent->message, "core_socket") ||
        !Q_strcasecmp(ent->message, "reconstruction_chamber")) && !ent->pathtarget)
        ent->pathtarget = "charged";
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

void SP_trigger_raid_item(edict_t *ent)
{
    if (!ent->target) ent->target = "power_core";
    if (!ent->pathtarget) ent->pathtarget = "uncharged";
    if (!ent->combattarget) ent->combattarget = "charged";
    if (ent->delay <= 0.0f) ent->delay = 1.0f;
    InitTrigger(ent);
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
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS && CarriedItem(CarryState(player));
}

bool RaidCarry_BlocksWeapons(edict_t *player)
{
    return RaidCarry_IsCarrying(player) && !IsWeaponRelic(CarriedItem(CarryState(player)));
}

bool RaidCarry_IsWeaponRelic(edict_t *player)
{
    return RaidCarry_IsCarrying(player) && IsWeaponRelic(CarriedItem(CarryState(player)));
}

float RaidCarry_MovementScale(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player)) return 1.0f;
    return std::clamp(CarriedItem(CarryState(player))->speed, 0.1f, 1.0f);
}

vec3_t RaidCarry_HeldOrigin(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player))
        return player ? player->s.origin : vec3_t{};
    edict_t *item = CarriedItem(CarryState(player));
    if (!item || IsWeaponRelic(item))
        return player->s.origin;
    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    const vec3_t start = player->s.origin + vec3_t{ 0, 0, 20.0f };
    const vec3_t desired = start + forward * 13.0f;
    const trace_t trace = gi.trace(start, item->mins, item->maxs, desired, player, MASK_PLAYERSOLID);
    return trace.endpos;
}

trace_t RaidCarry_PmoveTrace(gvec3_cref_t start, gvec3_cptr_t mins, gvec3_cptr_t maxs,
    gvec3_cref_t end, const edict_t *passent, contents_t contentmask)
{
    trace_t body = gi.trace(start, *mins, *maxs, end, passent, contentmask);
    edict_t *player = const_cast<edict_t *>(passent);
    if (!RaidCarry_IsCarrying(player))
        return body;
    edict_t *item = CarriedItem(CarryState(player));
    if (!item || IsWeaponRelic(item))
        return body;

    vec3_t forward;
    AngleVectors(player->client->v_angle, forward, nullptr, nullptr);
    const vec3_t offset = forward * 13.0f + vec3_t{ 0, 0, 20.0f };
    trace_t carried = gi.trace(start + offset, item->mins, item->maxs, end + offset, passent, contentmask);
    if (carried.fraction < body.fraction || carried.startsolid || carried.allsolid)
    {
        carried.endpos = start + (end - start) * carried.fraction;
        return carried;
    }
    return body;
}

bool RaidCarry_Drop(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player)) return false;
    edict_t *item = CarriedItem(CarryState(player));
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

void RaidCarry_OnPlayerDeath(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player))
        return;
    edict_t *item = CarriedItem(CarryState(player));
    if (item->deathtarget && item->timestamp && item->timestamp <= level.time)
        ReturnRaidItemToOrigin(player, item, "respawn");
    else
        RaidCarry_Drop(player);
}

void RaidCarry_Update(edict_t *player)
{
    if (!RaidCarry_IsCarrying(player)) return;
    if (player->deadflag || player->health <= 0) { RaidCarry_Drop(player); return; }
    edict_t *item = CarriedItem(CarryState(player));
    if (!IsWeaponRelic(item))
        UpdateCharging(player, item);
    if (IsWeaponRelic(item))
    {
        gitem_t *weapon = RelicWeapon(item);
        // Quake's empty-ammo fallback can stow the relic without going through
        // Use_Weapon. Treat either a pending or completed automatic switch as
        // an ordinary atomic relic ejection.
        if (!weapon || (player->client->newweapon && player->client->newweapon != weapon) ||
            (player->client->pers.weapon && player->client->pers.weapon != weapon))
        {
            RaidCarry_Drop(player);
            return;
        }
    }
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
            ItemState(entity).charged = ItemState(entity).initial_charged;
            RestoreRaidItem(entity);
        }
        else if (!Q_strcasecmp(entity->classname, "raid_gadget"))
            GadgetState(entity).occupied = false;
    }
    // Occupancy is derived encounter state. Recreate it lazily so an edict slot
    // reused after a map/reset can never inherit a previous gadget's state.
    gadget_states.clear();
    item_states.clear();
    for (raid_carry_state_t &state : carry_states)
        state = {};
    RaidItems_OnMapReady();
}

void RaidItems_OnMapReady()
{
    for (uint32_t item_index = game.maxclients + 1; item_index < globals.num_edicts; ++item_index)
    {
        edict_t *item = &g_edicts[item_index];
        if (!item->inuse || !item->classname || Q_strcasecmp(item->classname, "raid_item") ||
            !ItemState(item).initial_charged)
            continue;

        for (uint32_t gadget_index = game.maxclients + 1; gadget_index < globals.num_edicts; ++gadget_index)
        {
            edict_t *gadget = &g_edicts[gadget_index];
            if (!gadget->inuse || !gadget->classname || Q_strcasecmp(gadget->classname, "raid_gadget") ||
                !gadget->message || Q_strcasecmp(gadget->message, "core_socket") ||
                GadgetState(gadget).occupied || (gadget->s.origin - item->s.origin).length() > 1.0f)
                continue;
            if (gadget->target && item->message && Q_strcasecmp(gadget->target, item->message))
                continue;

            SetGadgetOccupied(gadget, item);
            break;
        }
    }
}

void RaidItems_RunFrame()
{
    for (uint32_t i = game.maxclients + 1; i < globals.num_edicts; ++i)
    {
        edict_t *item = &g_edicts[i];
        if (!item->inuse || !item->classname || Q_strcasecmp(item->classname, "raid_item") ||
            !ItemState(item).charged || !item->itemtarget || Q_strcasecmp(item->itemtarget, "electric_sparks"))
            continue;
        raid_item_state_t &state = ItemState(item);
        if (level.time < state.next_vfx)
            continue;
        vec3_t origin = item->s.origin;
        edict_t *carrier = nullptr;
        for (edict_t *player : active_players())
            if (CarriedItem(CarryState(player)) == item)
            {
                carrier = player;
                origin = RaidCarry_HeldOrigin(player);
                break;
            }
        float volatile_progress = 0.0f;
        if (carrier && item->timestamp > level.time && item->deathtarget &&
            !Q_strcasecmp(item->deathtarget, "volatile"))
        {
            const float total_duration = item->delay > 0.0f
                ? item->delay
                : RaidDirector_StatusDuration(item->deathtarget, 15.0f);
            const float remaining = (item->timestamp - level.time).seconds();
            volatile_progress = std::clamp(1.0f - remaining / std::max(0.1f, total_duration), 0.0f, 1.0f);
        }

        // Three emitters establish the charged state. A carried Volatile core
        // adds up to three more and pulses faster as its deadline approaches.
        constexpr std::array<vec3_t, 6> spark_offsets = {
            vec3_t{ 0, 0, 5 }, vec3_t{ 5, -3, 0 }, vec3_t{ -4, 4, -3 },
            vec3_t{ 3, 5, 3 }, vec3_t{ -5, -2, 4 }, vec3_t{ 2, -5, -4 }
        };
        const int emitter_count = 3 + static_cast<int>(std::floor(volatile_progress * 3.0f));
        for (int emitter = 0; emitter < emitter_count; ++emitter)
            SpawnDamage(TE_ELECTRIC_SPARKS, origin + spark_offsets[emitter], { 0, 0, 1 }, 8);
        const float minimum_interval = 0.25f - volatile_progress * 0.17f;
        const float maximum_interval = 0.55f - volatile_progress * 0.39f;
        state.next_vfx = level.time + gtime_t::from_sec(frandom(minimum_interval, maximum_interval));
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
