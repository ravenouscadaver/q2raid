// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "raid_director.h"

#include "json/json.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct raid_director_runtime_t
{
    bool        initialized = false;
    bool        loaded = false;
    std::string mapname;
    std::string script_path;
    std::string encounter_name;
    std::string state;
    Json::Value document;
};

struct raid_color_cycle_t
{
    std::vector<std::string> targets;
    std::vector<rgba_t>      colors;
    gtime_t                  started_at;
    float                    step_seconds = 0.5f;
    float                    transition_seconds = 0.5f;
};

raid_director_runtime_t director;
std::vector<raid_color_cycle_t> color_cycles;
cvar_t *raid_script = nullptr;
cvar_t *raid_script_root = nullptr;
cvar_t *raid_autoload = nullptr;

void RaidDirector_ClearDocument()
{
    director.loaded = false;
    director.script_path.clear();
    director.encounter_name.clear();
    director.state.clear();
    director.document = Json::Value();
    color_cycles.clear();
}

int32_t RaidDirector_PackColor(const rgba_t &color)
{
    return color.a | (color.b << 8) | (color.g << 16) | (color.r << 24);
}

bool RaidDirector_ReadColor(const Json::Value &value, rgba_t &color)
{
    if (!value.isArray() || value.size() < 3 || value.size() > 4)
        return false;

    for (Json::ArrayIndex i = 0; i < value.size(); ++i)
        if (!value[i].isNumeric() || value[i].asInt() < 0 || value[i].asInt() > 255)
            return false;

    color = {
        static_cast<uint8_t>(value[0].asInt()),
        static_cast<uint8_t>(value[1].asInt()),
        static_cast<uint8_t>(value[2].asInt()),
        static_cast<uint8_t>(value.size() == 4 ? value[3].asInt() : 255)
    };
    return true;
}

void RaidDirector_SetNamedColor(const std::string &targetname, const rgba_t &color)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        entity->s.skinnum = RaidDirector_PackColor(color);
        ++matches;
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] color target '{}' not found\n", targetname);
}

void RaidDirector_FireTarget(const std::string &targetname, edict_t *activator)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        ++matches;
        if (entity->use)
            entity->use(entity, activator ? activator : entity, activator ? activator : entity);
        else
            gi.Com_PrintFmt("[raid] target '{}' ({}) has no use function\n", targetname, entity->classname);
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] fire target '{}' not found\n", targetname);
}

void RaidDirector_DisableEntity(const std::string &targetname)
{
    edict_t *entity = nullptr;
    int matches = 0;
    while ((entity = G_FindByString<&edict_t::targetname>(entity, targetname.c_str())))
    {
        entity->solid = SOLID_NOT;
        entity->touch = nullptr;
        entity->use = nullptr;
        gi.linkentity(entity);
        ++matches;
    }

    if (!matches)
        gi.Com_PrintFmt("[raid] disable target '{}' not found\n", targetname);
}

void RaidDirector_ExecuteEnter(const std::string &state_name, edict_t *activator)
{
    const Json::Value &operations = director.document["states"][state_name]["enter"];
    if (!operations.isArray())
        return;

    for (const Json::Value &operation : operations)
    {
        const std::string op = operation.get("op", "").asString();
        if (op == "fire_target")
        {
            RaidDirector_FireTarget(operation.get("target", "").asString(), activator);
        }
        else if (op == "disable_entity")
        {
            RaidDirector_DisableEntity(operation.get("target", "").asString());
        }
        else if (op == "color_cycle")
        {
            raid_color_cycle_t cycle;
            cycle.started_at = level.time;
            cycle.step_seconds = std::max(0.05f, operation.get("step_seconds", 0.5f).asFloat());
            cycle.transition_seconds = std::clamp(operation.get("transition_seconds", cycle.step_seconds).asFloat(), 0.0f, cycle.step_seconds);

            for (const Json::Value &target : operation["targets"])
                cycle.targets.push_back(target.asString());
            for (const Json::Value &value : operation["colors"])
            {
                rgba_t color;
                if (RaidDirector_ReadColor(value, color))
                    cycle.colors.push_back(color);
            }

            if (cycle.targets.empty() || cycle.colors.size() < 2)
            {
                gi.Com_Print("[raid] color_cycle requires targets and at least two colors\n");
                continue;
            }

            color_cycles.push_back(std::move(cycle));
        }
        else
        {
            gi.Com_PrintFmt("[raid] Unknown operation '{}' in state '{}'\n", op, state_name);
        }
    }
}

