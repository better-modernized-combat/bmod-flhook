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

	// Put things that are performed on plugin load here!
	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));
		// If the config file is missing values, don't allow users to interact with the plugin
		if (global->config->hackableSolarArchetype.empty() || global->config->hackingStartedMessage.empty() || global->config->hackingFinishedMessage.empty() ||
		    !global->config->hackingMessageRadius)
		{
			global->pluginActive = false;
		}
		global->targetHash = CreateID(global->config->hackableSolarArchetype.c_str());
		global->npcCommunicator =
		    static_cast<Plugins::Npc::NpcCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Plugins::Npc::NpcCommunicator::pluginName));
		if (!global->npcCommunicator)
		{
			global->pluginActive = false;
			AddLog(LogType::Normal, LogLevel::Err, "NPC.dll must be loaded for this plugin to function.");
		}
	}

	//  Return true if this player is within fDistance of the target solar.
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
		auto pos2 = res2.value().first;

		// Check if the player is within fDistance of the target solar.
		if (Hk::Math::Distance3D(pos, pos2) < fDistance)
		{
			return true;
		}
		return false;
	}

	void timerTick()
	{
		for (auto i = 0u; i != global->activeHacks.size(); i++)
		{
			auto& info = global->activeHacks[i];
			info.time -= 5;
			if (info.time < 0)
			{
				continue;
			}

			// Checks if the player has a ship before proceeding. This handles disconnects, crashing and docking. It should also handle the player leaving the
			// system, but I've not tested it.
			if (Hk::Player::GetShip(i).has_error())
			{
				PrintUserCmdText(i, L"You have left the area, the hack has failed.");
				Hk::Client::PlaySoundEffect(i, CreateID("bm_hack_failed"));
				info.time = 0;
				continue;
			}

			//  Check if the player is in range of the solar while the hack is ongoing.
			if (!IsInSolarRange(i, info.target, global->config->hackingRadius))
			{
				if (info.beenWarned)
				{
					PrintUserCmdText(i, L"You're out of range, the hack has failed.");
					info.time = 0;
					auto playerShip = Hk::Player::GetShip(i).value();
					IObjInspectImpl* inspect;
					uint iDunno;
					GetShipInspect(playerShip, inspect, iDunno);
					Hk::Admin::UnLightFuse((IObjRW*)inspect, CreateID("bm_hack_ship_fuse"));
					Hk::Client::PlaySoundEffect(i, CreateID("bm_hack_failed"));
				}
				else
				{
					PrintUserCmdText(i, L"Warning, you are moving out of range, the hack will fail if you persist.");
					Hk::Client::PlaySoundEffect(i, CreateID("bm_hack_warn"));
					info.beenWarned = true;
				}

				continue;
			}

			// Reset the beenWarned variable
			info.beenWarned = false;

			// Complete the hack when the timer finishes.
			if (!info.time)
			{
				// TODO: Decrement the owning faction's reputation slightly on success.
				// TODO: Togggle off the 'active hack' effect.
				// TODO: Have any spawned NPCs 'cloak' and despawn a set time after the hack has finished.
				// TODO: Toggle off the kill reward state on the initiating player.
				// TODO: Dynamic elements of the success message.

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

				auto formattedEndHackMessage =
				    std::vformat(global->config->hackingFinishedMessage, std::make_wformat_args(hackerEndName, sectorEndCoordinate, realName));
				PrintLocalUserCmdText(i, formattedEndHackMessage, global->config->hackingMessageRadius);

				// PrintLocalUserCmdText(i, global->config->hackingFinishedMessage, global->config->hackingMessageRadius);

				Hk::Client::PlaySoundEffect(i, CreateID("ui_receive_money"));
				Hk::Client::PlaySoundEffect(i, CreateID("ui_end_scan"));
				auto playerShip = Hk::Player::GetShip(i).value();
				IObjInspectImpl* inspect;
				uint iDunno;
				GetShipInspect(playerShip, inspect, iDunno);
				Hk::Admin::UnLightFuse((IObjRW*)inspect, CreateID("bm_hack_ship_fuse"));

				static std::random_device dev;
				static std::mt19937 engine = std::mt19937(dev());
				static auto dist = std::uniform_int_distribution(global->config->rewardCashMin, global->config->rewardCashMax);
				auto randomCash = dist(engine);
				Hk::Player::AddCash(i, randomCash);

				// TODO: Populate these fields properly
				Vector rewardPos = {{200.f}, {200.f}, {200.f}};
				auto rewardSystem = "Li03";
				auto rewardSector = Hk::Math::VectorToSectorCoord<std::wstring>(CreateID(rewardSystem), rewardPos);

				auto formattedHackRewardMessage = std::format(
				    L"{} credits diverted from local financial ansibles, a point of interest has been revealed in sector {} ({:.0f}, {:.0f}, {:.0f}).",
				    randomCash,
				    rewardSector,
				    rewardPos.x,
				    rewardPos.y,
				    rewardPos.z);
				PrintUserCmdText(i, formattedHackRewardMessage);

				// TODO: Put this on a confirgurable timer
				for (auto obj : info.spawnedNpcList)
				{
					pub::SpaceObj::Destroy(obj, VANISH);
					Hk::Client::PlaySoundEffect(i, CreateID("cloak_rh_fighter"));
				}

				// TODO: Send any ephemeral rewards to the successful player privately.
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

		// Start the Hack, sending a message to everyone within the hackingStartedMessage radius.
		auto hackerName = Hk::Client::GetCharacterNameByID(client).value();
		auto hackerPos = Hk::Solar::GetLocation(client, IdType::Client).value().first;
		auto hackerSystem = Hk::Solar::GetSystemBySpaceId(target).value();
		auto sectorCoordinate = Hk::Math::VectorToSectorCoord<std::wstring>(hackerSystem, hackerPos);
		auto formattedStartHackMessage = std::vformat(global->config->hackingStartedMessage, std::make_wformat_args(hackerName, sectorCoordinate));
		PrintLocalUserCmdText(client, formattedStartHackMessage, global->config->hackingMessageRadius);

		// TODO: Get number of group members for hacker and spawn 2 NPCs per
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
		auto& info = global->activeHacks[client];
		for (auto i = 0; i < groupSize.size(); i++)
		{
			info.spawnedNpcList.emplace_back(spawnGuardNPCs(L"l_defender"));
			info.spawnedNpcList.emplace_back(spawnGuardNPCs(L"l_defender"));
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

		// TODO: Attach and play a VFX on the object as it is being hacked.
		// GetShipInspect(target, inspect, iDunno);
		// Hk::Admin::LightFuse((IObjRW*)inspect, CreateID("bm_hack_ship_fuse"), 0.f, 5.f, 0);

		// Set the target solar hostile to the player.
		int reputation;
		pub::Player::GetRep(client, reputation);
		int npcReputation;
		pub::SpaceObj::GetRep(target, npcReputation);
		pub::Reputation::SetAttitude(npcReputation, reputation, -0.9f);

		// Start a Timer equal to the hackingTime config variable
		hack.time = global->config->hackingTime;
		hack.target = target;
		Hk::Client::PlaySoundEffect(client, CreateID("ui_new_story_star"));
		Hk::Client::PlaySoundEffect(client, CreateID("ui_begin_scan"));

		// TODO: Prevent other players from attempting to hack the satellite at the same time.
		// TODO: Satellite cycling per system
		// TODO: Rewards, coordinates, small random amount of credits
		// TODO: Reduce rep on successful hack
		// TODO: Play Comms, music, etc
		// TODO: Reward for players that kill offending player
	}

	// Define usable chat commands here
	const std::vector commands = {{
	    CreateUserCommand(L"/hack", L"", userCmdStartHack, L"Starts a hacking session against a valid solar."),
	}};

} // namespace Plugins::Hacking

// namespace Plugins::Template

using namespace Plugins::Hacking;

REFL_AUTO(type(Config), field(hackableSolarArchetype), field(hackingStartedMessage), field(hackingFinishedMessage), field(hackingMessageRadius),
    field(hackingTime), field(rewardCashMin), field(rewardCashMax), field(hackingTime));

DefaultDllMainSettings(LoadSettings);

const std::vector<Timer> timers = {{timerTick, 5}};
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