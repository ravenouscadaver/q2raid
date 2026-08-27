#pragma once

struct edict_t;
struct usercmd_t;

bool RaidDowned_IsDown(edict_t *player);
bool RaidDowned_InterceptFatalDamage(edict_t *player);
void RaidDowned_ToggleTest(edict_t *player);
void RaidDowned_FilterCommand(edict_t *player, usercmd_t &cmd);
void RaidDowned_Update(edict_t *player);
void RaidDowned_OnDeath(edict_t *player);
void RaidDowned_Disconnect(edict_t *player);
void RaidDowned_ResetAll();
