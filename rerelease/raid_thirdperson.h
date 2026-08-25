#pragma once

struct edict_t;

void RaidThirdPerson_Toggle(edict_t *player);
void RaidThirdPerson_SetCarry(edict_t *player, bool carrying, const char *model = nullptr, float scale = 0.0f);
void RaidThirdPerson_Update(edict_t *player);
void RaidThirdPerson_Disconnect(edict_t *player);
