#include "g_local.h"
#include "raid_bots.h"
#include "raid_director.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
enum class bot_task_t
{
    move,
    follow,
    operate,
    hold
};

struct entity_ref_t
{
    uint32_t number = 0;
    int32_t spawn_count = 0;
};

struct bot_state_t
{
    entity_ref_t bot;
    entity_ref_t goal;
    entity_ref_t gadget;
    entity_ref_t actor;
    bot_task_t task = bot_task_t::move;
    float tolerance = 24.0f;
    float duration = 0.0f;
    gtime_t operation_started;
    bool completed = false;
    bool hold = false;
};

std::vector<bot_state_t> bot_states;

entity_ref_t Ref(edict_t *entity)
{
    return entity ? entity_ref_t{ entity->s.number, entity->spawn_count } : entity_ref_t{};
}

edict_t *Resolve(const entity_ref_t &ref)
{
    if (!ref.number || ref.number >= globals.num_edicts)
        return nullptr;
    edict_t *entity = &g_edicts[ref.number];
    return entity->inuse && entity->spawn_count == ref.spawn_count ? entity : nullptr;
}

edict_t *NamedEntity(const char *targetname)
{
    return targetname && *targetname ? G_FindByString<&edict_t::targetname>(nullptr, targetname) : nullptr;
}

edict_t *FindBot(const char *name)
{
    for (edict_t *player : active_players())
    {
        if (!(player->svflags & SVF_BOT) || !player->client)
            continue;
        if (!name || !*name || !Q_strcasecmp(name, "first") ||
            (player->targetname && !Q_strcasecmp(player->targetname, name)) ||
            !Q_strcasecmp(player->client->pers.netname, name))
            return player;
    }
    return nullptr;
}

void ReplaceState(edict_t *bot, bot_state_t state)
{
    bot_states.erase(std::remove_if(bot_states.begin(), bot_states.end(), [bot](const bot_state_t &existing) {
        edict_t *existing_bot = Resolve(existing.bot);
        return existing_bot == bot;
    }), bot_states.end());
    bot_states.push_back(std::move(state));
}

void Fail(bot_state_t &state, const char *signal)
{
    if (edict_t *goal = Resolve(state.goal))
        RaidDirector_NotifyEntityEvent(goal, signal, Resolve(state.bot));
    state.completed = true;
    state.hold = false;
}
}

void SP_raid_bot_goal(edict_t *ent)
{
    if (ent->radius <= 0.0f) ent->radius = 24.0f;
    ent->solid = SOLID_NOT;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    gi.linkentity(ent);
}

bool RaidBots_MoveTo(const char *bot_name, const char *goal_name, float tolerance)
{
    edict_t *bot = FindBot(bot_name);
    edict_t *goal = NamedEntity(goal_name);
    if (!bot || !goal)
        return false;
    ReplaceState(bot, { Ref(bot), Ref(goal), {}, {}, bot_task_t::move,
        tolerance > 0.0f ? tolerance : (goal->radius > 0.0f ? goal->radius : 24.0f) });
    RaidDirector_NotifyEntityEvent(goal, "bot_assigned", bot);
    return true;
}

bool RaidBots_Follow(const char *bot_name, edict_t *actor)
{
    edict_t *bot = FindBot(bot_name);
    if (!bot || !actor)
        return false;
    bot_state_t state;
    state.bot = Ref(bot);
    state.actor = Ref(actor);
    state.task = bot_task_t::follow;
    ReplaceState(bot, std::move(state));
    return true;
}

bool RaidBots_Operate(const char *bot_name, const char *goal_name, const char *gadget_name,
    float tolerance, float duration, bool hold)
{
    edict_t *bot = FindBot(bot_name);
    edict_t *goal = NamedEntity(goal_name);
    edict_t *gadget = NamedEntity(gadget_name);
    if (!bot || !goal || !gadget)
        return false;
    bot_state_t state;
    state.bot = Ref(bot);
    state.goal = Ref(goal);
    state.gadget = Ref(gadget);
    state.task = bot_task_t::operate;
    state.tolerance = tolerance > 0.0f ? tolerance : (goal->radius > 0.0f ? goal->radius : 24.0f);
    state.duration = std::max(0.0f, duration);
    state.hold = hold;
    ReplaceState(bot, std::move(state));
    RaidDirector_NotifyEntityEvent(goal, "bot_assigned", bot);
    return true;
}

bool RaidBots_BlocksWeapons(edict_t *bot)
{
    return bot && std::any_of(bot_states.begin(), bot_states.end(), [bot](const bot_state_t &state) {
        return Resolve(state.bot) == bot && (state.task == bot_task_t::operate || state.task == bot_task_t::hold);
    });
}

void RaidBots_RunFrame()
{
    for (bot_state_t &state : bot_states)
    {
        edict_t *bot = Resolve(state.bot);
        if (!bot || !bot->client || !(bot->svflags & SVF_BOT) || bot->deadflag || bot->health <= 0)
        {
            Fail(state, "bot_failed");
            continue;
        }
        if (state.task == bot_task_t::follow)
        {
            edict_t *actor = Resolve(state.actor);
            if (!actor || gi.Bot_FollowActor(bot, actor) == GoalReturnCode::Error)
                Fail(state, "bot_failed");
            continue;
        }

        edict_t *goal = Resolve(state.goal);
        if (!goal)
        {
            Fail(state, "bot_failed");
            continue;
        }
        const GoalReturnCode result = gi.Bot_MoveToPoint(bot, goal->s.origin, state.tolerance);
        if (result == GoalReturnCode::Error)
        {
            Fail(state, "bot_failed");
            continue;
        }
        if (result != GoalReturnCode::Finished)
            continue;

        if (state.task == bot_task_t::move)
        {
            RaidDirector_NotifyEntityEvent(goal, "bot_goal_reached", bot);
            state.completed = true;
            continue;
        }

        edict_t *gadget = Resolve(state.gadget);
        if (!gadget)
        {
            Fail(state, "bot_failed");
            continue;
        }
        bot->client->weapon_fire_buffered = false;
        bot->client->latched_buttons &= ~BUTTON_ATTACK;
        bot->client->buttons &= ~BUTTON_ATTACK;
        gi.Edict_ForceLookAtPoint(bot, gadget->s.origin);
        if (!state.operation_started)
        {
            state.operation_started = level.time;
            RaidDirector_NotifyEntityEvent(gadget, "bot_operate_begin", bot);
        }
        if (!state.completed && level.time >= state.operation_started + gtime_t::from_sec(state.duration))
        {
            RaidDirector_NotifyEntityEvent(gadget, "bot_operate_complete", bot);
            state.completed = true;
            state.task = state.hold ? bot_task_t::hold : bot_task_t::move;
        }
    }
    bot_states.erase(std::remove_if(bot_states.begin(), bot_states.end(), [](const bot_state_t &state) {
        return state.completed && !state.hold;
    }), bot_states.end());
}

void RaidBots_Reset()
{
    bot_states.clear();
}
