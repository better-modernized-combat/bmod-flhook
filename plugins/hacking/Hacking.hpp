#pragma once

#include <FLHook.hpp>
#include <plugin.h>
#include "../npc_control/NPCControl.h"
// #include "../bountyhunt/BountyHunt.h"
// #include "../death_penalty/DeathPenalty.h"

namespace Plugins::Hacking
{

	//! Configurable fields for this plugin
	struct Config final : Reflectable
	{
		std::string File() override { return "config/hacking.json"; }

		// TODO: put variables into the hackable strings.

		// Reflectable fields
		//! Hackable solar archetypes
		std::string hackableSolarArchetype = "bm_hackable_sat";
		//! Message sent to the local area when the hack starts.
		std::wstring hackingStartedMessage = L"A communications buoy is being hacked by {0} in sector {1}!";
		//! Message sent to the local area when the hack ends.
		std::wstring hackingFinishedMessage =
		    L"{0} has completed their hack of the communications buoy in sector {1} and retrieved sensitive data from the {2}!";
		//! Radius in which the message is sent
		float hackingMessageRadius = 25000.f;
		//! Time taken for the hack to complete.
		int hackingTime = 60;
		//! The radius the player needs to be within to initiate and sustain the hack.
		float hackingRadius = 750.f;
		//! The minimum credits rewarded when the player completes the hack.
		int rewardCashMin = 2500;
		//! The maximum credits rewarded when the player completes the hack.
		int rewardCashMax = 5000;
	};

	struct HackInfo
	{
		uint target = 0;
		int time = 0;
		bool beenWarned = false;
		std::vector<uint> spawnedNpcList;
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
		bool pluginActive = true;
		uint targetHash = 0;
		std::array<HackInfo, 255> activeHacks;
		Plugins::Npc::NpcCommunicator* npcCommunicator = nullptr;
	};
} // namespace Plugins::Hacking