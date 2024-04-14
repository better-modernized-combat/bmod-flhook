/**
 * @date Jan, 2024
 * @author IrateRedKite
 * @defgroup Hacking Hacking
 * @brief
 * This plugin allows players to 'hack' a specified type of solar by remaining close to it under difficult circumstances for rewards.
 *
 * @paragraph cmds Player Commands
 * - hack - Initiates a hacking session against the targeted solar
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @paragraph configuration Configuration
 * TBC
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * This plugin depends on NPC Control
 */

// Includes
#include "Hacking.hpp"

#include <ranges>

// TODO: Fuses are inconsistent in space, find a way to have these be consistent.
// TODO: General pass on variable names
namespace Plugins::Hacking
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();

	// Function: Generates a random int between min and max
	int RandomNumber(int min, int max)
	{
		static std::random_device dev;
		static auto engine = std::mt19937(dev());
		auto range = std::uniform_int_distribution(min, max);
		return range(engine);
	}

	// Function: load the JSON config file
	void LoadSettings()
	{
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));

		// Checks if the configured hacking time value is valid
		if ((global->config->hackingTime % 5) != 0)
		{
			global->pluginActive = false;
			AddLog(LogType::Normal, LogLevel::Err, "Hacking time must be a multiple of 5.");
		}

		// Checks to see if the NPC.dll IPC interface has been made availlable and prevents the plugin from loading further if it has not.
		global->npcCommunicator =
		    static_cast<Plugins::Npc::NpcCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Plugins::Npc::NpcCommunicator::pluginName));
		if (!global->npcCommunicator)
		{
			global->pluginActive = false;
			AddLog(LogType::Normal, LogLevel::Err, "NPC.dll must be loaded for this plugin to function.");
		}

		// Same for Solarcontrol
		global->solarCommunicator = static_cast<Plugins::SolarControl::SolarCommunicator*>(
		    PluginCommunicator::ImportPluginCommunicator(Plugins::SolarControl::SolarCommunicator::pluginName));
		if (!global->solarCommunicator)
		{
			global->pluginActive = false;
			AddLog(LogType::Normal, LogLevel::Err, "Solar.dll must be loaded for this plugin to function");
		}

		for (const auto& [key, value] : global->config->guardNpcMap)
		{
			global->guardNpcMap[MakeId(key.c_str())] = value;
		}

		for (const auto& [key, value] : global->config->factionNpcMap)
		{
			auto& npcList = global->factionNpcMap[MakeId(key.c_str())] = {};
			npcList.reserve(value.size());

			for (const auto& str : value)
			{
				npcList.emplace_back(stows(str));
			}
		}

		for (const auto& [key, value] : global->config->opposingFactions)
		{
			auto& factions = global->opposingFactions[MakeId(key.c_str())] = {};
			for (const auto& faction : value)
			{
				factions.emplace_back(MakeId(faction.c_str()));
			}
		}

		for (const auto& [key, value] : global->config->initialObjectiveSolars)
		{
			ObjectiveSolarCategories solars;
			for (const auto& id : value)
			{
				solars.rotatingSolars.emplace_back(CreateID(id.c_str()));
			}

			solars.currentIndex = RandomNumber(0, solars.rotatingSolars.size() - 1);
			global->solars[key] = solars;
		}

		for (auto& item : global->config->objectiveZoneList)
		{
			auto zone = CreateID(item.c_str());
			global->hashedObjectiveZoneList.insert(global->hashedObjectiveZoneList.begin(), zone);
		}
		AddLog(LogType::Normal, LogLevel::Debug, std::format("Loaded {} possible objective spawn locations", global->hashedObjectiveZoneList.size()));

		for (auto& item : global->config->solarGroups)
		{
			for (auto& component : item.solarComponents)
			{
				if (component.relativePos.size() != 3)
				{
					component.relativePos = {0.0, 0.0, 0.0};
					AddLog(LogType::Normal,
					    LogLevel::Err,
					    std::format("The solarComponent {} has an invalid relative position value, using 0, 0, 0 instead", component.solarName));
				}
				if (component.rotation.size() != 3)
				{
					component.rotation = {0.0, 0.0, 0.0};
					AddLog(LogType::Normal,
					    LogLevel::Err,
					    std::format("The solarComponent {} has an invalid rotation value, using 0, 0, 0 instead", component.solarName));
				}
			}
		}
	}

	//  Function: Attaches and lights a fuse on a player ship
	void LightShipFuse(uint client, const std::string& fuse)
	{
		auto playerShip = Hk::Player::GetShip(client).value();
		IObjInspectImpl* inspect;
		uint iDunno;

		GetShipInspect(playerShip, inspect, iDunno);
		Hk::Admin::LightFuse((IObjRW*)inspect, CreateID(fuse.c_str()), 0.f, 5.f, 0);
	}

	// Function: Detatches and unlights a fuse on a player ship
	void UnLightShipFuse(uint client, const std::string& fuse)
	{
		auto playerShip = Hk::Player::GetShip(client).value();
		IObjInspectImpl* inspect;
		uint iDunno;

		GetShipInspect(playerShip, inspect, iDunno);
		Hk::Admin::UnLightFuse((IObjRW*)inspect, CreateID(fuse.c_str()));
	}

	// Function: returns true if this player is within fDistance of the target solar
	bool IsInSolarRange(ClientId client, uint solar, float distance)
	{
		// Get the Player position
		auto playerPos = Hk::Solar::GetLocation(client, IdType::Client);
		if (playerPos.has_error())
		{
			PrintUserCmdText(client, L"Failed to get client position, something went wrong.");
			return false;
		}

		// Get the Solar position
		auto solarPos = Hk::Solar::GetLocation(solar, IdType::Solar);
		if (solarPos.has_error())
		{
			PrintUserCmdText(client, L"Failed to get target position, something went wrong.");
			return false;
		}

		// Check if the player is within distance of the target solar
		if (!(Hk::Math::Distance3D(playerPos.value().first, solarPos.value().first) < distance))
		{
			return false;
		}

		return true;
	}

	// TODO: Get this working with NPCs (Currently server-client desync means this just allows them to shoot whilost invisible)
	void ToggleCloak(uint npc, ushort cloak, bool state)
	{
		XActivateEquip ActivateEq;
		ActivateEq.bActivate = state;
		ActivateEq.iSpaceId = npc;
		ActivateEq.sId = cloak;

		Server.ActivateEquip(0, ActivateEq);
	}

	// Function: Spawns ships between min and max of a given type for a given faction. Picks randomly from the list for that faction defined in the config
	std::vector<SpawnedObject*> SpawnRandomShips(ushort faction, const Vector& pos, uint system, int min, int max)
	{
		std::vector<SpawnedObject*> spawnedNpcs;
		auto& factionShips = global->factionNpcMap[faction];

		for (int i = 0; i < RandomNumber(min, max); i++)
		{
			auto randomNpc = factionShips[RandomNumber(0, factionShips.size() - 1)];
			Matrix defaultNpcRot = {{{1.f, 0, 0}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}};

			uint id = global->npcCommunicator->CreateNpc(randomNpc, pos, defaultNpcRot, system, true);
			auto& npc = global->spawnedNpcs.emplace_back(id, 0, Hk::Time::GetUnixSeconds());
			spawnedNpcs.emplace_back(&npc);
		}

		return spawnedNpcs;
	}

	// What happens when our /hack command is called by a player
	void UserCmdStartHack(ClientId& client)
	{
		// Check to make sure the plugin has loaded dependencies and settings.
		if (!global->pluginActive)
		{
			PrintUserCmdText(client, L"There was an error loading this plugin, please contact your server administrator.");
			return;
		}

		// Check if the player is docked.
		if (Hk::Player::GetShip(client).has_error())
		{
			PrintUserCmdText(client, L"You must be in space to use this function.");
			return;
		}

		// Check if the player has a valid target
		const auto res = Hk::Player::GetTarget(client);
		if (res.has_error())
		{
			PrintUserCmdText(client, L"You must select a valid target to use this function.");
			return;
		}

		auto target = res.value();
		// Check if the player is within hackingInitiateRadius
		if (bool inRange = IsInSolarRange(client, target, global->config->hackingInitiateRadius); !inRange)
		{
			PrintUserCmdText(client, L"The target you have selected is too far away to initiate a hacking attempt. Please get closer.");
			return;
		}

		// Check if the selected target's archetypeId matches what we've defined in the config.
		bool solarFound = false;
		for (auto& i : global->solars | std::views::values)
		{
			auto& solar = i.rotatingSolars[i.currentIndex];

			if (solar.solar != target)
			{
				continue;
			}

			if (solar.isHacked)
			{
				PrintUserCmdText(client, L"Someone has recently hacked this target and it has been secured.");
				return;
			}

			if (solar.isHacking)
			{
				PrintUserCmdText(client, L"Someone else is already attempting to hack this target.");
				return;
			}

			solarFound = true;
			break;
		}

		if (!solarFound)
		{
			PrintUserCmdText(client, L"The target you have selected is not currently hackable, please select a valid target.");
			return;
		}

		// Check if the player is already doing a hack.
		auto& hack = global->activeHacks[client];
		if (hack.time > 0)
		{
			PrintUserCmdText(client, L"You are already hacking an object.");
			return;
		}

		hack.target = target;
		Hk::Client::PlaySoundEffect(client, CreateID("ui_new_story_star"));
		Hk::Client::PlaySoundEffect(client, CreateID("ui_begin_scan"));

		// Start the Hack, sending a message to everyone within the hackingStartedMessage radius.
		for (auto& i : global->solars | std::views::values)
		{
			i.rotatingSolars[i.currentIndex].isHacking = true;
		}

		auto hackerPos = Hk::Solar::GetLocation(client, IdType::Client).value().first;
		auto hackerSystem = Hk::Solar::GetSystemBySpaceId(target).value();
		auto formattedStartHackMessage = std::vformat(global->config->hackingStartedMessage,
		    std::make_wformat_args(Hk::Client::GetCharacterNameByID(client).value(), Hk::Math::VectorToSectorCoord<std::wstring>(hackerSystem, hackerPos)));
		PrintLocalUserCmdText(client, formattedStartHackMessage, global->config->hackingMessageRadius);

		// Get the solar's reputation
		int solarReputation;
		pub::SpaceObj::GetRep(hack.target, solarReputation);
		uint solarFaction;
		pub::Reputation::GetAffiliation(solarReputation, solarFaction);

		SpawnRandomShips(solarFaction, hackerPos, hackerSystem, global->config->minNpcGuards, global->config->maxNpcGuards);

		PrintUserCmdText(client,
		    std::format(L"You have started a hack, remain within {:.0f}m of the target for {} seconds in order to complete successful data retrieval.",
		        (global->config->hackingSustainRadius),
		        global->config->hackingTime));

		// Start the attached FX for the player and the hack
		LightShipFuse(client, global->config->shipFuse);

		// Sets the relative health of the target to half in order to play the hacking radius FX fuse. This is horrible, but seems to be our only option.
		pub::SpaceObj::SetRelativeHealth(target, 0.5f);

		// Set the target solar hostile to the player temporarily.
		// TODO: Make this a function
		int reputation;
		pub::Player::GetRep(client, reputation);
		int npcReputation;
		pub::SpaceObj::GetRep(target, npcReputation);
		pub::Reputation::SetAttitude(npcReputation, reputation, -0.9f);

		// Start the Hack timer
		hack.time = global->config->hackingTime;
	}

	// Define usable chat commands here
	const std::vector commands = {{
	    CreateUserCommand(L"/hack", L"", UserCmdStartHack, L"Starts a hacking session against a valid solar."),
	}};

} // namespace Plugins::Hacking

