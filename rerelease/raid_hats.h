#pragma once

struct edict_t;

void SP_raid_hat(edict_t *ent);
void RaidHats_ApplyMonster(edict_t *monster);
void RaidHats_OnMonsterKilled(edict_t *monster, edict_t *attacker);
void RaidHats_UpdateHUD(edict_t *player);
bool RaidHats_SetObserverInverted(edict_t *hat, bool inverted, edict_t *activator = nullptr);
void RaidHats_Reset();
void RaidHats_ClearMap();
