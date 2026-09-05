#pragma once

#include "game.h"

struct edict_t;

void RaidGrenade_Press(edict_t *player);
void RaidGrenade_Release(edict_t *player);
void RaidGrenade_FilterCommand(edict_t *player, usercmd_t &cmd);
void RaidGrenade_Update(edict_t *player);
void RaidGrenade_OnDeath(edict_t *player);
void RaidGrenade_Disconnect(edict_t *player);
void RaidGrenade_ResetAll();
