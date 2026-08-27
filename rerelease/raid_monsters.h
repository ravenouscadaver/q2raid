#pragma once

struct edict_t;

void SP_raid_monster_door(edict_t *ent);
void RaidMonsters_PrepareRosters();
void RaidMonsters_Reset();
void RaidMonsters_ClearMap();
bool RaidMonsters_SetEnabled(const char *targetname, bool enabled);
void RaidMonsters_Dump();
