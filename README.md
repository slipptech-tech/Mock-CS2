<div align="center">

# MOCK CS2

[![Open Source](https://img.shields.io/badge/OPEN%20SOURCE-Yes-brightgreen?style=for-the-badge&logo=github)](https://github.com/slipptech-tech/BitLauncher)
[![C++](https://img.shields.io/badge/C%2B%2B-Project-blue?style=for-the-badge&logo=cplusplus&logoColor=white)](https://isocpp.org/)
[![MIT License](https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge&logo=opensourceinitiative&logoColor=white)](https://github.com/slipptech-tech/BitLauncher/blob/main/LICENSE)

</div>

For proper operation, we recommend using OpenCode. Provide the full path to your cs2.exe executable so the tool can identify the target file and assist with analyzing the offsets used by your project.

Instructions:


[Download Node.js](https://nodejs.org/dist/v22.23.3/node-v22.23.3-x64.msi) before running the project. if u want a correct version of opencode without unnecessary bugs

Open Cmd and run: npm install -g @opencode/cli

Install and launch OpenCode.

Specify the full path to cs2.exe.

Run the verification process for the offsets used by your project.

Review the results and identify any values that may need updating after a Counter-Strike 2 update.

Important: Verification results depend on the current game version and the analysis methods used. After each CS2 update, recheck your offsets to help prevent errors and ensure compatibility.

For reliable results, use an up-to-date tool and carefully review the output before applying any changes to your project.

How it works? 

When you give OpenCode this file, it starts analyzing it. 
Basically, it has a structure like CS2, with different offsets for validity checks like in the original CS2.

### 

```cpp
const u32 dwEntityList            = 40990760;
const u32 dwLocalPlayerPawn       = 39200776;
const u32 dwLocalPlayerController = 39034984;
const u32 dwViewMatrix            = 39223200;

const u32 m_iTeamNum          = 999;
const u32 m_iHealth            = 844;
const u32 m_lifeState           = 852;
const u32 m_fFlags              = 1012;
const u32 m_vecVelocity         = 1072;
const u32 m_pGameSceneNode      = 816;

const u32 m_vecOrigin            = 128;
const u32 m_vecAbsOrigin         = 200;

const u32 m_ArmorValue           = 7884;
const u32 m_bIsScoped             = 7840;
const u32 m_bIsDefusing           = 7842;
const u32 m_entitySpottedState    = 7816;
const u32 m_angEyeAngles          = 13808;
const u32 m_pWeaponServices       = 4848;
const u32 m_pMovementServices     = 4912;
const u32 m_flFlashDuration       = 5392;

const u32 m_iClip1                = 6440;
const u32 m_pReserveAmmo          = 6448;
const u32 m_AttributeManager      = 4752;
const u32 m_iItemDefinitionIndex  = 442;
const u32 m_hActiveWeapon         = 96;
const u32 m_iAmmo                 = 104;
const u32 m_bInReload             = 6716;

const u32 m_hPlayerPawn           = 2348;
const u32 m_iPawnBotDifficulty    = 2380;
const u32 m_pInGameMoneyServices  = 2072;
const u32 m_iPawnHealth           = 2360;
const u32 m_iPawnArmor            = 2364;
const u32 m_bPawnHasHelmet        = 2369;
const u32 m_bPawnIsAlive          = 2356;
const u32 m_iszPlayerName          = 1788;
const u32 m_iAccount              = 64;

const u32 m_vecViewOffset         = 3936;
const u32 m_hModel                = 160;

// What PLAG's runtime probe has to discover on its own
const u32 nodeModelState = 0x60;
const u32 modelBoneArray = 0x40;

} // namespace off

u8* clientBase = nullptr;

// CS2 keeps its entity handles as (address & 0x7FFFFFFF), which only round
// trips when the entity lives below 2 GB. The mock therefore carves its
// allocations out of a low region instead of whatever VirtualAlloc hands out.

u8* g_heap = nullptr;
size_t g_heapUsed = 0;

const u64 kHeapBase = 0x20000000ull;
const u64 kHeapSize = 128ull * 1024 * 1024;

u8* alloc(size_t bytes) {
    if (!g_heap) {
        void* p = VirtualAlloc(
            (void*)kHeapBase,
            kHeapSize,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
        );

        if (!p) {
            printf("heap reserve failed (%lu)\n", GetLastError());
            ExitProcess(1);
        }

        g_heap = static_cast<u8*>(p);
    }

    const size_t aligned = (bytes + 0xFFF) & ~(size_t)0xFFF;

    if (g_heapUsed + aligned > kHeapSize) {
        printf("mock heap exhausted\n");
        ExitProcess(1);
    }

    u8* block = g_heap + g_heapUsed;
    g_heapUsed += aligned;

    std::memset(block, 0, bytes);

    return block;
}

template <typename T>
void put(u8* base, u32 offset, const T& value) {
    std::memcpy(base + offset, &value, sizeof(T));
}
```

# LICENSED UNDER MIT-LICENSE
