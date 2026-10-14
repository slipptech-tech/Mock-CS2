// PLAG integration harness - a fake cs2.exe.
//
// CS2 is not required to verify the overlay: this process builds the exact
// memory layout the real game exposes (module relative globals inside a
// client.dll sized image, an entity list of handles, controllers, pawns,
// weapons and a bone matrix array) using the very same offsets PLAG downloads
// from the dumper. Starting it and then starting PLAG exercises the complete
// game layer: attach, module resolution, entity list stride probing, pawn
// classification, controller mapping, weapon chain, bone chain probing, world to
// screen projection and every visual that draws from that state.
//
// It is a test tool, not part of the cheat.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

namespace {

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using f32 = float;

// ---------------------------------------------------------------------------
// offsets as published by the a2x/cs2-dumper output (client.dll)
namespace off {
const u32 dwEntityList           = 40990760;
const u32 dwLocalPlayerPawn      = 39200776;
const u32 dwLocalPlayerController = 39034984;
const u32 dwViewMatrix           = 39223200;

const u32 m_iTeamNum = 999, m_iHealth = 844, m_lifeState = 852, m_fFlags = 1012;
const u32 m_vecVelocity = 1072, m_pGameSceneNode = 816;

const u32 m_vecOrigin = 128, m_vecAbsOrigin = 200;

const u32 m_ArmorValue = 7884, m_bIsScoped = 7840, m_bIsDefusing = 7842;
const u32 m_entitySpottedState = 7816, m_angEyeAngles = 13808, m_pWeaponServices = 4848;
const u32 m_pMovementServices = 4912;
const u32 m_flFlashDuration = 5392;

const u32 m_iClip1 = 6440, m_pReserveAmmo = 6448, m_AttributeManager = 4752;
const u32 m_iItemDefinitionIndex = 442, m_hActiveWeapon = 96, m_iAmmo = 104, m_bInReload = 6716;

const u32 m_hPlayerPawn = 2348, m_iPawnBotDifficulty = 2380, m_pInGameMoneyServices = 2072;
const u32 m_iPawnHealth = 2360, m_iPawnArmor = 2364, m_bPawnHasHelmet = 2369;
const u32 m_bPawnIsAlive = 2356, m_iszPlayerName = 1788, m_iAccount = 64;

const u32 m_vecViewOffset = 3936, m_hModel = 160;

// what PLAG's runtime probe has to discover on its own
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
        void* p = VirtualAlloc((void*)kHeapBase, kHeapSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!p) { printf("heap reserve failed (%lu)\n", GetLastError()); ExitProcess(1); }
        g_heap = static_cast<u8*>(p);
    }
    const size_t aligned = (bytes + 0xFFF) & ~(size_t)0xFFF;
    if (g_heapUsed + aligned > kHeapSize) { printf("mock heap exhausted\n"); ExitProcess(1); }
    u8* block = g_heap + g_heapUsed;
    g_heapUsed += aligned;
    std::memset(block, 0, bytes);
    return block;
}

template <typename T>
void put(u8* base, u32 offset, const T& value) {
    std::memcpy(base + offset, &value, sizeof(T));
}

// ---------------------------------------------------------------------------
struct Mat4 { f32 m[4][4]; };

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            f32 sum = 0.f;
            for (int k = 0; k < 4; ++k) sum += a.m[r][k] * b.m[k][c];
            out.m[r][c] = sum;
        }
    return out;
}

struct Vec3 { f32 x, y, z; };

Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
f32   dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
Vec3 normalize(const Vec3& v) {
    const f32 len = std::sqrt(dot(v, v));
    return len > 0.f ? Vec3{ v.x / len, v.y / len, v.z / len } : Vec3{ 0, 0, 0 };
}

// right handed view matrix with -Z forward, row major, translation in column 3
Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& upHint) {
    const Vec3 f = normalize(target - eye);
    const Vec3 r = normalize(cross(upHint, f));
    const Vec3 u = cross(f, r);
    Mat4 v{};
    v.m[0][0] = r.x;  v.m[0][1] = r.y;  v.m[0][2] = r.z;  v.m[0][3] = -dot(r, eye);
    v.m[1][0] = u.x;  v.m[1][1] = u.y;  v.m[1][2] = u.z;  v.m[1][3] = -dot(u, eye);
    v.m[2][0] = -f.x; v.m[2][1] = -f.y; v.m[2][2] = -f.z; v.m[2][3] = dot(f, eye);
    v.m[3][0] = 0;    v.m[3][1] = 0;    v.m[3][2] = 0;    v.m[3][3] = 1;
    return v;
}

