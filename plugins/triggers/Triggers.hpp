#pragma once

#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::Triggers
{
	// Loadable json configuration
	struct Config : Reflectable
	{
		std::string File() override { return "config/triggers.json"; }
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
	};
} // namespace Plugins::Triggers