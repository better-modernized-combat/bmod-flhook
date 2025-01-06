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

namespace Plugins::Triggers
{
	const auto global = std::make_unique<Global>();

	static int GetRandomNumber(int min, int max)
	{
		auto range = std::uniform_int_distribution(min, max);
		return range(global->randomEngine);
	}

	static int GetRandomWeight(const std::vector<int>& weights)
	{
		std::discrete_distribution<> dist(weights.begin(), weights.end());
		auto weightIndex = dist(global->randomEngine);
		return weightIndex;
	}

	static void LightShipFuse(uint client, const std::string& fuse)
	{
		auto playerShip = Hk::Player::GetShip(client).value();
		IObjRW* inspect;
		StarSystem* iDunno;

		GetShipInspect(playerShip, inspect, iDunno);
		Hk::Admin::LightFuse(inspect, CreateID(fuse.c_str()), 0.f, 5.f, 0);
	}

	static void UnLightShipFuse(uint client, const std::string& fuse)
	{
		auto playerShip = Hk::Player::GetShip(client).value();
		IObjRW* inspect;
		StarSystem* iDunno;

		GetShipInspect(playerShip, inspect, iDunno);
		Hk::Admin::UnLightFuse(inspect, CreateID(fuse.c_str()));
	}

	static bool ClientIsInRangeOfSolar(ClientId client, uint solar, float distance)
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

	static void LoadSettings()
	{
		std::random_device dev;
		global->randomEngine = std::mt19937 {dev()};

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

		// Preprocess the terminal list so we don't have to hash it each time.
		for (auto& group : global->config->terminalGroups)
		{
			RuntimeTerminalGroup runtimeGroup;
			runtimeGroup.data = &group;
			for (const auto& terminal : group.terminalList)
			{
				runtimeGroup.terminalList.emplace_back(CreateID(terminal.c_str()));
			}
			global->runtimeGroups.emplace_back(runtimeGroup);
		}
	}

	/** @ingroup Triggers
	 * @brief Creates a point of interest and it's accompanying NPCs if there are any defined.
	 */
	static void CreatePoiEvent(const Event& event, Position& position)
	{
		Vector pos = {position.coordinates[0], position.coordinates[1], position.coordinates[2]};
		Matrix mat = EulerMatrix({0.f, 0.f, 0.f});

		for (const auto& object : global->solarCommunicator->CreateSolarFormation(event.solarFormation, pos, CreateID(position.system.c_str())))
		{
			SpawnedObject spawnedObject;
			spawnedObject.spaceId = object;
			spawnedObject.despawnTime = Hk::Time::GetUnixSeconds() + event.lifetimeInSeconds;
			global->spawnedObjects.emplace_back(spawnedObject);
			spawnedObject.position = &position;
		}

		for (const auto& npcs : event.npcs)
		{
			// Spawn the NPC, calculcate it's despawn time, and emplace the information back into the global tracking vector.
			SpawnedObject spawnedObject;
			spawnedObject.spaceId = global->npcCommunicator->CreateNpc(npcs.first, pos, mat, CreateID(position.system.c_str()), true);
			spawnedObject.despawnTime = Hk::Time::GetUnixSeconds() + event.lifetimeInSeconds;

			global->spawnedObjects.emplace_back(spawnedObject);
		}
	}

