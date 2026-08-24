// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "raid_director.h"

#include "json/json.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

raid_director_runtime_t director;
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
    // The operation queue and timers will be advanced here. Keeping the hook
    // server-side from the first scaffold prevents client-owned Directors.
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
    gi.Com_PrintFmt("[raid] State: '{}' -> '{}'\n", previous, director.state);
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