using namespace Plugins::Hacking;

// Generates the JSON file
REFL_AUTO(type(SolarPositions), field(solarName), field(relativePos), field(rotation));
REFL_AUTO(type(SolarGroup), field(name), field(presentFactions), field(minShipCount), field(maxShipCount), field(solarComponents), field(npcsToSpawn),
    field(spawnWeight));
REFL_AUTO(type(Config), field(hackingStartedMessage), field(hackingFinishedMessage), field(hackingMessageRadius), field(hackingTime), field(rewardCashMin),
    field(rewardCashMax), field(hackingTime), field(guardNpcPersistTime), field(minNpcGuards), field(maxNpcGuards), field(hackingSustainRadius),
    field(hackingInitiateRadius), field(guardNpcMap), field(factionNpcMap), field(initialObjectiveSolars), field(useFuses), field(shipFuse),
    field(objectiveZoneList), field(solarGroups), field(opposingFactions), field(poiPersistTimeInSeconds), field(npcPersistTimeInSeconds));

DefaultDllMainSettings(LoadSettings);

const std::vector<Timer> timers = {{FiveSecondTick, 5}, {TwentyMinuteTick, 240}, {OneMinuteTick, 240}};
extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Hacking");
	pi->shortName("Hacking");
	pi->mayUnload(true);
	pi->commands(&commands);
	pi->timers(&timers);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
}