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
					pub::SpaceObj::Destroy(obj.spaceId, VANISH);
					Hk::Client::PlaySoundEffect(client, CreateID("cloak_rh_fighter"));

					// ToggleCloak(obj.spaceId, obj.cloakId, true);
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
			CompleteObjective(info, client);
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

	void OneMinuteTick()
	{
		auto currentTime = Hk::Time::GetUnixSeconds();
		// Every minute we clean up any left over NPCs and Solars
		for (auto npc = global->spawnedNpcs.begin(); npc != global->spawnedNpcs.end(); ++npc)
		{
			if (npc->spawnTime + global->config->npcPersistTimeInSeconds < currentTime)
			{
				pub::SpaceObj::Destroy(npc->spaceId, DestroyType::VANISH);
				global->spawnedNpcs.erase(npc);
				npc--;
				continue;
			}
		}

		for (auto solar = global->spawnedSolars.begin(); solar != global->spawnedSolars.end(); ++solar)
		{
			if (solar->spawnTime + global->config->poiPersistTimeInSeconds < currentTime)
			{
				pub::SpaceObj::Destroy(solar->spaceId, DestroyType::VANISH);
				global->spawnedSolars.erase(solar);
				solar--;
				continue;
			}
		}
	}
} // namespace Plugins::Hacking