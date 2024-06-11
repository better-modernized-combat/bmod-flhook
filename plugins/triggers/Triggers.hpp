#pragma once

#include <FLHook.hpp>
#include <plugin.h>
#include "../npc_control/NPCControl.h"
#include "../solar_control/SolarControl.h"

namespace Plugins::Triggers
{

	struct PlayerConfig final : Reflectable
	{
		bool usePrompt = true;
		bool hackPrompt = true;
	};

	struct Position final : Reflectable
	{
		std::vector<float> coordinates = {0, 0, 0};
		std::string system = "li01";
		uint despawnTime = 0;
	};

	struct SpawnedObject
	{
		uint spaceId = 0;
		uint despawnTime = 0;
	};

	struct Event final : Reflectable
	{
		std::wstring solarFormation = L"test_solar_formation_01";
		std::string name = "Test Event";
		std::map<std::wstring, int> npcs;
		int spawnWeight = 1;
		std::wstring descriptionLowInfo = L"A point of interest has been revealed in sector {0}.";
		std::wstring descriptionMedInfo = L"A point of interest has been revealed in sector {0}. Time Window: {1} Minutes";
		std::wstring descriptionHighInfo = L"A point of interest has been revealed in sector {0} ({2:.0f}, {3:.0f}, {4:.0f}). Time Window: {1} Minutes";
		int lifetimeInSeconds = 1200;
	};

	struct EventFamily final : Reflectable
	{
		std::string name = "Test Event Family";
		int spawnWeight = 1;
		std::vector<Event> eventList;
		std::vector<Position> spawnPositionList;
	};

	struct TerminalGroup final : Reflectable
	{
		std::string terminalGroupName = "terminal_group_1";
		std::string terminalName = "Communications Buoy";
		uint cooldownTimeInSeconds = 1200;

		int useTimeInSeconds = 30;
		int hackTimeInSeconds = 90;

		float hackHostileChance = 0.5f;
		float hackRepReduction = 0.05f;
		int minHostileHackHostileNpcs = 1;
		int maxHostileHackHostileNpcs = 3;

		int useCostInCredits = 7500;
		int minHackRewardInCredits = 2000;
		int maxHackRewardInCredits = 4000;

		std::vector<std::string> terminalList;
		std::vector<EventFamily> eventFamilyUseList;
		std::vector<EventFamily> eventFamilyHackList;

		// Possible NPCs that can spawn for this terminal if a hack attempt rolls hostile.
		std::vector<std::wstring> hostileHackNpcs;
	};

	struct RuntimeTerminalGroup
	{
		uint activeClient = 0;
		TerminalGroup* data = nullptr;
		std::vector<uint> terminalList;
		uint lastActivatedTime = 0;
		uint currentTerminal = 0;
		bool currentTerminalIsLawful;

		bool playerHasBeenWarned = false;
	};

	// Loadable json configuration
	struct Config final : Reflectable
	{
		std::string File() override { return "config/triggers.json"; }

		std::vector<TerminalGroup> terminalGroups;

		float terminalInitiateRadiusInMeters = 750;
		float terminalSustainRadiusInMeters = 2000;
		float terminalNotifyAllRadiusInMeters = 100000;
		int hackNpcLifetimeInSeconds = 600;
		std::wstring messageHackStartNotifyAll = L"A {0} is being hacked by {1} in sector {2}!";
		std::wstring messageHackFinishNotifyAll = L"{0} has completed their hack of the {1} in sector {2} and retrieved sensitive data from the {3}!";

		std::map<std::string, std::vector<std::string>> factionNpcSpawnList;

		bool terminalHealthAdjustmentForStatus = true;
		std::string shipActiveTerminalFuse;
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
		Plugins::Npc::NpcCommunicator* npcCommunicator = nullptr;
		Plugins::SolarControl::SolarCommunicator* solarCommunicator = nullptr;
		bool pluginActive = true;
		std::map<CAccount*, PlayerConfig> playerConfigs;
		std::vector<RuntimeTerminalGroup> runtimeGroups;
		std::vector<SpawnedObject> spawnedObjects;
	};
} // namespace Plugins::Triggers