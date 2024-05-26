/**
 * @date May, 2024
 * @author IrateRedKite
 * @defgroup Triggers Triggers
 * @brief
 * Placeholder brief description for Triggers
 *
 * @paragraph cmds Player Commands
 * There are no player commands in this plugin.
 *
 * @paragraph adminCmds Admin Commands
 * There are no admin commands in this plugin.
 *
 * @paragraph configuration Configuration
 * No configuration file is needed.
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 *
 * @paragraph optional Optional Plugin Dependencies
 * This plugin depends on NPC Control and Solar Control
 */

#include "Triggers.hpp"

#include <random>

// TODO: Settings validation
// TODO: Spawning system
// TODO: Cooldown timers and hacking checks

namespace Plugins::Triggers
{
	const auto global = std::make_unique<Global>();

	int GetRandomNumber(int min, int max)
	{
		static std::random_device dev;
		static auto engine = std::mt19937(dev());
		auto range = std::uniform_int_distribution(min, max);
		return range(engine);
	}

	int GetRandomWeight(const std::vector<int>& weights)
	{
		std::discrete_distribution<> dist(weights.begin(), weights.end());
		static std::mt19937 engine;
		auto weightIndex = dist(engine);
		return weightIndex;
	}

	// Function: returns true if this player is within fDistance of the target solar
	bool clientIsInRangeOfSolar(ClientId client, uint solar, float distance)
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

	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));

		// Set the npcCommunicator and solarCommunicator interfaces and check if they are available
		global->npcCommunicator =
		    static_cast<Plugins::Npc::NpcCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Plugins::Npc::NpcCommunicator::pluginName));

		global->solarCommunicator = static_cast<Plugins::SolarControl::SolarCommunicator*>(
		    PluginCommunicator::ImportPluginCommunicator(Plugins::SolarControl::SolarCommunicator::pluginName));

		// Prevent the plugin from progressing further and disable all functions if either interface is not found
		if (!global->npcCommunicator)
		{
			Console::ConErr(std::format("npc.dll not found. The plugin is required for this module to function."));
			global->pluginActive = false;
		}

		if (!global->solarCommunicator)
		{
			Console::ConErr(std::format("solar.dll not found. The plugin is required for this module to function."));
			global->pluginActive = false;
		}

		if (!global->pluginActive)
		{
			Console::ConErr(std::format("Critical components of Triggers were not found or were configured incorrectly. The plugin has been disabled."));
			return;
		}
	}

	/** @ingroup Triggers
	 * @brief Creates a point of interested and it's accompanying NPCs if there are any defined.
	 */
	// Not called CreateEvent because Windows is awful
	void CreatePoiEvent(const Event& event, const Position& position)
	{
		Vector pos = {position.coordinates[0], position.coordinates[1], position.coordinates[2]};
		Matrix mat = EulerMatrix({0.f, 0.f, 0.f});

		global->solarCommunicator->CreateSolarFormation(event.solarFormation, pos, CreateID(position.system.c_str()));

		for (const auto& npcs : event.npcs)
		{
			global->npcCommunicator->CreateNpc(npcs.first, pos, mat, CreateID(position.system.c_str()), true);
		}
	}

	// TODO: Maybe see if you can move the weight selection into the GetRandomWeights function fully and do the loop in there

	/** @ingroup Triggers
	 * @brief Completes a terminal interaction, rewards the player and spawns a random event selected from the appropriate pool
	 */
	void CompleteTerminalInteraction(TerminalGroup& terminalGroup, TriggerInfo terminalInfo, uint client, bool isLawful)
	{
		auto& eventFamililyList = isLawful ? terminalGroup.eventFamilyUseList : terminalGroup.eventFamilyHackList;

		std::vector<int> familyWeights;
		for (const auto& eventFamily : eventFamililyList)
		{
			familyWeights.emplace_back(eventFamily.spawnWeight);
		}
		auto& family = eventFamililyList[GetRandomWeight(familyWeights)];

		std::vector<int> eventWeights;
		for (const auto& event : eventFamililyList[GetRandomWeight(familyWeights)].eventList)
		{
			eventWeights.emplace_back(event.spawnWeight);
		}
		auto& event = family.eventList[GetRandomWeight(eventWeights)];

		// Select a random position
		Position* position = nullptr;
		int counter = 0;
		do
		{
			position = &family.spawnPositionList[GetRandomNumber(0, family.spawnPositionList.size())];
			if (counter++ > 30)
			{
				Console::ConErr(std::format("Unable to find a valid spawn position for {}. Please check your config has an appropriate number of spawn "
				                            "locations defined for this family.",
				    family.name));
				return;
			}
		} while (position->despawnTime == 0);

		// Set the Despawn Time
		position->despawnTime = Hk::Time::GetUnixSeconds();

		std::wstring rewardSectorMessage = Hk::Math::VectorToSectorCoord<std::wstring>(
		    CreateID(position->system.c_str()), Vector {position->coordinates[0], position->coordinates[1], position->coordinates[2]});

		Console::ConDebug(std::format("Spawning the event '{}' at {},{},{} in {}",
		    event.name,
		    position->coordinates[0],
		    position->coordinates[1],
		    position->coordinates[2],
		    position->system));

		CreatePoiEvent(event, *position);

		// TODO: Gotta print the messages here for the hacking client and the system with appropriate info
		// TODO: std::vformat for args passed into the description
		PrintUserCmdText(client, event.descriptionMedInfo);
		// TODO: std::format for global->config->messageHackFinishNotifyAll to feed in positional data, faction and client.
	}

	void UserCmdStartTerminalInteraction(ClientId& client, TriggerInfo triggerInfo, bool isLawful)
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
		if (bool inRange = clientIsInRangeOfSolar(client, target, global->config->terminalInitiateRadiusInMeters); !inRange)
		{
			PrintUserCmdText(client, L"The target you have selected is too far away to initiate a hacking attempt. Please get closer.");
			return;
		}

		TerminalGroup* group = nullptr;

		// Check if this solar is on the list of availlable terminals
		for (auto& terminalGroup : global->config->terminalGroups)
		{
			auto found = false;
			for (const auto& terminal : terminalGroup.terminalList)
			{
				if (CreateID(terminal.c_str()) == target)
				{
					found = true;
					break;
				}
			}

			if (found)
			{
				group = &terminalGroup;
				break;
			}
		}

		if (!group)
		{
			PrintUserCmdText(client, L"The target you have selected is not currently hackable, please select a valid target.");
			return;
		}
		// TODO: check for is hacking and is hacked and on cooldown

		triggerInfo.target = target;
		triggerInfo.inProgress = true;
		Hk::Client::PlaySoundEffect(client, CreateID("ui_new_story_star"));
		Hk::Client::PlaySoundEffect(client, CreateID("ui_begin_scan"));

		auto clientPos = Hk::Solar::GetLocation(client, IdType::Client).value().first;
		auto clientSystem = Hk::Solar::GetSystemBySpaceId(target).value();

		if (!isLawful)
		{
			PrintLocalUserCmdText(client,
			    std::vformat(global->config->messageHackStartNotifyAll,
			        std::make_wformat_args(
			            Hk::Client::GetCharacterNameByID(client).value(), Hk::Math::VectorToSectorCoord<std::wstring>(clientSystem, clientPos))),
			    global->config->terminalNotifyAllRadiusInMeters);
		}

		// group contains data, proceed
		// spawn NPCs
		// start FX
		// start timer
		// Set solar hostile
	}

} // namespace Plugins::Triggers

