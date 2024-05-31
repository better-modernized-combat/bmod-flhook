#pragma once

#include <FLHook.hpp>
#include <plugin.h>
#include "../npc_control/NPCControl.h"
#include "../solar_control/SolarControl.h"

namespace Plugins::Triggers
{

	struct Position final : Reflectable
	{
		std::vector<float> coordinates = {0, 0, 0};
		std::string system = "li01";
		uint despawnTime = 0;
	};

	struct Event final : Reflectable
	{
		std::wstring solarFormation = L"test_solar_formation_01";
		std::string name = "Test Event";
		std::map<std::wstring, int> npcs;
		int spawnWeight = 1;
		std::wstring descriptionLowInfo = L"This is a placeholder low information description for the event that has been triggered.";
		std::wstring descriptionMedInfo = L"This is a placeholder medium information description for the event that has been triggered.";
		std::wstring descriptionHighInfo = L"This is a placeholder high information description for the event that has been triggered.";
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
		uint lastActivatedTime = 0;
		bool useInProgress = false;

		int useTimeInSeconds = 30;
		int hackTimeInSeconds = 90;

		float hackHostileChance = 0.5f;
		int minHostileHackHostileNpcs = 1;
		int maxHostileHackHostileNpcs = 3;

		int useCostInCredits = 7500;
		int minHackRewardInCredits = 2000;
		int maxHackRewardInCredits = 4000;

		std::string messageLawfulUse = "This is the message you see describing the terminal function and cost when you lawfully use the terminal.";
		std::string messageUnlawfulHack =
		    "This is the message you see when you attempt to hack the terminal that describes the possible penalties and rewards.";

		std::vector<std::string> terminalList;
		std::vector<EventFamily> eventFamilyUseList;
		std::vector<EventFamily> eventFamilyHackList;
	};

	// Loadable json configuration
	struct Config final : Reflectable
	{
		std::string File() override { return "config/triggers.json"; }

		std::vector<TerminalGroup> terminalGroups;

		float terminalInitiateRadiusInMeters = 750;
		float terminalSustainRadiusInMeters = 2000;
		float terminalNotifyAllRadiusInMeters = 100000;
		std::wstring messageHackStartNotifyAll = L"A {0} is being hacked by {1} in sector {2}!";
		std::wstring messageHackFinishNotifyAll = L"{0} has completed their hack of the {1} in sector {2} and retrieved sensitive data from the {3}!";

		std::map<std::string, std::vector<std::string>> factionNpcSpawnList;

		bool terminalHealthAdjustmentForStatus = true;
		std::string shipActiveTerminalFuse;
	};

	struct TriggerInfo
	{
		uint target = 0;
		uint time = 0;
		bool playerHasBeenWarned = false;
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
		Plugins::Npc::NpcCommunicator* npcCommunicator = nullptr;
		Plugins::SolarControl::SolarCommunicator* solarCommunicator = nullptr;
		bool pluginActive = true;
	};
} // namespace Plugins::Triggers