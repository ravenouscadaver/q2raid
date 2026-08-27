#pragma once

struct edict_t;
struct trace_t;

void SP_raid_item(edict_t *ent);
void SP_raid_gadget(edict_t *ent);
void SP_trigger_raid_deposit(edict_t *ent);
void SP_trigger_raid_item(edict_t *ent);
void SP_raid_hovertext(edict_t *ent);
bool RaidCarry_IsCarrying(edict_t *player);
bool RaidCarry_BlocksWeapons(edict_t *player);
bool RaidCarry_IsWeaponRelic(edict_t *player);
float RaidCarry_MovementScale(edict_t *player);
vec3_t RaidCarry_HeldOrigin(edict_t *player);
trace_t RaidCarry_PmoveTrace(gvec3_cref_t start, gvec3_cptr_t mins, gvec3_cptr_t maxs,
    gvec3_cref_t end, const edict_t *passent, contents_t contentmask);
bool RaidCarry_Drop(edict_t *player);
void RaidCarry_OnPlayerDeath(edict_t *player);
void RaidCarry_Update(edict_t *player);
void RaidItems_RunFrame();
void RaidCarry_ResetAll();
void RaidHover_RunFrame();
void RaidHover_Reset();
