#include "Hacking.hpp"

// TODO: Make sure spawned stuff has a proper faction assigned, solars in formation currently do not
// TODO: If a zone area is in use, reserve it, so additional stuff can't spawn in it.

namespace Plugins::Hacking
{

	std::vector<SpawnedObject*> SpawnNpcGroup(const Vector& pos, uint system, const SolarGroup& solarGroup)
	{
		std::vector<SpawnedObject*> spawnedNpcs;

		for (const auto& [key, value] : solarGroup.npcsToSpawn)
		{
			for (int i = 0; i < value; i++)
			{
				uint spaceId = global->npcCommunicator->CreateNpc(stows(key), pos, {{{1.f, 0, 0}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}}}, system, true);
				auto& solar = global->spawnedNpcs.emplace_back(spaceId, 0, Hk::Time::GetUnixSeconds());
				spawnedNpcs.emplace_back(&solar);
			}
		}
	}

	std::vector<SpawnedObject*> SpawnSolarGroup(const Vector& pos, uint system, const SolarGroup& solarGroup)
	{
		std::vector<SpawnedObject*> spawnedComponents;

		for (auto component : solarGroup.solarComponents)
		{
			uint spaceId = global->solarCommunicator->CreateSolar(stows(component.solarName),
			    {
			        pos.x + component.relativePos[0],
			        pos.y + component.relativePos[1],
			        pos.z + component.relativePos[2],
			    },
			    EulerMatrix(Vector {component.rotation[0], component.rotation[1], component.rotation[2]}),
			    system,
			    true,
			    false);
			auto& solar = global->spawnedSolars.emplace_back(spaceId, 0, Hk::Time::GetUnixSeconds());
			spawnedComponents.emplace_back(&solar);
		}

		return spawnedComponents;
	}

	void CreateRewardPointOfInterest(uint client, HackInfo hack)
	{
		// TODO: This will need filtering for systems post-goose when other areas are opened up

		uint systemId;
		pub::Player::GetSystem(client, systemId);
		auto zonePos = Universe::get_zone(global->hashedObjectiveZoneList[RandomNumber(0, global->hashedObjectiveZoneList.size())])->vPos;
		auto rewardSector = Hk::Math::VectorToSectorCoord<std::wstring>(systemId, zonePos);

		// Send a private message to the player notifying them of their randomCash reward and POI location.
		auto formattedHackRewardMessage =
		    std::format(L"A point of interest has been revealed in sector {} ({:.0f}, {:.0f}, {:.0f}).", rewardSector, zonePos.x, zonePos.y, zonePos.z);
		PrintUserCmdText(client, formattedHackRewardMessage);

		// Vector spawnPosition = {8142, 107, 81435};

		auto poi = RandomNumber(0, global->config->solarGroups.size() - 1);

		std::vector<int> weights {};
		for (const auto& solarGroup : global->config->solarGroups)
		{
			weights.push_back(solarGroup.spawnWeight);
		}

		std::discrete_distribution<> dist(weights.begin(), weights.end());
		std::mt19937 engine;
		auto numberIndex = dist(engine);

		Console::ConDebug(std::format("Spawning POI {}", global->config->solarGroups[numberIndex].name));

		SpawnSolarGroup(zonePos, systemId, global->config->solarGroups[numberIndex]);
		SpawnNpcGroup(zonePos, systemId, global->config->solarGroups[numberIndex]);
	}

	// Function: This function is called when an initial objective is completed.
	void CompleteObjective(HackInfo& info, uint client)
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
		CreateRewardPointOfInterest(client, info);
		PrintUserCmdText(client, std::format(L"{} credits diverted from local financial ansibles", randomCash));

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

} // namespace Plugins::Hacking