Mat4 perspective(f32 fovY, f32 aspect, f32 zNear, f32 zFar) {
    const f32 t = 1.f / std::tan(fovY * 0.5f);
    Mat4 p{};
    p.m[0][0] = t / aspect;
    p.m[1][1] = t;
    p.m[2][2] = (zFar + zNear) / (zNear - zFar);
    p.m[2][3] = (2.f * zFar * zNear) / (zNear - zFar);
    p.m[3][2] = -1.f;
    return p;
}

// ---------------------------------------------------------------------------
struct MockPlayer {
    u8* controller = nullptr;
    u8* pawn       = nullptr;
    u8* weapon     = nullptr;
    u8* econ       = nullptr;
    u8* ammoBlock  = nullptr;
    u8* services   = nullptr;
    u8* node       = nullptr;
    u8* modelState = nullptr;
    u8* model      = nullptr;
    u8* bones      = nullptr;
    u8* nameBuffer = nullptr;
    u8* money      = nullptr;
    Vec3 origin{};
    Vec3 velocity{};
    int  team = 0;
    int  health = 100;
    int  armor = 0;
    bool alive = true;
    bool bot = false;
    bool flashed = false;
    bool spotted = true;
    u16  weaponId = 7;
    f32  phase = 0.f;
    float speed = 1.f;
};

const int kPlayerCount = 20;
MockPlayer g_players[kPlayerCount];
u64*       g_handleArray = nullptr;
u32        g_handleCount = 0;

const char* kNames[kPlayerCount] = {
    "LocalPlayer", "Vitaliy", "s1mple", "ZywOo", "device", "s1mple2", "blameF", "rain", "n0rb3r7",
    "Magisk", "Snappi", "tabseN", "coldzera", "dupreeh", "ropz", "karrigan", "HooXi", "JUICE",
    "m0NESY", "woxic"
};

void buildSkeleton(u8* bones) {
    // source2 style bone ids, translation only matrices
    struct Bone { int index; f32 x, y, z; };
    static const Bone layout[] = {
        { 0,   0.f,   0.f,  38.f },   // pelvis
        { 1,   0.f,   0.f,  44.f },   // spine 1
        { 2,   0.f,   0.f,  50.f },   // spine 2
        { 3,   0.f,   0.f,  56.f },   // spine 3
        { 4,   0.f,   0.f,  64.f },   // neck
        { 6,   0.f,   0.f,  72.f },   // head
        { 7,   0.f,   0.f,  60.f },   // upper chest
        { 8,  16.f,   0.f,  59.f },   // right shoulder
        { 9,  24.f,   0.f,  59.f },   // right arm
        { 10, 32.f,   0.f,  59.f },   // right elbow
        { 11, 40.f,   0.f,  59.f },   // right hand
        { 13,-16.f,   0.f,  59.f },   // left shoulder
        { 14,-24.f,   0.f,  59.f },   // left arm
        { 15,-32.f,   0.f,  59.f },   // left elbow
        { 16,-40.f,   0.f,  59.f },   // left hand
        { 17,  0.f, -10.f,  38.f },   // right hip
        { 18,  0.f, -10.f,  21.f },   // right knee
        { 19,  0.f, -10.f,   2.f },   // right ankle
        { 21,  0.f,  10.f,  38.f },   // left hip
        { 22,  0.f,  10.f,  21.f },   // left knee
        { 23,  0.f,  10.f,   2.f },   // left ankle
        { 24,   0.f,   0.f,  76.f },  // jaw
        { 31,   2.f,  -3.f,  73.f },  // eye left
        { 32,  -2.f,  -3.f,  73.f },  // eye right
        { 33,   0.f,   0.f,  50.f },  // spine 4
        { 34,   0.f,   0.f,  47.f },  // spine 5
    };

    for (int i = 0; i < 128; ++i) {
        f32 m[16] = { 1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1 };
        std::memcpy(bones + (size_t)i * 64, m, sizeof(m));
    }
    for (const Bone& b : layout) {
        f32 m[16] = { 1,0,0,0,  0,1,0,0,  0,0,1,0,  b.x, b.y, b.z, 1 };
        std::memcpy(bones + (size_t)b.index * 64, m, sizeof(m));
    }
}

