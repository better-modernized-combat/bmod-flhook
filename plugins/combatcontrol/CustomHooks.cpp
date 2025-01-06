#include "Combatcontrol.h"

FARPROC ShipDestroyedOrigFunc;
FARPROC CELauncherFireOrigFunc;
FARPROC fpOldExplosionHit;

__declspec(naked) void ShipDestroyedNaked()
{
	__asm
	{
		push ecx
		push[esp + 0xC]
		push[esp + 0xC]
		push ecx
		call Plugins::Combatcontrol::ShipDestroyed
		pop ecx
		mov eax, [ShipDestroyedOrigFunc]
		jmp eax
	}
}

constexpr uint CGuidedInitRetAddr = 0x62ACCB6;
__declspec(naked) void CGuidedInitNaked()
{
	__asm {
		push ecx
		push[esp + 0x8]
		call Plugins::Combatcontrol::GuidedInit
		pop ecx
		push esi
		push edi
		mov edi, [esp + 0xC]
		jmp CGuidedInitRetAddr
	}
}

FARPROC MineDestroyedOrigFunc;
__declspec(naked) void MineDestroyedNaked()
{
	__asm {
		push ecx
		push[esp + 0xC]
		push[esp + 0xC]
		push ecx
		call Plugins::Combatcontrol::MineDestroyed
		pop ecx
		test al, al
		jz skipLabel
		mov eax, [MineDestroyedOrigFunc]
		jmp eax
		skipLabel :
		ret 0x8
	}
}

FARPROC GuidedDestroyedOrigFunc;
__declspec(naked) void GuidedDestroyedNaked()
{

	__asm {
		push ecx
		push[esp + 0xC]
		push[esp + 0xC]
		push ecx
		call Plugins::Combatcontrol::GuidedDestroyed
		pop ecx
		test al, al
		jz skipLabel
		mov eax, [GuidedDestroyedOrigFunc]
		jmp eax
		skipLabel :
		ret 0x8
	}
}

// patch stuff
struct PATCH_INFO_ENTRY
{
	ulong pAddress;
	void* pNewValue;
	uint iSize;
	void* pOldValue;
	bool bAlloced;
};

struct PATCH_INFO
{
	const char* szBinName;
	ulong	pBaseAddress;

	PATCH_INFO_ENTRY piEntries[128];
};

PATCH_INFO piServerDLL =
{
	"server.dll", 0x6CE0000,
	{
		{0x6D661C4,			&MineDestroyedNaked,		4, &MineDestroyedOrigFunc,		false},
		{0x6D67274,			&ShipDestroyedNaked,		4, &ShipDestroyedOrigFunc,		false},
		{0x6D66694,			&GuidedDestroyedNaked,		4, &GuidedDestroyedOrigFunc,	false},
		{0x6D672A0,			&HookExplosionHitNaked,		4, &fpOldExplosionHit,		false},
		//{0x6D67330,		&ShipShieldDamageNaked,		4, &ShipShieldDamageOrigFunc,	false},
		//{0x6D666C0,		&GuidedExplosionHitNaked,	4, &GuidedExplosionHitOrigFunc, false},
		//{0x6D675F0,		&SolarExplosionHitNaked,	4, &SolarExplosionHitOrigFunc, false},
		//{0x6D6729C,		&ShipMunitionHit,			4, &ShipMunitionHitOrigFunc, false},
		//{0x6D67334,		&ShipColGrpDmgNaked,		4, &ShipColGrpDmgFunc, false},
		//{0x6D67300,		&ShipFuseLightNaked,		4, &ShipFuseLightFunc, false},

		{0,0,0,0} // terminate
	}
};

PATCH_INFO piCommonDLL =
{
	"common.dll", 0x6260000,
	{
		{0x639CF74,			&Plugins::Combatcontrol::CELauncherFire,		4, &CELauncherFireOrigFunc,	false},
		{0x0639D03C,		&Plugins::Combatcontrol::CELauncherFire,		4, nullptr,	false},
		{0x639D104,			&Plugins::Combatcontrol::CELauncherFire,		4, nullptr,	false},
		//{0x6D666C0,		&GuidedExplosionHitNaked,	4, &GuidedExplosionHitOrigFunc, false},
		//{0x6D675F0,		&SolarExplosionHitNaked,	4, &SolarExplosionHitOrigFunc, false},
		//{0x6D6729C,		&ShipMunitionHit,			4, &ShipMunitionHitOrigFunc, false},
		//{0x6D67334,		&ShipColGrpDmgNaked,		4, &ShipColGrpDmgFunc, false},
		//{0x6D67300,		&ShipFuseLightNaked,		4, &ShipFuseLightFunc, false},

		{0,0,0,0} // terminate
	}
};

bool Patch(PATCH_INFO& pi)
{
	HMODULE hMod = GetModuleHandle(pi.szBinName);
	if (!hMod)
		return false;

	for (uint i = 0; (i < sizeof(pi.piEntries) / sizeof(PATCH_INFO_ENTRY)); i++)
	{
		if (!pi.piEntries[i].pAddress)
			break;

		char* pAddress = (char*)hMod + (pi.piEntries[i].pAddress - pi.pBaseAddress);
		if (!pi.piEntries[i].pOldValue) {
			pi.piEntries[i].pOldValue = new char[pi.piEntries[i].iSize];
			pi.piEntries[i].bAlloced = true;
		}
		else
			pi.piEntries[i].bAlloced = false;

		ReadProcMem(pAddress, pi.piEntries[i].pOldValue, pi.piEntries[i].iSize);
		WriteProcMem(pAddress, &pi.piEntries[i].pNewValue, pi.piEntries[i].iSize);
	}

	return true;
}

void Detour(void* pOFunc, void* pHkFunc)
{
	DWORD dwOldProtection = 0; // Create a DWORD for VirtualProtect calls to allow us to write.
	BYTE bPatch[5]; // We need to change 5 bytes and I'm going to use memcpy so this is the simplest way.
	bPatch[0] = 0xE9; // Set the first byte of the byte array to the op code for the JMP instruction.
	VirtualProtect(pOFunc, 5, PAGE_EXECUTE_READWRITE, &dwOldProtection); // Allow us to write to the memory we need to change
	DWORD dwRelativeAddress = (DWORD)pHkFunc - (DWORD)pOFunc - 5; // Calculate the relative JMP address.
	memcpy(&bPatch[1], &dwRelativeAddress, 4); // Copy the relative address to the byte array.
	memcpy(pOFunc, bPatch, 5); // Change the first 5 bytes to the JMP instruction.
	VirtualProtect(pOFunc, 5, dwOldProtection, 0); // Set the protection back to what it was.
}

void Plugins::Combatcontrol::LoadHookOverrides()
{
	Patch(piServerDLL);
	Patch(piCommonDLL);

	auto hCommon = GetModuleHandleA("common");
	PatchCallAddr((char*)hCommon, 0x376E0, (char*)CELauncherFire);

	PatchCallAddr((char*)hCommon, 0x4CB81, (char*)MineSpin);
	PatchCallAddr((char*)hCommon, 0x4CAF1, (char*)MineImpulse);

	FARPROC CGuidedInit = FARPROC(0x62ACCB0);
	Detour(CGuidedInit, CGuidedInitNaked);
}