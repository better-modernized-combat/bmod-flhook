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

		// Reflectable fields
		//! Message sent to the local area when the hack starts.
		std::wstring hackingStartedMessage = L"A communications buoy is being hacked by {0} in sector {1}!";
		//! Message sent to the local area when the hack ends.
		std::wstring hackingFinishedMessage =
		    L"{0} has completed their hack of the communications buoy in sector {1} and retrieved sensitive data from the {2}!";
		//! Radius in which the message is sent to players in the area.
		float hackingMessageRadius = 25000.f;
		//! Time taken for the hack to complete.
		int hackingTime = 60;
		//! The radius the player needs to be within to initiate the hack.
		float hackingInitiateRadius = 750.f;
		//! The radius the player needs to be within to sustain the hack.
		float hackingSustainRadius = 2000.f;
		//! The minimum credits rewarded when the player completes the hack.
		int rewardCashMin = 2500;
		//! The maximum credits rewarded when the player completes the hack.
		int rewardCashMax = 5000;
		//! Amount of time spawned NPCs that guard the hacking buoy will persist after a hack has been completed or failed.
		int guardNpcPersistTime = 20;
		//! The minimum number of NPC guards that will spawn on an initial objective.
		int minNpcGuards = 2;
		//! The maximum number of NPC guards that will spawn on an initial objective.
		int maxNpcGuards = 3;
		//! Whether or not to attach VFX fuses to a ship that's hacking the initial objective. This also adjusts the health of an objective depending on its
		//! state in order to trigger additional fuses. These must be defined in the inis and are restricted to modded installs only. Enabling this shouldn't
		//! cause problems on a vanilla server.
		bool useFuses = true;
		//! The fuse to attach to the ship (If in use)
		std::string shipFuse = "bm_hack_ship_fuse";
		//! A Map of which NPCs can spawn for which factions when guarding objectives
		std::map<std::string, std::vector<std::string>> guardNpcMap {
		    {"li_n_grp", {{"l_defender"}, {"l_patriot"}, {"l_guardian"}}}, {"fc_x_grp", {{"x_hawk"}, {"x_falcon"}, {"x_eagle"}}}};
		//! A Map of which hackable solars, and the system that they are in.
		std::map<std::string, std::vector<std::string>> initialObjectiveSolars {
		    {"Li03", {{"bm_li03_hackable_1"}, {"bm_li03_hackable_2"}, {"bm_li03_hackable_3"}}},
		    {"Li01", {{"bm_li01_hackable_1"}, {"bm_li01_hackable_2"}, {"bm_li01_hackable_3"}}}};
	};

	struct HackInfo
	{
		uint target = 0;
		int time = 0;
		bool beenWarned = false;
		std::vector<uint> spawnedNpcList;
	};

	struct ObjectiveSolars
	{
		uint solar = 0;
		bool isHacking = false;
		bool isHacked = false;
	};

	struct ObjectiveSolarCategories
	{
		int time = 0;
		uint currentIndex = 0;
		std::vector<ObjectiveSolars> rotatingSolars;
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
		bool pluginActive = true;
		std::array<HackInfo, 255> activeHacks;
		Plugins::Npc::NpcCommunicator* npcCommunicator = nullptr;
		std::unordered_map<uint, std::vector<std::string>> guardNpcMap;
		std::map<std::string, ObjectiveSolarCategories> solars;
	};
} // namespace Plugins::Hacking