void createPlayer(MockPlayer& p, int index) {
    p.controller = alloc(0x2000);
    p.pawn       = alloc(0x4000);        // m_angEyeAngles sits at 13808
    p.weapon     = alloc(0x2000);
    p.econ       = alloc(0x1000);
    p.ammoBlock  = alloc(0x100);
    p.services   = alloc(0x100);
    p.node       = alloc(0x200);
    p.modelState = alloc(0x200);
    p.model      = alloc(0x200);
    p.bones      = alloc(128 * 64);
    p.nameBuffer = alloc(64);
    p.money      = alloc(0x100);

    std::strncpy((char*)p.nameBuffer, kNames[index], 63);
    buildSkeleton(p.bones);

    p.team = (index % 3 == 0) ? 3 : 2;
    p.health = 100 - (index % 4) * 15;
    p.armor = index % 2 ? 100 : 0;
    p.bot = index > 0 && (index % 3) == 0;
    p.weaponId = (u16)(index % 6 == 0 ? 8 : (index % 5 == 0 ? 9 : 7));
    p.speed = 30.f + (f32)(index % 5) * 12.f;
    p.phase = (f32)index * 0.7f;

    // controller
    put<u32>(p.controller, off::m_iszPlayerName, (u32)(uintptr_t)p.nameBuffer);
    put<u32>(p.controller, off::m_hPlayerPawn, (u32)((uintptr_t)p.pawn & 0x7FFFFFFFu));
    put<int >(p.controller, off::m_iPawnHealth, p.health);
    put<int >(p.controller, off::m_iPawnArmor, p.armor);
    put<u8 >(p.controller, off::m_bPawnHasHelmet, p.armor > 0 ? 1 : 0);
    put<u8 >(p.controller, off::m_bPawnIsAlive, 1);
    put<int >(p.controller, off::m_iPawnBotDifficulty, p.bot ? 2 : 0);
    put<u64>(p.controller, off::m_pInGameMoneyServices, (u64)(uintptr_t)p.money);
    put<int >(p.money, off::m_iAccount, index % 2 ? 4200 : 875);

    // weapon services / weapon / econ
    put<u32>(p.services, off::m_hActiveWeapon, (u32)((uintptr_t)p.weapon & 0x7FFFFFFFu));
    put<int >(p.services, off::m_iAmmo, 90 - index);
    put<int >(p.weapon, off::m_iClip1, 30 - (index % 12));
    put<u32>(p.weapon, off::m_pReserveAmmo, (u32)(uintptr_t)p.ammoBlock);
    put<u8 >(p.weapon, off::m_bInReload, 0);
    put<u32>(p.weapon, off::m_AttributeManager, (u32)(uintptr_t)p.econ);
    put<int >(p.econ, off::m_iItemDefinitionIndex, (int)p.weaponId);

    // model chain: pawn -> scene node -> model state -> model -> bones
    put<u32>(p.pawn, off::m_pGameSceneNode, (u32)(uintptr_t)p.node);
    put<u32>(p.node, off::nodeModelState, (u32)(uintptr_t)p.modelState);
    put<u32>(p.modelState, off::m_hModel, (u32)((uintptr_t)p.model & 0x7FFFFFFFu));
    // a real pointer occupies 8 bytes, m_boneCount follows it - same as CS2
    put<u64>(p.model, off::modelBoneArray, (u64)(uintptr_t)p.bones);
    put<int >(p.model, off::modelBoneArray + 8, 128);

    // pawn fields
    put<u32>(p.pawn, off::m_pWeaponServices, (u32)(uintptr_t)p.services);
    put<u32>(p.pawn, off::m_pMovementServices, (u32)(uintptr_t)p.services);
    put<int >(p.pawn, off::m_ArmorValue, p.armor);
    put<u8 >(p.pawn, off::m_bIsScoped, 0);
    put<u8 >(p.pawn, off::m_bIsDefusing, 0);
    put<u8 >(p.pawn, off::m_entitySpottedState, 1);
}

void writeHandle(u32 slot, u8* address) {
    // CS2 entity list entries are handles: the low 31 bits of the entity address
    g_handleArray[slot] = (u64)((uintptr_t)address & 0x7FFFFFFFu);
    if (slot >= g_handleCount) g_handleCount = slot + 1;
}

