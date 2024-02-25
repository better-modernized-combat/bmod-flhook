#include "Crafting.hpp"

namespace Plugins::Crafting
{
	void CreateSqlTables()
	{
		if (!global->sql.tableExists("processes"))
		{
			global->sql.exec
			(
				"CREATE TABLE processes ("
				"pid INTERGER NOT NULL UNIQUE,"
				"player TEXT NOT NULL,"
				"finish INTEGER NOT NULL,"
				"base INTEGER NOT NULL);"
			);
		}
	}

	int64 SqlAddCraftingProcess(const CraftingProcess& craftingProcess)
	{
		SQLite::Statement query(global->sql, "INSERT INTO processes (pid, player, finish, base) VALUES(?,?,?,?);");
		query.bind(1, craftingProcess.processId);
		query.bind(2, wstos(craftingProcess.playerNick));
		query.bind(3, static_cast<int>(craftingProcess.finishTime));
		query.bind(4, static_cast<int>(craftingProcess.baseId));
		query.exec();

		return global->sql.getLastInsertRowid();
	}

	int64 SqlRemoveCraftingProcess(const int processId)
	{
		SQLite::Statement query(global->sql, "DELETE FROM processes WHERE pid = ?;");
		query.bind(1, processId);
		query.exec();

		return global->sql.getChanges();
	}

	CraftingProcess SqlGetCraftingProcess(const int processId)
	{
		// If this fails it returns a default crafting process, which has 0 as PID.
		CraftingProcess result;

		SQLite::Statement query(global->sql, "SELECT * FROM processes WHERE pid = ?;");
		query.bind(1, processId);

		if (query.executeStep())
		{
			result.processId = query.getColumn(0).getInt();
			result.playerNick = stows(query.getColumn(1).getString());
			result.finishTime = static_cast<time_t>(query.getColumn(2).getInt());
			result.baseId = static_cast<uint>(query.getColumn(3).getInt());

			return result;
		}

		return result;
	}

	std::vector<CraftingProcess> SqlGetAllCraftingProcesses()
	{
		std::vector<CraftingProcess> result;

		SQLite::Statement query(global->sql, "SELECT * FROM processes;");

		while (query.executeStep())
		{
			CraftingProcess toAdd;
			toAdd.processId = query.getColumn(0).getInt();
			toAdd.playerNick = stows(query.getColumn(1).getString());
			toAdd.finishTime = static_cast<time_t>(query.getColumn(2).getInt());
			toAdd.baseId = static_cast<uint>(query.getColumn(3).getInt());
			result.push_back(toAdd);
		}

		return result;
	}
} // namespace Plugins::Crafting