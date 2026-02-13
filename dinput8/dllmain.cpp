#include <windows.h>
#include <filesystem>
#include <cstdio>

static HMODULE realDinput8 = nullptr;

void LoadReal() {
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\dinput8.dll");
    realDinput8 = LoadLibraryW(path);
}

FARPROC GetReal(const char* name) {
    return realDinput8 ? GetProcAddress(realDinput8, name) : nullptr;
}

extern "C" {

    HRESULT WINAPI DirectInput8Create(HINSTANCE a, DWORD b, REFIID c, LPVOID* d, LPUNKNOWN e) {
        auto fn = (decltype(&DirectInput8Create))GetReal("DirectInput8Create");
        return fn ? fn(a, b, c, d, e) : E_FAIL;
    }

    HRESULT WINAPI DllCanUnloadNow() {
        auto fn = (decltype(&DllCanUnloadNow))GetReal("DllCanUnloadNow");
        return fn ? fn() : S_FALSE;
    }

    HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
        auto fn = (decltype(&DllGetClassObject))GetReal("DllGetClassObject");
        return fn ? fn(rclsid, riid, ppv) : E_FAIL;
    }

    HRESULT WINAPI DllRegisterServer() {
        auto fn = (decltype(&DllRegisterServer))GetReal("DllRegisterServer");
        return fn ? fn() : S_OK;
    }

    HRESULT WINAPI DllUnregisterServer() {
        auto fn = (decltype(&DllUnregisterServer))GetReal("DllUnregisterServer");
        return fn ? fn() : S_OK;
    }

}

void LoadMods() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    auto baseDir = std::filesystem::path(exePath).parent_path();
    auto modsDir = baseDir / L"mods";

    if (!std::filesystem::exists(modsDir)) {
        std::filesystem::create_directory(modsDir);
        return;
    }

    for (auto& p : std::filesystem::directory_iterator(modsDir)) {
        if (p.path().extension() == L".dll") {
            LoadLibraryW(p.path().c_str());
        }
    }
}

DWORD WINAPI InitThread(LPVOID) {
    Sleep(3000);
    LoadMods();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(GetModuleHandle(nullptr));
        LoadReal();
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
