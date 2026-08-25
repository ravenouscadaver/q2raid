#pragma once

struct edict_t;

void SP_raid_item(edict_t *ent);
void SP_raid_gadget(edict_t *ent);
void SP_trigger_raid_deposit(edict_t *ent);
bool RaidCarry_IsCarrying(edict_t *player);
bool RaidCarry_Drop(edict_t *player);
void RaidCarry_Update(edict_t *player);

