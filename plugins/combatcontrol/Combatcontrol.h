#pragma once

#include <FLHook.hpp>

namespace Plugins::Combatcontrol
{
	struct BurstFireData
	{
		int magSize;
		float reloadTime;
	};

	struct BurstFireGunData
	{
		int maxMagSize;
		int bulletsLeft;
		float reloadTime;
	};

	struct MineInfo
	{
		float armingTime = 0.0f;
		float dispersionAngle = 0.0f;
		bool detonateOnEndLifetime = false;
		bool stopSpin = false;
	};

	struct GuidedData
	{
		bool noTrackingAlert = false;
		uint trackingBlacklist = 0;
		float armingTime = 0.0f;
		float topSpeed = 0.0f;
	};

	struct ExplosionDamageData
	{
		uint weaponType = 0;
		float percentageDamageHull = 0.0f;
		float percentageDamageShield = 0.0f;
		float percentageDamageEnergy = 0.0f;
		int armorPen = 0;
		float detDist = 0.0f;
		bool cruiseDisrupt = false;
		bool damageSolars = true;
		bool missileDestroy = false;
	};

	struct SpeedCheck
	{
		float targetSpeed = 0;
		uint checkCounter = 0;
	};

	//! Global data for this plugin
	struct Global final
	{
		// Other fields
		ReturnCode returnCode = ReturnCode::Default;

		std::unordered_map<uint, std::unordered_map<ushort, BurstFireGunData>> shipGunData;
		std::unordered_map<uint, BurstFireData> burstGunData;

		std::unordered_map<uint, GuidedData> guidedDataMap;
		std::unordered_map<uint, MineInfo> mineInfoMap;
		std::unordered_map<uint, float> cmDispersionMap;
		std::unordered_map<uint, ExplosionDamageData> explosionTypeMap;
		std::unordered_map<uint, uint> newMissileUpdateMap;

		std::unordered_map<uint, SpeedCheck> topSpeedWatch;
	};

	extern const std::unique_ptr<Global> global;

	bool __stdcall ShipExplosionHit(IObjRW* iobj, ExplosionDamageEvent* explosion, DamageList* dmg);
	FireResult __fastcall CELauncherFire(CELauncher* gun, void* edx, const Vector& pos);
	void __stdcall ShipDestroyed(IObjRW* iobj, bool isKill, uint killerId);
	void __fastcall GuidedInit(CGuided* guided, void* edx, CGuided::CreateParms& parms);
	bool __stdcall MineDestroyed(IObjRW* iobj, bool isKill, uint killerId);
	bool __stdcall GuidedDestroyed(IObjRW* iobj, bool isKill, uint killerId);
	void MineImpulse(CMine* mine, Vector& launchVec);
	void MineSpin(CMine* mine, Vector& spinVec);
	void LoadHookOverrides();

} // namespace Plugins::Combatcontrol

void HookExplosionHitNaked();