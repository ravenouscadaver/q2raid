// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "g_local.h"
#include "raid_build_id.h"
#include "raid_director.h"
#include "raid_monsters.h"

namespace
{
enum class raid_event_fact_id_t
{
	none,
	trigger_activate,
	monster_door_activated,
	monster_door_deactivated,
	monster_door_deploy,
	monster_door_replenish,
	monster_door_leash_return
};

enum class raid_event_producer_id_t
{
	none,
	trigger_multi_path,
	raid_monster_door_runtime
};

enum class raid_event_payload_family_t
{
	source_actor
};

struct raid_event_capability_desc_t
{
	raid_event_fact_id_t fact;
	raid_event_producer_id_t producer;
	raid_event_payload_family_t payload;
	const char *source_classname;
	const char *signal;
};

struct raid_event_physical_ref_t
{
	uint32_t entity_number = 0;
	int32_t spawn_count = 0;
};

constexpr raid_event_capability_desc_t raid_event_capabilities[] = {
	{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::trigger_multi_path,
		raid_event_payload_family_t::source_actor, "trigger_multiple", "activate" },
	{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::trigger_multi_path,
		raid_event_payload_family_t::source_actor, "trigger_once", "activate" },
	{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::trigger_multi_path,
		raid_event_payload_family_t::source_actor, "trigger_counter", "activate" },
	{ raid_event_fact_id_t::monster_door_activated, raid_event_producer_id_t::raid_monster_door_runtime,
		raid_event_payload_family_t::source_actor, "raid_monster_door", "activated" },
	{ raid_event_fact_id_t::monster_door_deactivated, raid_event_producer_id_t::raid_monster_door_runtime,
		raid_event_payload_family_t::source_actor, "raid_monster_door", "deactivated" },
	{ raid_event_fact_id_t::monster_door_deploy, raid_event_producer_id_t::raid_monster_door_runtime,
		raid_event_payload_family_t::source_actor, "raid_monster_door", "deploy" },
	{ raid_event_fact_id_t::monster_door_replenish, raid_event_producer_id_t::raid_monster_door_runtime,
		raid_event_payload_family_t::source_actor, "raid_monster_door", "replenish" },
	{ raid_event_fact_id_t::monster_door_leash_return, raid_event_producer_id_t::raid_monster_door_runtime,
		raid_event_payload_family_t::source_actor, "raid_monster_door", "leash_return" }
};

constexpr size_t raid_event_capability_count = sizeof(raid_event_capabilities) / sizeof(raid_event_capabilities[0]);

const char *RaidEvent_FactName(raid_event_fact_id_t fact)
{
	switch (fact)
	{
	case raid_event_fact_id_t::trigger_activate: return "trigger_activate";
	case raid_event_fact_id_t::monster_door_activated: return "monster_door_activated";
	case raid_event_fact_id_t::monster_door_deactivated: return "monster_door_deactivated";
	case raid_event_fact_id_t::monster_door_deploy: return "monster_door_deploy";
	case raid_event_fact_id_t::monster_door_replenish: return "monster_door_replenish";
	case raid_event_fact_id_t::monster_door_leash_return: return "monster_door_leash_return";
	default: return "none";
	}
}

const char *RaidEvent_ProducerName(raid_event_producer_id_t producer)
{
	switch (producer)
	{
	case raid_event_producer_id_t::trigger_multi_path: return "trigger_multi_path";
	case raid_event_producer_id_t::raid_monster_door_runtime: return "raid_monster_door_runtime";
	default: return "none";
	}
}

const char *RaidEvent_PayloadName(raid_event_payload_family_t payload)
{
	switch (payload)
	{
	case raid_event_payload_family_t::source_actor: return "source_actor";
	default: return "unknown";
	}
}

bool RaidEvent_SameString(const char *a, const char *b)
{
	return a && b && Q_strcasecmp(a, b) == 0;
}

bool RaidEvent_ValidateRegistry(const raid_event_capability_desc_t *entries, size_t count, const char **reason)
{
	if (reason)
		*reason = nullptr;
	if (!entries || !count)
	{
		if (reason)
			*reason = "registry is empty";
		return false;
	}

	for (size_t i = 0; i < count; ++i)
	{
		const raid_event_capability_desc_t &entry = entries[i];
		if (entry.fact == raid_event_fact_id_t::none || entry.producer == raid_event_producer_id_t::none ||
			!entry.source_classname || !*entry.source_classname || !entry.signal || !*entry.signal)
		{
			if (reason)
				*reason = "capability entry contains an empty/none field";
			return false;
		}

		for (size_t j = i + 1; j < count; ++j)
		{
			const raid_event_capability_desc_t &other = entries[j];
			const bool same_binding = RaidEvent_SameString(entry.source_classname, other.source_classname) &&
				RaidEvent_SameString(entry.signal, other.signal);
			if (same_binding && (entry.fact != other.fact || entry.producer != other.producer))
			{
				if (reason)
					*reason = "same source-class/signal binding has conflicting fact or producer";
				return false;
			}
			if (entry.fact == other.fact && entry.producer != other.producer)
			{
				if (reason)
					*reason = "same normalized fact has conflicting canonical producers";
				return false;
			}
		}
	}
	return true;
}

const raid_event_capability_desc_t *RaidEvent_FindCapability(const char *classname, const char *signal)
{
	if (!classname || !signal)
		return nullptr;
	for (const raid_event_capability_desc_t &entry : raid_event_capabilities)
		if (RaidEvent_SameString(entry.source_classname, classname) && RaidEvent_SameString(entry.signal, signal))
			return &entry;
	return nullptr;
}

bool RaidEvent_IsKnownSignal(const char *signal)
{
	if (!signal || !*signal)
		return false;
	for (const raid_event_capability_desc_t &entry : raid_event_capabilities)
		if (RaidEvent_SameString(entry.signal, signal))
			return true;
	return false;
}

bool RaidEvent_PhysicalRefIsCurrent(const raid_event_physical_ref_t &ref)
{
	if (!ref.entity_number || ref.entity_number >= globals.num_edicts)
		return false;
	const edict_t *entity = &g_edicts[ref.entity_number];
	return entity->inuse && entity->spawn_count == ref.spawn_count;
}
}

