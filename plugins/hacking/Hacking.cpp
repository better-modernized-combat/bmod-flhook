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
	const auto global = std::make_unique<Global>();

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
		for (const auto& [key, value] : global->config->guardNpcMap)
		{
			global->guardNpcMap[MakeId(key.c_str())] = value;
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
	}

	// Function: Cleans up after an objective is complete, removing any NPCs and resetting any active timers or fuses
	void CleanUp(HackInfo& info)
	{
		pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
		info.spawnedNpcList.clear();
		info.target = 0;
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
		if (Hk::Math::Distance3D(playerPos.value().first, solarPos.value().first) < distance)
		{
			return true;
		}
		return false;
	}

	// Function: Spawns ships between min and max of a given type for a given faction. Picks randomly from the list for that faction defined in the config
	void SpawnRandomShips(const Vector& pos, uint system, uint client, HackInfo& hack, uint solarFaction)
	{
		auto randomDist = RandomNumber(0, 1) ? 1000.f : -1000.f;
		const auto spawnGuardNPCs = [pos, system, randomDist](const std::wstring& guardNpc) {
			Vector guardNpcPos = {{pos.x + randomDist}, {pos.y + randomDist}, {pos.z + randomDist}};
			Matrix defaultNpcRot = {{{1.f, 0, 0}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}};
			return global->npcCommunicator->CreateNpc(guardNpc, guardNpcPos, defaultNpcRot, system, true);
		};

		auto groupSize = Hk::Player::GetGroupMembers(client).value();

		// Iterates over global->guardNpcMap and checks to see if it contains the faction belonging to solarFaction
		if (!global->guardNpcMap.contains(solarFaction))
		{
			AddLog(LogType::Normal, LogLevel::Err, std::format("No defined NPCs belong to faction {}", solarFaction));
			return;
		}

		auto& npcShipList = global->guardNpcMap.at(solarFaction);

		for (const auto& _ : groupSize)
		{
			for (int i = 0; i < RandomNumber(global->config->minNpcGuards, global->config->maxNpcGuards); i++)
			{
				auto randRes = RandomNumber(0, npcShipList.size() - 1);
				const auto& npcToSpawn = npcShipList[randRes];
				hack.spawnedNpcList.emplace_back(spawnGuardNPCs(stows(npcToSpawn)));
			}
		}
	}

	// Function: This function is called when an initial objective is completed.
	void InitialObjectiveComplete(const HackInfo& info, uint client)
	{
		if (!info.time)
		{
			// Grab all the information needed to populate the proximity message sent to the players:
			auto hackerEndName = Hk::Client::GetCharacterNameByID(client).value();
			auto hackerEndPos = Hk::Solar::GetLocation(client, IdType::Client).value().first;
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
			PrintLocalUserCmdText(client, formattedEndHackMessage, global->config->hackingMessageRadius);

			// Adjust the hacker's reputation with the owning faction by -0.05
			auto hackerOriginalRep = Hk::Player::GetRep(client, npcFaction);
			auto hackerNewRep = hackerOriginalRep.value() - 0.05f;
			int playerRepInstance;
			pub::Player::GetRep(client, playerRepInstance);
			pub::Reputation::SetReputation(playerRepInstance, npcFaction, hackerNewRep);

			// Pick a number between rewardCashMin and rewardCashMax and and credit the player that amount for completion of the hack.
			auto randomCash = RandomNumber(global->config->rewardCashMin, global->config->rewardCashMax);
			Hk::Player::AddCash(client, randomCash);

			// Grab all the information needed to populate the private message to the player.
			// TODO: Populate these fields properly
			Vector rewardPos = {{200.f}, {200.f}, {200.f}};
			auto rewardSystem = "Li03";
			auto rewardSector = Hk::Math::VectorToSectorCoord<std::wstring>(CreateID(rewardSystem), rewardPos);

			// Send a private message to the player notifying them of their randomCash reward and POI location.
			auto formattedHackRewardMessage =
			    std::format(L"{} credits diverted from local financial ansibles, a point of interest has been revealed in sector {} ({:.0f}, {:.0f}, {:.0f}).",
			        randomCash,
			        rewardSector,
			        rewardPos.x,
			        rewardPos.y,
			        rewardPos.z);
			PrintUserCmdText(client, formattedHackRewardMessage);

			// Play some sounds to telegraph the successful completion of the hack.
			Hk::Client::PlaySoundEffect(client, CreateID("ui_receive_money"));
			Hk::Client::PlaySoundEffect(client, CreateID("ui_end_scan"));
			UnLightShipFuse(client, global->config->shipFuse);
			// Turn off the solar fuse:
			pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
		}
	}

	// Function: handles events that occur after the initial objective has been started and events that occur as the completion timer counts down.
	void ObjectiveTimerTick()
	{
		// TODO: put in warning for non multiple of 5 values with these times
		// Start the timer
		for (auto client = 0u; client != global->activeHacks.size(); client++)
		{
			auto& info = global->activeHacks[client];
			info.time -= 5;
			if (info.time < 0)
			{
				if (info.time == (global->config->guardNpcPersistTime * -1))
				{
					for (auto obj : info.spawnedNpcList)
					{
						// TODO: Make the NPCs here really cloak rather than just despawning
						pub::SpaceObj::Destroy(obj, VANISH);
						Hk::Client::PlaySoundEffect(client, CreateID("cloak_rh_fighter"));
					}
					CleanUp(info);
				}
				continue;
			}

			// Checks if the player has a ship before proceeding. This handles disconnects, crashing and docking.
			if (Hk::Player::GetShip(client).has_error())
			{
				// Notify the player, play an audio cue, turn off the fuse and reset the timer, failing the objective.
				PrintUserCmdText(client, L"You have left the area, the hack has failed.");
				Hk::Client::PlaySoundEffect(client, CreateID("ui_select_remove"));
				pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
				info.time = 0;
				for (auto& i : global->solars | std::views::values)
				{
					i.rotatingSolars[i.currentIndex].isHacking = false;
				}
				continue;
			}

			// Check if the player is in range of the solar while the hack is ongoing. There's a grace period for leaving the area where the player is warned.
			if (!IsInSolarRange(client, info.target, global->config->hackingSustainRadius))
			{
				// Set the target's relative Health to half, triggering the effect fuse
				pub::SpaceObj::SetRelativeHealth(info.target, 0.4f);

				if (info.beenWarned)
				{
					// Notify the player, play an audio cue, turn off the fuses, reset the timer and the warning flag, failing the objective.
					PrintUserCmdText(client, L"You're out of range, the hack has failed.");
					Hk::Client::PlaySoundEffect(client, CreateID("ui_select_remove"));
					UnLightShipFuse(client, global->config->shipFuse);
					pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
					info.time = 0;
					info.beenWarned = false;
					for (auto& i : global->solars | std::views::values)
					{
						i.rotatingSolars[i.currentIndex].isHacking = false;
					}
				}
				else
				{
					// Warn the player they're out of range for a cycle and set the beenWarned variable.
					PrintUserCmdText(client, L"Warning, you are moving out of range, the hack will fail if you don't get closer.");
					// Play a sound effect for the player to telegraph that they're leaving the area.
					Hk::Client::PlaySoundEffect(client, CreateID("ui_filter_operation"));
					info.beenWarned = true;
					pub::SpaceObj::SetRelativeHealth(info.target, 0.4f);
				}

				continue;
			}

			// Reset the beenWarned variable when the hack ends or fails.
			info.beenWarned = false;

			// Complete the hack when the timer finishes.
			InitialObjectiveComplete(info, client);
		}
	}

	// Function: Ticks regularly and cycles through the possible objective targets.
	void GlobalTimerTick()
	{
		for (auto& list : global->solars | std::views::values)
		{
			if (std::ranges::find_if(global->activeHacks,
			        [list](const HackInfo& item) { return item.target == list.rotatingSolars[list.currentIndex].solar; }) == global->activeHacks.end())
			{
				pub::SpaceObj::SetRelativeHealth(list.rotatingSolars[list.currentIndex].solar, 1.f);
			}

			list.currentIndex++;
			AddLog(LogType::Normal, LogLevel::Debug, std::format("Cycling a list hack target to {}", list.rotatingSolars[list.currentIndex].solar));
			if (list.currentIndex == list.rotatingSolars.size())
			{
				list.currentIndex = 0;
			}
			pub::SpaceObj::SetRelativeHealth(list.rotatingSolars[list.currentIndex].solar, 0.6f);
			list.rotatingSolars[list.currentIndex].isHacked = false;
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
			if (i.rotatingSolars[i.currentIndex].isHacking == true)
			{
				PrintUserCmdText(client, L"Someone else is already attempting to hack this target.");
				return;
			}
			if (i.rotatingSolars[i.currentIndex].isHacked == true)
			{
				PrintUserCmdText(client, L"Someone has recently hacked this target and it has been secured.");
				return;
			}

			if (i.rotatingSolars[i.currentIndex].solar == target)
			{
				solarFound = true;
				break;
			}
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
		auto hackerName = Hk::Client::GetCharacterNameByID(client).value();
		auto hackerPos = Hk::Solar::GetLocation(client, IdType::Client).value().first;
		auto hackerSystem = Hk::Solar::GetSystemBySpaceId(target).value();
		auto sectorCoordinate = Hk::Math::VectorToSectorCoord<std::wstring>(hackerSystem, hackerPos);
		auto formattedStartHackMessage = std::vformat(global->config->hackingStartedMessage, std::make_wformat_args(hackerName, sectorCoordinate));
		PrintLocalUserCmdText(client, formattedStartHackMessage, global->config->hackingMessageRadius);

		// Get the solar's reputation
		int solarReputation;
		pub::SpaceObj::GetRep(hack.target, solarReputation);
		uint solarFaction;
		pub::Reputation::GetAffiliation(solarReputation, solarFaction);

		SpawnRandomShips(hackerPos, hackerSystem, client, hack, solarFaction);

		PrintUserCmdText(client,
		    std::format(L"You have started a hack, remain within {:.0f}m of the target for {} seconds in order to complete successful data retrieval.",
		        (global->config->hackingSustainRadius),
		        global->config->hackingTime));

		// Start the attached FX for the player and the hack
		LightShipFuse(client, global->config->shipFuse);
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
	}

	// Define usable chat commands here
	const std::vector commands = {{
	    CreateUserCommand(L"/hack", L"", userCmdStartHack, L"Starts a hacking session against a valid solar."),
	}};

} // namespace Plugins::Hacking

using namespace Plugins::Hacking;

// Generates the JSON file
REFL_AUTO(type(Config), field(hackableSolarArchetype), field(hackingStartedMessage), field(hackingFinishedMessage), field(hackingMessageRadius),
    field(hackingTime), field(rewardCashMin), field(rewardCashMax), field(hackingTime), field(guardNpcPersistTime), field(minNpcGuards), field(maxNpcGuards),
    field(hackingSustainRadius), field(hackingInitiateRadius), field(guardNpcMap), field(initialObjectiveSolars), field(useFuses), field(shipFuse));

DefaultDllMainSettings(LoadSettings);

const std::vector<Timer> timers = {{ObjectiveTimerTick, 5}, {GlobalTimerTick, 240}};
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