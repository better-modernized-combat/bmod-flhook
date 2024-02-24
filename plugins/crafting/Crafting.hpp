#pragma once

#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::Crafting
{
	struct CraftingProcess
	{
		int processId = 0;
		std::wstring playerNick;
		time_t finishTime;
		uint baseId;
	};

	// Loadable json configuration
	struct Config : Reflectable
	{
		std::string File() override { return "config/crafting.json"; }
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;

		SQLite::Database sql = SqlHelpers::Create("crafting.sqlite");
	};

	extern const std::unique_ptr<Global> global;

	void CreateSqlTables();
	int64 SqlAddCraftingProcess(const CraftingProcess& craftingProcess);
	int64 SqlRemoveCraftingProcess(const int processId);
	CraftingProcess SqlGetCraftingProcess(const int processId);
} // namespace Plugins::Crafting