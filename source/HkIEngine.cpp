#include "Global.hpp"
#include <unordered_set>

/**************************************************************************************************************
// misc flserver engine function hooks
**************************************************************************************************************/

namespace IEngineHook
{

	/**************************************************************************************************************
	// ship create & destroy
	**************************************************************************************************************/

	FARPROC g_OldInitCShip;

	void __stdcall CShip__Init(CShip* ship)
	{
		CallPluginsAfter(HookedCall::IEngine__CShip__Init, ship);
	}

	__declspec(naked) void Naked__CShip__Init()
	{
		__asm {
        push ecx
        push [esp+8]
        call g_OldInitCShip
        call CShip__Init
        ret 4
		}
	}

	FARPROC g_OldDestroyCShip;

	void __stdcall CShip__Destroy(CShip* ship)
	{
		CallPluginsBefore(HookedCall::IEngine__CShip__Destroy, ship);
	}

	__declspec(naked) void Naked__CShip__Destroy()
	{
		__asm {
        push ecx
        push ecx
        call CShip__Destroy
        pop ecx
        jmp g_OldDestroyCShip
		}
	}

	/**************************************************************************************************************
	// flserver memory leak bugfix
	**************************************************************************************************************/

	int __cdecl FreeReputationVibe(int const& p1)
	{
		__asm {
        mov eax, p1
        push eax
        mov eax, [hModServer]
        add eax, 0x65C20
        call eax
        add esp, 4
		}

		return Reputation::Vibe::Free(p1);
	}

	/**************************************************************************************************************
	**************************************************************************************************************/

	void __cdecl UpdateTime(double interval)
	{
		CallPluginsBefore(HookedCall::IEngine__UpdateTime, interval);

		Timing::UpdateGlobalTime(interval);

		CallPluginsAfter(HookedCall::IEngine__UpdateTime, interval);
	}

	/**************************************************************************************************************
	**************************************************************************************************************/

	uint g_LastTicks = 0;

	static void* dummy;
	void __stdcall ElapseTime(float interval)
	{
		CallPluginsBefore(HookedCall::IEngine__ElapseTime, interval);

		dummy = &Server;
		Server.ElapseTime(interval);

		CallPluginsAfter(HookedCall::IEngine__ElapseTime, interval);

		// low server load missile jitter bug fix
		uint curLoad = GetTickCount() - g_LastTicks;
		if (curLoad < 5)
		{
			uint fakeLoad = 5 - curLoad;
			Sleep(fakeLoad);
		}
		g_LastTicks = GetTickCount();
	}

	/**************************************************************************************************************
	**************************************************************************************************************/

	int __cdecl DockCall(const uint& shipId, const uint& spaceId, int dockPortIndex, DOCK_HOST_RESPONSE response)
	{
		//	dockPortIndex == -1, response -> 2 --> Dock Denied!
		//	dockPortIndex == -1, response -> 3 --> Dock in Use
		//	dockPortIndex != -1, response -> 4 --> Dock ok, proceed (dockPortIndex starts from 0 for docking point 1)
		//	dockPortIndex == -1, response -> 5 --> now DOCK!

		CallPluginsBefore(HookedCall::IEngine__DockCall, shipId, spaceId, dockPortIndex, response);

		int retVal = 0;
		TRY_HOOK
		{
			// Print out a message when a player ship docks.
			if (FLHookConfig::c()->messages.dockingMessages && response == PROCEED_DOCK)
			{
				const auto client = Hk::Client::GetClientIdByShip(shipId);
				if (client.has_value())
				{
					std::wstring wscMsg = L"Traffic control alert: %player has requested to dock";
					wscMsg = ReplaceStr(wscMsg, L"%player", (const wchar_t*)Players.GetActiveCharacterName(client.value()));
					PrintLocalUserCmdText(client.value(), wscMsg, 15000);
				}
			}
			// Actually dock
			retVal = pub::SpaceObj::Dock(shipId, spaceId, dockPortIndex, response);
		}
		CATCH_HOOK({})

		CallPluginsAfter(HookedCall::IEngine__DockCall, shipId, spaceId, dockPortIndex, response);

		return retVal;
	}

	/**************************************************************************************************************
	**************************************************************************************************************/

	FARPROC g_OldLaunchPosition;

	bool __stdcall LaunchPosition(uint spaceId, struct CEqObj& obj, Vector& position, Matrix& orientation, int dock)
	{
		auto [retVal, skip] =
		    CallPluginsBefore<bool>(HookedCall::IEngine__LaunchPosition, spaceId, obj, position, orientation, dock);
		if (skip)
			return retVal;

		return obj.launch_pos(position, orientation, dock);
	}

