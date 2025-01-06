/**
 * @date Jan, 2023
 * @author Lazrius
 * @defgroup Warehouse Warehouse
 * @brief
 * The Warehouse plugin allows players to deposit and withdraw various commodities and equipment on NPC stations, stored on internal SQL database.
 *
 * @paragraph cmds Player Commands
 * -warehouse store <ID> <count> - deposits specified equipment or commodity into the base warehouse
 * -warehouse list - lists unmounted equipment and commodities avalable for deposit
 * -warehouse withdraw <ID> <count> - transfers specified equipment or commodity into your ship cargo hold
 * -warehouse liststored - lists equipment and commodities deposited
 *
 * @paragraph adminCmds Admin Commands
 * None
 *
 * @paragraph configuration Configuration
 * @code
 * {
 *     "costPerStackStore": 100,
 *     "costPerStackWithdraw": 50,
 *     "restrictedBases": ["Li01_01_base", "Li01_02_base"],
 *     "restrictedItems": ["li_gun01_mark01","li_gun01_mark02"]
 * }
 * @endcode
 *
 * @paragraph ipc IPC Interfaces Exposed
 * This plugin does not expose any functionality.
 */

#include "Combatcontrol.h"

namespace Plugins::Combatcontrol
{
	const std::unique_ptr<Global> global = std::make_unique<Global>();

