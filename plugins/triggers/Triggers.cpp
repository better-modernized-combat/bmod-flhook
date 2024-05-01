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

// TODO: Settings validation
// TODO: Spawning system
// TODO: Cooldown timers and hacking checks

namespace Plugins::Triggers
{
	const auto global = std::make_unique<Global>();

	int RandomNumber(int min, int max)
	{
		static std::random_device dev;
		static auto engine = std::mt19937(dev());
		auto range = std::uniform_int_distribution(min, max);
		return range(engine);
	}

	int RandomWeight(const std::vector<int>& weights)
	{
		std::discrete_distribution<> dist(weights.begin(), weights.end());
		static std::mt19937 engine;
		auto weightIndex = dist(engine);
		return weightIndex;
	}

	void LoadSettings()
	{
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
	}

	/** @ingroup Triggers
	 * @brief Creates a point of interested and it's accompanying NPCs if there are any defined.
	 */
	void CreateEvent(const Event& event, const Position& position)
	{
		Vector pos = {position.coordinates[0], position.coordinates[1], position.coordinates[2]};
		Matrix mat = EulerMatrix({0.f, 0.f, 0.f});
		
		global->solarCommunicator->CreateSolarFormation(event.solarFormation, pos, CreateID(position.system.c_str()));

		for (const auto& npcs : event.npcs)
		{
			global->npcCommunicator->CreateNpc(npcs.first, pos, mat, CreateID(position.system.c_str()), true);
		}
	}

	//TODO: Maybe see if you can move the weight selection into the RandomWeights function fully and do the loop in there

	/** @ingroup Triggers
	 * @brief Completes a terminal interaction, rewards the player and spawns a random event selected from the appropriate pool
	 */
	void CompleteTerminalInteraction(const TerminalGroup& terminalGroup, bool isLawful)
	{

		//TODO: Probably make this if check more granular and swap out the lists + messages based on the isLawful bool rather than repeating all this code again
		if (isLawful)
		{
			std::vector<int> familyWeights;
			for (const auto& eventFamily : terminalGroup.eventFamilyUseList)
			{
				familyWeights.emplace_back(eventFamily.spawnWeight);
			}
			auto& family = terminalGroup.eventFamilyUseList[RandomWeight(familyWeights)];

			std::vector<int> eventWeights;
			for (const auto& event : terminalGroup.eventFamilyUseList[RandomWeight(familyWeights)].eventList)
			{
				eventWeights.emplace_back(event.spawnWeight);
			}
			auto& event = family.eventList[RandomWeight(eventWeights)];
			auto& position = family.spawnPositionList[RandomNumber(0, family.spawnPositionList.size())];

			Console::ConDebug(std::format("Spawning the event '{}' at {},{},{} in {}",
			    event.name,
			    position.coordinates[0],
			    position.coordinates[1],
			    position.coordinates[2], 
				position.system));

			CreateEvent(event, position);
		}

		else if (!isLawful)
		{
			std::vector<int> weights;
			for (const auto& eventFamily : terminalGroup.eventFamilyHackList)
			{
				weights.emplace_back(eventFamily.spawnWeight);
			}
			RandomWeight(weights);
		}
	}

} // namespace Plugins::Triggers

using namespace Plugins::Triggers;

REFL_AUTO(type(Position), field(coordinates), field(system));
REFL_AUTO(type(Event), field(name), field(solarFormation), field(npcs), field(spawnWeight), field(descriptionLowInfo), field(descriptionMedInfo),
    field(descriptionHighInfo), field(lifetimeInSeconds));
REFL_AUTO(type(EventFamily), field(name), field(spawnWeight), field(eventList), field(spawnPositionList));
REFL_AUTO(type(TerminalGroup), field(terminalName), field(cooldownTimeInSeconds), field(useTimeInSeconds), field(hackTimeInSeconds), field(hackHostileChance),
    field(minHostileHackHostileNpcs), field(maxHostileHackHostileNpcs), field(useCostInCredits), field(minHackRewardInCredits), field(maxHackRewardInCredits),
    field(messageLawfulUse), field(messageUnlawfulHack), field(terminalList), field(eventFamilyUseList), field(eventFamilyHackList));
REFL_AUTO(type(Config), field(terminalGroups), field(terminalInitiateRadiusInMeters), field(terminalSustainRadiusInMeters),
    field(terminalNotifyAllRadiusInMeters), field(messageHackStartNotifyAll), field(messageHackFinishNotifyAll), field(factionNpcSpawnList),
    field(terminalHealthAdjustmentForStatus), field(shipActiveTerminalFuse));

DefaultDllMainSettings(LoadSettings);

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Triggers");
	pi->shortName("triggers");
	pi->mayUnload(true);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
}