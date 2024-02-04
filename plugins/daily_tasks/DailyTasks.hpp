﻿#pragma once

#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::DailyTasks
{
	// Loadable json configuration
	struct Config : Reflectable
	{
		std::string File() override { return "config/daily_tasks.json"; }
		//! The number of times per day, a player may reset and reroll their assigned daily tasks.
		int taskResetAmount = 1;
		//! The number of randomly generated tasks a player will receive each day.
		int taskQuantity = 3;
		//! The minimum possible amount of credits that can be awarded for completing a daily task.
		int minCreditsReward = 5000;
		//! The maximum possible amount of credits that can be awarded for completing a daily task.
		int maxCreditsReward = 10000;
		//! A pool of possible item rewards for completing a daily task.
		std::map<std::string, std::vector<int>> itemRewardPool {
		    {{"commodity_luxury_consumer_goods"}, {{5}, {10}}}, {{"commodity_diamonds"}, {{25}, {40}}}, {{"commodity_alien_artifacts"}, {{10}, {25}}}};
		//! Possible target bases for trade tasks.
		std::vector<std::string> taskTradeBaseTargets {{"li03_01_base"}, {"li03_02_base"}, {"li03_03_base"}};
		//! Possible target items and their minimum and maximum quantity for trade tasks.
		std::map<std::string, std::vector<int>> taskTradeItemTargets {
		    {{"commodity_cardamine"}, {{5}, {10}}}, {{"commodity_scrap_metal"}, {{25}, {40}}}, {{"commodity_construction_machinery"}, {{10}, {25}}}};
		//! Possible options for item acquisition tasks. Parameters are item and quantity.
		std::map<std::string, std::vector<int>> taskItemAcquisitionTargets {
		    {{"commodity_optronics"}, {{3}, {5}}}, {{"commodity_super_alloys"}, {{10}, {15}}}, {{"commodity_mox_fuel"}, {{8}, {16}}}};
		//! Possible options for NPC kill tasks. Parameters are target faction and quantity.
		std::map<std::string, std::vector<int>> taskNpcKillTargets = {{{"fc_x_grp"}, {{3}, {5}}}, {{"li_n_grp"}, {{10}, {15}}}};
		//! Possible options for player kill tasks. Parameters are minimum and maximum quantity of kills needed.
		std::vector<int> taskPlayerKillTargets = {{1}, {3}};
		//! The server time at which daily tasks will reset.
		int resetTime = 12;
	};

	struct Task : Reflectable
	{
		int taskType = 0;
		int quantity = 0;
		uint itemTarget = 0;
		uint baseTarget = 0;
		uint npcFactionTarget = 0;
		std::string taskDescription;
		bool isCompleted = false;
		uint setTime = 0;
	};

	struct Tasks : Reflectable
	{
		std::vector<Task> tasks;
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
		std::map<uint, bool> playerTaskAllocationStatus;
		std::map<uint, std::vector<int>> itemRewardPool;
		std::map<uint, std::vector<int>> taskTradeItemTargets;
		std::map<uint, std::vector<int>> taskItemAcquisitionTargets;
		std::map<uint, std::vector<int>> taskNpcKillTargets;
		std::vector<uint> taskTradeBaseTargets;
		std::vector<int> taskTypePool;
		std::map<CAccount*, Tasks> accountTasks;
	};
} // namespace Plugins::DailyTasks