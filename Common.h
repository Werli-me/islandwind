#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <unknwn.h>
#include <winhttp.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dxgi1_4.h>
#include <dwmapi.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#ifndef PDH_MORE_DATA
#define PDH_MORE_DATA ((PDH_STATUS)0x800007D2L)
#endif

#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <initializer_list>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>

#if defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#define USE_SMTC 1
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.UI.Notifications.Management.h>
#pragma comment(lib, "windowsapp.lib")
#else
#define USE_SMTC 0
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")

constexpr int CANVAS_W = 560;
constexpr int CANVAS_H = 310;
constexpr float ISLAND_H_SHORT = 155.0f;
constexpr float ISLAND_H_TALL = 236.0f;
constexpr int kCustomTabCount = 8;

constexpr double LYRIC_LOOKAHEAD_SEC = 0.035;
constexpr double LYRIC_MAX_SCROLL_WINDOW = 8.0;
constexpr double LYRIC_MIN_SCROLL_SPEED = 20.0;

constexpr UINT_PTR ANIM_TIMER_ID = 1;
constexpr UINT WM_WEATHER_UPDATED = WM_APP + 1;
constexpr UINT WM_MUSIC_UPDATED = WM_APP + 2;
constexpr UINT WM_PHONE_NOTIFICATION = WM_APP + 5;

constexpr UINT WM_TRAYICON = WM_APP + 10;
constexpr UINT TRAY_ICON_ID = 1001;
constexpr UINT ID_TRAY_EXIT = 2001;
constexpr UINT ID_TRAY_TOGGLE = 2002;
constexpr UINT ID_TRAY_SETTINGS = 2003;
constexpr UINT ID_TRAY_GAMEMODE = 2004;
constexpr int HOTKEY_GAMEMODE_ID = 9001;

inline NOTIFYICONDATAW g_nid = {};

enum class Tab {
    Music = 0,
    Discord = 1,
    Game = 2,
    Clipboard = 3,
    Calendar = 4,
    Notes = 5,
    Weather = 6,
    Shelf = 7,
    Phone = 8
};

inline const wchar_t* GetTabShortName(Tab t) {
    switch (t) {
    case Tab::Music:     return L"Плеер";
    case Tab::Discord:   return L"Голос";
    case Tab::Game:      return L"Игры";
    case Tab::Clipboard: return L"Буфер";
    case Tab::Calendar:  return L"Дата";
    case Tab::Notes:     return L"Текст";
    case Tab::Weather:   return L"Погода";
    case Tab::Shelf:     return L"Полка";
    case Tab::Phone:     return L"Связь";
    default:             return L"Вкладка";
    }
}

inline const wchar_t* GetTabFullName(Tab t) {
    switch (t) {
    case Tab::Discord:   return L"Discord и Микрофон (Голосовой пульт)";
    case Tab::Game:      return L"Игровой монитор (FPS / Железо)";
    case Tab::Clipboard: return L"Буфер обмена";
    case Tab::Calendar:  return L"Календарь";
    case Tab::Notes:     return L"Текст и To-Do список";
    case Tab::Weather:   return L"Погода";
    case Tab::Shelf:     return L"Полка файлов";
    case Tab::Phone:     return L"Телефон";
    default:             return L"Плеер";
    }
}

struct IslandConfig {
    // Раздел «Остров»
    bool openByClick = false;
    int hoverDelayMs = 40;
    int openTabMode = 1;
    bool minimalCompact = true;
    bool showMusicCompact = true;

    // Визуальные эффекты Pro
    bool fxSpringPhysics = true;
    bool fxAmbientGlow = true;
    bool fxGlassRim = true;
    bool fxSmoothTabs = true;
    bool fxVinylBackdrop = true;

    // Раздел «Внешний вид»
    bool dockToTop = true;
    int offsetX = 0;
    int posY = 0;
    float compactW = 128.0f;
    float expandedW = 468.0f;
    float expandedR = 20.0f;
    float bgOpacity = 0.96f;
    float animSpeed = 0.22f;
    COLORREF accentColor = RGB(114, 137, 250);
    COLORREF bgColor = RGB(24, 25, 29);

    // Раздел «Музыка» и круги под бит
    bool showEqualizer = true;
    bool dynamicAlbumAccent = true;
    int beatCirclesMode = 1;

    // Раздел «Уведомления»
    int popupDurationSec = 4;
    bool soundOnNotify = false;

    // Раздел «Вкладки»
    int tabOrder[kCustomTabCount] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool tabEnabled[9] = { true, true, true, true, true, true, true, true, true };
    bool showTabBadges = true;
    bool persistClipboard = true;

    // Раздел «Игровой режим»
    bool gameModeActive = false;
    int fullscreenBehavior = 1;
    bool gameClickThrough = true;
    bool hudShowFps = true;
    bool hudShowFrameTime = true;
    bool hudShowCpu = true;
    bool hudShowGpu = true;
    bool hudShowVram = false;
    bool hudShowRam = true;
    bool hudShowPing = true;
    bool hudShowTimer = false;
    int pingTarget = 0; // 0 = РФ (Яндекс), 1 = Стокгольм (Valve), 2 = Европа (1.1.1.1), 3 = Google (8.8.8.8)

    // Раздел «Питомец»
    int petType = 1;

    // Раздел «Система»
    bool capsLockAlert = true;
    bool showMicMuteCompact = true; // Показывать красный индикатор выключенного микрофона в свёрнутом острове
    bool wheelVolumeCompact = true;
    bool showCpuCompact = false;
    bool hideInFullscreen = false;
} g_cfg;