bool RaidDirector_EventCapabilityRegistryValid()
{
	return RaidEvent_ValidateRegistry(raid_event_capabilities, raid_event_capability_count, nullptr);
}

bool RaidDirector_EventCapabilitySupported(const char *classname, const char *signal, const char **producer)
{
	if (producer)
		*producer = nullptr;
	if (!RaidDirector_EventCapabilityRegistryValid())
		return false;
	const raid_event_capability_desc_t *capability = RaidEvent_FindCapability(classname, signal);
	if (!capability)
		return false;
	if (producer)
		*producer = RaidEvent_ProducerName(capability->producer);
	return true;
}

void RaidDirector_DumpEventCapabilities()
{
	const char *reason = nullptr;
	const bool valid = RaidEvent_ValidateRegistry(raid_event_capabilities, raid_event_capability_count, &reason);
	gi.Com_PrintFmt("[raid] Event capabilities: registry={}, entries={}\n", valid ? "valid" : "INVALID", raid_event_capability_count);
	if (!valid)
	{
		gi.Com_PrintFmt("[raid]   registry error: {}\n", reason ? reason : "unknown");
		return;
	}

	for (const raid_event_capability_desc_t &entry : raid_event_capabilities)
		gi.Com_PrintFmt("[raid]   {} {} -> fact={} producer={} payload={}\n",
			entry.source_classname, entry.signal, RaidEvent_FactName(entry.fact),
			RaidEvent_ProducerName(entry.producer), RaidEvent_PayloadName(entry.payload));
}