	__declspec(naked) void Naked__LaunchPosition()
	{
		__asm { 
        push ecx // 4
        push [esp+8+8] // 8
        push [esp+12+4] // 12
        push [esp+16+0] // 16
        push ecx
        push [ecx+176]
        call LaunchPosition	
        pop ecx
        ret 0x0C
		}
	}

	/**************************************************************************************************************
	**************************************************************************************************************/

	struct LOAD_REP_DATA
	{
		uint iRepId;
		float fAttitude;
	};

	struct REP_DATA_LIST
	{
		uint iDunno;
		LOAD_REP_DATA* begin;
		LOAD_REP_DATA* end;
	};

	bool __stdcall LoadReputationFromCharacterFile(REP_DATA_LIST* savedReps, LOAD_REP_DATA* repToSave)
	{
		// check of the rep id is valid
		if (repToSave->iRepId == 0xFFFFFFFF)
			return false; // rep id not valid!

		LOAD_REP_DATA* repIt = savedReps->begin;

		while (repIt != savedReps->end)
		{
			if (repIt->iRepId == repToSave->iRepId)
				return false; // we already saved this rep!

			repIt++;
		}

		// everything seems fine, add
		return true;
	}

	FARPROC g_OldLoadReputationFromCharacterFile;

	__declspec(naked) void Naked__LoadReputationFromCharacterFile()
	{
		__asm {
        push ecx // save ecx because thiscall
        push [esp+4+4+8] // rep data
        push ecx // rep data list
        call LoadReputationFromCharacterFile
        pop ecx // recover ecx
        test al, al
        jz abort_lbl
        jmp [g_OldLoadReputationFromCharacterFile]
abort_lbl:
        ret 0x0C
		}
	}

	struct StarSystemMock
	{
		uint systemId;
		StarSystem starSystem;
	};

	struct iobjCache
	{
		StarSystem* cacheStarSystem;
		CObject::Class objClass;
	};

	std::unordered_set<uint> playerShips;
	std::unordered_map<uint, iobjCache> cacheSolarIObjs;
	std::unordered_map<uint, iobjCache> cacheNonsolarIObjs;

	FARPROC FindStarListRet = FARPROC(0x6D0C846);

	PBYTE fpOldStarSystemFind;

	typedef MetaListNode* (__thiscall* FindIObjOnList)(MetaList&, uint searchedId);
	FindIObjOnList FindIObjOnListFunc = FindIObjOnList(0x6CF4F00);

	typedef IObjRW* (__thiscall* FindIObjInSystem)(StarSystemMock& starSystem, uint searchedId);
	FindIObjInSystem FindIObjFunc = FindIObjInSystem(0x6D0C840);