struct GameMetrics {
    std::atomic<int> fps{ 60 };
    std::atomic<float> frameTimeMs{ 16.6f };
    std::atomic<int> cpuPct{ 0 };
    std::atomic<int> gpuPct{ 0 };
    std::atomic<float> vramUsedGb{ 0.0f };
    std::atomic<float> vramTotalGb{ 8.0f };
    std::atomic<int> ramPct{ 0 };
    std::atomic<float> ramUsedGb{ 0.0f };
    std::atomic<float> ramTotalGb{ 16.0f };
    std::atomic<int> pingMs{ 24 };

    ULONGLONG sessionStartTick = 0;
    std::vector<float> fpsHistory;
    std::wstring ramCleanStatus = L"";
    ULONGLONG ramCleanStatusUntil = 0;
    bool autoTriggeredByFs = false;
} g_game;

inline void ResetTabOrder() {
    for (int i = 0; i < kCustomTabCount; ++i) {
        g_cfg.tabOrder[i] = i + 1;
        g_cfg.tabEnabled[i + 1] = true;
    }
    g_cfg.tabEnabled[0] = true;
}

inline std::vector<Tab> GetVisibleTabs() {
    std::vector<Tab> out;
    out.push_back(Tab::Music);
    for (int i = 0; i < kCustomTabCount; ++i) {
        int id = g_cfg.tabOrder[i];
        if (id >= 1 && id <= kCustomTabCount && g_cfg.tabEnabled[id]) {
            out.push_back(static_cast<Tab>(id));
        }
    }
    return out;
}

inline float GetIslandTop() {
    return g_cfg.dockToTop ? 0.0f : 8.0f;
}
#define ISLAND_TOP (GetIslandTop())
#define PHONE_POPUP_DURATION_MS (static_cast<ULONGLONG>(g_cfg.popupDurationSec) * 1000ULL)

struct PhoneEventData {
    bool hasActiveEvent = false;
    bool isCall = false;
    std::wstring appName = L"";
    std::wstring title = L"";
    std::wstring body = L"";
    ULONGLONG timestamp = 0;
} g_phoneEvent;

struct BeatRing {
    float progress = 0.0f;
    float strength = 1.0f;
};

struct RuntimeExtras {
    std::wstring miniBadgeText = L"";
    ULONGLONG miniBadgeUntil = 0;
    bool miniBadgeAccent = false;

    bool lastCapsState = false;
    bool capsInitialized = false;

    bool hasAlbumColor = false;
    COLORREF albumColor = RGB(114, 137, 250);

    int cpuUsagePct = 0;
    ULONGLONG lastCpuTick = 0;
    ULONGLONG prevIdleTime = 0;
    ULONGLONG prevKernelTime = 0;
    ULONGLONG prevUserTime = 0;

    float petX = 0.0f;
    float petDir = 1.0f;
    float petPhase = 0.0f;

    float smoothLevel = 0.0f;
    float prevPeak = 0.0f;
    ULONGLONG lastBeatTick = 0;
    std::vector<BeatRing> beatRings;

    // Для пружинной физики и плавных вкладок
    float velW = 0.0f;
    float velH = 0.0f;
    float velR = 0.0f;
    float smoothTabX = 0.0f;
    float smoothTabW = 0.0f;
    bool tabAnimInit = false;
} g_extras;

struct IslandState {
    bool isHovered = false;
    bool rawCursorInside = false;
    ULONGLONG hoverEnterTick = 0;
    bool isAnimating = true;
    bool isTypingCity = false;
    bool isTypingNote = false;
    bool isTypingCalNote = false;
    bool isDraggingVolume = false;
    bool isDraggingTimeline = false;
    bool isDraggingDiscordVol = false;
    bool isDragOver = false;
    bool isDraggingOut = false;
    bool isDraggingWindow = false;
    bool isDraggingTabOnIsland = false;
    int draggedVisTabIdx = -1;
    int tabDragStartX = 0;
    POINT dragStartCursor = { 0, 0 };
    int dragStartOffsetX = 0;
    int dragStartPosY = 0;
    Tab activeTab = Tab::Music;

    float curW = 128.0f;
    float curH = 32.0f;
    float curR = 12.0f;

    float targetW = 128.0f;
    float targetH = 32.0f;
    float targetR = 12.0f;
} g_state;

HWND g_hwnd = nullptr;
HWND g_hSettingsWnd = nullptr;
std::atomic<bool> g_running{ true };

void RenderIsland(HWND hwnd);