void RaidDirector_CheckEventCapability(const char *targetname, const char *signal)
{
	const char *reason = nullptr;
	if (!RaidEvent_ValidateRegistry(raid_event_capabilities, raid_event_capability_count, &reason))
	{
		gi.Com_PrintFmt("[raid] Event capability check: locator='{}', signal='{}', result=REGISTRY_INVALID ({})\n",
			targetname ? targetname : "", signal ? signal : "", reason ? reason : "unknown");
		return;
	}
	if (!signal || !*signal || !RaidEvent_IsKnownSignal(signal))
	{
		gi.Com_PrintFmt("[raid] Event capability check: locator='{}', signal='{}', result=UNKNOWN_SIGNAL\n",
			targetname ? targetname : "", signal ? signal : "");
		return;
	}

	int matches = 0;
	int supported = 0;
	int unsupported = 0;
	int stale = 0;
	edict_t *entity = nullptr;
	while (targetname && *targetname && (entity = G_FindByString<&edict_t::targetname>(entity, targetname)))
	{
		++matches;
		const raid_event_physical_ref_t ref = { entity->s.number, entity->spawn_count };
		const char *classname = entity->classname ? entity->classname : "";
		if (!RaidEvent_PhysicalRefIsCurrent(ref))
		{
			++stale;
			gi.Com_PrintFmt("[raid]   entity={} spawn_count={} classname='{}' result=STALE_OR_UNRESOLVABLE_SOURCE\n",
				ref.entity_number, ref.spawn_count, classname);
			continue;
		}

		const raid_event_capability_desc_t *capability = RaidEvent_FindCapability(classname, signal);
		if (!capability)
		{
			++unsupported;
			gi.Com_PrintFmt("[raid]   entity={} spawn_count={} classname='{}' supported=false\n",
				ref.entity_number, ref.spawn_count, classname);
			continue;
		}

		++supported;
		gi.Com_PrintFmt("[raid]   entity={} spawn_count={} classname='{}' supported=true fact={} producer={}\n",
			ref.entity_number, ref.spawn_count, classname, RaidEvent_FactName(capability->fact),
			RaidEvent_ProducerName(capability->producer));
	}

	const char *result = "KNOWN_SIGNAL_SOURCE_MISSING";
	if (matches > 0 && supported == 0)
		result = stale == matches ? "STALE_OR_UNRESOLVABLE_SOURCE" : "KNOWN_SIGNAL_WRONG_CLASS";
	else if (supported == 1)
		result = "SUPPORTED_SINGLE";
	else if (supported > 1)
		result = "SUPPORTED_MULTIPLE";

	gi.Com_PrintFmt("[raid] Event capability check: locator='{}', signal='{}', matches={}, supported={}, unsupported={}, stale={}, result={}\n",
		targetname ? targetname : "", signal, matches, supported, unsupported, stale, result);
}

bool RaidDirector_RunEventCapabilitySelfTest()
{
	constexpr raid_event_capability_desc_t valid_shared_producer[] = {
		{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::trigger_multi_path,
			raid_event_payload_family_t::source_actor, "trigger_multiple", "activate" },
		{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::trigger_multi_path,
			raid_event_payload_family_t::source_actor, "trigger_once", "activate" }
	};
	constexpr raid_event_capability_desc_t conflicting_producer[] = {
		{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::trigger_multi_path,
			raid_event_payload_family_t::source_actor, "trigger_multiple", "activate" },
		{ raid_event_fact_id_t::trigger_activate, raid_event_producer_id_t::raid_monster_door_runtime,
			raid_event_payload_family_t::source_actor, "trigger_once", "activate" }
	};

	const char *reason = nullptr;
	const bool production_ok = RaidEvent_ValidateRegistry(raid_event_capabilities, raid_event_capability_count, &reason);
	gi.Com_PrintFmt("[raid] Event capability selftest: production registry {}{}{}\n",
		production_ok ? "PASS" : "FAIL", production_ok ? "" : " (", production_ok ? "" : (reason ? reason : "unknown"));
	if (!production_ok)
		gi.Com_Print(")\n");

	reason = nullptr;
	const bool shared_ok = RaidEvent_ValidateRegistry(valid_shared_producer,
		sizeof(valid_shared_producer) / sizeof(valid_shared_producer[0]), &reason);
	gi.Com_PrintFmt("[raid] Event capability selftest: shared canonical producer {}\n", shared_ok ? "PASS" : "FAIL");

	reason = nullptr;
	const bool conflict_rejected = !RaidEvent_ValidateRegistry(conflicting_producer,
		sizeof(conflicting_producer) / sizeof(conflicting_producer[0]), &reason);
	gi.Com_PrintFmt("[raid] Event capability selftest: conflicting canonical producer {}\n", conflict_rejected ? "REJECTED AS EXPECTED" : "FAIL");

	const raid_event_physical_ref_t stale_ref = { globals.num_edicts, 0 };
	const bool stale_rejected = !RaidEvent_PhysicalRefIsCurrent(stale_ref);
	gi.Com_PrintFmt("[raid] Event capability selftest: stale physical ref {}\n", stale_rejected ? "REJECTED AS EXPECTED" : "FAIL");

	const bool pass = production_ok && shared_ok && conflict_rejected && stale_rejected;
	gi.Com_PrintFmt("[raid] Event capability selftest: overall {}\n", pass ? "PASS" : "FAIL");
	return pass;
}

