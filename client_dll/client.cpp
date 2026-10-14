// PLAG test harness - stand-in for client.dll.
//
// CS2 publishes module relative offsets (dwEntityList and friends) that live
// inside client.dll at a few megabytes into its image. This DLL exists only to
// give the mock process an image of that size, so PLAG resolves the exact same
// addresses it would against the real game.
#include <windows.h>

// uninitialised globals land in .bss, so the file stays small while the image
// is large enough to cover every published RVA. No `extern` here: that would
// make this a declaration instead of a definition and the linker would drop it.
__declspec(dllexport) unsigned char g_clientRegion[48 * 1024 * 1024];

extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}