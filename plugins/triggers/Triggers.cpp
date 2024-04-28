// Includes
#include "Triggers.hpp"

#include <random>

namespace Plugins::Triggers
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

	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));

		// Set the npcCommunicator and solarCommunicator interfaces and check if they are availlable
		global->npcCommunicator =
		    static_cast<Plugins::Npc::NpcCommunicator*>(PluginCommunicator::ImportPluginCommunicator(Plugins::Npc::NpcCommunicator::pluginName));

		global->solarCommunicator = static_cast<Plugins::SolarControl::SolarCommunicator*>(
		    PluginCommunicator::ImportPluginCommunicator(Plugins::SolarControl::SolarCommunicator::pluginName));

		// Prevent the plugin from progressing further and disable all functions if either interface is not found.
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