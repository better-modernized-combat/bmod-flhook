// Includes
#include "Crafting.hpp"

#include <random>

namespace Plugins::Crafting
{
	const auto global = std::make_unique<Global>();

	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));
	}

} // namespace Plugins::Crafting

using namespace Plugins::Crafting;

REFL_AUTO(type(Config));

DefaultDllMainSettings(LoadSettings);

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	// Full name of your plugin
	pi->name("Crafting");
	// Shortened name, all lower case, no spaces. Abbreviation when possible.
	pi->shortName("crafting");
	pi->mayUnload(true);
	pi->returnCode(&global->returnCode);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
}