	/** @ingroup Triggers
	 * @brief Completes a terminal interaction, rewards the player and spawns a random event selected from the appropriate pool
	 */
	static void CompleteTerminalInteraction(RuntimeTerminalGroup& group)
	{
		auto& eventFamilyList = group.currentTerminalIsLawful ? group.data->eventFamilyUseList : group.data->eventFamilyHackList;

		if (group.currentTerminalIsLawful && group.data->useCostInCredits > Hk::Player::GetCash(group.activeClient).value())
		{
			PrintUserCmdText(group.activeClient, L"You no longer have enough credits to complete this transaction. Data retrieval has been cancelled.");
			group.lastActivatedTime = 0;
			group.activeClient = 0;
			return;
		}
		if (group.currentTerminalIsLawful)
		{
			PrintUserCmdText(group.activeClient, std::format(L"Your account has been charged {} credits.", group.data->useCostInCredits));
			Hk::Player::AdjustCash(group.activeClient, -group.data->useCostInCredits);
			Hk::Client::PlaySoundEffect(group.activeClient, CreateID("ui_execute_transaction"));
		}
		else
		{
			auto randomCash = GetRandomNumber(group.data->minHackRewardInCredits, group.data->maxHackRewardInCredits);
			Hk::Player::AddCash(group.activeClient, randomCash);

			PrintUserCmdText(group.activeClient, std::format(L"{} credits diverted from local financial ansibles", randomCash));
			Hk::Client::PlaySoundEffect(group.activeClient, CreateID("ui_receive_money"));
		}

		std::vector<int> familyWeights;
		for (const auto& eventFamily : eventFamilyList)
		{
			familyWeights.emplace_back(eventFamily.spawnWeight);
		}
		auto& family = eventFamilyList[GetRandomWeight(familyWeights)];

		std::vector<int> eventWeights;
		for (const auto& event : family.eventList)
		{
			eventWeights.emplace_back(event.spawnWeight);
		}
		auto& event = family.eventList[GetRandomWeight(eventWeights)];

		//  Select a random position
		Position* position = nullptr;
		int counter = 0;
		const auto unix = Hk::Time::GetUnixSeconds();
		do
		{
			position = &family.spawnPositionList[GetRandomNumber(0, family.spawnPositionList.size() - 1)];

			if (counter++ > 30)
			{
				Console::ConErr(std::format("Unable to find a valid spawn position for {}. Please check your config has an appropriate number of spawn "
				                            "locations defined for this family.",
				    family.name));
				PrintUserCmdText(group.activeClient, L"Unable to find a point of interest, any credits spent have been refunded.");

				if (group.currentTerminalIsLawful)
				{
					Hk::Player::AdjustCash(group.activeClient, group.data->useCostInCredits);
				}
				return;
			}

			if (!position->despawnTime)
			{
				position->despawnTime = unix;
			}

		} while (position->despawnTime != unix);

		std::wstring rewardSectorMessage = Hk::Math::VectorToSectorCoord<std::wstring>(
		    CreateID(position->system.c_str()), Vector {position->coordinates[0], position->coordinates[1], position->coordinates[2]});

		Console::ConDebug(std::format("Spawning the event '{}' at {},{},{} in {}",
		    event.name,
		    position->coordinates[0],
		    position->coordinates[1],
		    position->coordinates[2],
		    position->system));

		CreatePoiEvent(event, *position);

		auto timeInMinutes = event.lifetimeInSeconds / 60;

		PrintUserCmdText(
		    group.activeClient, std::vformat(event.descriptionMedInfo, std::make_wformat_args(rewardSectorMessage, timeInMinutes)));
		Hk::Client::PlaySoundEffect(group.activeClient, CreateID("ui_end_scan"));

		//  Fetch the terminal's reputation and affiliation values
		int terminalReputation;
		pub::SpaceObj::GetRep(group.currentTerminal, terminalReputation);
		uint terminalAffiliation;
		pub::Reputation::GetAffiliation(terminalReputation, terminalAffiliation);

		if (!group.currentTerminalIsLawful)
		{
			uint npcFactionIds;
			pub::Reputation::GetGroupName(terminalAffiliation, npcFactionIds);

			auto terminalName = stows(group.data->terminalName);
			auto factionName = Hk::Message::GetWStringFromIdS(npcFactionIds);
			PrintLocalUserCmdText(group.activeClient,
			    std::vformat(global->config->messageHackFinishNotifyAll,
			        std::make_wformat_args(Hk::Client::GetCharacterNameByID(group.activeClient).value(),
			            terminalName,
			            rewardSectorMessage,
			            factionName)),
			    global->config->terminalNotifyAllRadiusInMeters);
		}

		// Get the IDS Name for the faction We use this in several messages.
		uint npcFactionShortIds;
		pub::Reputation::GetShortGroupName(terminalAffiliation, npcFactionShortIds);

		auto terminalId = std::to_wstring(group.currentTerminal).substr(0, 2);
		auto timestampPart = std::format("{:%H%M}", std::chrono::system_clock::now());

		constexpr char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		char randomLetter = letters[GetRandomNumber(0, 25)];

		auto account = Hk::Client::GetAccountByClientID(group.activeClient);
		Trace trace {};

		trace.packetId = std::format(L"{}-{}{}-{}", Hk::Message::GetWStringFromIdS(npcFactionShortIds), randomLetter, terminalId, stows(timestampPart));
		trace.despawnTime = Hk::Time::GetUnixSeconds() + event.lifetimeInSeconds;
		trace.traceLocation = rewardSectorMessage;
		trace.traceName = stows(event.name);
		trace.playerName = Hk::Client::GetCharacterNameByID(group.activeClient).value();

		PrintUserCmdText(group.activeClient, std::format(L"Adding data packet {} to local storage.", trace.packetId));

		auto emplacement = global->traceMap.try_emplace(account);
		emplacement.first->second.emplace_back(trace);
	}

