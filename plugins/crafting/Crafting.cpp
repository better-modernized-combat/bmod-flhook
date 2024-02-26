#include "Crafting.hpp"

namespace Plugins::Crafting
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();

	time_t GetCurrentTimeT()
	{
		return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	}

	std::vector<CraftingProcess> GetFinishedCraftingProcesses(std::vector<CraftingProcess>& craftingProcesses)
	{
		time_t currentTimeT = GetCurrentTimeT();
		std::vector<CraftingProcess> result;
		std::ranges::copy_if(craftingProcesses, std::back_inserter(result), [currentTimeT](const CraftingProcess& craftingProcess) 
		{
			return craftingProcess.finishTime <= currentTimeT;
		});
		
		return result;
	}

	cpp::result<BaseModifier, Error> BaseIdToBaseModifier(BaseId baseId)
	{
		auto found = std::ranges::find_if(
		    global->config->baseModifiers, [baseId](const BaseModifier& baseModifier) { return baseId == CreateID(wstos(baseModifier.baseNick).c_str()); });
		if (found == global->config->baseModifiers.end())
		{
			return cpp::fail(Error::InvalidBase);
		}

		return *found;
	}

	cpp::result<CraftingRecipe, Error> ItemNickToCraftingRecipe(std::wstring itemNick)
	{
		auto found = std::ranges::find_if(global->config->craftingRecipes, [itemNick](const CraftingRecipe& craftingRecipe) { return itemNick == craftingRecipe.itemNick; });
		if (found == global->config->craftingRecipes.end())
		{
			return cpp::fail(Error::InvalidGood);
		}

		return *found;
	}

	void LoadSettings()
	{
		// Load JSON config
		auto config = Serializer::JsonToObject<Config>();
		global->config = std::make_unique<Config>(std::move(config));

		// Create tables
		CreateSqlTables();

		// Load running crafting processes from database & check which finished
		global->runningCraftingProcesses = SqlGetAllCraftingProcesses();
		auto finishedCraftingProcesses = GetFinishedCraftingProcesses(global->runningCraftingProcesses);

		// Remove all crafting processes which finished from global vector
		// and then from database
		// TODO: Use Item Nick to Recipe helper
		std::erase_if(global->runningCraftingProcesses, [finishedCraftingProcesses](const CraftingProcess& craftingProcess) {
			return std::ranges::any_of(finishedCraftingProcesses,
			    [&craftingProcess](const CraftingProcess& finishedCraftingProcess) { return finishedCraftingProcess.processId == craftingProcess.processId; });
		});
		for (const auto& finishedCraftingProcess : finishedCraftingProcesses)
		{
			SqlRemoveCraftingProcess(finishedCraftingProcess.processId);
		}

		// Push resulting items of finished processes to player warehouses.
		// TODO: do ^ & Test, see below
		// TESTING START
		for (const auto& craftingProcess : finishedCraftingProcesses)
		{
			Console::ConErr(std::to_string(craftingProcess.processId));
		}
		// TESTING END

		// Check that all craftable items in bases from config have a crafting recipe
		// associated with them
		std::set<std::wstring> recipeNicks;
		for (const auto& craftingRecipe : global->config->craftingRecipes)
		{
			recipeNicks.insert(craftingRecipe.itemNick);
		}
		for (const auto& baseModifier : global->config->baseModifiers)
		{
			for (const auto& item : baseModifier.availableItems)
			{
				if (!recipeNicks.contains(item))
				{
					Console::ConErr(std::format("{} does not have a recipe defined, but is craftable at base {}.", wstos(item), wstos(baseModifier.baseNick)));
				}
			}
		}

		// TODO: Check if each item has only one recipe defined
	}

	bool isCraftable(ClientId client, BaseId base, const CraftingRecipe& craftingRecipe)
	{
		// 1. Has the player enough money?
		if (Hk::Player::GetCash(client).value_or(0) < craftingRecipe.cost)
		{
			PrintUserCmdText(client, L"Insufficient Cash.");
			return false;
		}

		// 2. Is the player reputable enough at the current base?
		if (Hk::Player::GetRep(client, Hk::Solar::GetAffiliation(base).value()).value_or(0.0) < craftingRecipe.requiredRep)
		{
			PrintUserCmdText(client, L"Insufficient Rep.");
			PrintUserCmdText(client, std::to_wstring(Hk::Player::GetRep(client, Hk::Solar::GetAffiliation(base).value()).value()));
			return false;
		}

		// 3. Does player have all the items required?
		// TODO: see above

		return true;
	}

	void PrintAvailableForCrafting(ClientId client, BaseId base, const bool all)
	{
		auto baseModifer = BaseIdToBaseModifier(base);
		if (baseModifer.has_value() && all)
		{
			for (const auto& item : baseModifer.value().availableItems)
			{
				PrintUserCmdText(client, item);
				return;
			}
		}
		else if (baseModifer.has_value() && !all)
		{
			auto itemCopy = baseModifer.value().availableItems;
			std::erase_if(itemCopy,
			    [client, base](std::wstring const& itemNick) { return isCraftable(client, base, ItemNickToCraftingRecipe(itemNick).value()); });
			// TODO: Does not reach this for some reason?
			PrintUserCmdText(client, L"Happened 2");
			for (const auto& itemNick : itemCopy)
			{
				PrintUserCmdText(client, itemNick);
				return;
			}
		}
		else
		{
			PrintUserCmdText(client, L"No items are available for crafting at this base.");
			return;
		}
	}

	void UserCmdCrafting(ClientId& client, const std::wstring& param)
	{
		const std::wstring cmd1 = GetParam(param, ' ', 0);
		const std::wstring cmd2 = GetParam(param, ' ', 1);
		const std::wstring cmd3 = GetParam(param, ' ', 2);

		if (cmd1.empty())
		{
			PrintUserCmdText(client, 
				L"/crafting start <item nick> <quantity> : Starts a crafting process for the specified item(s),"
			    L"/crafting running : Lists all of your running crafting processes."
			    L"/crafting show : Lists all items which are available to you for crafting at this base."
			    L"/crafting show all : Lists items available for crafting at base, regardless of requirements."
			    L"/crafting cancel <process ID> : Cancels one of your running crafting processes."
			);
			return;
		}

		// Check if player is docked at base, otherwise none of the following stuff
		// is important
		const auto currentBase = Hk::Player::GetCurrentBase(client);
		if (currentBase.has_error())
		{
			PrintUserCmdText(client, L"You must be docked at a base to craft!");
			return;
		}

		if (cmd1 == L"start")
		{
			/* TODO: CRAFTING PROCESS START */
			return;
		}

		if (cmd1 == L"running")
		{
			/* TODO: LIST RUNNING CRAFTING PROCESSES*/
			return;
		}

		if (cmd1 == L"show" && cmd2 == L"")
		{
			PrintAvailableForCrafting(client, currentBase.value(), false);
			return;
		}

		if (cmd1 == L"show" && cmd2 == L"all")
		{
			PrintAvailableForCrafting(client, currentBase.value(), true);
			return;
		}

		if (cmd2 == L"cancel")
		{
			/* TODO: CANCEL RUNNING CRAFTING PROCESS */
			return;
		}

		PrintUserCmdText(client, L"Invalid command. Try /crafting for usage info.");
	}

	const std::vector commands = {{CreateUserCommand(L"/crafting", L"", UserCmdCrafting, L"")}};

} // namespace Plugins::Crafting

using namespace Plugins::Crafting;

REFL_AUTO(type(CraftingRecipe), field(itemNick), field(cost), field(requiredRep), field(requiredItems), field(craftingDuration))
REFL_AUTO(type(BaseModifier), field(baseNick), field(timeMod), field(costMod), field(availableItems))
REFL_AUTO(type(Config), field(craftingRecipes), field(baseModifiers));

DefaultDllMainSettings(LoadSettings);

extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("Crafting");
	pi->shortName("crafting");
	pi->mayUnload(true);
	pi->returnCode(&global->returnCode);
	pi->commands(&commands);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
}