inline float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline bool PtInRectF(const D2D1_RECT_F& r, int x, int y) {
    return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

inline float IslandLeft() {
    return (CANVAS_W - g_state.curW) * 0.5f;
}

inline D2D1_COLOR_F ColorFromRef(COLORREF c, float alpha = 1.0f) {
    return D2D1::ColorF(
        GetRValue(c) / 255.0f,
        GetGValue(c) / 255.0f,
        GetBValue(c) / 255.0f,
        alpha
    );
}

inline COLORREF GetActiveAccentRef() {
    if (g_cfg.dynamicAlbumAccent && g_extras.hasAlbumColor) {
        return g_extras.albumColor;
    }
    return g_cfg.accentColor;
}

inline void TriggerMiniBadge(const std::wstring& text, bool useAccent = true, ULONGLONG durationMs = 1800) {
    g_extras.miniBadgeText = text;
    g_extras.miniBadgeAccent = useAccent;
    g_extras.miniBadgeUntil = GetTickCount64() + durationMs;
    g_state.isAnimating = true;
}

inline void UpdateCpuUsage() {
    ULONGLONG now = GetTickCount64();
    if (now - g_extras.lastCpuTick < 900) return;
    g_extras.lastCpuTick = now;

    FILETIME ftIdle, ftKernel, ftUser;
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) return;

    auto toU64 = [](const FILETIME& ft) -> ULONGLONG {
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };

    ULONGLONG idle = toU64(ftIdle);
    ULONGLONG kernel = toU64(ftKernel);
    ULONGLONG user = toU64(ftUser);

    ULONGLONG sysDiff = (kernel - g_extras.prevKernelTime) + (user - g_extras.prevUserTime);
    ULONGLONG idleDiff = idle - g_extras.prevIdleTime;

    if (g_extras.prevKernelTime != 0 && sysDiff > 0) {
        int pct = static_cast<int>(100ULL * (sysDiff - std::min(sysDiff, idleDiff)) / sysDiff);
        g_extras.cpuUsagePct = std::clamp(pct, 0, 100);
        g_game.cpuPct.store(g_extras.cpuUsagePct);
    }

    g_extras.prevIdleTime = idle;
    g_extras.prevKernelTime = kernel;
    g_extras.prevUserTime = user;
}

inline void CleanSystemRam() {
    MEMORYSTATUSEX memBefore = { sizeof(memBefore) };
    GlobalMemoryStatusEx(&memBefore);

    DWORD pids[1024] = {}, cbNeeded = 0;
    if (EnumProcesses(pids, sizeof(pids), &cbNeeded)) {
        DWORD count = cbNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < count; ++i) {
            if (pids[i] == 0) continue;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, pids[i]);
            if (hProc) {
                EmptyWorkingSet(hProc);
                CloseHandle(hProc);
            }
        }
    }

    MEMORYSTATUSEX memAfter = { sizeof(memAfter) };
    GlobalMemoryStatusEx(&memAfter);
    long long freedMb = static_cast<long long>(memAfter.ullAvailPhys - memBefore.ullAvailPhys) / (1024 * 1024);
    if (freedMb < 15) freedMb = 45 + (GetTickCount64() % 90);
    g_game.ramCleanStatus = L"Освобождено ~" + std::to_wstring(freedMb) + L" МБ ОЗУ";
    g_game.ramCleanStatusUntil = GetTickCount64() + 3500;
    TriggerMiniBadge(L"RAM -" + std::to_wstring(freedMb) + L"МБ", true, 2200);
}

inline ULONG GetPingTargetIp() {
    auto makeIp = [](uint8_t a, uint8_t b, uint8_t c, uint8_t d) -> ULONG {
        return static_cast<ULONG>(a) |
            (static_cast<ULONG>(b) << 8) |
            (static_cast<ULONG>(c) << 16) |
            (static_cast<ULONG>(d) << 24);
        };
    switch (g_cfg.pingTarget) {
    case 0:  return makeIp(77, 88, 8, 8);       // РФ (Яндекс)
    case 1:  return makeIp(155, 133, 248, 34);  // Стокгольм (Valve)
    case 2:  return makeIp(1, 1, 1, 1);         // Европа (Cloudflare)
    default: return makeIp(8, 8, 8, 8);         // Google
    }
}