using namespace Plugins::Triggers;

REFL_AUTO(type(Position), field(coordinates), field(system));
REFL_AUTO(type(Event), field(name), field(solarFormation), field(npcs), field(spawnWeight), field(descriptionLowInfo), field(descriptionMedInfo),
    field(descriptionHighInfo), field(lifetimeInSeconds));
REFL_AUTO(type(EventFamily), field(name), field(spawnWeight), field(eventList), field(spawnPositionList));
REFL_AUTO(type(TerminalGroup), field(terminalGroupName), field(terminalName), field(cooldownTimeInSeconds), field(useTimeInSeconds), field(hackTimeInSeconds),
    field(hackHostileChance), field(minHostileHackHostileNpcs), field(maxHostileHackHostileNpcs), field(useCostInCredits), field(minHackRewardInCredits),
    field(maxHackRewardInCredits), field(messageLawfulUse), field(messageUnlawfulHack), field(terminalList), field(eventFamilyUseList),
    field(eventFamilyHackList));
REFL_AUTO(type(Config), field(terminalGroups), field(terminalInitiateRadiusInMeters), field(terminalSustainRadiusInMeters),
    field(terminalNotifyAllRadiusInMeters), field(messageHackStartNotifyAll), field(messageHackFinishNotifyAll), field(factionNpcSpawnList),
    field(terminalHealthAdjustmentForStatus), field(shipActiveTerminalFuse));

DefaultDllMainSettings(LoadSettings);

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Triggers");
	pi->shortName("triggers");
	pi->mayUnload(true);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
}