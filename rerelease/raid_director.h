// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#pragma once

#include "json/json-forwards.h"

// Server-authoritative raid encounter coordinator. There is deliberately one
// runtime instance in the game DLL; clients only receive its normal replicated
// gameplay outputs.
void RaidDirector_Init();
void RaidDirector_Shutdown();
void RaidDirector_ResetForMap(const char *mapname);
void RaidDirector_OnMapReady();
void RaidDirector_RunFrame();

bool RaidDirector_Load(const char *path);
bool RaidDirector_Reload();
bool RaidDirector_SetState(const char *state_name);
void RaidDirector_Dump();

void RaidDirector_WriteSave(Json::Value &output);
void RaidDirector_ReadSave(const Json::Value &input);
