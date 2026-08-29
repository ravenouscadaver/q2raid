#include "g_local.h"
#include "m_insane.h"
#include "raid_downed.h"
#include "raid_player_pose.h"
#include "raid_thirdperson.h"

#include <array>

namespace
{
struct downed_state_t
{
    bool downed = false;
    int saved_health = 1;
    int damage_buffer = 0;
    int frame = FRAME_crawl1;
    gtime_t next_frame;
    gtime_t damage_grace_until;
    gtime_t bleedout_at;
    gtime_t next_pain_sound;
};

downed_state_t states[MAX_CLIENTS];

downed_state_t &State(edict_t *player)
{
    return states[player->s.number - 1];
}

bool ValidPlayer(edict_t *player)
{
    return player && player->client && player->s.number >= 1 && player->s.number <= MAX_CLIENTS;
}

void EnterDowned(edict_t *player)
{
    static cvar_t *downed_buffer = gi.cvar("raid_downed_damage_buffer", "25", CVAR_NOFLAGS);
    static cvar_t *bleedout = gi.cvar("raid_downed_bleedout", "20", CVAR_NOFLAGS);
    downed_state_t &state = State(player);
    if (state.downed || player->deadflag)
        return;
    state.downed = true;
    state.saved_health = std::max(1, player->max_health / 4);
    state.damage_buffer = std::max(0, downed_buffer->integer);
    state.frame = FRAME_crawl1;
    state.next_frame = level.time + 100_ms;
    // Shotguns and similar attacks arrive as several T_Damage calls. Keep the
    // rest of the attack that caused the down from instantly finishing it.
    state.damage_grace_until = level.time + FRAME_TIME_S;
    state.bleedout_at = level.time + gtime_t::from_sec(std::clamp(bleedout->value, 3.0f, 300.0f));
    state.next_pain_sound = level.time + gtime_t::from_sec(frandom(1.5f, 3.0f));
    player->health = 1;
    player->client->buttons &= ~BUTTON_ATTACK;
    player->client->latched_buttons &= ~BUTTON_ATTACK;
    player->client->ps.gunindex = 0;
    RaidThirdPerson_SetPresentation(player, true, "models/monsters/insane/tris.md2", state.frame);
    gi.LocClient_Print(player, PRINT_HIGH, "MARINE DOWN - CRAWL TO COVER\n");
}

void LeaveDowned(edict_t *player, bool restore_health)
{
    downed_state_t &state = State(player);
    if (!state.downed)
        return;
    state.downed = false;
    RaidThirdPerson_SetPresentation(player, false, nullptr, 0);
    if (restore_health)
        player->health = std::max(1, state.saved_health);
    state = {};
}
}

bool RaidDowned_IsDown(edict_t *player)
{
    return ValidPlayer(player) && State(player).downed;
}

bool RaidDowned_InterceptFatalDamage(edict_t *player)
{
    if (!coop->integer || !ValidPlayer(player) || player->deadflag)
        return false;
    if (State(player).downed)
    {
        if (level.time < State(player).damage_grace_until)
        {
            player->health = 1;
            return true;
        }
        // Downed durability is deliberately separate from inventory armour so
        // revival cannot grant, consume, or corrupt the player's real armour.
        // health was one before this hit, so the negative result tells us how
        // much of the dedicated buffer this attack consumed.
        const int damage = std::max(1, 1 - player->health);
        State(player).damage_buffer -= damage;
        if (State(player).damage_buffer > 0)
        {
            player->health = 1;
            return true;
        }
        return false;
    }
    if (player->health <= -25)
        return false;
    EnterDowned(player);
    return true;
}

void RaidDowned_ToggleTest(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    if (State(player).downed)
    {
        LeaveDowned(player, true);
        gi.LocClient_Print(player, PRINT_HIGH, "DOWNED TEST REVIVED\n");
    }
    else
        EnterDowned(player);
}

void RaidDowned_FilterCommand(edict_t *player, usercmd_t &cmd)
{
    static cvar_t *crawl_scale = gi.cvar("raid_downed_crawl_scale", "0.18", CVAR_NOFLAGS);
    if (!RaidDowned_IsDown(player))
        return;
    const float scale = std::clamp(crawl_scale->value, 0.05f, 1.0f);
    cmd.forwardmove *= scale;
    cmd.sidemove *= scale;
    cmd.buttons &= ~BUTTON_ATTACK;
}

void RaidDowned_Update(edict_t *player)
{
    if (!RaidDowned_IsDown(player))
        return;
    downed_state_t &state = State(player);
    if (level.time >= state.bleedout_at)
    {
        // Let the normal death callback leave the downed presentation so the
        // replacement model/camera cannot survive into respawn.  A small
        // lethal hit produces an ordinary corpse instead of gibbing the player.
        // Downed crawling uses the crouched player flag; clear it before
        // player_die chooses an animation so bleedout uses one of the normal
        // standing death sequences rather than the crouch-death pose.
        state.damage_buffer = 0;
        player->client->ps.pmove.pm_flags &= ~PMF_DUCKED;
        T_Damage(player, player, player, vec3_origin, player->s.origin, vec3_origin,
            2, 0, DAMAGE_NO_PROTECTION, MOD_TRIGGER_HURT);
        RaidPlayer_HoldBleedoutCorpsePose(player);
        return;
    }
    const bool moving = std::abs(player->client->cmd.forwardmove) > 1.0f ||
        std::abs(player->client->cmd.sidemove) > 1.0f;
    if (!moving)
    {
        state.frame = FRAME_crawl1;
        state.next_frame = level.time + 100_ms;
    }
    else if (level.time >= state.next_frame)
    {
        state.frame = state.frame >= FRAME_crawl9 ? FRAME_crawl1 : state.frame + 1;
        state.next_frame = level.time + 100_ms;
    }
    if (level.time >= state.next_pain_sound)
    {
        static constexpr std::array<const char *, 5> downed_sounds = {
            "insane/insane7.wav",
            "player/male/pain25_1.wav",
            "player/male/pain50_1.wav",
            "player/male/pain75_1.wav",
            "player/male/pain100_1.wav"
        };
        gi.sound(player, CHAN_VOICE,
            gi.soundindex(downed_sounds[irandom(0, static_cast<int>(downed_sounds.size() - 1))]),
            0.8f, ATTN_NORM, 0.0f);
        state.next_pain_sound = level.time + gtime_t::from_sec(frandom(3.5f, 6.5f));
    }
    player->client->buttons &= ~BUTTON_ATTACK;
    player->client->latched_buttons &= ~BUTTON_ATTACK;
    player->client->ps.gunindex = 0;
    RaidThirdPerson_SetPresentation(player, true, "models/monsters/insane/tris.md2", state.frame);
}

void RaidDowned_OnDeath(edict_t *player)
{
    if (ValidPlayer(player))
        LeaveDowned(player, false);
}

void RaidDowned_Disconnect(edict_t *player)
{
    if (!ValidPlayer(player))
        return;
    LeaveDowned(player, false);
    State(player) = {};
}

void RaidDowned_ResetAll()
{
    for (edict_t *player : active_players())
        if (RaidDowned_IsDown(player))
            LeaveDowned(player, true);
    for (downed_state_t &state : states)
        state = {};
}