	IObjRW* FindNonSolar(StarSystemMock* starSystem, uint searchedId)
	{
		MetaListNode* node = FindIObjOnListFunc(starSystem->starSystem.shipList, searchedId);
		if (node)
		{
			cacheNonsolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		node = FindIObjOnListFunc(starSystem->starSystem.lootList, searchedId);
		if (node)
		{
			cacheNonsolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		node = FindIObjOnListFunc(starSystem->starSystem.guidedList, searchedId);
		if (node)
		{
			cacheNonsolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		node = FindIObjOnListFunc(starSystem->starSystem.mineList, searchedId);
		if (node)
		{
			cacheNonsolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		node = FindIObjOnListFunc(starSystem->starSystem.counterMeasureList, searchedId);
		if (node)
		{
			cacheNonsolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		return nullptr;
	}

	IObjRW* FindSolar(StarSystemMock* starSystem, uint searchedId)
	{
		MetaListNode* node = FindIObjOnListFunc(starSystem->starSystem.solarList, searchedId);
		if (node)
		{
			cacheSolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		node = FindIObjOnListFunc(starSystem->starSystem.asteroidList, searchedId);
		if (node)
		{
			cacheSolarIObjs[searchedId] = { node->value->starSystem, node->value->cobj->objectClass };
			return node->value;
		}
		return nullptr;
	}

	IObjRW* __stdcall FindInStarList(StarSystemMock* starSystem, uint searchedId)
	{
		static StarSystem* lastFoundInSystem = nullptr;
		static uint lastFoundItem = 0;
		IObjRW* retVal = nullptr;

		if (searchedId == 0)
		{
			return nullptr;
		}

		if (lastFoundItem == searchedId && lastFoundInSystem != &starSystem->starSystem)
		{
			return nullptr;
		}

		if (searchedId & 0x80000000) // check if solar
		{
			auto iter = cacheSolarIObjs.find(searchedId);
			if (iter == cacheSolarIObjs.end())
			{
				return FindSolar(starSystem, searchedId);
			}

			if (iter->second.cacheStarSystem != &starSystem->starSystem)
			{
				lastFoundItem = searchedId;
				lastFoundInSystem = iter->second.cacheStarSystem;
				return nullptr;
			}

			MetaListNode* node;
			switch (iter->second.objClass)
			{
			case CObject::Class::CSOLAR_OBJECT:
				node = FindIObjOnListFunc(starSystem->starSystem.solarList, searchedId);
				if (node)
				{
					retVal = node->value;
				}
				break;
			case CObject::Class::CASTEROID_OBJECT:
				node = FindIObjOnListFunc(starSystem->starSystem.asteroidList, searchedId);
				if (node)
				{
					retVal = node->value;
				}
				break;
			}
		}
		else
		{
			if (!playerShips.count(searchedId)) // player can swap systems, for them search just the system's shiplist
			{
				auto iter = cacheNonsolarIObjs.find(searchedId);
				if (iter == cacheNonsolarIObjs.end())
				{
					return FindNonSolar(starSystem, searchedId);
				}

				if (iter->second.cacheStarSystem != &starSystem->starSystem)
				{
					lastFoundItem = searchedId;
					lastFoundInSystem = iter->second.cacheStarSystem;
					return nullptr;
				}

				MetaListNode* node;
				switch (iter->second.objClass)
				{
				case CObject::Class::CSHIP_OBJECT:
					node = FindIObjOnListFunc(starSystem->starSystem.shipList, searchedId);
					if (node)
					{
						retVal = node->value;
					}
					break;
				case CObject::Class::CLOOT_OBJECT:
					node = FindIObjOnListFunc(starSystem->starSystem.lootList, searchedId);
					if (node)
					{
						retVal = node->value;
					}
					break;
				case CObject::Class::CGUIDED_OBJECT:
					node = FindIObjOnListFunc(starSystem->starSystem.guidedList, searchedId);
					if (node)
					{
						retVal = node->value;
					}
					break;
				case CObject::Class::CMINE_OBJECT:
					node = FindIObjOnListFunc(starSystem->starSystem.mineList, searchedId);
					if (node)
					{
						retVal = node->value;
					}
					break;
				case CObject::Class::CCOUNTERMEASURE_OBJECT:
					node = FindIObjOnListFunc(starSystem->starSystem.counterMeasureList, searchedId);
					if (node)
					{
						retVal = node->value;
					}
					break;
				}
			}
			else
			{
				MetaListNode* node = FindIObjOnListFunc(starSystem->starSystem.shipList, searchedId);
				if (node)
				{
					retVal = node->value;
				}
			}
		}

		return retVal;
	}

	__declspec(naked) void FindInStarListNaked()
	{
		__asm
		{
			push ecx
			push[esp + 0x8]
			sub ecx, 4
			push ecx
			call FindInStarList
			pop ecx
			ret 0x4
		}
	}

	void __stdcall GameObjectDestructor(uint id)
	{
		if (id & 0x80000000)
		{
			cacheSolarIObjs.erase(id);
		}
		else
		{
			cacheNonsolarIObjs.erase(id);
		}
	}

	uint GameObjectDestructorRet = 0x6CEE4A7;
	__declspec(naked) void GameObjectDestructorNaked()
	{
		__asm {
			push ecx
			mov ecx, [ecx + 0x4]
			mov ecx, [ecx + 0xB0]
			push ecx
			call GameObjectDestructor
			pop ecx
			push 0xFFFFFFFF
			push 0x6d60776
			jmp GameObjectDestructorRet
		}
	}

	struct CObjNode
	{
		CObjNode* next;
		CObjNode* prev;
		CObject* cobj;
	};

	struct CObjEntryNode
	{
		CObjNode* first;
		CObjNode* last;
	};

	struct CObjList
	{
		uint dunno;
		CObjEntryNode* entry;
		uint size;
	};

	std::unordered_map<CObject*, CObjNode*> CMineMap;
	std::unordered_map<CObject*, CObjNode*> CCmMap;
	std::unordered_map<CObject*, CObjNode*> CBeamMap;
	std::unordered_map<CObject*, CObjNode*> CGuidedMap;
	std::unordered_map<CObject*, CObjNode*> CSolarMap;
	std::unordered_map<CObject*, CObjNode*> CShipMap;
	std::unordered_map<CObject*, CObjNode*> CAsteroidMap;
	std::unordered_map<CObject*, CObjNode*> CLootMap;
	std::unordered_map<CObject*, CObjNode*> CEquipmentMap;
	std::unordered_map<CObject*, CObjNode*> CObjectMap;

	std::unordered_map<uint, CSimple*> CAsteroidMap2;

	typedef CObjList* (__cdecl* CObjListFunc)(CObject::Class);
	CObjListFunc CObjListFind = CObjListFunc(0x62AE690);

	typedef void(__thiscall* RemoveCobjFromVector)(CObjList*, void*, CObjNode*);
	RemoveCobjFromVector removeCObjNode = RemoveCobjFromVector(0x62AF830);

	uint CObjAllocJmp = 0x62AEE55;
	__declspec(naked) CSimple* __cdecl CObjAllocCallOrig(CObject::Class objClass)
	{
		__asm {
			push ecx
			mov eax, [esp + 8]
			jmp CObjAllocJmp
		}
	}

	void __fastcall CObjDestr(CObject* cobj)
	{
		std::unordered_map<CObject*, CObjNode*>* cobjMap;
		switch (cobj->objectClass) {
		case CObject::CASTEROID_OBJECT:
			CAsteroidMap2.erase(reinterpret_cast<CSimple*>(cobj)->id);
			cobjMap = &CAsteroidMap;
			break;
		case CObject::CEQUIPMENT_OBJECT:
			cobjMap = &CEquipmentMap;
			break;
		case CObject::COBJECT_MASK:
			cobjMap = &CObjectMap;
			break;
		case CObject::CSOLAR_OBJECT:
			cobjMap = &CSolarMap;
			break;
		case CObject::CSHIP_OBJECT:
			cobjMap = &CShipMap;
			break;
		case CObject::CLOOT_OBJECT:
			cobjMap = &CLootMap;
			break;
		case CObject::CBEAM_OBJECT:
			cobjMap = &CBeamMap;
			break;
		case CObject::CGUIDED_OBJECT:
			cobjMap = &CGuidedMap;
			break;
		case CObject::CCOUNTERMEASURE_OBJECT:
			cobjMap = &CCmMap;
			break;
		case CObject::CMINE_OBJECT:
			cobjMap = &CMineMap;
			break;
		}

		auto item = cobjMap->find(cobj);
		if (item != cobjMap->end())
		{
			CObjList* cobjList = CObjListFind(cobj->objectClass);
			cobjMap->erase(item);
			static uint dummy;
			removeCObjNode(cobjList, &dummy, item->second);
		}
	}

	uint CObjDestrRetAddr = 0x62AF447;
	__declspec(naked) void CObjDestrOrgNaked()
	{
		__asm {
			push ecx
			call CObjDestr
			pop ecx
			push 0xFFFFFFFF
			push 0x06394364
			jmp CObjDestrRetAddr
		}
	}

	CObject* __cdecl CObjAllocDetour(CObject::Class objClass)
	{
		CSimple* retVal = CObjAllocCallOrig(objClass);
		CObjList* cobjList = CObjListFind(objClass);

		switch (objClass)
		{
		case CObject::CASTEROID_OBJECT:
			CAsteroidMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CEQUIPMENT_OBJECT:
			CEquipmentMap[retVal] = cobjList->entry->last;
			break;
		case CObject::COBJECT_MASK:
			CObjectMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CSOLAR_OBJECT:
			CSolarMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CSHIP_OBJECT:
			CShipMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CLOOT_OBJECT:
			CLootMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CBEAM_OBJECT:
			CBeamMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CGUIDED_OBJECT:
			CGuidedMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CCOUNTERMEASURE_OBJECT:
			CCmMap[retVal] = cobjList->entry->last;
			break;
		case CObject::CMINE_OBJECT:
			CMineMap[retVal] = cobjList->entry->last;
			break;
		}

		return retVal;
	}

	CObject* __cdecl CObjectFindDetour(const uint& spaceObjId, CObject::Class objClass)
	{
		if (objClass != CObject::CASTEROID_OBJECT)
		{
			return CObject::Find(spaceObjId, objClass);
		}

		auto result = CAsteroidMap2.find(spaceObjId);
		if (result != CAsteroidMap2.end())
		{
			++result->second->referenceCounter;
			return result->second;
		}
		return nullptr;
	}

	void __fastcall CAsteroidInit(CSimple* csimple, void* edx, CSimple::CreateParms& param)
	{
		CAsteroidMap2[param.id] = csimple;
	}

	constexpr uint CAsteroidInitRetAddr = 0x62A28F6;
	__declspec(naked) void CAsteroidInitNaked()
	{
		__asm {
			push ecx
			push[esp + 0x8]
			call CAsteroidInit
			pop ecx
			push esi
			push edi
			mov edi, [esp + 0xC]
			jmp CAsteroidInitRetAddr
		}
	}

	/**************************************************************************************************************
	**************************************************************************************************************/

} // namespace IEngine