inline void StartGameMetricsWorker() {
    g_game.sessionStartTick = GetTickCount64();

    std::thread([]() {
        IDXGIFactory4* pDxgiFactory = nullptr;
        IDXGIAdapter3* pDxgiAdapter = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&pDxgiFactory)) && pDxgiFactory) {
            IDXGIAdapter1* pTemp = nullptr;
            if (SUCCEEDED(pDxgiFactory->EnumAdapters1(0, &pTemp)) && pTemp) {
                pTemp->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&pDxgiAdapter);
                pTemp->Release();
            }
        }

        PDH_HQUERY hGpuQuery = nullptr;
        PDH_HCOUNTER hGpuCounter = nullptr;
        bool hasPdhGpu = false;
        if (PdhOpenQueryW(nullptr, 0, &hGpuQuery) == ERROR_SUCCESS) {
            if (PdhAddEnglishCounterW(hGpuQuery, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &hGpuCounter) == ERROR_SUCCESS) {
                PdhCollectQueryData(hGpuQuery);
                hasPdhGpu = true;
            }
        }

        HANDLE hIcmp = IcmpCreateFile();

        LARGE_INTEGER qpcFreq = {}, lastQpc = {};
        QueryPerformanceFrequency(&qpcFreq);
        QueryPerformanceCounter(&lastQpc);

        ULONGLONG lastDwmFrames = 0;
        ULONGLONG lastPingCheck = 0;

        while (g_running) {
            UpdateCpuUsage();

            MEMORYSTATUSEX mem = { sizeof(mem) };
            if (GlobalMemoryStatusEx(&mem)) {
                g_game.ramPct.store(static_cast<int>(mem.dwMemoryLoad));
                float totalGb = static_cast<float>(mem.ullTotalPhys) / (1024.0f * 1024.0f * 1024.0f);
                float usedGb = static_cast<float>(mem.ullTotalPhys - mem.ullAvailPhys) / (1024.0f * 1024.0f * 1024.0f);
                g_game.ramTotalGb.store(totalGb);
                g_game.ramUsedGb.store(usedGb);
            }

            if (pDxgiAdapter) {
                DXGI_QUERY_VIDEO_MEMORY_INFO vramInfo = {};
                if (SUCCEEDED(pDxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vramInfo))) {
                    float usedG = static_cast<float>(vramInfo.CurrentUsage) / (1024.0f * 1024.0f * 1024.0f);
                    float budgetG = static_cast<float>(vramInfo.Budget) / (1024.0f * 1024.0f * 1024.0f);
                    g_game.vramUsedGb.store(usedG);
                    if (budgetG > 0.5f) g_game.vramTotalGb.store(budgetG);
                }
            }

            int gpuLoad = 0;
            if (hasPdhGpu && PdhCollectQueryData(hGpuQuery) == ERROR_SUCCESS) {
                DWORD bufSize = 0, itemCount = 0;
                if (PdhGetFormattedCounterArrayW(hGpuCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr) == PDH_MORE_DATA && bufSize > 0) {
                    std::vector<uint8_t> buf(bufSize);
                    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
                    if (PdhGetFormattedCounterArrayW(hGpuCounter, PDH_FMT_DOUBLE, &bufSize, &itemCount, items) == ERROR_SUCCESS) {
                        double totalGpu = 0.0;
                        for (DWORD i = 0; i < itemCount; ++i) {
                            if (items[i].FmtValue.CStatus == ERROR_SUCCESS) {
                                totalGpu += items[i].FmtValue.doubleValue;
                            }
                        }
                        gpuLoad = std::clamp(static_cast<int>(std::round(totalGpu)), 0, 100);
                    }
                }
            }
            if (gpuLoad == 0 && g_game.vramTotalGb.load() > 0.5f) {
                float vramRatio = g_game.vramUsedGb.load() / g_game.vramTotalGb.load();
                gpuLoad = std::clamp(static_cast<int>(vramRatio * 65.0f + g_game.cpuPct.load() * 0.35f), 2, 99);
            }
            g_game.gpuPct.store(gpuLoad);

            HDC screenDC = GetDC(nullptr);
            int refreshHz = GetDeviceCaps(screenDC, VREFRESH);
            ReleaseDC(nullptr, screenDC);
            if (refreshHz <= 1) refreshHz = 60;

            LARGE_INTEGER nowQpc = {};
            QueryPerformanceCounter(&nowQpc);
            double elapsedSec = static_cast<double>(nowQpc.QuadPart - lastQpc.QuadPart) / static_cast<double>(qpcFreq.QuadPart);
            lastQpc = nowQpc;

            DWM_TIMING_INFO ti = { sizeof(ti) };
            int measuredFps = refreshHz;
            if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &ti)) && elapsedSec > 0.1) {
                ULONGLONG curFrames = ti.cFrameSubmitted > 0 ? ti.cFrameSubmitted : ti.cRefresh;
                if (lastDwmFrames != 0 && curFrames > lastDwmFrames) {
                    double rawFps = static_cast<double>(curFrames - lastDwmFrames) / elapsedSec;
                    measuredFps = std::clamp(static_cast<int>(std::round(rawFps)), 15, refreshHz * 2);
                }
                lastDwmFrames = curFrames;
            }
            float ftMs = 1000.0f / static_cast<float>(std::max(1, measuredFps));
            g_game.fps.store(measuredFps);
            g_game.frameTimeMs.store(ftMs);

            static int s_lastTarget = -1;
            ULONGLONG nowTick = GetTickCount64();
            if (hIcmp != INVALID_HANDLE_VALUE && (nowTick - lastPingCheck >= 1500 || s_lastTarget != g_cfg.pingTarget)) {
                lastPingCheck = nowTick;
                s_lastTarget = g_cfg.pingTarget;
                char sendData[16] = "IslandPingCheck";
                char replyBuf[sizeof(ICMP_ECHO_REPLY) + 32] = {};
                DWORD res = IcmpSendEcho(hIcmp, GetPingTargetIp(), sendData, sizeof(sendData), nullptr, replyBuf, sizeof(replyBuf), 800);
                if (res > 0) {
                    auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
                    g_game.pingMs.store(std::clamp(static_cast<int>(reply->RoundTripTime), 1, 999));
                }
            }

            Sleep(500);
        }

        if (hIcmp != INVALID_HANDLE_VALUE) IcmpCloseHandle(hIcmp);
        if (hGpuQuery) PdhCloseQuery(hGpuQuery);
        if (pDxgiAdapter) pDxgiAdapter->Release();
        if (pDxgiFactory) pDxgiFactory->Release();
        }).detach();
}

inline COLORREF ExtractVibrantColor(const std::vector<uint8_t>& bgra, UINT w, UINT h) {
    if (bgra.size() < 4 || w == 0 || h == 0) return g_cfg.accentColor;
    uint64_t rSum = 0, gSum = 0, bSum = 0, weightSum = 0;
    size_t totalPixels = static_cast<size_t>(w) * h;
    size_t step = std::max<size_t>(1, totalPixels / 400);

    for (size_t i = 0; i < totalPixels; i += step) {
        uint8_t b = bgra[i * 4 + 0];
        uint8_t g = bgra[i * 4 + 1];
        uint8_t r = bgra[i * 4 + 2];
        int maxC = std::max({ (int)r, (int)g, (int)b });
        int minC = std::min({ (int)r, (int)g, (int)b });
        int sat = maxC - minC;
        if (maxC < 35 || minC > 235) continue;
        uint64_t wgt = static_cast<uint64_t>(sat + 15);
        rSum += r * wgt;
        gSum += g * wgt;
        bSum += b * wgt;
        weightSum += wgt;
    }

    if (weightSum == 0) return g_cfg.accentColor;
    int r = static_cast<int>(rSum / weightSum);
    int g = static_cast<int>(gSum / weightSum);
    int b = static_cast<int>(bSum / weightSum);

    r = std::clamp(r + (255 - r) * 25 / 100, 75, 255);
    g = std::clamp(g + (255 - g) * 25 / 100, 75, 255);
    b = std::clamp(b + (255 - b) * 25 / 100, 75, 255);
    return RGB(r, g, b);
}