bool RaidDirector_Validate(const Json::Value &root, std::string &error)
{
    if (!root.isObject())
    {
        error = "root must be an object";
        return false;
    }

    if (!root.isMember("initial_state") || !root["initial_state"].isString() || root["initial_state"].asString().empty())
    {
        error = "initial_state must be a non-empty string";
        return false;
    }

    if (!root.isMember("states") || !root["states"].isObject() || root["states"].empty())
    {
        error = "states must be a non-empty object";
        return false;
    }

    const std::string initial_state = root["initial_state"].asString();
    if (!root["states"].isMember(initial_state))
    {
        error = fmt::format("initial_state '{}' is not defined in states", initial_state);
        return false;
    }

    for (const std::string &name : root["states"].getMemberNames())
    {
        const Json::Value &state = root["states"][name];
        if (!state.isObject())
        {
            error = fmt::format("state '{}' must be an object", name);
            return false;
        }

        if (state.isMember("enter") && !state["enter"].isArray())
        {
            error = fmt::format("state '{}'.enter must be an array", name);
            return false;
        }

        if (state.isMember("enter"))
        {
            for (const Json::Value &operation : state["enter"])
            {
                if (!operation.isObject() || !operation.isMember("op") || !operation["op"].isString())
                {
                    error = fmt::format("state '{}'.enter operations require a string op", name);
                    return false;
                }
            }
        }
    }

    if (root.isMember("events") && !root["events"].isArray())
    {
        error = "events must be an array";
        return false;
    }

    return true;
}

std::filesystem::path RaidDirector_ResolvePath(const char *path)
{
    std::filesystem::path requested(path ? path : "");
    if (requested.is_absolute() || !raid_script_root || !*raid_script_root->string)
        return requested;

    return std::filesystem::path(raid_script_root->string) / requested;
}
}

void RaidDirector_Init()
{
    raid_script = gi.cvar("raid_script", "", CVAR_NOFLAGS);
    raid_script_root = gi.cvar("raid_script_root", ".", CVAR_NOFLAGS);
    raid_autoload = gi.cvar("raid_autoload", "1", CVAR_NOFLAGS);

    director = {};
    director.initialized = true;
    gi.Com_Print("[raid] Director initialized (single server authority)\n");
}

void RaidDirector_Shutdown()
{
    RaidDirector_ClearDocument();
    director.initialized = false;
}

void RaidDirector_ResetForMap(const char *mapname)
{
    RaidDirector_ClearDocument();
    director.mapname = mapname ? mapname : "";
}

void RaidDirector_OnMapReady()
{
    if (!director.initialized || !raid_autoload || !raid_autoload->integer || !raid_script || !*raid_script->string)
        return;

    RaidDirector_Load(raid_script->string);
}

void RaidDirector_RunFrame()
{
    for (raid_color_cycle_t &cycle : color_cycles)
    {
        const float elapsed = std::max(0.0f, (level.time - cycle.started_at).seconds<float>());
        const int step = static_cast<int>(std::floor(elapsed / cycle.step_seconds));
        const float within_step = std::fmod(elapsed, cycle.step_seconds);
        float fraction = cycle.transition_seconds > 0.0f ? std::min(1.0f, within_step / cycle.transition_seconds) : 1.0f;
        fraction = fraction * fraction * (3.0f - (2.0f * fraction));

        for (size_t i = 0; i < cycle.targets.size(); ++i)
        {
            const rgba_t &from = cycle.colors[(i + step) % cycle.colors.size()];
            const rgba_t &to = cycle.colors[(i + step + 1) % cycle.colors.size()];
            const auto channel = [fraction](uint8_t a, uint8_t b) {
                return static_cast<uint8_t>(std::clamp(a + ((b - a) * fraction), 0.0f, 255.0f));
            };
            RaidDirector_SetNamedColor(cycle.targets[i], {
                channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)
            });
        }
    }
}

void RaidDirector_NotifyEntityEvent(edict_t *source, const char *signal, edict_t *activator)
{
    if (!director.loaded || !source || !source->targetname || !signal)
        return;

    const Json::Value &events = director.document["events"];
    if (!events.isArray())
        return;

    for (const Json::Value &event : events)
    {
        if (event.get("source", "").asString() != source->targetname || event.get("signal", "activate").asString() != signal)
            continue;

        const std::string from_state = event.get("from_state", "").asString();
        if (!from_state.empty() && from_state != director.state)
            continue;

        const std::string next_state = event.get("set_state", "").asString();
        if (!next_state.empty())
        {
            if (!director.document["states"].isMember(next_state))
            {
                gi.Com_PrintFmt("[raid] Event from '{}' references unknown state '{}'\n", source->targetname, next_state);
                continue;
            }

            const std::string previous = director.state;
            director.state = next_state;
            color_cycles.clear();
            gi.Com_PrintFmt("[raid] Event '{}:{}': '{}' -> '{}'\n", source->targetname, signal, previous, director.state);
            RaidDirector_ExecuteEnter(director.state, activator);
        }
    }
}