	static bool HandleDisconnect(RuntimeTerminalGroup& group)
	{
		// Checks if the player has a ship before proceeding. This handles disconnects, crashing and docking.
		if (Hk::Player::GetShip(group.activeClient).has_value())
		{
			return false;
		}

		// Notify the player, play an audio cue, turn off the fuse and reset the timer, failing the objective.
		PrintUserCmdText(group.activeClient, L"You have left the area, the hack has failed.");
		Hk::Client::PlaySoundEffect(group.activeClient, CreateID("ui_select_remove"));
		pub::SpaceObj::SetRelativeHealth(group.currentTerminal, 1.f);
		group.activeClient = 0;
		group.lastActivatedTime = 0;
		return true;
	}

	static bool HandleOutOfRange(RuntimeTerminalGroup& group)
	{
		if (ClientIsInRangeOfSolar(group.activeClient, group.currentTerminal, global->config->terminalSustainRadiusInMeters))
		{
			return false;
		}

		// Set the target's relative Health to half, triggering the effect fuse
		pub::SpaceObj::SetRelativeHealth(group.currentTerminal, 0.4f);

		if (group.playerHasBeenWarned)
		{
			// Notify the player, play an audio cue, turn off the fuses, reset the timer and the warning flag, failing the objective.
			PrintUserCmdText(group.activeClient, L"You're out of range, the hack has failed.");
			Hk::Client::PlaySoundEffect(group.activeClient, CreateID("ui_select_remove"));
			UnLightShipFuse(group.activeClient, global->config->shipActiveTerminalFuse);
			pub::SpaceObj::SetRelativeHealth(group.currentTerminal, 1.f);
			group.activeClient = 0;
			group.lastActivatedTime = 0;
			group.playerHasBeenWarned = false;
			return true;
		}

		// Warn the player they're out of range for a cycle and set the beenWarned variable.
		PrintUserCmdText(group.activeClient, L"Warning, you are moving out of range, the hack will fail if you don't get closer.");

		// Play a sound effect for the player to telegraph that they're leaving the area.
		Hk::Client::PlaySoundEffect(group.activeClient, CreateID("ui_filter_operation"));
		pub::SpaceObj::SetRelativeHealth(group.currentTerminal, 0.4f);

		group.playerHasBeenWarned = true;
		return true;
	}

	static void ProcessActiveTerminal(RuntimeTerminalGroup& group)
	{
		if (!group.activeClient)
		{
			// If there's no active client, simply don't proceed
			return;
		}

		if (HandleDisconnect(group))
		{
			// All we need to do here is return, as everything is handled in the function.
			return;
		}

		if (HandleOutOfRange(group))
		{
			// All we need to do here is return, as everything is handled in the function.
			return;
		}

		if (Hk::Time::GetUnixSeconds() >=
		    group.lastActivatedTime + (group.currentTerminalIsLawful ? group.data->useTimeInSeconds : group.data->hackTimeInSeconds))
		{
			CompleteTerminalInteraction(group);
			pub::SpaceObj::SetRelativeHealth(group.currentTerminal, 1.f);
			UnLightShipFuse(group.activeClient, global->config->shipActiveTerminalFuse);
			group.activeClient = 0;
			group.lastActivatedTime = Hk::Time::GetUnixSeconds();
			return;
		}
	}