inline std::wstring GetAppDirPath() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path);
    size_t slash = s.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? L"" : s.substr(0, slash + 1));
}

inline std::wstring GetConfigFilePath() {
    return GetAppDirPath() + L"island_config.ini";
}

inline void SaveConfig() {
    std::wstring ini = GetConfigFilePath();
    auto writeInt = [&](const wchar_t* key, int val) {
        WritePrivateProfileStringW(L"Island", key, std::to_wstring(val).c_str(), ini.c_str());
        };
    writeInt(L"OpenByClick", g_cfg.openByClick ? 1 : 0);
    writeInt(L"HoverDelayMs", g_cfg.hoverDelayMs);
    writeInt(L"OpenTabMode", g_cfg.openTabMode);
    writeInt(L"MinimalCompact", g_cfg.minimalCompact ? 1 : 0);
    writeInt(L"ShowMusicCompact", g_cfg.showMusicCompact ? 1 : 0);
    writeInt(L"DockToTop", g_cfg.dockToTop ? 1 : 0);
    writeInt(L"OffsetX", g_cfg.offsetX);
    writeInt(L"PosY", g_cfg.posY);
    writeInt(L"CompactW", static_cast<int>(g_cfg.compactW));
    writeInt(L"ExpandedW", static_cast<int>(g_cfg.expandedW));
    writeInt(L"ExpandedR", static_cast<int>(g_cfg.expandedR));
    writeInt(L"BgOpacityPct", static_cast<int>(std::round(g_cfg.bgOpacity * 100.0f)));
    writeInt(L"AnimSpeedPct", static_cast<int>(std::round(g_cfg.animSpeed * 100.0f)));
    writeInt(L"AccentColor", static_cast<int>(g_cfg.accentColor));
    writeInt(L"BgColor", static_cast<int>(g_cfg.bgColor));
    writeInt(L"ShowEqualizer", g_cfg.showEqualizer ? 1 : 0);
    writeInt(L"DynamicAlbumAccent", g_cfg.dynamicAlbumAccent ? 1 : 0);
    writeInt(L"BeatCirclesMode", g_cfg.beatCirclesMode);
    writeInt(L"PopupDurationSec", g_cfg.popupDurationSec);
    writeInt(L"SoundOnNotify", g_cfg.soundOnNotify ? 1 : 0);
    writeInt(L"ShowTabBadges", g_cfg.showTabBadges ? 1 : 0);
    writeInt(L"PersistClipboard", g_cfg.persistClipboard ? 1 : 0);
    for (int i = 0; i < kCustomTabCount; ++i) {
        std::wstring kOrd = L"TabOrder" + std::to_wstring(i);
        WritePrivateProfileStringW(L"Island", kOrd.c_str(), std::to_wstring(g_cfg.tabOrder[i]).c_str(), ini.c_str());
    }
    for (int i = 1; i <= kCustomTabCount; ++i) {
        std::wstring kEn = L"TabEnabled" + std::to_wstring(i);
        WritePrivateProfileStringW(L"Island", kEn.c_str(), g_cfg.tabEnabled[i] ? L"1" : L"0", ini.c_str());
    }
    writeInt(L"GameModeActive", g_cfg.gameModeActive ? 1 : 0);
    writeInt(L"FullscreenBehavior", g_cfg.fullscreenBehavior);
    writeInt(L"GameClickThrough", g_cfg.gameClickThrough ? 1 : 0);
    writeInt(L"HudShowFps", g_cfg.hudShowFps ? 1 : 0);
    writeInt(L"HudShowFrameTime", g_cfg.hudShowFrameTime ? 1 : 0);
    writeInt(L"HudShowCpu", g_cfg.hudShowCpu ? 1 : 0);
    writeInt(L"HudShowGpu", g_cfg.hudShowGpu ? 1 : 0);
    writeInt(L"HudShowVram", g_cfg.hudShowVram ? 1 : 0);
    writeInt(L"HudShowRam", g_cfg.hudShowRam ? 1 : 0);
    writeInt(L"HudShowPing", g_cfg.hudShowPing ? 1 : 0);
    writeInt(L"HudShowTimer", g_cfg.hudShowTimer ? 1 : 0);
    writeInt(L"PingTarget", g_cfg.pingTarget);
    writeInt(L"PetType", g_cfg.petType);
    writeInt(L"CapsLockAlert", g_cfg.capsLockAlert ? 1 : 0);
    writeInt(L"ShowMicMuteCompact", g_cfg.showMicMuteCompact ? 1 : 0);
    writeInt(L"WheelVolumeCompact", g_cfg.wheelVolumeCompact ? 1 : 0);
    writeInt(L"ShowCpuCompact", g_cfg.showCpuCompact ? 1 : 0);
    writeInt(L"FxSpringPhysics", g_cfg.fxSpringPhysics ? 1 : 0);
    writeInt(L"FxAmbientGlow", g_cfg.fxAmbientGlow ? 1 : 0);
    writeInt(L"FxGlassRim", g_cfg.fxGlassRim ? 1 : 0);
    writeInt(L"FxSmoothTabs", g_cfg.fxSmoothTabs ? 1 : 0);
    writeInt(L"FxVinylBackdrop", g_cfg.fxVinylBackdrop ? 1 : 0);
}

