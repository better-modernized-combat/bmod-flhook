#pragma once

#include <FLHook.hpp>
#include <plugin.h>
#include "../npc_control/NPCControl.h"
#include "../solar_control/SolarControl.h"

namespace Plugins::Triggers
{

	struct Position final : Reflectable
	{
		std::vector<float> location;
		std::string system;
	};

	struct Event final : Reflectable
	{
		std::wstring solarFormation;
		std::map<std::wstring, int> npcs;
		int spawnWeight;
		std::string eventLowInfoDescription;
		std::string eventMedInfoDescription;
		std::string eventHighInfoDescription;
		int lifetimeInSeconds;
	};

	struct TerminalGroup final : Reflectable
	{
		std::string terminalName = "Communications Buoy";
		int cooldownTimeInSeconds;
		int useTimeInSeconds;
		int hackTimeInSeconds;

		float hackHostileChance;
		int minHostileHackHostileNpcs;
		int maxHostileHackHostileNpcs;

		int useCostInCredits;
		int minHackRewardInCredits;
		int maxHackRewardInCredits;

		std::string lawfulUseMessage;
		std::string unlawfulHackMessage;

		std::vector<std::wstring> terminalList;
		std::vector<Event> useEventList;
		std::vector<Event> hackEventList;
		std::vector<Position> spawnPositionList;
	};

	// Loadable json configuration
	struct Config final : Reflectable
	{
		std::string File() override { return "config/triggers.json"; }

		std::vector<TerminalGroup> terminalGroups;

		float hackAndUseInitiateRadiusInMeters;
		float hackAndUseSustainRadiusInMeters;
		float hackNotifyAllRadiusInMeters;
		std::wstring hackStartNotifyAllMessage = L"A {0} is being hacked by {1} in sector {2}!";
		std::wstring hackFinishNotifyAllMessage = L"{0} has completed their hack of the {1} in sector {2} and retrieved sensitive data from the {3}!";

		std::map<std::string, std::vector<std::string>> factionNpcSpawnList {};

		bool useHealthAdjustmentForTerminalStatus;
		std::string shipActiveHackFuse;
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