void tick(f32 t) {
    // camera slowly pans so the projection is exercised over time
    const f32 yaw = std::sin(t * 0.25f) * 14.f;
    const Vec3 eye{ 0.f, 0.f, 64.f };
    const Vec3 target{ 4000.f, (f32)std::tan(yaw * 0.017453f) * 4000.f, 48.f };
    const Mat4 vp = multiply(perspective(60.f * 0.017453f, 1920.f / 1080.f, 10.f, 20000.f),
                             lookAt(eye, target, Vec3{ 0, 0, 1 }));
    std::memcpy(clientBase + off::dwViewMatrix, vp.m, sizeof(vp.m));

    for (int i = 0; i < kPlayerCount; ++i) {
        MockPlayer& p = g_players[i];

        if (i == 0) {
            // the local pawn: fixed position, view angles follow the pan
            p.origin = { 0.f, 0.f, 0.f };
            put<f32>(p.pawn, off::m_angEyeAngles, yaw);
            put<f32>(p.pawn, off::m_angEyeAngles + 4, 0.f);
            put<f32>(p.pawn, off::m_angEyeAngles + 8, 0.f);
        } else {
            const f32 lateral = (f32)((i - 1) % 5 - 2) * 230.f;
            const f32 depth = 320.f + (f32)((i - 1) / 5) * 420.f;
            const f32 sway = std::sin(t * p.speed * 0.05f + p.phase) * 55.f;
            p.origin = { depth, lateral + sway, 0.f };
            p.velocity = { 0.f, std::cos(t * p.speed * 0.05f + p.phase) * p.speed, 0.f };

            if (p.alive) {
                // a scripted duel: the first enemy in the crosshair takes hits
                const int victim = 1 + ((int)(t / 3.f) % 3);
                if (i == victim) {
                    const int tick = ((int)(t * 4.f)) % 40;
                    p.health = tick < 30 ? (100 - tick * 8) : 0;
                    if (p.health <= 0) p.alive = false;
                } else {
                    p.health = 100;
                    p.alive = true;
                }
            }
        }

        put<f32>(p.node, off::m_vecAbsOrigin + 0, p.origin.x);
        put<f32>(p.node, off::m_vecAbsOrigin + 4, p.origin.y);
        put<f32>(p.node, off::m_vecAbsOrigin + 8, p.origin.z);
        put<f32>(p.node, off::m_vecOrigin + 0, p.origin.x);
        put<f32>(p.node, off::m_vecOrigin + 4, p.origin.y);
        put<f32>(p.node, off::m_vecOrigin + 8, p.origin.z);

        put<int >(p.pawn, off::m_iTeamNum, p.team);
        put<int >(p.pawn, off::m_iHealth, p.health);
        put<int >(p.pawn, off::m_lifeState, p.alive ? 0 : 2);
        put<u32 >(p.pawn, off::m_fFlags, (u32)(i % 4 == 0 ? 8 : 0));
        put<f32 >(p.pawn, off::m_vecVelocity + 0, p.velocity.x);
        put<f32 >(p.pawn, off::m_vecVelocity + 4, p.velocity.y);
        put<f32 >(p.pawn, off::m_vecVelocity + 8, 0.f);
        put<f32 >(p.pawn, off::m_vecViewOffset + 0, 0.f);
        put<f32 >(p.pawn, off::m_vecViewOffset + 4, 0.f);
        put<f32 >(p.pawn, off::m_vecViewOffset + 8, 64.f);

        p.flashed = (i == 7);
        put<f32 >(p.pawn, off::m_flFlashDuration, p.flashed ? 3.2f : 0.f);
        put<u8 >(p.pawn, off::m_entitySpottedState, p.spotted ? 1 : 0);
        put<int >(p.controller, off::m_iPawnHealth, p.health);
        put<u8 >(p.controller, off::m_bPawnIsAlive, p.alive ? 1 : 0);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    char dir[MAX_PATH]{};
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    char* slash = std::strrchr(dir, '\\');
    if (slash) *slash = 0;
    const std::string clientPath = std::string(dir) + "\\client.dll";

    FILE* diag = nullptr;
    std::string logPath = std::string(dir) + "\\mock.log";
    freopen_s(&diag, logPath.c_str(), "w", stdout);

    std::printf("dir=%s client=%s\n", dir, clientPath.c_str());
    std::fflush(stdout);

    HMODULE client = LoadLibraryA(clientPath.c_str());
    if (!client) {
        std::printf("FAIL load library: error %lu\n", GetLastError());
        std::fflush(stdout);
        return 1;
    }
    clientBase = reinterpret_cast<u8*>(client);
    std::printf("client.dll base=%p region=%p\n", (void*)clientBase, (void*)clientBase);
    std::fflush(stdout);

    g_handleArray = (u64*)alloc(512 * sizeof(u64));

    // controllers and pawns occupy the low slots exactly like CS2 does
    for (int i = 0; i < kPlayerCount; ++i) {
        createPlayer(g_players[i], i);
        writeHandle((u32)i + 1, g_players[i].controller);
        writeHandle((u32)i + 64, g_players[i].pawn);
    }

    // globals inside the client.dll image
    put<u64>(clientBase, off::dwEntityList, (u64)(uintptr_t)g_handleArray);
    put<u32>(clientBase, off::dwLocalPlayerPawn, (u32)((uintptr_t)g_players[0].pawn & 0x7FFFFFFFu));
    put<u32>(clientBase, off::dwLocalPlayerController, (u32)((uintptr_t)g_players[0].controller & 0x7FFFFFFFu));

    std::printf("mock cs2 ready: %d players, handles=%u, entity list @ %p\n",
                kPlayerCount, g_handleCount, (void*)g_handleArray);
    std::fflush(stdout);

    const LARGE_INTEGER freq{ 1000 }, start{ 0 };
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    while (true) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        const f32 t = (f32)((double)(now.QuadPart - counter.QuadPart) / (double)freq.QuadPart);
        tick(t);
        Sleep(16);
    }
    return 0;
}