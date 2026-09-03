// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "json/forwards.h"

struct edict_t;

// Server-authoritative raid encounter coordinator. There is deliberately one
// runtime instance in the game DLL; clients only receive its normal replicated
// gameplay outputs.
void RaidDirector_Init();
void RaidDirector_Shutdown();
void RaidDirector_ResetForMap(const char *mapname);
void RaidDirector_OnMapReady();
void RaidDirector_RunFrame();
void RaidDirector_NotifyEntityEvent(edict_t *source, const char *signal, edict_t *activator);
void RaidDirector_NotifyMonsterAlerted(edict_t *monster, edict_t *player);
void RaidDirector_SetEntityEventField(edict_t *entity, const char *field, const char *value);
void RaidDirector_SetListenerField(edict_t *entity, int listener, const char *field, const char *value);
void RaidDirector_OnClientDisconnect(edict_t *player);
// Returns true when the Director owns the wipe/reset lifecycle and the stock
// cooperative map restart must be suppressed.
bool RaidDirector_OnPartyWipe();
void RaidDirector_ApplyStatus(edict_t *player, const char *status, float duration, const char *stack_policy);
void RaidDirector_ClearStatus(edict_t *player, const char *status);
float RaidDirector_StatusDuration(const char *status, float fallback);

bool RaidDirector_Load(const char *path);
bool RaidDirector_Reload();
bool RaidDirector_ResetEncounter();
bool RaidDirector_SetState(const char *state_name);
void RaidDirector_Dump();
void RaidDirector_TestFlash(bool dark);

void RaidDirector_WriteSave(Json::Value &output);
void RaidDirector_ReadSave(const Json::Value &input);
