#pragma once

struct edict_t;

void RaidThirdPerson_Toggle(edict_t *player);
void RaidThirdPerson_SetCarry(edict_t *player, bool carrying);
void RaidThirdPerson_Update(edict_t *player);
void RaidThirdPerson_Disconnect(edict_t *player);