	static void TerminalInteractionTimer()
	{
		// Loops over the active terminals every 5 seconds and processes them.
		for (auto& group : global->runtimeGroups)
		{
			ProcessActiveTerminal(group);
		}
	}

	static void CleanupTimer()
	{
		auto currentTime = Hk::Time::GetUnixSeconds();

		for (auto object = global->spawnedObjects.begin(); object != global->spawnedObjects.end();)
		{
			if (currentTime > object->despawnTime)
			{
				object->position->despawnTime = 0;
				pub::SpaceObj::Destroy(object->spaceId, VANISH);
				object = global->spawnedObjects.erase(object);
			}
			else
			{
				++object;
			}
		}
	}

	static void SavePlayerConfigToJson(CAccount* account)
	{
		auto settingsPath = Hk::Client::GetAccountDirName(account);
		char dataPath[MAX_PATH];
		GetUserDataPath(dataPath);
		Serializer::SaveToJson(global->playerConfigs[account], std::format("{}\\Accts\\MultiPlayer\\{}\\triggers.json", dataPath, wstos(settingsPath)));
	}

	static void LoadPlayerConfigFromJson(CAccount* account)
	{
		auto settingsPath = Hk::Client::GetAccountDirName(account);
		char dataPath[MAX_PATH];
		GetUserDataPath(dataPath);
		auto settings = Serializer::JsonToObject<PlayerConfig>(std::format("{}\\Accts\\MultiPlayer\\{}\\triggers.json", dataPath, wstos(settingsPath)), true);
		global->playerConfigs[account] = settings;
	}

	static void OnLogin([[maybe_unused]] struct SLoginInfo const& login, ClientId& client)
	{
		auto account = Hk::Client::GetAccountByClientID(client);
		auto accountId = account->wszAccId;
		LoadPlayerConfigFromJson(account);
		AddLog(LogType::Normal, LogLevel::Debug, std::format("Loading settings for {} from stored json file...", wstos(accountId)));
	}

	static void UserCmdShowTraces(ClientId& client)
	{
		auto account = Hk::Client::GetAccountByClientID(client);
		if (global->traceMap[account].empty())
		{
			PrintUserCmdText(client, L"You have no traces stored in your ship's databank right now.");
			return;
		}

		PrintUserCmdText(client, L"");
		auto& traceMap = global->traceMap[account];

		// Loop once initially to remove any expired traces
		for (auto iter = traceMap.begin(); iter != traceMap.end(); ++iter)
		{
			if (Hk::Time::GetUnixSeconds() >= iter->despawnTime)
			{
				global->traceMap[account].erase(iter);
				return;
			}
		}

		for (auto iter = traceMap.begin(); iter != traceMap.end(); ++iter)
		{
			std::wstring remainingTimeMessage;
			if (iter->despawnTime - Hk::Time::GetUnixSeconds() < 60)
			{
				remainingTimeMessage = L"less than 1 minute";
			}
			if (iter->despawnTime - Hk::Time::GetUnixSeconds() < 120)
			{
				remainingTimeMessage = L"1 minute";
			}
			else
			{
				remainingTimeMessage = std::format(L"{} minutes", (iter->despawnTime - Hk::Time::GetUnixSeconds()) / 60);
			}

			PrintUserCmdText(client,
			    std::format(L"PACKET ID: {}\nDETECTED: {}\nLOCATION: Sector {}\nTIME WINDOW: {}\n",
			        iter->packetId,
			        iter->traceName,
			        iter->traceLocation,
			        remainingTimeMessage));
		}
	}

