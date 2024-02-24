#pragma once

#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::Crafting
{
	// Loadable json configuration
	struct Config : Reflectable
	{
		std::string File() override { return "config/crafting.json"; }
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
	};
} // namespace Plugins::Crafting