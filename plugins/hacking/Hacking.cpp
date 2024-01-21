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
 * This plugin has no dependencies.
 */

// Includes
#include "Hacking.hpp"

namespace Plugins::Hacking
{
	const auto global = std::make_unique<Global>();

	// TODO: Please put things in functions dear god

	// Function breakdown:
	// Spawn NPC
	// Random Numbers
	// Initial checks to validate hackable
	// Grab something's reputation

	// Put things that are performed on plugin load here!
	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));
		// Checks to see if the config file is missing valuesprevents the plugin from loading further if it has not.
		if (global->config->hackableSolarArchetype.empty() || global->config->hackingStartedMessage.empty() || global->config->hackingFinishedMessage.empty() ||
		    !global->config->hackingTime || !global->config->rewardCashMin || !global->config->rewardCashMax || !global->config->guardNpcPersistTime)
		{
			global->pluginActive = false;
		}
		global->targetHash = CreateID(global->config->hackableSolarArchetype.c_str());
		// Checks to see if the NPC.dll IPC interface has been made availlable and prevents the plugin from loading further if it has not.
		global->npcCommunicator =
		    static_cast<Plugins::Npc::NpcCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Plugins::Npc::NpcCommunicator::pluginName));
		if (!global->npcCommunicator)
		{
			global->pluginActive = false;
			AddLog(LogType::Normal, LogLevel::Err, "NPC.dll must be loaded for this plugin to function.");
		}
		for (auto& [key, value] : global->config->configGuardNpcMap)
		{
			global->guardNpcMap[MakeId(key.c_str())] = value;
		}
	}

	// This function returns true if this player is within fDistance of the target solar.
	bool IsInSolarRange(ClientId client, uint solar, float fDistance)
	{
		// Get the player's position
		auto res = Hk::Solar::GetLocation(client, IdType::Client);
		if (res.has_error())
		{
			PrintUserCmdText(client, L"Failed to get client position, something went wrong.");
			return false;
		}
		auto pos = res.value().first;
		// Get the Solar position
		auto res2 = Hk::Solar::GetLocation(solar, IdType::Solar);
		if (res.has_error())
		{
			PrintUserCmdText(client, L"Failed to get target position, something went wrong.");
			return false;
		}

		// Check if the player is within fDistance of the target solar.
		if (auto pos2 = res2.value().first; Hk::Math::Distance3D(pos, pos2) < fDistance)
		{
			return true;
		}
		return false;
	}

	// This function handles events that occur after the hack has been initiated and upon hack completion.
	void hackTimerTick()
	{
		// Start the timer
		for (auto i = 0u; i != global->activeHacks.size(); i++)
		{
			auto& info = global->activeHacks[i];
			info.time -= 5;
			if (info.time < 0)
			{
				if (info.time == (global->config->guardNpcPersistTime * -1))
				{
					for (auto obj : info.spawnedNpcList)
					{
						// TODO: Make them really cloak
						// pub::SpaceObj::EQInfo cloakStruct;
						// pub::SpaceObj::ActivateEquipment(obj, cloakStruct);

						pub::SpaceObj::Destroy(obj, VANISH);
						Hk::Client::PlaySoundEffect(i, CreateID("cloak_rh_fighter"));
					}
					// Clean up when the hack ends
					pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
					info.spawnedNpcList.clear();
					info.target = 0;
				}
				continue;
			}

			// Checks if the player has a ship before proceeding. This handles disconnects, crashing and docking.
			if (Hk::Player::GetShip(i).has_error())
			{
				PrintUserCmdText(i, L"You have left the area, the hack has failed.");
				Hk::Client::PlaySoundEffect(i, CreateID("bm_hack_failed"));
				// Turn off the solar fuse:
				pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
				info.time = 0;
				continue;
			}

			//  Check if the player is in range of the solar while the hack is ongoing. There's a grace period for leaving the area where the player is warned.
			if (!IsInSolarRange(i, info.target, global->config->hackingRadius))
			{
				pub::SpaceObj::SetRelativeHealth(info.target, 0.5f);
				// If the player actually fails the hack by leaving range, print a message and initiate cleanup.
				if (info.beenWarned)
				{
					PrintUserCmdText(i, L"You're out of range, the hack has failed.");
					info.time = 0;
					auto playerShip = Hk::Player::GetShip(i).value();
					IObjInspectImpl* inspect;
					uint iDunno;
					GetShipInspect(playerShip, inspect, iDunno);
					Hk::Admin::UnLightFuse((IObjRW*)inspect, CreateID("bm_hack_ship_fuse"));
					// Play a sound effect for the player to telegraph that they''re leaving the area've failed the hack.
					Hk::Client::PlaySoundEffect(i, CreateID("bm_hack_failed"));
					info.beenWarned = false;
					// Turn off the solar fuse:
					pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
				}
				else
				{
					// Warn the player they're out of range for a cycle and set the beenWarned variable.
					PrintUserCmdText(i, L"Warning, you are moving out of range, the hack will fail if you don't get closer.");
					// Play a sound effect for the player to telegraph that they're leaving the area.
					Hk::Client::PlaySoundEffect(i, CreateID("bm_hack_warn"));
					info.beenWarned = true;
					pub::SpaceObj::SetRelativeHealth(info.target, 0.4f);
				}

				continue;
			}

			// Reset the beenWarned variable when the hack ends or fails.
			info.beenWarned = false;

			// Complete the hack when the timer finishes.
			if (!info.time)
			{
				// TODO: Toggle off the kill reward state on the initiating player.

				// Grab all the information needed to populate the proximity message sent to the players:
				auto hackerEndName = Hk::Client::GetCharacterNameByID(i).value();
				auto hackerEndPos = Hk::Solar::GetLocation(i, IdType::Client).value().first;
				auto hackerEndSystem = Hk::Solar::GetSystemBySpaceId(info.target).value();
				auto sectorEndCoordinate = Hk::Math::VectorToSectorCoord<std::wstring>(hackerEndSystem, hackerEndPos);
				int npcReputation;
				pub::SpaceObj::GetRep(info.target, npcReputation);
				uint npcFaction;
				pub::Reputation::GetAffiliation(npcReputation, npcFaction);
				uint npcIds;
				pub::Reputation::GetGroupName(npcFaction, npcIds);
				auto realName = Hk::Message::GetWStringFromIdS(npcIds);

				// Send the message to everyone within hackingMessageRadius
				auto formattedEndHackMessage =
				    std::vformat(global->config->hackingFinishedMessage, std::make_wformat_args(hackerEndName, sectorEndCoordinate, realName));
				PrintLocalUserCmdText(i, formattedEndHackMessage, global->config->hackingMessageRadius);

				// Adjust the hacker's reputation with the owning faction by -0.05
				auto hackerOriginalRep = Hk::Player::GetRep(i, npcFaction);
				auto hackerNewRep = hackerOriginalRep.value() - 0.05f;
				int playerRepInstance;
				pub::Player::GetRep(i, playerRepInstance);
				pub::Reputation::SetReputation(playerRepInstance, npcFaction, hackerNewRep);

				// Pick a number between rewardCashMin and rewardCashMax and and credit the player that amount for completion of the hack.
				static std::random_device dev;
				static auto engine = std::mt19937(dev());
				static auto dist = std::uniform_int_distribution(global->config->rewardCashMin, global->config->rewardCashMax);
				auto randomCash = dist(engine);
				Hk::Player::AddCash(i, randomCash);

				// Grab all the information needed to populate the private message to the player.
				// TODO: Populate these fields properly
				Vector rewardPos = {{200.f}, {200.f}, {200.f}};
				auto rewardSystem = "Li03";
				auto rewardSector = Hk::Math::VectorToSectorCoord<std::wstring>(CreateID(rewardSystem), rewardPos);

				// Send a private message to the player notifying them of their randomCash reward and POI location.
				auto formattedHackRewardMessage = std::format(
				    L"{} credits diverted from local financial ansibles, a point of interest has been revealed in sector {} ({:.0f}, {:.0f}, {:.0f}).",
				    randomCash,
				    rewardSector,
				    rewardPos.x,
				    rewardPos.y,
				    rewardPos.z);
				PrintUserCmdText(i, formattedHackRewardMessage);

				// Play some sounds to telegraph the successful completion of the hack.
				Hk::Client::PlaySoundEffect(i, CreateID("ui_receive_money"));
				Hk::Client::PlaySoundEffect(i, CreateID("ui_end_scan"));
				auto playerShip = Hk::Player::GetShip(i).value();
				IObjInspectImpl* inspect;
				uint iDunno;
				GetShipInspect(playerShip, inspect, iDunno);
				Hk::Admin::UnLightFuse((IObjRW*)inspect, CreateID("bm_hack_ship_fuse"));

				// Turn off the solar fuse:
				pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
			}
		}
	}

	// What happens when our /hack command is called by a player
	void userCmdStartHack(ClientId& client)
	{
		// Check to make sure the plugin has loaded dependencies and settings.
		if (!global->pluginActive)
		{
			PrintUserCmdText(client, L"There was an error loading this plugin, please contact your server administrator.");
			return;
		}

		// Check if the player is docked.
		auto isDocked = Hk::Player::GetShip(client).has_error();
		if (isDocked)
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
		uint archetypeId;
		pub::SpaceObj::GetSolarArchetypeID(target, archetypeId);

		// Check if the player is within hackingRadius
		bool inRange = IsInSolarRange(client, target, global->config->hackingRadius);
		if (!inRange)
		{
			PrintUserCmdText(client, L"The target you have selected is too far away to initiate a hacking attempt. Please get closer.");
			return;
		}

		// Check if the selected target's archetypeId matches what we've defined in the config.
		if (archetypeId != global->targetHash)
		{
			PrintUserCmdText(client, L"The target you have selected is not hackable, please select a valid target.");
			return;
		}

		// Check if the player is already doing a hack.
		auto& hack = global->activeHacks[client];
		if (hack.time > 0)
		{
			PrintUserCmdText(client, L"You are already hacking an object.");
			return;
		}

		// Start a Timer equal to the hackingTime config variable
		hack.target = target;
		Hk::Client::PlaySoundEffect(client, CreateID("ui_new_story_star"));
		Hk::Client::PlaySoundEffect(client, CreateID("ui_begin_scan"));

		// Start the Hack, sending a message to everyone within the hackingStartedMessage radius.
		auto hackerName = Hk::Client::GetCharacterNameByID(client).value();
		auto hackerPos = Hk::Solar::GetLocation(client, IdType::Client).value().first;
		auto hackerSystem = Hk::Solar::GetSystemBySpaceId(target).value();
		auto sectorCoordinate = Hk::Math::VectorToSectorCoord<std::wstring>(hackerSystem, hackerPos);
		auto formattedStartHackMessage = std::vformat(global->config->hackingStartedMessage, std::make_wformat_args(hackerName, sectorCoordinate));
		PrintLocalUserCmdText(client, formattedStartHackMessage, global->config->hackingMessageRadius);

		// Spawns a defined Guard NPC when the hack starts within 3-4km of the hacker.
		static std::random_device dev;
		static std::mt19937 engine = std::mt19937(dev());
		static auto dist = std::uniform_int_distribution(0, 1);
		auto randomDist = dist(engine) ? 1000.f : -1000.f;
		const auto spawnGuardNPCs = [hackerPos, hackerSystem, randomDist](const std::wstring& guardNpc) {
			Vector guardNpcPos = {{hackerPos.x + randomDist}, {hackerPos.y + randomDist}, {hackerPos.z + randomDist}};
			Matrix defaultNpcRot = {{{1.f, 0, 0}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}};
			return global->npcCommunicator->CreateNpc(guardNpc, guardNpcPos, defaultNpcRot, hackerSystem, true);
		};

		auto groupSize = Hk::Player::GetGroupMembers(client).value();

		// Get the solar's reputation
		int solarReputation;
		pub::SpaceObj::GetRep(hack.target, solarReputation);
		uint solarFaction;
		pub::Reputation::GetAffiliation(solarReputation, solarFaction);

		// TODO: This is still failing, think you're comparing the wrong values. guardNpcMap is showing short, apparantly correct values, solarfaction might
		// need further processing. Use full CreateId and uints
		if (!global->guardNpcMap.contains(solarFaction))
		{
			// TODO: Improve this error
			AddLog(LogType::Normal, LogLevel::Err, "No defined NPCs belong to a faction nickname");
			return;
		}

		auto& npcShipList = global->guardNpcMap.at(solarFaction);

		for (auto i = 0; i < groupSize.size(); i++)
		{
			// TODO: Pick randomly from ships listed in appropriate vector

			int listSize = npcShipList.size();
			static std::random_device dev;
			static auto engine = std::mt19937(dev());
			static auto dist = std::uniform_int_distribution(0, listSize);
			auto randRes = dist(engine);

			auto& npcToSpawn = npcShipList[randRes];

			// TODO: Make a config/map for what can spawn here per faction.
			hack.spawnedNpcList.emplace_back(spawnGuardNPCs(stows(npcToSpawn)));
			hack.spawnedNpcList.emplace_back(spawnGuardNPCs(stows(npcToSpawn)));
		}

		PrintUserCmdText(client,
		    std::format(L"You have started a hack, remain within {:.0f}m of the target for {} seconds in order to complete successful data retrieval.",
		        (global->config->hackingRadius),
		        global->config->hackingTime));

		// Start the attached FX for the player and the hack
		auto playerShip = Hk::Player::GetShip(client).value();
		IObjInspectImpl* inspect;
		uint iDunno;

		GetShipInspect(playerShip, inspect, iDunno);
		Hk::Admin::LightFuse((IObjRW*)inspect, CreateID("bm_hack_ship_fuse"), 0.f, 5.f, 0);

		// Sets the relative health of the target to half in order to play the hacking radius FX fuse. This is horrible, but seems to be our only option.
		pub::SpaceObj::SetRelativeHealth(target, 0.5f);

		// Set the target solar hostile to the player temporarily.
		int reputation;
		pub::Player::GetRep(client, reputation);
		int npcReputation;
		pub::SpaceObj::GetRep(target, npcReputation);
		pub::Reputation::SetAttitude(npcReputation, reputation, -0.9f);

		// Start the Hack timer
		hack.time = global->config->hackingTime;

		// TODO: Prevent other players from attempting to hack the satellite at the same time.
		// TODO: Satellite cycling per system
		// TODO: Reward for players that kill offending player
	}

	// Define usable chat commands here
	const std::vector commands = {{
	    CreateUserCommand(L"/hack", L"", userCmdStartHack, L"Starts a hacking session against a valid solar."),
	}};

} // namespace Plugins::Hacking

// namespace Plugins::Template

using namespace Plugins::Hacking;

// TODO: Add GuardNpcMap to the REFLs
// Generates the JSON file
REFL_AUTO(type(Config), field(hackableSolarArchetype), field(hackingStartedMessage), field(hackingFinishedMessage), field(hackingMessageRadius),
    field(hackingTime), field(rewardCashMin), field(rewardCashMax), field(hackingTime), field(guardNpcPersistTime));

DefaultDllMainSettings(LoadSettings);

const std::vector<Timer> timers = {{hackTimerTick, 5}};
extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	// Full name of your plugin
	pi->name("Hacking");
	// Shortened name, all lower case, no spaces. Abbreviation when possible.
	pi->shortName("Hacking");
	pi->mayUnload(true);
	pi->commands(&commands);
	pi->timers(&timers);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
}