	static void UserCmdTogglePlayerConfigs(ClientId& client, const std::wstring& param)
	{
		auto option = GetParam(param, L' ', 1);
		auto account = Hk::Client::GetAccountByClientID(client);

		if (option == L"togglehack")
		{
			global->playerConfigs[account].hackPrompt = !global->playerConfigs[account].hackPrompt;
			PrintUserCmdText(client, std::format(L"Toggled the hacking prompt: {}", global->playerConfigs[account].hackPrompt ? L"ON" : L"OFF"));
			SavePlayerConfigToJson(account);
		}
		else if (option == L"toggleuse")
		{
			global->playerConfigs[account].usePrompt = !global->playerConfigs[account].usePrompt;
			PrintUserCmdText(client, std::format(L"Toggled the use prompt: {}", global->playerConfigs[account].usePrompt ? L"ON" : L"OFF"));
			SavePlayerConfigToJson(account);
		}
		else
		{
			PrintUserCmdText(client, L"Configuration Options:");
			PrintUserCmdText(client,
			    std::format(L"'/terminal configure togglehack': Toggles the warning when attempting to hack a terminal with '/terminal hack': {}",
			        global->playerConfigs[account].hackPrompt ? L"ON" : L"OFF"));
			PrintUserCmdText(client,
			    std::format(L"'/terminal configure toggleuse': Toggles the warning when attempting to use a terminal with '/terminal use': {}",
			        global->playerConfigs[account].usePrompt ? L"ON" : L"OFF"));
		}
	}

