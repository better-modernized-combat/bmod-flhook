// Includes
#include "Triggers.hpp"

#include <random>

namespace Plugins::Triggers
{
	const auto global = std::make_unique<Global>();

	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));
	}

} // namespace Plugins::Triggers

using namespace Plugins::Triggers;

REFL_AUTO(type(Config));

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