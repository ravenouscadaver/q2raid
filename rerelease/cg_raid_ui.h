#pragma once

#include <cstdint>

struct player_state_t;
struct vrect_t;

void CG_RaidUI_TouchPics();
void CG_RaidUI_Draw(const player_state_t *ps, const vrect_t &hud_vrect, int32_t scale);
