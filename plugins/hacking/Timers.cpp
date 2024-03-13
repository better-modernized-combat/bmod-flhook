#include <ranges>

#include "Hacking.hpp"

namespace Plugins::Hacking
{

	// Function: Cleans up after an objective is complete, removing any NPCs and resetting any active timers or fuses
	void CleanUp(HackInfo& info)
	{
		pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
		info.spawnedNpcList.clear();
		info.target = 0;
	}

	// Function: This function is called when an initial objective is completed.
	void InitialObjectiveComplete(HackInfo& info, uint client)
	{
		// Grab all the information needed to populate the proximity message sent to the players:
		auto sectorEndCoordinate = Hk::Math::VectorToSectorCoord<std::wstring>(
		    Hk::Solar::GetSystemBySpaceId(info.target).value(), Hk::Solar::GetLocation(client, IdType::Client).value().first);

		int npcReputation;
		pub::SpaceObj::GetRep(info.target, npcReputation);

		uint npcFaction, npcIds;
		pub::Reputation::GetAffiliation(npcReputation, npcFaction);
		pub::Reputation::GetGroupName(npcFaction, npcIds);

		auto realName = Hk::Message::GetWStringFromIdS(npcIds);

		// Send the message to everyone within hackingMessageRadius
		auto formattedEndHackMessage = std::vformat(
		    global->config->hackingFinishedMessage, std::make_wformat_args(Hk::Client::GetCharacterNameByID(client).value(), sectorEndCoordinate, realName));
		PrintLocalUserCmdText(client, formattedEndHackMessage, global->config->hackingMessageRadius);

		// Adjust the hacker's reputation with the owning faction by -0.05
		auto hackerNewRep = Hk::Player::GetRep(client, npcFaction).value() - 0.05f;
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

		for (auto& i : global->solars | std::views::values)
		{
			for (auto& solar : i.rotatingSolars)
			{
				if (solar.solar == info.target)
				{
					solar.isHacking = false;
					solar.isHacked = true;
					break;
				}
			}
		}

		// Play some sounds to telegraph the successful completion of the hack.
		Hk::Client::PlaySoundEffect(client, CreateID("ui_receive_money"));
		Hk::Client::PlaySoundEffect(client, CreateID("ui_end_scan"));
		UnLightShipFuse(client, global->config->shipFuse);
		// Turn off the solar fuse:
		pub::SpaceObj::SetRelativeHealth(info.target, 1.f);
	}

	bool HandleDisconnect(HackInfo& info, uint client)
	{
		// Checks if the player has a ship before proceeding. This handles disconnects, crashing and docking.
		if (Hk::Player::GetShip(client).has_value())
		{
			return false;
		}

		// Notify the player, play an audio cue, turn off the fuse and reset the timer, failing the objective.
		PrintUserCmdText(client, L"You have left the area, the hack has failed.");
		Hk::Client::PlaySoundEffect(client, CreateID("ui_select_remove"));
		pub::SpaceObj::SetRelativeHealth(info.target, 1.f);

		info.time = 0;
		for (auto& i : global->solars | std::views::values)
		{
			i.rotatingSolars[i.currentIndex].isHacking = false;
		}

		return true;
	}

	bool HandleOutOfRange(HackInfo& info, uint client)
	{
		if (IsInSolarRange(client, info.target, global->config->hackingSustainRadius))
		{
			return false;
		}

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

			return true;
		}

		if (info.time == 0)
		{
			info.time = 5;
		}

		// Warn the player they're out of range for a cycle and set the beenWarned variable.
		PrintUserCmdText(client, L"Warning, you are moving out of range, the hack will fail if you don't get closer.");

		// Play a sound effect for the player to telegraph that they're leaving the area.
		Hk::Client::PlaySoundEffect(client, CreateID("ui_filter_operation"));
		pub::SpaceObj::SetRelativeHealth(info.target, 0.4f);

		info.beenWarned = true;
		return true;
	}

	// TODO: Limit user-configurable time to be increments of 5 seconds (remeber npc persist time is also mult of 5)
	void ProcessActiveHack(const uint client)
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
					pub::SpaceObj::Destroy(obj.npcId, VANISH);
					Hk::Client::PlaySoundEffect(client, CreateID("cloak_rh_fighter"));

					// ToggleCloak(obj.npcId, obj.cloakId, true);
				}

				CleanUp(info);
			}

			return;
		}

		if (HandleDisconnect(info, client))
		{
			return;
		}

		// Check if the player is in range of the solar while the hack is ongoing. There's a grace period for leaving the area where the player is warned.
		if (HandleOutOfRange(info, client))
		{
			return;
		}

		if (info.beenWarned)
		{
			info.beenWarned = false;
			PrintUserCmdText(client, L"You are back in range of the objective.");
		}

		if (info.time == 0)
		{
			// Complete the hack when the timer finishes.
			InitialObjectiveComplete(info, client);
		}
	}

	// Function: handles events that occur after the initial objective has been started and events that occur as the completion timer counts down.
	void FiveSecondTick()
	{
		// Start the timer
		for (auto client = 0u; client != global->activeHacks.size(); client++)
		{
			ProcessActiveHack(client);
		}
	}

	// Function: Ticks regularly and cycles through the possible objective targets.
	void TwentyMinuteTick()
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
} // namespace Plugins::Hacking