bool RaidDirector_Load(const char *path)
{
    if (!director.initialized)
    {
        gi.Com_Print("[raid] Director is not initialized\n");
        return false;
    }
    if (!path || !*path)
    {
        gi.Com_Print("[raid] No encounter JSON path supplied\n");
        return false;
    }

    const std::filesystem::path resolved = RaidDirector_ResolvePath(path);
    std::ifstream stream(resolved, std::ios::in | std::ios::binary);
    if (!stream)
    {
        gi.Com_PrintFmt("[raid] Couldn't open encounter JSON '{}'\n", resolved.string());
        return false;
    }

    Json::CharReaderBuilder reader;
    reader["collectComments"] = false;
    Json::Value root;
    JSONCPP_STRING parse_error;
    if (!Json::parseFromStream(reader, stream, &root, &parse_error))
    {
        gi.Com_PrintFmt("[raid] Couldn't parse encounter JSON '{}': {}\n", resolved.string(), parse_error);
        return false;
    }

    std::string validation_error;
    if (!RaidDirector_Validate(root, validation_error))
    {
        gi.Com_PrintFmt("[raid] Invalid encounter JSON '{}': {}\n", resolved.string(), validation_error);
        return false;
    }

    director.document = std::move(root);
    director.script_path = path;
    director.encounter_name = director.document.get("encounter", director.mapname).asString();
    director.state = director.document["initial_state"].asString();
    director.loaded = true;

    RaidDirector_ExecuteEnter(director.state, nullptr);

    gi.Com_PrintFmt("[raid] Loaded encounter '{}' from '{}' (initial state '{}', {} states)\n",
        director.encounter_name,
        resolved.string(),
        director.state,
        director.document["states"].size());
    return true;
}

bool RaidDirector_Reload()
{
    if (director.script_path.empty())
    {
        if (raid_script && *raid_script->string)
            return RaidDirector_Load(raid_script->string);

        gi.Com_Print("[raid] No encounter JSON has been selected\n");
        return false;
    }

    const std::string path = director.script_path;
    return RaidDirector_Load(path.c_str());
}

bool RaidDirector_SetState(const char *state_name)
{
    if (!director.loaded)
    {
        gi.Com_Print("[raid] No encounter JSON is loaded\n");
        return false;
    }
    if (!state_name || !*state_name || !director.document["states"].isMember(state_name))
    {
        gi.Com_PrintFmt("[raid] Unknown Director state '{}'\n", state_name ? state_name : "");
        return false;
    }

    const std::string previous = director.state;
    director.state = state_name;
    color_cycles.clear();
    gi.Com_PrintFmt("[raid] State: '{}' -> '{}'\n", previous, director.state);
    RaidDirector_ExecuteEnter(director.state, nullptr);
    return true;
}

void RaidDirector_Dump()
{
    gi.Com_PrintFmt("[raid] Director: initialized={}, loaded={}, map='{}', encounter='{}', state='{}', script='{}'\n",
        director.initialized,
        director.loaded,
        director.mapname,
        director.encounter_name,
        director.state,
        director.script_path);

    if (!director.loaded)
        return;

    gi.Com_Print("[raid] States:");
    for (const std::string &name : director.document["states"].getMemberNames())
        gi.Com_PrintFmt(" {}{}", name, name == director.state ? "*" : "");
    gi.Com_Print("\n");
}

void RaidDirector_WriteSave(Json::Value &output)
{
    output = Json::Value(Json::objectValue);
    output["loaded"] = director.loaded;
    output["script"] = director.script_path;
    output["state"] = director.state;
}

void RaidDirector_ReadSave(const Json::Value &input)
{
    if (!input.isObject() || !input.get("loaded", false).asBool())
        return;

    const std::string script = input.get("script", "").asString();
    const std::string state = input.get("state", "").asString();
    if (!script.empty() && RaidDirector_Load(script.c_str()) && !state.empty())
        RaidDirector_SetState(state.c_str());
}
