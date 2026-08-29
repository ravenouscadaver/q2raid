#include "g_local.h"
#include "m_player.h"
#include "raid_player_pose.h"

void RaidPlayer_HoldBleedoutCorpsePose(edict_t *player)
{
    if (!player || !player->client || !player->deadflag || player->s.modelindex == 0)
        return;

    player->client->ps.pmove.pm_flags &= ~PMF_DUCKED;
    player->s.modelindex = MODELINDEX_PLAYER;
    player->s.frame = FRAME_death308;
    player->s.old_frame = FRAME_death308;
    player->client->anim_priority = ANIM_DEATH;
    player->client->anim_end = FRAME_death308;
    player->client->anim_duck = false;
    player->client->anim_run = false;
    player->client->anim_time = level.time;
    gi.linkentity(player);
}