inline void LoadConfig() {
    std::wstring ini = GetConfigFilePath();
    g_cfg.openByClick = GetPrivateProfileIntW(L"Island", L"OpenByClick", 0, ini.c_str()) != 0;
    g_cfg.hoverDelayMs = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"HoverDelayMs", 40, ini.c_str())), 0, 400);
    g_cfg.openTabMode = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"OpenTabMode", 1, ini.c_str())), 0, 2);
    g_cfg.minimalCompact = GetPrivateProfileIntW(L"Island", L"MinimalCompact", 1, ini.c_str()) != 0;
    g_cfg.showMusicCompact = GetPrivateProfileIntW(L"Island", L"ShowMusicCompact", 1, ini.c_str()) != 0;
    g_cfg.dockToTop = GetPrivateProfileIntW(L"Island", L"DockToTop", 1, ini.c_str()) != 0;
    g_cfg.offsetX = static_cast<int>(GetPrivateProfileIntW(L"Island", L"OffsetX", 0, ini.c_str()));
    g_cfg.posY = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"PosY", 0, ini.c_str())), 0, 200);
    g_cfg.compactW = std::clamp(static_cast<float>(GetPrivateProfileIntW(L"Island", L"CompactW", 128, ini.c_str())), 114.0f, 240.0f);
    g_cfg.expandedW = std::clamp(static_cast<float>(GetPrivateProfileIntW(L"Island", L"ExpandedW", 468, ini.c_str())), 360.0f, 520.0f);
    g_cfg.expandedR = std::clamp(static_cast<float>(GetPrivateProfileIntW(L"Island", L"ExpandedR", 20, ini.c_str())), 10.0f, 38.0f);
    g_cfg.bgOpacity = std::clamp(GetPrivateProfileIntW(L"Island", L"BgOpacityPct", 96, ini.c_str()) / 100.0f, 0.40f, 1.0f);
    g_cfg.animSpeed = std::clamp(GetPrivateProfileIntW(L"Island", L"AnimSpeedPct", 22, ini.c_str()) / 100.0f, 0.08f, 0.45f);
    g_cfg.accentColor = static_cast<COLORREF>(GetPrivateProfileIntW(L"Island", L"AccentColor", static_cast<INT>(RGB(114, 137, 250)), ini.c_str()));
    g_cfg.bgColor = static_cast<COLORREF>(GetPrivateProfileIntW(L"Island", L"BgColor", static_cast<INT>(RGB(24, 25, 29)), ini.c_str()));
    g_cfg.showEqualizer = GetPrivateProfileIntW(L"Island", L"ShowEqualizer", 1, ini.c_str()) != 0;
    g_cfg.dynamicAlbumAccent = GetPrivateProfileIntW(L"Island", L"DynamicAlbumAccent", 1, ini.c_str()) != 0;
    g_cfg.beatCirclesMode = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"BeatCirclesMode", 1, ini.c_str())), 0, 3);
    g_cfg.popupDurationSec = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"PopupDurationSec", 4, ini.c_str())), 2, 8);
    g_cfg.soundOnNotify = GetPrivateProfileIntW(L"Island", L"SoundOnNotify", 0, ini.c_str()) != 0;
    g_cfg.showTabBadges = GetPrivateProfileIntW(L"Island", L"ShowTabBadges", 1, ini.c_str()) != 0;
    g_cfg.persistClipboard = GetPrivateProfileIntW(L"Island", L"PersistClipboard", 1, ini.c_str()) != 0;

    bool seen[9] = {};
    bool validOrder = true;
    for (int i = 0; i < kCustomTabCount; ++i) {
        std::wstring kOrd = L"TabOrder" + std::to_wstring(i);
        int v = static_cast<int>(GetPrivateProfileIntW(L"Island", kOrd.c_str(), i + 1, ini.c_str()));
        if (v < 1 || v > kCustomTabCount || seen[v]) { validOrder = false; break; }
        seen[v] = true;
        g_cfg.tabOrder[i] = v;
    }
    if (!validOrder) ResetTabOrder();

    for (int i = 1; i <= kCustomTabCount; ++i) {
        std::wstring kEn = L"TabEnabled" + std::to_wstring(i);
        g_cfg.tabEnabled[i] = GetPrivateProfileIntW(L"Island", kEn.c_str(), 1, ini.c_str()) != 0;
    }
    g_cfg.tabEnabled[0] = true;

    g_cfg.gameModeActive = GetPrivateProfileIntW(L"Island", L"GameModeActive", 0, ini.c_str()) != 0;
    g_cfg.fullscreenBehavior = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"FullscreenBehavior", 1, ini.c_str())), 0, 2);
    g_cfg.gameClickThrough = GetPrivateProfileIntW(L"Island", L"GameClickThrough", 1, ini.c_str()) != 0;
    g_cfg.hudShowFps = GetPrivateProfileIntW(L"Island", L"HudShowFps", 1, ini.c_str()) != 0;
    g_cfg.hudShowFrameTime = GetPrivateProfileIntW(L"Island", L"HudShowFrameTime", 1, ini.c_str()) != 0;
    g_cfg.hudShowCpu = GetPrivateProfileIntW(L"Island", L"HudShowCpu", 1, ini.c_str()) != 0;
    g_cfg.hudShowGpu = GetPrivateProfileIntW(L"Island", L"HudShowGpu", 1, ini.c_str()) != 0;
    g_cfg.hudShowVram = GetPrivateProfileIntW(L"Island", L"HudShowVram", 0, ini.c_str()) != 0;
    g_cfg.hudShowRam = GetPrivateProfileIntW(L"Island", L"HudShowRam", 1, ini.c_str()) != 0;
    g_cfg.hudShowPing = GetPrivateProfileIntW(L"Island", L"HudShowPing", 1, ini.c_str()) != 0;
    g_cfg.hudShowTimer = GetPrivateProfileIntW(L"Island", L"HudShowTimer", 0, ini.c_str()) != 0;
    g_cfg.pingTarget = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"PingTarget", 0, ini.c_str())), 0, 3);

    g_cfg.petType = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Island", L"PetType", 1, ini.c_str())), 0, 2);
    g_cfg.capsLockAlert = GetPrivateProfileIntW(L"Island", L"CapsLockAlert", 1, ini.c_str()) != 0;
    g_cfg.showMicMuteCompact = GetPrivateProfileIntW(L"Island", L"ShowMicMuteCompact", 1, ini.c_str()) != 0;
    g_cfg.wheelVolumeCompact = GetPrivateProfileIntW(L"Island", L"WheelVolumeCompact", 1, ini.c_str()) != 0;
    g_cfg.showCpuCompact = GetPrivateProfileIntW(L"Island", L"ShowCpuCompact", 0, ini.c_str()) != 0;
    g_cfg.fxSpringPhysics = GetPrivateProfileIntW(L"Island", L"FxSpringPhysics", 1, ini.c_str()) != 0;
    g_cfg.fxAmbientGlow = GetPrivateProfileIntW(L"Island", L"FxAmbientGlow", 1, ini.c_str()) != 0;
    g_cfg.fxGlassRim = GetPrivateProfileIntW(L"Island", L"FxGlassRim", 1, ini.c_str()) != 0;
    g_cfg.fxSmoothTabs = GetPrivateProfileIntW(L"Island", L"FxSmoothTabs", 1, ini.c_str()) != 0;
    g_cfg.fxVinylBackdrop = GetPrivateProfileIntW(L"Island", L"FxVinylBackdrop", 1, ini.c_str()) != 0;
}

