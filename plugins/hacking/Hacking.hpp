#pragma once

#include <FLHook.hpp>
#include <plugin.h>

namespace Plugins::Hacking
{

	//! Configurable fields for this plugin
	struct Config final : Reflectable
	{
		std::string File() override { return "config/hacking.json"; }

		// TODO: put variables into the hackable strings.

		// Reflectable fields
		//! Hackable solar archetypes
		std::string hackableSolarArchetype = "bm_hackable_sat";
		//! Message sent to the local area when the hack starts.
		std::wstring hackingStartedMessage = L"A communications buoy is being hacked by %player at %pos!";
		//! Message sent to the local area when the hack ends.
		std::wstring hackingFinishedMessage =
		    L"%player has completed their hack of the communications buoy at %pos and retrieved sensitive data from %faction!";
		//! Radius in which the message is sent
		float hackingMessageRadius = 10000.f;
		//! Time taken for the hack to complete.
		int hackingTime = 60;
		//! The radius the player needs to be within to initiate and sustain the hack.
		float hackingRadius = 500.f;
	};

	struct HackInfo
	{
		uint target = 0;
		int time = 0;
		bool beenWarned = false;
	};

	struct Global
	{
		std::unique_ptr<Config> config = nullptr;
		ReturnCode returnCode = ReturnCode::Default;
		bool pluginActive = true;
		uint targetHash = 0;
		std::array<HackInfo, 255> activeHacks;
	};
} // namespace Plugins::Hacking