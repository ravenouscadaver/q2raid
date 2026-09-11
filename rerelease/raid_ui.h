#pragma once

struct edict_t;
struct usercmd_t;

bool RaidUI_Open(edict_t *player, edict_t *source);
bool RaidUI_IsActive(edict_t *player);
bool RaidUI_HandleInput(edict_t *player, usercmd_t *cmd);
void RaidUI_UpdateHUD(edict_t *player);
void RaidUI_Close(edict_t *player);
void RaidUI_Disconnect(edict_t *player);
void RaidUI_Reset();