	static void UserCmdStartTerminalInteraction(ClientId& client, const std::wstring& param)
	{
		// Check to make sure the plugin has loaded dependencies and settings.
		if (!global->pluginActive)
		{
			PrintUserCmdText(client, L"There was an error loading this plugin, please contact your server administrator.");
			return;
		}

		auto action = GetParam(param, L' ', 0);
		auto confirm = GetParam(param, L' ', 1);

		if (action == L"show")
		{
			UserCmdShowTraces(client);
			return;
		}

		if (action == L"configure")
		{
			UserCmdTogglePlayerConfigs(client, param);
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

		RuntimeTerminalGroup* group = nullptr;

		//  Check if this solar is on the list of available terminals
		for (auto& terminalGroup : global->runtimeGroups)
		{
			auto found = false;
			for (const auto& terminal : terminalGroup.terminalList)
			{
				if (terminal == target)
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
			PrintUserCmdText(client, L"The target you have selected is not a valid target for terminal interaction.");
			return;
		}

		// Check if the subcommand is valid
		auto isLawful = action == L"use";
		group->currentTerminalIsLawful = isLawful;
		if (!isLawful && action != L"hack")
		{
			PrintUserCmdText(client, L"Invalid terminal command, valid options are 'hack', 'use', 'show' and 'configure'.");
			return;
		}

		// Check if the player is within hackingInitiateRadius
		if (bool inRange = ClientIsInRangeOfSolar(client, target, global->config->terminalInitiateRadiusInMeters); !inRange)
		{
			PrintUserCmdText(client,
			    std::format(L"The target you have selected is too far away to interact with. Please get within {}m in order to interact with the terminal.",
			        global->config->terminalInitiateRadiusInMeters));
			return;
		}

		// Check for ongoing hack
		if (group->activeClient)
		{
			PrintUserCmdText(client, L"The target you have selected is already in use, please try again later.");
			return;
		}

		// There's probably a more elegant way to handle this message
		// Check for cooldown
		if (Hk::Time::GetUnixSeconds() <= group->lastActivatedTime + group->data->cooldownTimeInSeconds)
		{
			if (auto remainingTime = (group->lastActivatedTime + group->data->cooldownTimeInSeconds) - Hk::Time::GetUnixSeconds(); remainingTime < 60)
			{
				PrintUserCmdText(client,
				    std::format(L"The target you have selected is currently on cooldown. This {} will be available again in less than 1 minute.",
				        stows(group->data->terminalName)));
				return;
			}

			if (auto remainingTime = (group->lastActivatedTime + group->data->cooldownTimeInSeconds) - Hk::Time::GetUnixSeconds();
			    (remainingTime - 60) <= (120 - 60))

			{
				PrintUserCmdText(client,
				    std::format(L"The target you have selected is currently on cooldown. This {} will be available again in 1 minute.",
				        stows(group->data->terminalName)));
				return;
			}
			else
			{
				PrintUserCmdText(client,
				    std::format(L"The target you have selected is currently on cooldown. This {} will be available again in {} minutes.",
				        stows(group->data->terminalName),
				        remainingTime / 60));
				return;
			}
		}

		Vector clientPos;
		Matrix clientRot;
		pub::SpaceObj::GetLocation(Players[client].shipId, clientPos, clientRot);

		uint clientSystem;
		pub::SpaceObj::GetSystem(target, clientSystem);

		// Fetch the terminal's reputation and affiliation values
		int terminalReputation;
		pub::SpaceObj::GetRep(target, terminalReputation);
		uint terminalAffiliation;
		pub::Reputation::GetAffiliation(terminalReputation, terminalAffiliation);

		// Get the IDS Name for the faction We use this in several messages.
		uint npcFactionIds;
		pub::Reputation::GetGroupName(terminalAffiliation, npcFactionIds);

		// Fetch the player's reputation values
		int playerReputation;
		pub::Player::GetRep(client, playerReputation);

		// If the hack is unlawful, roll to see if there's a rep hit and hostile spawn.
		if (!isLawful)
		{
			if (confirm != L"confirm" && global->playerConfigs[Hk::Client::GetAccountByClientID(client)].hackPrompt)
			{
				PrintUserCmdText(client,
				    std::format(L"Hacking this terminal is an unlawful act and may affect your reputation with {}, as well as possibly provoking a hostile "
				                L"response. Do you wish to proceed? To proceed, type '/terminal hack confirm'.",
				        Hk::Message::GetWStringFromIdS(npcFactionIds)));
				return;
			}

			auto terminalName = stows(group->data->terminalName);
			auto coords = Hk::Math::VectorToSectorCoord<std::wstring>(clientSystem, clientPos);
			// This fires regardless of chance-based hostility.
			PrintLocalUserCmdText(client,
			    std::vformat(global->config->messageHackStartNotifyAll,
			        std::make_wformat_args(terminalName,
			            Hk::Client::GetCharacterNameByID(client).value(),
			            coords)),
			    global->config->terminalNotifyAllRadiusInMeters);

			if (GetRandomNumber(0, 100) <= int(group->data->hackHostileChance * 100))
			{
				for (int i = 0; i < GetRandomNumber(group->data->minHostileHackHostileNpcs, group->data->maxHostileHackHostileNpcs); i++)
				{
					Vector npcSpawnPos = {
					    clientPos.x + GetRandomNumber(-2000, 2000), clientPos.y + GetRandomNumber(-2000, 2000), clientPos.z + GetRandomNumber(-2000, 2000)};

					// Spawns an NPC from the group's possible pool and adds it to the list for this terminalGroup's live NPCs.
					SpawnedObject npcObject;
					npcObject.spaceId =
					    global->npcCommunicator->CreateNpc(group->data->hostileHackNpcs[GetRandomNumber(0, group->data->hostileHackNpcs.size() - 1)],
					        npcSpawnPos,
					        EulerMatrix({0.f, 0.f, 0.f}),
					        clientSystem,
					        true);

					npcObject.despawnTime = Hk::Time::GetUnixSeconds() + global->config->hackNpcLifetimeInSeconds;
					global->spawnedObjects.emplace_back(npcObject);
				}

				// Temporarily set the faction hostile to the player.
				pub::Reputation::SetAttitude(terminalReputation, playerReputation, -0.9f);

				// Decrement the player's reputation by group->hackRepReduction
				pub::Reputation::SetReputation(
				    playerReputation, terminalAffiliation, Hk::Player::GetRep(client, terminalAffiliation).value() - group->data->hackRepReduction);

				PrintUserCmdText(client,
				    std::format(L"Your attempt to hack the {} has been detected and your reputation with {} has been adjusted by -{} accordingly.",
				        stows(group->data->terminalName),
				        Hk::Message::GetWStringFromIdS(npcFactionIds),
				        group->data->hackRepReduction));
			}
		}
		else
		{
			if (confirm != L"confirm" && global->playerConfigs[Hk::Client::GetAccountByClientID(client)].usePrompt)
			{
				PrintUserCmdText(client,
				    std::format(L"Downloading data from this {} will cost {} credits and will take {} seconds. Do you wish to proceed? To proceed type "
				                L"'/terminal use confirm'.",
				        stows(group->data->terminalName),
				        group->data->useCostInCredits,
				        group->data->useTimeInSeconds));
				return;
			}

			if (Hk::Player::GetRep(client, terminalAffiliation).value() <= -0.25)
			{
				PrintUserCmdText(client,
				    std::format(L"Your reputation with {} isn't high enough to legally make use of this {}.",
				        Hk::Message::GetWStringFromIdS(npcFactionIds),
				        stows(group->data->terminalName)));
				return;
			}
			else
			{
				// Listen for response commands and if so:

				if (Hk::Player::GetCash(client).value() < group->data->useCostInCredits)
				{
					PrintUserCmdText(client,
					    std::format(L"You don't have enough credits to use this terminal. You need a total of {} in your account to proceed.",
					        group->data->useCostInCredits));
					return;
				}
			}
		}

		PrintUserCmdText(client,
		    std::format(L"Remain within {:.0f}m of the target for {} seconds in order to complete successful data retrieval.",
		        (global->config->terminalSustainRadiusInMeters),
		        isLawful ? group->data->useTimeInSeconds : group->data->hackTimeInSeconds));

		// Timer uses a ternary to determine which value

		group->activeClient = client;
		group->lastActivatedTime = Hk::Time::GetUnixSeconds();
		Hk::Client::PlaySoundEffect(client, CreateID("ui_new_story_star"));
		LightShipFuse(client, global->config->shipActiveTerminalFuse);
		pub::SpaceObj::SetRelativeHealth(target, 0.5f);

		// Assign the terminal target to the global vector
		group->currentTerminal = target;
		return;
	}

	// Define usable chat commands here
	const std::vector commands = {{
	    CreateUserCommand(L"/terminal", L"", UserCmdStartTerminalInteraction, L"Starts a user interaction with a valid solar object."),
	}};

} // namespace Plugins::Triggers

using namespace Plugins::Triggers;

REFL_AUTO(type(PlayerConfig), field(usePrompt), field(hackPrompt));
REFL_AUTO(type(Position), field(coordinates), field(system));
REFL_AUTO(type(Event), field(name), field(solarFormation), field(npcs), field(spawnWeight), field(descriptionLowInfo), field(descriptionMedInfo),
    field(descriptionHighInfo), field(lifetimeInSeconds));
REFL_AUTO(type(EventFamily), field(name), field(spawnWeight), field(eventList), field(spawnPositionList));
REFL_AUTO(type(TerminalGroup), field(terminalGroupName), field(terminalName), field(cooldownTimeInSeconds), field(useTimeInSeconds), field(hackTimeInSeconds),
    field(hackHostileChance), field(minHostileHackHostileNpcs), field(maxHostileHackHostileNpcs), field(useCostInCredits), field(minHackRewardInCredits),
    field(maxHackRewardInCredits), field(terminalList), field(eventFamilyUseList), field(eventFamilyHackList), field(hackRepReduction), field(hostileHackNpcs));
REFL_AUTO(type(Config), field(terminalGroups), field(terminalInitiateRadiusInMeters), field(terminalSustainRadiusInMeters),
    field(terminalNotifyAllRadiusInMeters), field(messageHackStartNotifyAll), field(messageHackFinishNotifyAll), field(factionNpcSpawnList),
    field(terminalHealthAdjustmentForStatus), field(shipActiveTerminalFuse), field(hackNpcLifetimeInSeconds));

DefaultDllMainSettings(LoadSettings);

const std::vector<Timer> timers = {{TerminalInteractionTimer, 5}, {CleanupTimer, 60}};
extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Triggers");
	pi->shortName("triggers");
	pi->mayUnload(true);
	pi->commands(&commands);
	pi->timers(&timers);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
	pi->emplaceHook(HookedCall::IServerImpl__Login, &OnLogin, HookStep::After);
}