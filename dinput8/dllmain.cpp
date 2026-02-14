#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>

static HMODULE realDinput8 = nullptr;
static std::wofstream gLog;

// -------- helpers --------

std::filesystem::path GetBaseDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    return std::filesystem::path(exePath).parent_path();
}

std::wstring Now() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t buf[64];
    swprintf_s(buf, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    return buf;
}

HWND FindGameWindow() {
    HWND h = GetTopWindow(nullptr);
    DWORD pid = GetCurrentProcessId();

    while (h) {
        DWORD winPid = 0;
        GetWindowThreadProcessId(h, &winPid);
        if (winPid == pid && IsWindowVisible(h)) {
            return h;
        }
        h = GetNextWindow(h, GW_HWNDNEXT);
    }
    return nullptr;
}

// -------- process-wide guard --------

bool AcquireProcessGuard() {
    HANDLE h = CreateMutexW(nullptr, TRUE, L"Global\\NiohModLoader_Init");
    if (!h) return true;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        return false;
    }
    return true;
}

// -------- logging --------

void OpenLog() {
    try {
        auto modsDir = GetBaseDir() / L"mods";
        if (!std::filesystem::exists(modsDir)) {
            std::filesystem::create_directory(modsDir);
        }
        gLog.open(modsDir / L"loader.log", std::ios::out | std::ios::app);
    }
    catch (...) {}
}

void Log(const std::wstring& msg) {
    try {
        if (gLog.is_open()) {
            gLog << Now() << msg << L"\n";
            gLog.flush();
        }
    }
    catch (...) {}
}

// -------- overlay --------

HWND gOverlay = nullptr;
UINT_PTR gOverlayTimer = 1;
HWND gGameHwnd = nullptr;

DWORD WINAPI OverlayFollowThread(LPVOID) {
    while (gOverlay) {
        HWND fg = GetForegroundWindow();
        if (fg == gGameHwnd) {
            ShowWindow(gOverlay, SW_SHOW);
        }
        else {
            ShowWindow(gOverlay, SW_HIDE);
        }
        Sleep(100);
    }
    return 0;
}

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TIMER && wParam == gOverlayTimer) {
        DestroyWindow(hWnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        gOverlay = nullptr;
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void CreateOverlayWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"NiohModLoaderOverlay";

    RegisterClassExW(&wc);

    for (int i = 0; i < 200; ++i) {
        gGameHwnd = FindGameWindow();
        if (gGameHwnd) break;
        Sleep(50);
    }
    if (!gGameHwnd) return;

    gOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"",
        WS_POPUP,
        10, 10, 260, 50,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    SetWindowLongPtrW(gOverlay, GWLP_HWNDPARENT, (LONG_PTR)gGameHwnd);
    SetLayeredWindowAttributes(gOverlay, 0, 255, LWA_ALPHA);
    ShowWindow(gOverlay, SW_SHOW);
    SetTimer(gOverlay, gOverlayTimer, 20000, nullptr);

    CreateThread(nullptr, 0, OverlayFollowThread, nullptr, 0, nullptr);
}

void DrawOverlayText(const wchar_t* text) {
    if (!gOverlay) return;

    HDC hdc = GetDC(gOverlay);

    RECT r{ 0, 0, 260, 50 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(230, 230, 230));

    HFONT font = CreateFontW(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
    );

    HFONT old = (HFONT)SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);
    DeleteObject(font);

    ReleaseDC(gOverlay, hdc);
}

// -------- proxy forwarding --------

void LoadReal() {
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\dinput8.dll");
    realDinput8 = LoadLibraryW(path);
}

FARPROC GetReal(const char* name) {
    if (!realDinput8) return nullptr;
    return GetProcAddress(realDinput8, name);
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

// -------- mod loading --------

void LoadMods() {
    try {
        auto modsDir = GetBaseDir() / L"mods";
        Log(L"[Loader] Scanning mods directory");

        if (!std::filesystem::exists(modsDir)) {
            std::filesystem::create_directory(modsDir);
            Log(L"[Loader] Created mods directory");
            return;
        }

        for (auto& modDir : std::filesystem::directory_iterator(modsDir)) {
            if (!modDir.is_directory()) continue;

            std::filesystem::path dllPath;

            for (auto& f : std::filesystem::directory_iterator(modDir.path())) {
                if (f.path().extension() == L".dll") {
                    dllPath = f.path();
                    break;
                }
            }

            if (dllPath.empty()) {
                Log(L"[Loader] No DLL found in: " + modDir.path().wstring());
                continue;
            }

            Log(L"[Loader] Loading mod: " + dllPath.wstring());
            LoadLibraryW(dllPath.c_str());
        }

        Log(L"[Loader] Finished loading mods");
    }
    catch (...) {
        Log(L"[Loader] Exception during LoadMods");
    }
}

DWORD WINAPI InitThread(LPVOID) {
    OpenLog();
    Log(L"[Loader] Session started");

    CreateOverlayWindow();
    DrawOverlayText(L"Mod Loader active");

    Sleep(5000);
    Log(L"[Loader] InitThread started");
    LoadMods();

    return 0;
}

// -------- entry --------

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        if (!AcquireProcessGuard()) return TRUE;
        DisableThreadLibraryCalls(GetModuleHandle(nullptr));
        LoadReal();
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
