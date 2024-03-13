#include "Hacking.hpp"

namespace Plugins::Hacking
{
	enum class PoiType
	{
		FactionAmbush = 0
	};

	void SpawnFactionAmbush()
	{
	}

	void CreateRewardPointOfInterest(uint client)
	{
		Vector rewardPos = {{200.f}, {200.f}, {200.f}};
		auto rewardSystem = "Li03";
		auto rewardSector = Hk::Math::VectorToSectorCoord<std::wstring>(CreateID(rewardSystem), rewardPos);

		// Send a private message to the player notifying them of their randomCash reward and POI location.
		auto formattedHackRewardMessage =
		    std::format(L"A point of interest has been revealed in sector {} ({:.0f}, {:.0f}, {:.0f}).", rewardSector, rewardPos.x, rewardPos.y, rewardPos.z);
		PrintUserCmdText(client, formattedHackRewardMessage);

		// Random number 0, PoiType
		auto randomPoi = PoiType::FactionAmbush;
		switch (randomPoi)
		{
			case PoiType::FactionAmbush:
				SpawnFactionAmbush();
				break;
			default:
				// Log something was bad
				break;
		}
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
		CreateRewardPointOfInterest(client);
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