void Svcmd_Test_f()
{
	gi.LocClient_Print(nullptr, PRINT_HIGH, "Svcmd_Test_f()\n");
}

/*
==============================================================================

PACKET FILTERING


You can add or remove addresses from the filter list with:

addip <ip>
removeip <ip>

The ip address is specified in dot format, and any unspecified digits will match any value, so you can specify an entire
class C network with "addip 192.246.40".

Removeip will only remove an address specified exactly the same way.  You cannot addip a subnet, then removeip a single
host.

listip
Prints the current list of filters.

writeip
Dumps "addip <ip>" commands to listip.cfg so it can be execed at a later date.  The filter lists are not saved and
restored by default, because I beleive it would cause too much confusion.

filterban <0 or 1>

If 1 (the default), then ip addresses matching the current list will be prohibited from entering the game.  This is the
default setting.

If 0, then only addresses matching the list will be allowed.  This lets you easily set up a private game, or a game that
only allows players from your local network.


==============================================================================
*/

struct ipfilter_t
{
	unsigned mask;
	unsigned compare;
};

constexpr size_t MAX_IPFILTERS = 1024;

ipfilter_t ipfilters[MAX_IPFILTERS];
int		   numipfilters;

/*
=================
StringToFilter
=================
*/
static bool StringToFilter(const char *s, ipfilter_t *f)
{
	char num[128];
	int	 i, j;
	byte b[4];
	byte m[4];

	for (i = 0; i < 4; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}

	for (i = 0; i < 4; i++)
	{
		if (*s < '0' || *s > '9')
		{
			gi.LocClient_Print(nullptr, PRINT_HIGH, "Bad filter address: {}\n", s);
			return false;
		}

		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}

	f->mask = *(unsigned *) m;
	f->compare = *(unsigned *) b;

	return true;
}

/*
=================
SV_FilterPacket
=================
*/
bool SV_FilterPacket(const char *from)
{
	int		 i;
	unsigned in;
	byte	 m[4];
	const char	 *p;

	i = 0;
	p = from;
	while (*p && i < 4)
	{
		m[i] = 0;
		while (*p >= '0' && *p <= '9')
		{
			m[i] = m[i] * 10 + (*p - '0');
			p++;
		}
		if (!*p || *p == ':')
			break;
		i++;
		p++;
	}

	in = *(unsigned *) m;

	for (i = 0; i < numipfilters; i++)
		if ((in & ipfilters[i].mask) == ipfilters[i].compare)
			return filterban->integer;

	return !filterban->integer;
}

/*
=================
SV_AddIP_f
=================
*/
void SVCmd_AddIP_f()
{
	int i;

	if (gi.argc() < 3)
	{
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage:  addip <ip-mask>\n");
		return;
	}

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].compare == 0xffffffff)
			break; // free spot
	if (i == numipfilters)
	{
		if (numipfilters == MAX_IPFILTERS)
		{
			gi.LocClient_Print(nullptr, PRINT_HIGH, "IP filter list is full\n");
			return;
		}
		numipfilters++;
	}

	if (!StringToFilter(gi.argv(2), &ipfilters[i]))
		ipfilters[i].compare = 0xffffffff;
}

/*
=================
SV_RemoveIP_f
=================
*/
void SVCmd_RemoveIP_f()
{
	ipfilter_t f;
	int		   i, j;

	if (gi.argc() < 3)
	{
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Usage:  sv removeip <ip-mask>\n");
		return;
	}

	if (!StringToFilter(gi.argv(2), &f))
		return;

	for (i = 0; i < numipfilters; i++)
		if (ipfilters[i].mask == f.mask && ipfilters[i].compare == f.compare)
		{
			for (j = i + 1; j < numipfilters; j++)
				ipfilters[j - 1] = ipfilters[j];
			numipfilters--;
			gi.LocClient_Print(nullptr, PRINT_HIGH, "Removed.\n");
			return;
		}
	gi.LocClient_Print(nullptr, PRINT_HIGH, "Didn't find {}.\n", gi.argv(2));
}

/*
=================
SV_ListIP_f
=================
*/
void SVCmd_ListIP_f()
{
	int	 i;
	byte b[4];

	gi.LocClient_Print(nullptr, PRINT_HIGH, "Filter list:\n");
	for (i = 0; i < numipfilters; i++)
	{
		*(unsigned *) b = ipfilters[i].compare;
		gi.LocClient_Print(nullptr, PRINT_HIGH, "{}.{}.{}.{}\n", b[0], b[1], b[2], b[3]);
	}
}