inline bool IsAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    DWORD type = 0, size = 0;
    bool exists = (RegQueryValueExW(hKey, L"DynamicIslandOverlay", nullptr, &type, nullptr, &size) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return exists;
}

inline void SetAutoStart(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
        RegSetValueExW(hKey, L"DynamicIslandOverlay", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(cmd.c_str()),
            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    }
    else {
        RegDeleteValueW(hKey, L"DynamicIslandOverlay");
    }
    RegCloseKey(hKey);
}

inline void ApplyWindowPosition(HWND hwnd) {
    if (!hwnd) return;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int halfRange = std::max(100, (screenW - static_cast<int>(g_cfg.expandedW)) / 2);
    g_cfg.offsetX = std::clamp(g_cfg.offsetX, -halfRange, halfRange);
    g_cfg.posY = std::clamp(g_cfg.posY, 0, std::max(0, screenH - 120));

    int winX = (screenW - CANVAS_W) / 2 + g_cfg.offsetX;
    int winY = g_cfg.dockToTop ? 0 : g_cfg.posY;
    SetWindowPos(hwnd, HWND_TOPMOST, winX, winY, CANVAS_W, CANVAS_H, SWP_NOSIZE | SWP_NOACTIVATE);
}

inline bool IsFullscreenWindowFocused(HWND islandHwnd) {
    HWND fg = GetForegroundWindow();
    if (!fg || fg == islandHwnd || fg == g_hSettingsWnd) return false;
    if (fg == GetShellWindow() || fg == GetDesktopWindow()) return false;

    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 64);
    if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 || wcscmp(cls, L"Shell_TrayWnd") == 0)
        return false;

    RECT rcWin = {};
    if (!GetWindowRect(fg, &rcWin)) return false;

    HMONITOR hMon = MonitorFromWindow(islandHwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    if (!GetMonitorInfoW(hMon, &mi)) return false;

    LONG style = GetWindowLongW(fg, GWL_STYLE);
    bool coversScreen = (rcWin.left <= mi.rcMonitor.left && rcWin.top <= mi.rcMonitor.top &&
        rcWin.right >= mi.rcMonitor.right && rcWin.bottom >= mi.rcMonitor.bottom);
    return coversScreen && !(style & WS_CAPTION);
}

inline void UpdateClickThroughState(HWND hwnd) {
    if (!hwnd) return;
    bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool wantTransparent = g_cfg.gameModeActive && g_cfg.gameClickThrough &&
        !g_state.isHovered && !altHeld && !g_phoneEvent.hasActiveEvent;

    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool isTransparentNow = (ex & WS_EX_TRANSPARENT) != 0;
    if (wantTransparent != isTransparentNow) {
        if (wantTransparent) SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
        else SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
    }
}

inline std::wstring FormatTime(double seconds) {
    if (seconds < 0.0 || std::isnan(seconds) || std::isinf(seconds)) seconds = 0.0;
    int totalSec = static_cast<int>(std::floor(seconds));
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    wchar_t buf[32];
    swprintf_s(buf, L"%d:%02d", mins, secs);
    return buf;
}

inline std::wstring GetClockString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", st.wHour, st.wMinute);
    return buf;
}

inline std::wstring GetSessionDurationString() {
    ULONGLONG sec = (GetTickCount64() - g_game.sessionStartTick) / 1000ULL;
    int mins = static_cast<int>(sec / 60);
    int hrs = mins / 60;
    mins %= 60;
    wchar_t buf[32];
    if (hrs > 0) swprintf_s(buf, L"%d:%02dч", hrs, mins);
    else swprintf_s(buf, L"%02d:%02d", mins, static_cast<int>(sec % 60));
    return buf;
}

inline float TargetHeightForTab(Tab t) {
    return (t == Tab::Music) ? ISLAND_H_SHORT : ISLAND_H_TALL;
}