	void LoadSettings()
	{
		LoadHookOverrides();

		INI_Reader ini;
		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		std::string currDir = std::string(szCurDir);
		std::string scFreelancerIniFile = currDir + R"(\freelancer.ini)";

		std::string gameDir = currDir.substr(0, currDir.length() - 4);
		gameDir += std::string(R"(\DATA\)");

		std::vector<std::string> equipFiles;
		std::vector<std::string> shipFiles;

		while (ini.read_header())
		{
			if (!ini.is_header("Data"))
			{
				continue;
			}
			while (ini.read_value())
			{
				if (ini.is_value("equipment"))
				{
					equipFiles.emplace_back(ini.get_value_string());
				}
				//else if (ini.is_value("ships"))
				//{
				//	shipFiles.emplace_back(ini.get_value_string());
				//}
			}
		}
		ini.close();


		for (std::string& equipFile : equipFiles)
		{
			equipFile = gameDir + equipFile;
			if (!ini.open(equipFile.c_str(), false))
			{
				continue;
			}
			while (ini.read_header())
			{
				uint nickname;
				uint explosionArch;
				if (ini.is_header("Gun"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("burst_fire"))
						{
							float baseRefire = ((Archetype::Gun*)Archetype::GetEquipment(nickname))->fRefireDelay;
							global->burstGunData[nickname] = { ini.get_value_int(0), baseRefire - ini.get_value_float(1) };
						}
					}
				}
				else if (ini.is_header("MineDropper"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("burst_fire"))
						{
							float baseRefire = ((Archetype::MineDropper*)Archetype::GetEquipment(nickname))->fRefireDelay;
							global->burstGunData[nickname] = { ini.get_value_int(0), baseRefire - ini.get_value_float(1) };
						}
						else if (ini.is_value("self_detonate"))
						{
							global->mineInfoMap[nickname].detonateOnEndLifetime = ini.get_value_bool(0);
						}
						else if (ini.is_value("mine_arming_time"))
						{
							global->mineInfoMap[nickname].armingTime = ini.get_value_float(0);
						}
						else if (ini.is_value("stop_spin"))
						{
							global->mineInfoMap[nickname].stopSpin = ini.get_value_bool(0);
						}
						else if (ini.is_value("dispersion_angle"))
						{
							global->mineInfoMap[nickname].dispersionAngle = ini.get_value_float(0) / (180.f / 3.14f);
						}
						else if (ini.is_value("explosion_arch"))
						{
							explosionArch = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("detonation_dist"))
						{
							global->explosionTypeMap[explosionArch].detDist = ini.get_value_float(0);
						}
					}
				}
				else if (ini.is_header("CounterMeasureDropper"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("burst_fire"))
						{
							float baseRefire = ((Archetype::CounterMeasureDropper*)Archetype::GetEquipment(nickname))->fRefireDelay;
							global->burstGunData[nickname] = { ini.get_value_int(0), baseRefire - ini.get_value_float(1) };
						}
					}
				}
				else if (ini.is_header("CounterMeasure"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("dispersion_angle"))
						{
							global->cmDispersionMap[nickname] = ini.get_value_float(0);
						}
					}
				}
				else if (ini.is_header("Munition"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string());
						}
						else if (ini.is_value("arming_time"))
						{
							global->guidedDataMap[nickname].armingTime = ini.get_value_float(0);
						}
						else if (ini.is_value("no_tracking_alert") && ini.get_value_bool(0))
						{
							global->guidedDataMap[nickname].noTrackingAlert = true;
						}
						else if (ini.is_value("tracking_blacklist"))
						{
							uint blacklistedTrackingTypesBitmap = 0;
							std::string typeStr = ToLower(ini.get_value_string(0));
							if (typeStr.find("fighter") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Fighter;
							if (typeStr.find("freighter") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Freighter;
							if (typeStr.find("transport") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Transport;
							if (typeStr.find("gunboat") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Gunboat;
							if (typeStr.find("cruiser") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Cruiser;
							if (typeStr.find("capital") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Capital;
							if (typeStr.find("guided") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Guided;
							if (typeStr.find("mine") != std::string::npos)
								blacklistedTrackingTypesBitmap |= Mine;

							global->guidedDataMap[nickname].trackingBlacklist = blacklistedTrackingTypesBitmap;
						}
						else if (ini.is_value("top_speed"))
						{
							global->guidedDataMap[nickname].topSpeed = ini.get_value_float(0) * ini.get_value_float(0);
						}
						else if (ini.is_value("explosion_arch"))
						{
							explosionArch = CreateID(ini.get_value_string(0));
						}
						else if (ini.is_value("detonation_dist"))
						{
							global->explosionTypeMap[explosionArch].detDist = ini.get_value_float(0);
						}
					}
				}
				/*
				else if (ini.is_header("ShieldGenerator"))
				{
					ShieldBoostData sb;
					bool FoundValue = false;

					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							currNickname = CreateID(ini.get_value_string());
						}
						else if (ini.is_value("shield_boost"))
						{
							sb.durationPerBattery = ini.get_value_float(0);
							sb.minimumDuration = ini.get_value_float(1);
							sb.maximumDuration = ini.get_value_float(2);
							sb.damageReduction = ini.get_value_float(3);
							sb.fuseId = CreateID(ini.get_value_string(4));
							FoundValue = true;
						}
						else if (ini.is_value("shield_boost_explosion"))
						{
							sb.hullBaseDamage = ini.get_value_float(0);
							sb.hullReflectDamagePercentage = ini.get_value_float(1);
							sb.hullDamageCap = ini.get_value_float(2);
							sb.energyBaseDamage = ini.get_value_float(3);
							sb.energyReflectDamagePercentage = ini.get_value_float(4);
							sb.energyDamageCap = ini.get_value_float(5);
							sb.radius = ini.get_value_float(6);
							sb.explosionFuseId = CreateID(ini.get_value_string(7));
						}
					}
					if (FoundValue)
					{
						shieldBoostMap[currNickname] = sb;
					}
				}*/
				else if (ini.is_header("Explosion"))
				{
					ExplosionDamageData damageType;
					bool foundItem = false;
					while (ini.read_value())
					{
						if (ini.is_value("nickname"))
						{
							nickname = CreateID(ini.get_value_string());
						}
						else if (ini.is_value("weapon_type"))
						{
							damageType.weaponType = CreateID(ini.get_value_string(0));
							foundItem = true;
						}
						else if (ini.is_value("damage_solars"))
						{
							damageType.damageSolars = ini.get_value_bool(0);
							foundItem = true;
						}
						else if (ini.is_value("armor_pen"))
						{
							damageType.armorPen = ini.get_value_int(0);
							foundItem = true;
						}
						else if (ini.is_value("percentage_damage_hull"))
						{
							damageType.percentageDamageHull = ini.get_value_float(0);
							foundItem = true;
						}
						else if (ini.is_value("percentage_damage_shield"))
						{
							damageType.percentageDamageShield = ini.get_value_float(0);
							foundItem = true;
						}
						else if (ini.is_value("percentage_damage_energy"))
						{
							damageType.percentageDamageEnergy = ini.get_value_float(0);
							foundItem = true;
						}
						else if (ini.is_value("cruise_disruptor"))
						{
							damageType.cruiseDisrupt = ini.get_value_bool(0);
							foundItem = true;
						}
						else if (ini.is_value("destroy_missiles"))
						{
							damageType.missileDestroy = ini.get_value_bool(0);
							foundItem = true;
						}
					}
					if (foundItem)
					{
						global->explosionTypeMap[nickname] = damageType;
					}
				}
			}
			ini.close();
		}
	}

	void MineSpin(CMine* mine, Vector& spinVec)
	{
		auto mineInfo = global->mineInfoMap.find(mine->archetype->iArchId);
		if (mineInfo == global->mineInfoMap.end() || !mineInfo->second.stopSpin)
		{
			PhySys::AddToAngularVelocityOS(mine, spinVec);
		}
	}

	void MineImpulse(CMine* mine, Vector& launchVec)
	{
		auto mineInfo = global->mineInfoMap.find(mine->archetype->iArchId);
		if (mineInfo != global->mineInfoMap.end() && mineInfo->second.dispersionAngle > 0.0f)
		{
			Vector randVecAxis = RandomVector(1.0f);

			Vector vxp = Hk::Math::VectorCross(randVecAxis, launchVec);
			Vector vxvxp = Hk::Math::VectorCross(randVecAxis, vxp);

			float angle = mineInfo->second.dispersionAngle;
			angle *= rand() % 10000 / 10000.f;

			vxp = Hk::Math::VectorMultiply(vxp, sinf(angle));
			vxvxp = Hk::Math::VectorMultiply(vxvxp, 1.0f - cosf(angle));

			launchVec.x += vxp.x + vxvxp.x;
			launchVec.y += vxp.y + vxvxp.y;
			launchVec.z += vxp.z + vxvxp.z;
		}

		PhySys::AddToVelocity(mine, launchVec);
	}

	FireResult __fastcall CELauncherFire(CELauncher* gun, void*edx, const Vector& pos)
	{
		using CELAUNCHERFIRE = FireResult(__thiscall*)(CELauncher*, const Vector&);
		const static CELAUNCHERFIRE gunfirefunc = CELAUNCHERFIRE(0x62995C0);

		FireResult fireResult = gunfirefunc(gun, pos);
		if (fireResult != 9)
		{
			return fireResult;
		}

		if (gun->owner->objectClass == CObject::CSOLAR_OBJECT || gun->owner->ownerPlayer)
		{
			return fireResult;
		}

		auto shipDataIter = global->shipGunData.find(gun->owner->id);
		if (shipDataIter == global->shipGunData.end())
		{
			return fireResult;
		}

		auto gunData = shipDataIter->second.find(gun->iSubObjId);
		if (gunData == shipDataIter->second.end())
		{
			return fireResult;
		}

		if (--gunData->second.bulletsLeft == 0)
		{
			gunData->second.bulletsLeft = gunData->second.maxMagSize;
			gun->refireDelayElapsed = gunData->second.reloadTime;
		}

		return fireResult;
	}

	void __stdcall ShipDestroyed(IObjRW* iobj, bool isKill, uint killerId)
	{
		if (iobj->is_player())
		{
			return;
		}

		global->shipGunData.erase(iobj->get_id());
	}

	void CShipInit(CShip*& ship)
	{
		if (ship->ownerPlayer)
		{
			return;
		}

		CEquipTraverser tr(EquipmentClass::Gun);
		CELauncher* gun;
		while (gun = reinterpret_cast<CELauncher*>(ship->equip_manager.Traverse(tr)))
		{
			auto burstGunDataIter = global->burstGunData.find(gun->archetype->iArchId);
			if (burstGunDataIter == global->burstGunData.end())
			{
				continue;
			}

			global->shipGunData[ship->id][gun->iSubObjId] = 
			{ burstGunDataIter->second.magSize,burstGunDataIter->second.magSize, burstGunDataIter->second.reloadTime };
		}
	}

	void __fastcall GuidedInit(CGuided* guided, void* edx, CGuided::CreateParms& parms)
	{

		global->newMissileUpdateMap[parms.id] = { 0 };

		auto owner = Hk::Solar::GetInspect(parms.id);
		if (!owner.has_error())
		{
			IObjRW* ownerTarget = nullptr;
			owner.value()->get_target(ownerTarget);
			if (!ownerTarget)
			{
				parms.target = nullptr;
				parms.subObjId = 0;
			}
		}

		auto guidedData = global->guidedDataMap.find(guided->archetype->iArchId);
		if (guidedData == global->guidedDataMap.end())
		{
			return;
		}

		auto& guidedInfo = guidedData->second;

		if (guidedInfo.trackingBlacklist && parms.target)
		{
			if (parms.target->cobj->type & guidedInfo.trackingBlacklist)
			{
				parms.target = nullptr;
				parms.subObjId = 0;
			}
		}

		if (guidedInfo.topSpeed)
		{
			global->topSpeedWatch[parms.id] = { guidedInfo.topSpeed, 0 };
		}

	}

	bool __stdcall MineDestroyed(IObjRW* iobj, bool isKill, uint killerId)
	{
		CMine* mine = reinterpret_cast<CMine*>(iobj->cobj);
		Archetype::Mine* mineArch = reinterpret_cast<Archetype::Mine*>(mine->archetype);

		auto mineInfo = global->mineInfoMap.find(mineArch->iArchId);
		if (mineInfo != global-> mineInfoMap.end())
		{
			if (isKill && mineArch->fLifeTime - mine->remainingLifetime < mineInfo->second.armingTime)
			{
				pub::SpaceObj::Destroy(((CSimple*)iobj->cobj)->id, DestroyType::VANISH);
				return false;
			}

			if (!isKill && mineInfo->second.detonateOnEndLifetime)
			{
				pub::SpaceObj::Destroy(((CSimple*)iobj->cobj)->id, DestroyType::FUSE);
				return false;
			}
		}
		return true;
	}

	bool __stdcall GuidedDestroyed(IObjRW* iobj, bool isKill, uint killerId)
	{
		global->newMissileUpdateMap.erase(iobj->cobj->id);

		auto guidedInfo = global->guidedDataMap.find(iobj->cobj->archetype->iArchId);
		if (guidedInfo == global->guidedDataMap.end())
		{
			return true;
		}

		if (guidedInfo->second.armingTime)
		{
			float armingTime = guidedInfo->second.armingTime;
			CGuided* guided = reinterpret_cast<CGuided*>(iobj->cobj);
			if (guided->lifetime < armingTime)
			{
				pub::SpaceObj::Destroy(((CSimple*)iobj->cobj)->id, DestroyType::VANISH);
				return false;
			}
		}
		return true;
	}

	int Update()
	{
		for (auto iter = global->topSpeedWatch.begin(); iter != global->topSpeedWatch.end(); )
		{

			auto iGuided = Hk::Solar::GetInspect(iter->first);
			if (iGuided.has_error())
			{
				iter = global->topSpeedWatch.erase(iter);
				continue;
			}

			CGuided* guided = reinterpret_cast<CGuided*>(iGuided.value()->cobj);
			SpeedCheck& speedData = iter->second;

			Vector velocityVec = guided->get_velocity();
			float velocity = Hk::Math::SquaredVectorMagnitude(velocityVec);

			if (velocity > speedData.targetSpeed * 1.02f)
			{

				Hk::Math::ResizeVector(velocityVec, sqrt(speedData.targetSpeed));
				guided->motorData = nullptr;

				const uint physicsPtr = *reinterpret_cast<uint*>(PCHAR(*reinterpret_cast<uint*>(uint(guided) + 84)) + 152);
				Vector* linearVelocity = reinterpret_cast<Vector*>(physicsPtr + 164);
				*linearVelocity = velocityVec;

				iter = global->topSpeedWatch.erase(iter);
				continue;
			}
			iter++;
		}

		for (auto iter = global->newMissileUpdateMap.begin(); iter != global->newMissileUpdateMap.end();)
		{

			uint counter = ++iter->second;

			if (counter == 1)
			{
				iter++;
				continue;
			}

			uint id = iter->first;
			IObjRW* guided = nullptr;
			StarSystem* starSystem = nullptr;
			GetShipInspect(id, guided, starSystem);

			if (!guided)
			{
				iter = global->newMissileUpdateMap.erase(iter);
				continue;
			}

			SSPObjUpdateInfoSimple ssp;
			ssp.iShip = iter->first;
			ssp.vPos = guided->cobj->vPosition;
			ssp.vDir = Hk::Math::MatrixToQuaternion(guided->cobj->mOrientation);
			ssp.throttle = 0;
			ssp.state = 0;

			for (auto& observer : starSystem->observerList)
			{
				ssp.fTimestamp = static_cast<float>(observer.timestamp);

				HookClient->Send_FLPACKET_COMMON_UPDATEOBJECT(observer.clientId, ssp);
			}

			if (counter >= 3)
			{
				iter = global->newMissileUpdateMap.erase(iter);
			}
			else
			{
				iter++;
			}
		}

		return 0;
	}

} // namespace Plugins::Combatcontrol

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FLHOOK STUFF
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace Plugins::Combatcontrol;

// Do things when the dll is loaded
BOOL WINAPI DllMain([[maybe_unused]] const HINSTANCE& hinstDLL, [[maybe_unused]] const DWORD fdwReason, [[maybe_unused]] const LPVOID& lpvReserved)
{
	return true;
}

// Functions to hook
extern "C" EXPORT void ExportPluginInfo(PluginInfo* pi)
{
	pi->name("combatcontrol");
	pi->shortName("combatcontrol");
	pi->mayUnload(false);
	pi->returnCode(&global->returnCode);
	pi->emplaceHook(HookedCall::FLHook__LoadSettings, &LoadSettings, HookStep::After);
	pi->emplaceHook(HookedCall::IEngine__CShip__Init, &CShipInit, HookStep::Before);
	pi->emplaceHook(HookedCall::IServerImpl__Update, &Update, HookStep::Before);
	pi->versionMajor(PluginMajorVersion::VERSION_04);
	pi->versionMinor(PluginMinorVersion::VERSION_00);
}