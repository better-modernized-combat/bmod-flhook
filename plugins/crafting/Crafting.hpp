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

	struct CraftingRecipe final : Reflectable
	{
		std::wstring itemNick = L"missile01_mark01";
		uint cost = 10;
		float requiredRep = 0.0;
		std::vector<std::wstring> requiredItems = {L"commodity_scrap_metal", L"commodity_scrap_metal", L"commodity_basic_alloys"};
		time_t craftingDuration = 60;
	};

	struct BaseModifier final : Reflectable
	{
		std::wstring baseNick = L"Br01_01_Base";
		float timeMod = 2.0;
		float costMod = 0.5;
		std::vector<std::wstring> availableItems = {L"missile01_mark01"};
	};

	// Loadable json configuration
	struct Config : Reflectable
	{
		std::string File() override { return "config/crafting.json"; }

		// Example crafting recipe & base modifier for json config
		std::vector<CraftingRecipe> craftingRecipes = {CraftingRecipe()};
		std::vector<BaseModifier> baseModifiers = {BaseModifier()};
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;

		// Database containing crafting processes if server goes offline
		SQLite::Database sql = SqlHelpers::Create("crafting.sqlite");

		// Crafting Processes at runtime
		std::vector<CraftingProcess> runningCraftingProcesses;
	};

	extern const std::unique_ptr<Global> global;

	void CreateSqlTables();
	int64 SqlAddCraftingProcess(const CraftingProcess& craftingProcess);
	int64 SqlRemoveCraftingProcess(const int processId);
	CraftingProcess SqlGetCraftingProcess(const int processId);
	std::vector<CraftingProcess> SqlGetAllCraftingProcesses();
} // namespace Plugins::Crafting