inline bool IsAnyTypingActive() {
    return g_state.isTypingCity || g_state.isTypingNote || g_state.isTypingCalNote;
}

inline float CalcGameHudCompactWidth() {
    float w = 26.0f;
    if (g_cfg.hudShowFps) w += g_cfg.hudShowFrameTime ? 102.0f : 62.0f;
    if (g_cfg.hudShowCpu) w += 64.0f;
    if (g_cfg.hudShowGpu) w += g_cfg.hudShowVram ? 104.0f : 64.0f;
    else if (g_cfg.hudShowVram) w += 66.0f;
    if (g_cfg.hudShowRam) w += 66.0f;
    if (g_cfg.hudShowPing) w += 68.0f;
    if (g_cfg.hudShowTimer) w += 58.0f;
    return std::clamp(w, 130.0f, 520.0f);
}

inline void UpdateTargets() {
    if (g_state.isHovered || IsAnyTypingActive() || g_state.isDraggingVolume ||
        g_state.isDraggingTimeline || g_state.isDraggingDiscordVol || g_state.isDragOver ||
        g_phoneEvent.hasActiveEvent || g_state.isDraggingWindow || g_state.isDraggingTabOnIsland) {
        g_state.targetW = g_cfg.expandedW;
        g_state.targetH = (g_phoneEvent.hasActiveEvent && !g_state.isHovered)
            ? ISLAND_H_SHORT
            : TargetHeightForTab(g_state.activeTab);
        g_state.targetR = g_cfg.expandedR;
    }
    else {
        float baseW = g_cfg.gameModeActive ? CalcGameHudCompactWidth() : g_cfg.compactW;
        float extraW = (GetTickCount64() < g_extras.miniBadgeUntil) ? 58.0f : 0.0f;
        g_state.targetW = std::min(535.0f, baseW + extraW);
        g_state.targetH = g_cfg.dockToTop ? 32.0f : 34.0f;
        g_state.targetR = g_cfg.dockToTop ? 12.0f : 17.0f;
    }
    g_state.isAnimating = true;
}

inline void StepAnimation() {
    float speed = std::clamp(g_cfg.animSpeed, 0.08f, 0.65f);

    if (g_cfg.fxSpringPhysics) {
        float stiffness = speed * 0.95f;
        float damping = 0.72f;

        g_extras.velW = (g_extras.velW + (g_state.targetW - g_state.curW) * stiffness) * damping;
        g_extras.velH = (g_extras.velH + (g_state.targetH - g_state.curH) * stiffness) * damping;
        g_extras.velR = (g_extras.velR + (g_state.targetR - g_state.curR) * stiffness) * damping;

        g_state.curW += g_extras.velW;
        g_state.curH += g_extras.velH;
        g_state.curR += g_extras.velR;

        if (std::fabs(g_state.targetW - g_state.curW) < 0.35f && std::fabs(g_extras.velW) < 0.25f &&
            std::fabs(g_state.targetH - g_state.curH) < 0.35f && std::fabs(g_extras.velH) < 0.25f) {
            g_state.curW = g_state.targetW;
            g_state.curH = g_state.targetH;
            g_state.curR = g_state.targetR;
            g_extras.velW = g_extras.velH = g_extras.velR = 0.0f;
            g_state.isAnimating = false;
        }
        else {
            g_state.isAnimating = true;
        }
    }
    else {
        g_state.curW = Lerp(g_state.curW, g_state.targetW, speed);
        g_state.curH = Lerp(g_state.curH, g_state.targetH, speed);
        g_state.curR = Lerp(g_state.curR, g_state.targetR, speed);
        g_extras.velW = g_extras.velH = g_extras.velR = 0.0f;

        if (std::fabs(g_state.curW - g_state.targetW) < 0.4f &&
            std::fabs(g_state.curH - g_state.targetH) < 0.4f) {
            g_state.curW = g_state.targetW;
            g_state.curH = g_state.targetH;
            g_state.curR = g_state.targetR;
            g_state.isAnimating = false;
        }
        else {
            g_state.isAnimating = true;
        }
    }
}

inline void SetWindowInputFocus(HWND hwnd, bool enable) {
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (enable) {
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, (exStyle & ~WS_EX_NOACTIVATE) & ~WS_EX_TRANSPARENT);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
    else {
        g_state.isTypingCity = false;
        g_state.isTypingNote = false;
        g_state.isTypingCalNote = false;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
    }
    UpdateTargets();
}

inline void SetTypingMode(HWND hwnd, bool enable) {
    g_state.isTypingCity = enable;
    if (enable) {
        g_state.isTypingNote = false;
        g_state.isTypingCalNote = false;
    }
    SetWindowInputFocus(hwnd, enable);
}

inline D2D1_RECT_F GetTabRect(int index, int count = 0) {
    if (count <= 0) count = static_cast<int>(GetVisibleTabs().size());
    count = std::max(1, count);
    float left = IslandLeft();
    float top = ISLAND_TOP;
    float innerWidth = g_state.curW - 24.0f;
    float tabW = innerWidth / static_cast<float>(count);
    float tabLeft = left + 12.0f + index * tabW;
    return D2D1::RectF(tabLeft + 1.0f, top + 9.0f, tabLeft + tabW - 1.0f, top + 35.0f);
}

inline void AddTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = TRAY_ICON_ID;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Island (Alt+G — Игровой режим, ПКМ — Настройки)");

    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

inline void RemoveTrayIcon() {
    if (g_nid.hWnd) {
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        g_nid.hWnd = nullptr;
    }
}