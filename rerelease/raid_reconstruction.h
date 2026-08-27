#pragma once

struct edict_t;
struct usercmd_t;

bool RaidReconstruction_OnDeath(edict_t *player);
void RaidReconstruction_OnDisconnect(edict_t *player);
bool RaidReconstruction_IsQueued(edict_t *player);
void RaidReconstruction_EnterSpectator(edict_t *player);
bool RaidReconstruction_HandleSpectatorInput(edict_t *player, usercmd_t *cmd);
bool RaidReconstruction_CanDeposit(edict_t *chamber);
bool RaidReconstruction_Complete(edict_t *chamber, edict_t *rescuer);
void RaidReconstruction_RunFrame();
void RaidReconstruction_Reset();
