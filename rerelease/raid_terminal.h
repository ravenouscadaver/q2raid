#pragma once

struct edict_t;
struct usercmd_t;

void SP_trigger_raid_terminal(edict_t *ent);
bool RaidTerminal_IsActive(edict_t *player);
bool RaidTerminal_HandleInput(edict_t *player, usercmd_t *cmd);
void RaidTerminal_Disconnect(edict_t *player);
void RaidTerminal_Reset();
