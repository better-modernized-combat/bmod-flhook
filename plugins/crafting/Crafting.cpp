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

		// TODO: Validate crafting recipes & base structs
	}

	void DummyTestFunc()
	{
		return;
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
			/* TODO: SHOW ITEMS AVAILABLE WITH REP */
			return;
		}

		if (cmd2 == L"show" && cmd2 == L"all")
		{
			/* TODO: SHOW ALL ITEMS NO MATTER REP */
			return;
		}

		if (cmd2 == L"cancel")
		{
			/* TODO: CANCEL RUNNING CRAFTING PROCESS */
			return;
		}

		if (cmd2 == L"test")
		{
			DummyTestFunc();
			return;
		}

		PrintUserCmdText(client, L"Invalid command. Try /crafting for usage info.");
	}

	const std::vector commands = {{CreateUserCommand(L"/crafting", L"", UserCmdCrafting, L"")}};

} // namespace Plugins::Crafting

using namespace Plugins::Crafting;

REFL_AUTO(type(Config));

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