// [Paril-KEX]
void SVCmd_NextMap_f()
{
	gi.LocBroadcast_Print(PRINT_HIGH, "$g_map_ended_by_server");
	EndDMLevel();
}

/*
=================
SV_WriteIP_f
=================
*/
void SVCmd_WriteIP_f(void)
{
	// KEX_FIXME: Sys_FOpen isn't available atm, just commenting this out since i don't think we even need this functionality - sponge
	/*
	FILE* f;

	byte	b[4];
	int		i;
	cvar_t* game;

	game = gi.cvar("game", "", 0);

	std::string name;
	if (!*game->string)
		name = std::string(GAMEVERSION) + "/listip.cfg";
	else
		name = std::string(game->string) + "/listip.cfg";

	gi.LocClient_Print(nullptr, PRINT_HIGH, "Writing {}.\n", name.c_str());

	f = Sys_FOpen(name.c_str(), "wb");
	if (!f)
	{
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Couldn't open {}\n", name.c_str());
		return;
	}

	fprintf(f, "set filterban %d\n", filterban->integer);

	for (i = 0; i < numipfilters; i++)
	{
		*(unsigned*)b = ipfilters[i].compare;
		fprintf(f, "sv addip %i.%i.%i.%i\n", b[0], b[1], b[2], b[3]);
	}

	fclose(f);
	*/
}

/*
=================
ServerCommand

ServerCommand will be called when an "sv" command is issued.
The game can issue gi.argc() / gi.argv() commands to get the rest
of the parameters
=================
*/
void ServerCommand()
{
	const char *cmd;

	cmd = gi.argv(1);
	if (Q_strcasecmp(cmd, "test") == 0)
		Svcmd_Test_f();
	else if (Q_strcasecmp(cmd, "addip") == 0)
		SVCmd_AddIP_f();
	else if (Q_strcasecmp(cmd, "removeip") == 0)
		SVCmd_RemoveIP_f();
	else if (Q_strcasecmp(cmd, "listip") == 0)
		SVCmd_ListIP_f();
	else if (Q_strcasecmp(cmd, "writeip") == 0)
		SVCmd_WriteIP_f();
	else if (Q_strcasecmp(cmd, "nextmap") == 0)
		SVCmd_NextMap_f();
	else if (Q_strcasecmp(cmd, "raid_load") == 0)
		RaidDirector_Load(gi.argv(2));
	else if (Q_strcasecmp(cmd, "raid_reload") == 0)
		RaidDirector_Reload();
	else if (Q_strcasecmp(cmd, "raid_reset") == 0)
		RaidDirector_ResetEncounter();
	else if (Q_strcasecmp(cmd, "raid_dump") == 0)
	{
		gi.Com_PrintFmt("[Q2RAID BUILD] {} {}\n", Q2RAID_BUILD_TAG, Q2RAID_BUILD_SHA);
		RaidDirector_Dump();
	}
	else if (Q_strcasecmp(cmd, "raid_event_caps") == 0)
		RaidDirector_DumpEventCapabilities();
	else if (Q_strcasecmp(cmd, "raid_event_check") == 0)
	{
		if (gi.argc() < 4)
			gi.Com_Print("[raid] Usage: sv raid_event_check <targetname> <signal>\n");
		else
			RaidDirector_CheckEventCapability(gi.argv(2), gi.argv(3));
	}
	else if (Q_strcasecmp(cmd, "raid_event_selftest") == 0)
		RaidDirector_RunEventCapabilitySelfTest();
	else if (Q_strcasecmp(cmd, "raid_monster_dump") == 0)
		RaidMonsters_Dump();
	else if (Q_strcasecmp(cmd, "raid_set_state") == 0)
		RaidDirector_SetState(gi.argv(2));
	else if (Q_strcasecmp(cmd, "raid_bot_add") == 0)
		gi.AddCommandString("bot_add\n");
	else if (Q_strcasecmp(cmd, "raid_bot_remove_all") == 0)
		gi.AddCommandString("bot_removeall\n");
	else if (Q_strcasecmp(cmd, "raid_test_flash") == 0)
		RaidDirector_TestFlash(false);
	else if (Q_strcasecmp(cmd, "raid_test_dark") == 0)
		RaidDirector_TestFlash(true);
	else
		gi.LocClient_Print(nullptr, PRINT_HIGH, "Unknown server command \"{}\"\n", cmd);
}
