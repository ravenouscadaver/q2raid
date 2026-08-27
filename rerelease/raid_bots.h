#pragma once

struct edict_t;

void SP_raid_bot_goal(edict_t *ent);
bool RaidBots_MoveTo(const char *bot_name, const char *goal_name, float tolerance);
bool RaidBots_Follow(const char *bot_name, edict_t *actor);
bool RaidBots_Operate(const char *bot_name, const char *goal_name, const char *gadget_name,
    float tolerance, float duration, bool hold);
bool RaidBots_BlocksWeapons(edict_t *bot);
void RaidBots_RunFrame();
void RaidBots_Reset();
