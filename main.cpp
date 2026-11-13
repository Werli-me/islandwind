#include "Common.h"
#include "Graphics.h"
#include "HttpUtils.h"
#include "WeatherTab.h"
#include "MusicTab.h"
#include "ShelfTab.h"
#include "PhoneListener.h"
#include <audiopolicy.h>

// ============================================================================
// МОДУЛЬ УПРАВЛЕНИЯ DISCORD (МИКРОФОН, НАУШНИКИ, ГРОМКОСТЬ DISCORD.EXE, ВЫХОД)
// ============================================================================

struct DiscordState {
    bool isRunning = false;
    bool micMuted = false;
    bool deafened = false;
    bool micWasMutedBeforeDeafen = false;
    float discordVolume = 1.0f;
    float micPeak = 0.0f;
    bool isDraggingVol = false;
} g_discord;

// ============================================================================
// НАТИВНЫЙ КЛИЕНТ DISCORD IPC (\\.\pipe\discord-ipc-0)
// ============================================================================

//struct DiscordIpcClient {
//    HANDLE hPipe = INVALID_HANDLE_VALUE;
//    bool connected = false;
//    bool authorized = false;
//    // Публичный Client ID приложения Discord (StreamKit / Overlay)
//    const char* clientId = "207646673902501888";
//
//    bool Connect() {
//        if (hPipe != INVALID_HANDLE_VALUE) return true;
//
//        for (int i = 0; i < 10; ++i) {
//            wchar_t pipeName[64];
//            swprintf_s(pipeName, L"\\\\.\\pipe\\discord-ipc-%d", i);
//            hPipe = CreateFileW(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
//            if (hPipe != INVALID_HANDLE_VALUE) break;
//        }
//        if (hPipe == INVALID_HANDLE_VALUE) return false;
//
//        // Переводим пайп в неблокирующий режим чтения, чтобы не тормозить UI
//        DWORD mode = PIPE_NOWAIT;
//        SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr);
//
//        // Отправляем Handshake (Opcode = 0)
//        std::string hs = std::string("{\"v\":1,\"client_id\":\"") + clientId + "\"}";
//        if (!SendPacket(0, hs)) {
//            Disconnect();
//            return false;
//        }
//        connected = true;
//        return true;
//    }
//
//    void Disconnect() {
//        if (hPipe != INVALID_HANDLE_VALUE) {
//            CloseHandle(hPipe);
//            hPipe = INVALID_HANDLE_VALUE;
//        }
//        connected = false;
//        authorized = false;
//    }
//
//    bool SendPacket(uint32_t opcode, const std::string& json) {
//        if (hPipe == INVALID_HANDLE_VALUE) return false;
//        struct Header {
//            uint32_t op;
//            uint32_t len;
//        } hdr = { opcode, static_cast<uint32_t>(json.size()) };
//
//        DWORD written = 0;
//        if (!WriteFile(hPipe, &hdr, sizeof(hdr), &written, nullptr) || written != sizeof(hdr)) {
//            Disconnect();
//            return false;
//        }
//        if (!WriteFile(hPipe, json.data(), hdr.len, &written, nullptr) || written != hdr.len) {
//            Disconnect();
//            return false;
//        }
//        return true;
//    }
//
//    // Запрос текущего состояния микрофона и наушников у Discord
//    void RequestVoiceSettings() {
//        if (!connected) return;
//        SendPacket(1, "{\"cmd\":\"GET_VOICE_SETTINGS\",\"args\":{},\"nonce\":\"vs_get\"}");
//    }
//
//    // Прямая установка mute / deaf через IPC (если авторизован)
//    bool SetVoiceMuteDeaf(bool mute, bool deaf) {
//        if (!connected || !authorized) return false;
//        std::string cmd = std::string("{\"cmd\":\"SET_VOICE_SETTINGS\",\"args\":{\"mute\":") +
//            (mute ? "true" : "false") + ",\"deaf\":" + (deaf ? "true" : "false") +
//            "},\"nonce\":\"vs_set\"}";
//        return SendPacket(1, cmd);
//    }
//
//    // Прямой выход из голосового канала через IPC (SELECT_VOICE_CHANNEL с channel_id: null)
//    bool DisconnectVoiceChannel() {
//        if (!connected || !authorized) return false;
//        return SendPacket(1, "{\"cmd\":\"SELECT_VOICE_CHANNEL\",\"args\":{\"channel_id\":null},\"nonce\":\"vc_leave\"}");
//    }
//
//    // Неблокирующее чтение ответов и событий из пайпа
//    void PollIncoming() {
//        if (!connected && !Connect()) return;
//
//        DWORD avail = 0;
//        if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &avail, nullptr) || avail < 8) return;
//
//        uint32_t op = 0, len = 0;
//        DWORD readBytes = 0;
//        if (!ReadFile(hPipe, &op, 4, &readBytes, nullptr) ||
//            !ReadFile(hPipe, &len, 4, &readBytes, nullptr) || len == 0 || len > 65536) {
//            Disconnect();
//            return;
//        }
//
//        std::string payload(len, '\0');
//        if (!ReadFile(hPipe, payload.data(), len, &readBytes, nullptr)) {
//            Disconnect();
//            return;
//        }
//
//        // Как только пришло событие READY — подписываемся на изменения VOICE_SETTINGS_UPDATE
//        if (payload.find("\"evt\":\"READY\"") != std::string::npos) {
//            RequestVoiceSettings();
//            SendPacket(1, "{\"cmd\":\"SUBSCRIBE\",\"evt\":\"VOICE_SETTINGS_UPDATE\",\"args\":{},\"nonce\":\"sub_vs\"}");
//        }
//
//        // Парсим реальный статус mute и deaf из JSON-ответа Discord
//        if (payload.find("\"VOICE_SETTINGS") != std::string::npos || payload.find("\"vs_get\"") != std::string::npos) {
//            if (payload.find("\"code\":") == std::string::npos) {
//                authorized = true;
//                size_t pDeaf = payload.find("\"deaf\":");
//                if (pDeaf != std::string::npos) {
//                    g_discord.deafened = (payload.compare(pDeaf + 7, 4, "true") == 0);
//                }
//                size_t pMute = payload.find("\"mute\":");
//                if (pMute != std::string::npos) {
//                    bool m = (payload.compare(pMute + 7, 4, "true") == 0);
//                    g_discord.micMuted = m || g_discord.deafened;
//                }
//            }
//        }
//    }
//} g_discordIpc;

inline IAudioEndpointVolume* g_pMicVolume = nullptr;
inline IAudioMeterInformation* g_pMicMeter = nullptr;

inline void InitDiscordAudio() {
    IMMDeviceEnumerator* pEnum = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnum))) {
        IMMDevice* pMic = nullptr;
        if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &pMic))) {
            pMic->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr, (void**)&g_pMicVolume);
            pMic->Activate(__uuidof(IAudioMeterInformation), CLSCTX_INPROC_SERVER, nullptr, (void**)&g_pMicMeter);
            pMic->Release();
        }
        pEnum->Release();
    }
    if (g_pMicVolume) {
        BOOL muted = FALSE;
        if (SUCCEEDED(g_pMicVolume->GetMute(&muted))) g_discord.micMuted = (muted != FALSE);
    }
}

inline void CleanupDiscordAudio() {
    if (g_pMicMeter) { g_pMicMeter->Release(); g_pMicMeter = nullptr; }
    if (g_pMicVolume) { g_pMicVolume->Release(); g_pMicVolume = nullptr; }
}

inline void PollDiscordMicLevel() {
    if (g_pMicVolume) {
        BOOL muted = FALSE;
        if (SUCCEEDED(g_pMicVolume->GetMute(&muted))) {
            g_discord.micMuted = (muted != FALSE);
        }
    }
    if (g_pMicMeter && !g_discord.micMuted) {
        float pk = 0.0f;
        if (SUCCEEDED(g_pMicMeter->GetPeakValue(&pk))) {
            g_discord.micPeak = Lerp(g_discord.micPeak, std::clamp(pk, 0.0f, 1.0f), 0.4f);
        }
    }
    else {
        g_discord.micPeak = 0.0f;
    }
}

inline void SendChordKey(WORD vkMain, bool ctrl = true, bool shift = true, bool alt = false) {
    auto sendKey = [](WORD vk, bool up) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = vk;
        in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
        in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0);
        SendInput(1, &in, sizeof(INPUT));
        };

    if (ctrl)  sendKey(VK_LCONTROL, false);
    if (shift) sendKey(VK_LSHIFT, false);
    if (alt)   sendKey(VK_LMENU, false);
    Sleep(15);

    sendKey(vkMain, false);
    Sleep(25);
    sendKey(vkMain, true);
    Sleep(15);

    if (alt)   sendKey(VK_LMENU, true);
    if (shift) sendKey(VK_LSHIFT, true);
    if (ctrl)  sendKey(VK_LCONTROL, true);
}

// Поиск аудиосессии Discord.exe и управление громкостью собеседников отдельно от системы
inline void SyncDiscordAppVolume(bool setNewVol, float newVol = 1.0f) {
    IMMDeviceEnumerator* pEnum = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnum))) return;

    IMMDevice* pRender = nullptr;
    if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pRender))) {
        IAudioSessionManager2* pMgr = nullptr;
        if (SUCCEEDED(pRender->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, nullptr, (void**)&pMgr))) {
            IAudioSessionEnumerator* pSessEnum = nullptr;
            if (SUCCEEDED(pMgr->GetSessionEnumerator(&pSessEnum))) {
                int count = 0;
                pSessEnum->GetCount(&count);
                bool foundDiscord = false;

                for (int i = 0; i < count; ++i) {
                    IAudioSessionControl* pCtrl = nullptr;
                    if (SUCCEEDED(pSessEnum->GetSession(i, &pCtrl))) {
                        IAudioSessionControl2* pCtrl2 = nullptr;
                        if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2))) {
                            DWORD pid = 0;
                            pCtrl2->GetProcessId(&pid);
                            if (pid != 0) {
                                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                                if (hProc) {
                                    wchar_t exePath[MAX_PATH] = {};
                                    DWORD sz = MAX_PATH;
                                    if (QueryFullProcessImageNameW(hProc, 0, exePath, &sz)) {
                                        std::wstring pathStr(exePath);
                                        for (auto& c : pathStr) c = towlower(c);
                                        if (pathStr.find(L"discord.exe") != std::wstring::npos) {
                                            foundDiscord = true;
                                            ISimpleAudioVolume* pVol = nullptr;
                                            if (SUCCEEDED(pCtrl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol))) {
                                                if (setNewVol) {
                                                    float clamped = std::clamp(newVol, 0.0f, 1.0f);
                                                    pVol->SetMasterVolume(clamped, nullptr);
                                                    g_discord.discordVolume = clamped;
                                                }
                                                else if (!g_discord.isDraggingVol) {
                                                    float curV = 1.0f;
                                                    if (SUCCEEDED(pVol->GetMasterVolume(&curV))) {
                                                        g_discord.discordVolume = curV;
                                                    }
                                                }
                                                pVol->Release();
                                            }
                                        }
                                    }
                                    CloseHandle(hProc);
                                }
                            }
                            pCtrl2->Release();
                        }
                        pCtrl->Release();
                    }
                }
                g_discord.isRunning = foundDiscord;
                pSessEnum->Release();
            }
            pMgr->Release();
        }
        pRender->Release();
    }
    pEnum->Release();
}

inline void ToggleDiscordMic() {
    // Если мы были в полном муте (наушники выкл) и нажали на микрофон — Discord автоматически снимает и Deafen
    if (g_discord.deafened) {
        g_discord.deafened = false;
        g_discord.micMuted = false;
    }
    else {
        g_discord.micMuted = !g_discord.micMuted;
    }

    if (g_pMicVolume) {
        g_pMicVolume->SetMute(g_discord.micMuted ? TRUE : FALSE, nullptr);
    }
    SendChordKey('M', true, true, false); // Ctrl + Shift + M
    TriggerMiniBadge(g_discord.micMuted ? L"\U0001F399 MIC OFF" : L"\U0001F399 MIC ON", !g_discord.micMuted, 1800);
}

inline void ToggleDiscordDeafen() {
    g_discord.deafened = !g_discord.deafened;

    if (g_discord.deafened) {
        // Запоминаем, был ли выключен микрофон до нажатия на наушники, и глушим его
        g_discord.micWasMutedBeforeDeafen = g_discord.micMuted;
        g_discord.micMuted = true;
        if (g_pMicVolume) g_pMicVolume->SetMute(TRUE, nullptr);
    }
    else {
        // При включении наушников обратно возвращаем микрофон в прежнее состояние (как это делает Discord)
        g_discord.micMuted = g_discord.micWasMutedBeforeDeafen;
        if (g_pMicVolume) g_pMicVolume->SetMute(g_discord.micMuted ? TRUE : FALSE, nullptr);
    }

    SendChordKey('D', true, true, false); // Ctrl + Shift + D
    TriggerMiniBadge(g_discord.deafened ? L"\U0001F507 ЗВУК ВЫКЛ" : L"\U0001F50A ЗВУК ВКЛ", !g_discord.deafened, 1800);
}

inline void LeaveDiscordCall() {
    SendChordKey('H', true, true, false);
    TriggerMiniBadge(L"\U0001F4DE ОТКЛЮЧЕНО", false, 1800);
}

inline D2D1_RECT_F GetDiscordBtnRect(int idx) {
    float left = IslandLeft() + 14.0f;
    float right = IslandLeft() + g_state.curW - 14.0f;
    float gap = 10.0f;
    float w = (right - left - 2.0f * gap) / 3.0f;
    float x = left + idx * (w + gap);
    return D2D1::RectF(x, ISLAND_TOP + 92.0f, x + w, ISLAND_TOP + 162.0f);
}

inline D2D1_RECT_F GetDiscordVolSliderRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    return D2D1::RectF(left + 142.0f, ISLAND_TOP + 184.0f, right - 54.0f, ISLAND_TOP + 214.0f);
}

inline void UpdateDiscordVolFromMouse(HWND hwnd, int mouseX) {
    D2D1_RECT_F r = GetDiscordVolSliderRect();
    float t = std::clamp((mouseX - r.left) / (r.right - r.left), 0.0f, 1.0f);
    SyncDiscordAppVolume(true, t);
    RenderIsland(hwnd);
}

inline void DrawDiscordTab(float left, float top, float right, const POINT& mp) {
    static ULONGLONG s_lastSync = 0;
    ULONGLONG now = GetTickCount64();
    if (now - s_lastSync > 1200) {
        s_lastSync = now;
        SyncDiscordAppVolume(false);
    }

    // Верхняя карточка: статус подключения и индикатор активации микрофона
    D2D1_RECT_F hdrCard = D2D1::RectF(left + 14.0f, top + 42.0f, right - 14.0f, top + 82.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(hdrCard, 12.0f, 12.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(hdrCard, 12.0f, 12.0f), g_pBrushBorder, 1.0f);

    float avatarCx = hdrCard.left + 24.0f;
    float avatarCy = (hdrCard.top + hdrCard.bottom) * 0.5f;
    bool isSpeaking = (!g_discord.micMuted && g_discord.micPeak > 0.04f);

    // Зеленое кольцо вокруг иконки, когда мы говорим в микрофон
    if (isSpeaking) {
        float ringR = 14.0f + g_discord.micPeak * 4.5f;
        g_pBrushIcon->SetColor(D2D1::ColorF(0.26f, 0.92f, 0.48f, 0.85f));
        g_pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(avatarCx, avatarCy), ringR, ringR), g_pBrushIcon, 2.2f);
        g_hasScrolling = true;
    }

    g_pBrushIcon->SetColor(D2D1::ColorF(0.35f, 0.40f, 0.95f, 0.9f)); // Фирменный цвет Discord Blurple
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(avatarCx, avatarCy), 12.0f, 12.0f), g_pBrushIcon);
    g_pRT->DrawText(L"\U0001F399", 2, g_pTextFormatTime,
        D2D1::RectF(avatarCx - 12.0f, avatarCy - 12.0f, avatarCx + 12.0f, avatarCy + 12.0f), g_pBrushText);

    std::wstring statusTitle = g_discord.isRunning ? L"Discord активен · Голосовой пульт" : L"Управление микрофоном и связью";
    std::wstring statusSub = g_discord.micMuted
        ? L"Микрофон выключен (Mute)"
        : (isSpeaking ? L"Вы сейчас говорите..." : L"Микрофон включен · Ожидание голоса");

    g_pRT->DrawText(statusTitle.c_str(), (UINT32)statusTitle.size(), g_pTextFormatLeftTitle,
        D2D1::RectF(hdrCard.left + 46.0f, hdrCard.top + 4.0f, hdrCard.right - 12.0f, hdrCard.top + 24.0f), g_pBrushText);
    g_pRT->DrawText(statusSub.c_str(), (UINT32)statusSub.size(), g_pTextFormatSmallMuted,
        D2D1::RectF(hdrCard.left + 46.0f, hdrCard.top + 22.0f, hdrCard.right - 12.0f, hdrCard.bottom - 4.0f),
        g_discord.micMuted ? g_pBrushAccent : (isSpeaking ? g_pBrushText : g_pBrushTextMuted));

    // Три большие кнопки управления звонком: 1. Микрофон | 2. Наушники | 3. Ливнуть
    struct BtnMeta {
        const wchar_t* icon;
        const wchar_t* title;
        const wchar_t* sub;
        bool activeRed;
    } btns[3] = {
        { g_discord.micMuted ? L"\U0001F507" : L"\U0001F399", g_discord.micMuted ? L"Размутиться" : L"Микрофон", L"Ctrl+Shift+M", g_discord.micMuted },
        { g_discord.deafened ? L"\U0001F507" : L"\U0001F3A7", g_discord.deafened ? L"Вкл. звук" : L"Наушники", L"Ctrl+Shift+D", g_discord.deafened },
        { L"\U0001F4DE", L"Ливнуть", L"Ctrl+Shift+H", true }
    };

    for (int i = 0; i < 3; ++i) {
        D2D1_RECT_F br = GetDiscordBtnRect(i);
        bool hot = PtInRectF(br, mp.x, mp.y);

        D2D1_COLOR_F bgCol = (i == 2)
            ? D2D1::ColorF(0.92f, 0.25f, 0.28f, hot ? 0.38f : 0.22f)
            : (btns[i].activeRed
                ? D2D1::ColorF(0.95f, 0.30f, 0.32f, hot ? 0.35f : 0.22f)
                : D2D1::ColorF(1.0f, 1.0f, 1.0f, hot ? 0.14f : 0.07f));

        FillRR(br, 12.0f, bgCol);
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(br, 12.0f, 12.0f),
            btns[i].activeRed ? g_pBrushAccent : g_pBrushBorder, 1.0f);

        g_pRT->DrawText(btns[i].icon, (UINT32)wcslen(btns[i].icon), g_pTextFormatTitle,
            D2D1::RectF(br.left, br.top + 6.0f, br.right, br.top + 28.0f), g_pBrushText);
        g_pRT->DrawText(btns[i].title, (UINT32)wcslen(btns[i].title), g_pTextFormatTitle,
            D2D1::RectF(br.left, br.top + 28.0f, br.right, br.top + 48.0f), g_pBrushText);
        g_pRT->DrawText(btns[i].sub, (UINT32)wcslen(btns[i].sub), g_pTextFormatBadge,
            D2D1::RectF(br.left, br.top + 48.0f, br.right, br.bottom - 4.0f), g_pBrushTextMuted);
    }

    // Нижняя панель: индивидуальный ползунок громкости собеседников (Discord.exe)
    D2D1_RECT_F volCard = D2D1::RectF(left + 14.0f, top + 172.0f, right - 14.0f, top + 224.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(volCard, 12.0f, 12.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(volCard, 12.0f, 12.0f), g_pBrushBorder, 1.0f);

    g_pRT->DrawText(L"Звук собеседников:", 18, g_pTextFormatLeftSub,
        D2D1::RectF(volCard.left + 12.0f, volCard.top, volCard.left + 130.0f, volCard.bottom), g_pBrushText);

    D2D1_RECT_F sr = GetDiscordVolSliderRect();
    float cy = (volCard.top + volCard.bottom) * 0.5f;
    D2D1_RECT_F track = D2D1::RectF(sr.left, cy - 3.0f, sr.right, cy + 3.0f);
    FillRR(track, 3.0f, D2D1::ColorF(1, 1, 1, 0.12f));

    float fillX = track.left + (track.right - track.left) * std::clamp(g_discord.discordVolume, 0.0f, 1.0f);
    if (fillX > track.left + 2.0f) {
        FillRR(D2D1::RectF(track.left, cy - 3.0f, fillX, cy + 3.0f), 3.0f, ColorFromRef(GetActiveAccentRef()));
    }
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(fillX, cy), 6.5f, 6.5f), g_pBrushText);

    int pct = static_cast<int>(std::round(g_discord.discordVolume * 100.0f));
    std::wstring pctStr = std::to_wstring(pct) + L"%";
    g_pRT->DrawText(pctStr.c_str(), (UINT32)pctStr.size(), g_pTextFormatRightSub,
        D2D1::RectF(sr.right + 4.0f, volCard.top, volCard.right - 10.0f, volCard.bottom), g_pBrushAccent);
}

// ============================================================================
// ОСТАЛЬНЫЕ ВКЛАДКИ: БУФЕР ОБМЕНА | КАЛЕНДАРЬ | ЗАМЕТКИ И TO-DO | ИГРЫ
// ============================================================================

struct ClipHistoryItem {
    std::wstring text;
    std::wstring timeStr;
};

struct TodoItem {
    bool done = false;
    std::wstring text;
};

struct CalendarEventItem {
    int year = 0;
    int month = 0;
    int day = 0;
    std::wstring text;
};

inline std::vector<ClipHistoryItem> g_clipHistory;
inline int g_clipScroll = 0;
inline DWORD g_lastClipSeq = 0;
inline std::wstring g_clipNotice;
inline ULONGLONG g_clipNoticeUntil = 0;

inline std::vector<std::wstring> g_notePages = { L"Заметка 1: быстрые мысли и идеи..." };
inline int g_activeNoteIdx = 0;
inline int g_notesMode = 0; // 0 = Заметки (несколько вкладок), 1 = To-Do список
inline std::vector<TodoItem> g_todoList = {
    { false, L"Настроить Dynamic Island" },
    { true,  L"Проверить Игровой режим (Alt + G)" }
};
inline std::wstring g_todoInput = L"";
inline int g_todoScroll = 0;

inline std::vector<CalendarEventItem> g_calEvents;
inline std::wstring g_calEventInput = L"";
inline int g_calEventScroll = 0;
inline int g_calYear = 0;
inline int g_calMonth = 0;
inline int g_calSelectedDay = 0;

inline std::wstring& GetActiveNotePage() {
    if (g_notePages.empty()) g_notePages.push_back(L"");
    g_activeNoteIdx = std::clamp(g_activeNoteIdx, 0, static_cast<int>(g_notePages.size()) - 1);
    return g_notePages[g_activeNoteIdx];
}

inline std::vector<int> GetEventsForSelectedDate() {
    std::vector<int> indices;
    for (size_t i = 0; i < g_calEvents.size(); ++i) {
        if (g_calEvents[i].year == g_calYear &&
            g_calEvents[i].month == g_calMonth &&
            g_calEvents[i].day == g_calSelectedDay) {
            indices.push_back(static_cast<int>(i));
        }
    }
    return indices;
}

inline bool DayHasEvents(int y, int m, int d) {
    for (const auto& ev : g_calEvents) {
        if (ev.year == y && ev.month == m && ev.day == d) return true;
    }
    return false;
}

inline void SaveExtraTabsData() {
    std::wstring notesFile = GetAppDirPath() + L"island_notes.txt";
    std::ofstream fn(notesFile, std::ios::binary | std::ios::trunc);
    if (fn.is_open()) {
        for (size_t i = 0; i < g_notePages.size(); ++i) {
            std::string u8 = WideToUtf8(g_notePages[i]);
            fn.write(u8.data(), u8.size());
            if (i + 1 < g_notePages.size()) {
                const char* sep = "\n---ISLAND_NOTE_SEP---\n";
                fn.write(sep, strlen(sep));
            }
        }
    }

    std::wstring todoFile = GetAppDirPath() + L"island_todo.txt";
    std::ofstream ft(todoFile, std::ios::binary | std::ios::trunc);
    if (ft.is_open()) {
        for (const auto& td : g_todoList) {
            std::string line = std::string(td.done ? "1\t" : "0\t") + WideToUtf8(td.text) + "\n";
            ft.write(line.data(), line.size());
        }
    }

    std::wstring calFile = GetAppDirPath() + L"island_calendar.txt";
    std::ofstream fc(calFile, std::ios::binary | std::ios::trunc);
    if (fc.is_open()) {
        for (const auto& ev : g_calEvents) {
            wchar_t prefix[32];
            swprintf_s(prefix, L"%04d-%02d-%02d\t", ev.year, ev.month, ev.day);
            std::string line = WideToUtf8(std::wstring(prefix) + ev.text) + "\n";
            fc.write(line.data(), line.size());
        }
    }

    std::wstring clipFile = GetAppDirPath() + L"island_clipboard.txt";
    if (!g_cfg.persistClipboard) {
        DeleteFileW(clipFile.c_str());
        return;
    }
    std::ofstream fclip(clipFile, std::ios::binary | std::ios::trunc);
    if (fclip.is_open()) {
        for (const auto& it : g_clipHistory) {
            std::wstring flat = it.text;
            for (auto& c : flat) if (c == L'\n' || c == L'\r') c = L' ';
            std::string line = WideToUtf8(it.timeStr + L"\t" + flat) + "\n";
            fclip.write(line.data(), line.size());
        }
    }
}

inline void LoadExtraTabsData() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    g_calYear = st.wYear;
    g_calMonth = st.wMonth;
    g_calSelectedDay = st.wDay;

    std::wstring notesFile = GetAppDirPath() + L"island_notes.txt";
    std::ifstream fn(notesFile, std::ios::binary);
    if (fn.is_open()) {
        std::string content((std::istreambuf_iterator<char>(fn)), std::istreambuf_iterator<char>());
        if (!content.empty()) {
            g_notePages.clear();
            std::string sep = "\n---ISLAND_NOTE_SEP---\n";
            size_t pos = 0;
            while (true) {
                size_t next = content.find(sep, pos);
                if (next == std::string::npos) {
                    g_notePages.push_back(Utf8ToWide(content.substr(pos)));
                    break;
                }
                g_notePages.push_back(Utf8ToWide(content.substr(pos, next - pos)));
                pos = next + sep.size();
            }
            if (g_notePages.empty()) g_notePages.push_back(L"");
        }
    }

    std::wstring todoFile = GetAppDirPath() + L"island_todo.txt";
    std::ifstream ft(todoFile);
    if (ft.is_open()) {
        g_todoList.clear();
        std::string line;
        while (std::getline(ft, line)) {
            if (line.size() >= 2 && line[1] == '\t') {
                bool done = (line[0] == '1');
                g_todoList.push_back({ done, Utf8ToWide(line.substr(2)) });
            }
        }
    }

    std::wstring calFile = GetAppDirPath() + L"island_calendar.txt";
    std::ifstream fcal(calFile);
    if (fcal.is_open()) {
        g_calEvents.clear();
        std::string line;
        while (std::getline(fcal, line)) {
            if (line.size() > 11 && line[4] == '-' && line[7] == '-' && line[10] == '\t') {
                int y = atoi(line.substr(0, 4).c_str());
                int m = atoi(line.substr(5, 2).c_str());
                int d = atoi(line.substr(8, 2).c_str());
                g_calEvents.push_back({ y, m, d, Utf8ToWide(line.substr(11)) });
            }
        }
    }

    if (g_cfg.persistClipboard) {
        std::wstring clipFile = GetAppDirPath() + L"island_clipboard.txt";
        std::ifstream fc(clipFile);
        std::string line;
        while (std::getline(fc, line) && g_clipHistory.size() < 20) {
            std::wstring wline = Utf8ToWide(line);
            size_t tab = wline.find(L'\t');
            if (tab != std::wstring::npos) {
                g_clipHistory.push_back({ wline.substr(tab + 1), wline.substr(0, tab) });
            }
        }
    }
    g_lastClipSeq = GetClipboardSequenceNumber();
}

inline void CopyWideToClipboard(HWND hwnd, const std::wstring& str) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (str.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hMem) {
        void* p = GlobalLock(hMem);
        if (p) {
            memcpy(p, str.c_str(), bytes);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
    g_lastClipSeq = GetClipboardSequenceNumber();
}

inline void PollClipboardChanges(HWND hwnd) {
    DWORD seq = GetClipboardSequenceNumber();
    if (seq == g_lastClipSeq) return;
    g_lastClipSeq = seq;

    std::wstring txt = TrimW(ReadClipboardText(hwnd));
    if (txt.empty() || txt.size() > 2000) return;

    for (auto it = g_clipHistory.begin(); it != g_clipHistory.end(); ++it) {
        if (it->text == txt) {
            g_clipHistory.erase(it);
            break;
        }
    }
    g_clipHistory.insert(g_clipHistory.begin(), { txt, GetClockString() });
    if (g_clipHistory.size() > 20) g_clipHistory.pop_back();
    if (g_cfg.persistClipboard) SaveExtraTabsData();
}

// --- 1. Вкладка «Буфер обмена» ---
inline D2D1_RECT_F GetClipClearBtnRect() {
    float right = IslandLeft() + g_state.curW;
    return D2D1::RectF(right - 90.0f, ISLAND_TOP + 40.0f, right - 14.0f, ISLAND_TOP + 62.0f);
}

inline D2D1_RECT_F GetClipCardRect(int slot) {
    float left = IslandLeft() + 14.0f;
    float right = IslandLeft() + g_state.curW - 14.0f;
    float y = ISLAND_TOP + 66.0f + slot * 52.0f;
    return D2D1::RectF(left, y, right, y + 46.0f);
}

inline D2D1_RECT_F GetClipCardCloseRect(const D2D1_RECT_F& card) {
    float cy = (card.top + card.bottom) * 0.5f;
    return D2D1::RectF(card.right - 28.0f, cy - 10.0f, card.right - 8.0f, cy + 10.0f);
}

inline void ClampClipScroll() {
    int maxSc = std::max(0, static_cast<int>(g_clipHistory.size()) - 3);
    g_clipScroll = std::clamp(g_clipScroll, 0, maxSc);
}

inline void DrawClipboardTab(float left, float top, float right, const POINT& mp) {
    ULONGLONG now = GetTickCount64();
    std::wstring hdr = (now < g_clipNoticeUntil && !g_clipNotice.empty())
        ? g_clipNotice
        : (L"История буфера: " + std::to_wstring(g_clipHistory.size()));
    g_pRT->DrawText(hdr.c_str(), (UINT32)hdr.size(), g_pTextFormatLeftSub,
        D2D1::RectF(left + 18.0f, top + 40.0f, right - 100.0f, top + 62.0f),
        (now < g_clipNoticeUntil) ? g_pBrushAccent : g_pBrushTextMuted);

    if (!g_clipHistory.empty()) {
        D2D1_RECT_F clr = GetClipClearBtnRect();
        bool hot = PtInRectF(clr, mp.x, mp.y);
        if (hot) FillRR(clr, 8.0f, D2D1::ColorF(1, 1, 1, 0.10f));
        g_pRT->DrawText(L"Очистить", 8, g_pTextFormatSub, clr, hot ? g_pBrushText : g_pBrushTextMuted);
    }

    if (g_clipHistory.empty()) {
        D2D1_RECT_F z = D2D1::RectF(left + 14.0f, top + 66.0f, right - 14.0f, top + 224.0f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(z, 16.0f, 16.0f), g_pBrushInputBg);
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(z, 16.0f, 16.0f), g_pBrushBorder, 1.0f);
        const wchar_t* msg = L"Буфер обмена пуст\nСкопируйте любой текст (Ctrl+C), и он появится здесь";
        g_pRT->DrawText(msg, (UINT32)wcslen(msg), g_pTextFormatSub, z, g_pBrushTextMuted);
        return;
    }

    for (int slot = 0; slot < 3; ++slot) {
        int idx = g_clipScroll + slot;
        if (idx >= static_cast<int>(g_clipHistory.size())) break;
        const auto& it = g_clipHistory[idx];

        D2D1_RECT_F card = GetClipCardRect(slot);
        bool hot = PtInRectF(card, mp.x, mp.y);
        FillRR(card, 11.0f, D2D1::ColorF(1, 1, 1, hot ? 0.13f : 0.07f));
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(card, 11.0f, 11.0f), g_pBrushBorder, 1.0f);

        std::wstring oneLine = it.text;
        for (auto& c : oneLine) if (c == L'\n' || c == L'\r') c = L' ';

        g_pRT->DrawText(it.timeStr.c_str(), (UINT32)it.timeStr.size(), g_pTextFormatTime,
            D2D1::RectF(card.left + 10.0f, card.top + 6.0f, card.left + 52.0f, card.bottom - 6.0f), g_pBrushAccent);

        D2D1_RECT_F tr = D2D1::RectF(card.left + 58.0f, card.top + 6.0f, card.right - 34.0f, card.bottom - 6.0f);
        if (hot) DrawScrollingText(SLOT_TABBODY, oneLine, g_pTextFormatLeftSub, tr, g_pBrushText);
        else DrawClippedText(oneLine, g_pTextFormatLeftSub, tr, g_pBrushText);

        D2D1_RECT_F cr = GetClipCardCloseRect(card);
        bool hotClose = PtInRectF(cr, mp.x, mp.y);
        if (hotClose) FillRR(cr, 8.0f, D2D1::ColorF(1, 1, 1, 0.18f));
        g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatSub, cr, hotClose ? g_pBrushText : g_pBrushTextMuted);
    }
}

// --- 2. Вкладка «Календарь» с поддержкой списка событий ---
inline int DaysInMonth(int y, int m) {
    static const int d[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) return 29;
    return d[std::clamp(m - 1, 0, 11)];
}

inline int FirstWeekdayMondayZero(int y, int m) {
    long days = DaysFromCivil(y, m, 1);
    int w = static_cast<int>((days + 3) % 7);
    return w < 0 ? w + 7 : w;
}

inline void AddCalendarEventFromInput() {
    std::wstring t = TrimW(g_calEventInput);
    if (t.empty()) return;
    g_calEvents.push_back({ g_calYear, g_calMonth, g_calSelectedDay, t });
    g_calEventInput.clear();
    SaveExtraTabsData();
}

inline void DrawCalendarTab(float left, float top, float right, const POINT& mp) {
    static const wchar_t* kMonths[12] = {
        L"Январь", L"Февраль", L"Март", L"Апрель", L"Май", L"Июнь",
        L"Июль", L"Август", L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь"
    };
    SYSTEMTIME stNow;
    GetLocalTime(&stNow);
    if (g_calYear == 0) { g_calYear = stNow.wYear; g_calMonth = stNow.wMonth; g_calSelectedDay = stNow.wDay; }

    D2D1_RECT_F leftCard = D2D1::RectF(left + 14.0f, top + 44.0f, left + 194.0f, top + 224.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(leftCard, 14.0f, 14.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(leftCard, 14.0f, 14.0f), g_pBrushBorder, 1.0f);

    std::wstring mTitle = std::wstring(kMonths[std::clamp(g_calMonth - 1, 0, 11)]) + L" " + std::to_wstring(g_calYear);
    g_pRT->DrawText(mTitle.c_str(), (UINT32)mTitle.size(), g_pTextFormatLeftTitle,
        D2D1::RectF(leftCard.left + 10.0f, leftCard.top + 6.0f, leftCard.right - 40.0f, leftCard.top + 26.0f), g_pBrushAccent);

    std::wstring selDayStr = std::to_wstring(g_calSelectedDay);
    g_pRT->DrawText(selDayStr.c_str(), (UINT32)selDayStr.size(), g_pTextFormatRightTitle,
        D2D1::RectF(leftCard.right - 42.0f, leftCard.top + 6.0f, leftCard.right - 10.0f, leftCard.top + 26.0f), g_pBrushText);

    D2D1_RECT_F bPrev = D2D1::RectF(leftCard.left + 8.0f, leftCard.top + 30.0f, leftCard.left + 34.0f, leftCard.top + 50.0f);
    D2D1_RECT_F bToday = D2D1::RectF(leftCard.left + 38.0f, leftCard.top + 30.0f, leftCard.right - 38.0f, leftCard.top + 50.0f);
    D2D1_RECT_F bNext = D2D1::RectF(leftCard.right - 34.0f, leftCard.top + 30.0f, leftCard.right - 8.0f, leftCard.top + 50.0f);
    FillRR(bPrev, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(bPrev, mp.x, mp.y) ? 0.16f : 0.07f));
    FillRR(bToday, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(bToday, mp.x, mp.y) ? 0.16f : 0.07f));
    FillRR(bNext, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(bNext, mp.x, mp.y) ? 0.16f : 0.07f));
    g_pRT->DrawText(L"<", 1, g_pTextFormatSub, bPrev, g_pBrushText);
    g_pRT->DrawText(L"Сегодня", 7, g_pTextFormatTime, bToday, g_pBrushText);
    g_pRT->DrawText(L">", 1, g_pTextFormatSub, bNext, g_pBrushText);

    D2D1_RECT_F inBox = D2D1::RectF(leftCard.left + 8.0f, leftCard.top + 56.0f, leftCard.right - 34.0f, leftCard.top + 80.0f);
    D2D1_RECT_F addBtn = D2D1::RectF(leftCard.right - 30.0f, leftCard.top + 56.0f, leftCard.right - 8.0f, leftCard.top + 80.0f);
    FillRR(inBox, 6.0f, D2D1::ColorF(1, 1, 1, 0.06f));
    if (g_state.isTypingCalNote) {
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(inBox, 6.0f, 6.0f), g_pBrushAccent, 1.2f);
    }
    FillRR(addBtn, 6.0f, ColorFromRef(GetActiveAccentRef(), PtInRectF(addBtn, mp.x, mp.y) ? 0.45f : 0.25f));
    g_pRT->DrawText(L"+", 1, g_pTextFormatTitle, addBtn, g_pBrushText);

    std::wstring inShow = g_calEventInput.empty()
        ? (g_state.isTypingCalNote ? L"|" : L"Новое событие...")
        : (g_calEventInput + (g_state.isTypingCalNote ? L"|" : L""));
    DrawClippedText(inShow, g_pTextFormatSmallMuted,
        D2D1::RectF(inBox.left + 6.0f, inBox.top, inBox.right - 4.0f, inBox.bottom),
        g_calEventInput.empty() ? g_pBrushTextMuted : g_pBrushText);

    std::vector<int> dayEvs = GetEventsForSelectedDate();
    if (dayEvs.empty()) {
        g_pRT->DrawText(L"На этот день событий нет", 24, g_pTextFormatTime,
            D2D1::RectF(leftCard.left + 8.0f, leftCard.top + 92.0f, leftCard.right - 8.0f, leftCard.bottom - 10.0f), g_pBrushTextMuted);
    }
    else {
        int maxSc = std::max(0, static_cast<int>(dayEvs.size()) - 3);
        g_calEventScroll = std::clamp(g_calEventScroll, 0, maxSc);
        for (int s = 0; s < 3; ++s) {
            int idxInDay = g_calEventScroll + s;
            if (idxInDay >= static_cast<int>(dayEvs.size())) break;
            const auto& ev = g_calEvents[dayEvs[idxInDay]];

            float ey = leftCard.top + 86.0f + s * 30.0f;
            D2D1_RECT_F er = D2D1::RectF(leftCard.left + 8.0f, ey, leftCard.right - 8.0f, ey + 26.0f);
            FillRR(er, 6.0f, D2D1::ColorF(1, 1, 1, 0.07f));
            DrawClippedText(L"\u2022 " + ev.text, g_pTextFormatSmallMuted,
                D2D1::RectF(er.left + 6.0f, er.top, er.right - 22.0f, er.bottom), g_pBrushText);

            D2D1_RECT_F delR = D2D1::RectF(er.right - 20.0f, er.top + 2.0f, er.right - 2.0f, er.bottom - 2.0f);
            g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatTime, delR, PtInRectF(delR, mp.x, mp.y) ? g_pBrushText : g_pBrushTextMuted);
        }
    }

    float gx0 = left + 202.0f;
    float gy0 = top + 44.0f;
    float cellW = (right - 14.0f - gx0) / 7.0f;
    float cellH = 25.0f;

    static const wchar_t* kDow[7] = { L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб", L"Вс" };
    for (int c = 0; c < 7; ++c) {
        D2D1_RECT_F r = D2D1::RectF(gx0 + c * cellW, gy0, gx0 + (c + 1) * cellW, gy0 + 20.0f);
        g_pRT->DrawText(kDow[c], 2, g_pTextFormatTime, r, (c >= 5) ? g_pBrushAccent : g_pBrushTextMuted);
    }

    int firstDow = FirstWeekdayMondayZero(g_calYear, g_calMonth);
    int totalDays = DaysInMonth(g_calYear, g_calMonth);
    for (int d = 1; d <= totalDays; ++d) {
        int pos = firstDow + (d - 1);
        int col = pos % 7;
        int row = pos / 7;
        D2D1_RECT_F cell = D2D1::RectF(
            gx0 + col * cellW + 1.5f,
            gy0 + 22.0f + row * cellH + 1.5f,
            gx0 + (col + 1) * cellW - 1.5f,
            gy0 + 22.0f + (row + 1) * cellH - 1.5f
        );
        bool isToday = (g_calYear == stNow.wYear && g_calMonth == stNow.wMonth && d == stNow.wDay);
        bool isSel = (d == g_calSelectedDay);
        bool hot = PtInRectF(cell, mp.x, mp.y);

        if (isToday) {
            FillRR(cell, 7.0f, ColorFromRef(GetActiveAccentRef(), 0.85f));
        }
        else if (isSel || hot) {
            FillRR(cell, 7.0f, D2D1::ColorF(1, 1, 1, isSel ? 0.16f : 0.08f));
            if (isSel) g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(cell, 7.0f, 7.0f), g_pBrushAccent, 1.0f);
        }

        std::wstring ds = std::to_wstring(d);
        g_pRT->DrawText(ds.c_str(), (UINT32)ds.size(), g_pTextFormatTime, cell, g_pBrushText);

        if (DayHasEvents(g_calYear, g_calMonth, d)) {
            float dcx = (cell.left + cell.right) * 0.5f;
            g_pBrushIcon->SetColor(isToday ? D2D1::ColorF(1, 1, 1, 1) : ColorFromRef(GetActiveAccentRef(), 1.0f));
            g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dcx, cell.bottom - 2.5f), 2.0f, 2.0f), g_pBrushIcon);
        }
    }
}

inline bool HandleCalendarTabClick(HWND hwnd, int mouseX, int mouseY) {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    float right = left + g_state.curW;
    D2D1_RECT_F leftCard = D2D1::RectF(left + 14.0f, top + 44.0f, left + 194.0f, top + 224.0f);

    D2D1_RECT_F bPrev = D2D1::RectF(leftCard.left + 8.0f, leftCard.top + 30.0f, leftCard.left + 34.0f, leftCard.top + 50.0f);
    D2D1_RECT_F bToday = D2D1::RectF(leftCard.left + 38.0f, leftCard.top + 30.0f, leftCard.right - 38.0f, leftCard.top + 50.0f);
    D2D1_RECT_F bNext = D2D1::RectF(leftCard.right - 34.0f, leftCard.top + 30.0f, leftCard.right - 8.0f, leftCard.top + 50.0f);
    D2D1_RECT_F inBox = D2D1::RectF(leftCard.left + 8.0f, leftCard.top + 56.0f, leftCard.right - 34.0f, leftCard.top + 80.0f);
    D2D1_RECT_F addBtn = D2D1::RectF(leftCard.right - 30.0f, leftCard.top + 56.0f, leftCard.right - 8.0f, leftCard.top + 80.0f);

    if (PtInRectF(bPrev, mouseX, mouseY)) {
        if (--g_calMonth < 1) { g_calMonth = 12; --g_calYear; }
        g_calSelectedDay = std::min(g_calSelectedDay, DaysInMonth(g_calYear, g_calMonth));
        g_calEventScroll = 0;
        return true;
    }
    if (PtInRectF(bNext, mouseX, mouseY)) {
        if (++g_calMonth > 12) { g_calMonth = 1; ++g_calYear; }
        g_calSelectedDay = std::min(g_calSelectedDay, DaysInMonth(g_calYear, g_calMonth));
        g_calEventScroll = 0;
        return true;
    }
    if (PtInRectF(bToday, mouseX, mouseY)) {
        SYSTEMTIME st; GetLocalTime(&st);
        g_calYear = st.wYear; g_calMonth = st.wMonth; g_calSelectedDay = st.wDay;
        g_calEventScroll = 0;
        return true;
    }
    if (PtInRectF(inBox, mouseX, mouseY)) {
        g_state.isTypingCalNote = true;
        g_state.isTypingNote = false;
        g_state.isTypingCity = false;
        SetWindowInputFocus(hwnd, true);
        return true;
    }
    if (PtInRectF(addBtn, mouseX, mouseY)) {
        if (g_calEventInput.empty()) {
            g_state.isTypingCalNote = true;
            SetWindowInputFocus(hwnd, true);
        }
        else {
            AddCalendarEventFromInput();
            SetWindowInputFocus(hwnd, false);
        }
        return true;
    }

    std::vector<int> dayEvs = GetEventsForSelectedDate();
    for (int s = 0; s < 3; ++s) {
        int idxInDay = g_calEventScroll + s;
        if (idxInDay >= static_cast<int>(dayEvs.size())) break;
        float ey = leftCard.top + 86.0f + s * 30.0f;
        D2D1_RECT_F er = D2D1::RectF(leftCard.left + 8.0f, ey, leftCard.right - 8.0f, ey + 26.0f);
        D2D1_RECT_F delR = D2D1::RectF(er.right - 20.0f, er.top + 2.0f, er.right - 2.0f, er.bottom - 2.0f);
        if (PtInRectF(delR, mouseX, mouseY)) {
            g_calEvents.erase(g_calEvents.begin() + dayEvs[idxInDay]);
            SaveExtraTabsData();
            return true;
        }
    }

    float gx0 = left + 202.0f;
    float gy0 = top + 44.0f;
    float cellW = (right - 14.0f - gx0) / 7.0f;
    float cellH = 25.0f;
    int firstDow = FirstWeekdayMondayZero(g_calYear, g_calMonth);
    int totalDays = DaysInMonth(g_calYear, g_calMonth);
    for (int d = 1; d <= totalDays; ++d) {
        int pos = firstDow + (d - 1);
        D2D1_RECT_F cell = D2D1::RectF(
            gx0 + (pos % 7) * cellW, gy0 + 22.0f + (pos / 7) * cellH,
            gx0 + ((pos % 7) + 1) * cellW, gy0 + 22.0f + ((pos / 7) + 1) * cellH);
        if (PtInRectF(cell, mouseX, mouseY)) {
            g_calSelectedDay = d;
            g_calEventScroll = 0;
            SetWindowInputFocus(hwnd, false);
            return true;
        }
    }
    return false;
}

// --- 3. Вкладка «Текст» (Независимые заметки + To-Do лист) ---
inline void AddTodoFromInput() {
    std::wstring t = TrimW(g_todoInput);
    if (t.empty()) return;
    g_todoList.insert(g_todoList.begin(), { false, t });
    g_todoInput.clear();
    SaveExtraTabsData();
}

inline void DrawNotesTab(float left, float top, float right, const POINT& mp) {
    D2D1_RECT_F modeNotes = D2D1::RectF(left + 14.0f, top + 40.0f, left + 86.0f, top + 62.0f);
    D2D1_RECT_F modeTodo = D2D1::RectF(left + 90.0f, top + 40.0f, left + 162.0f, top + 62.0f);
    FillRR(modeNotes, 7.0f, (g_notesMode == 0) ? ColorFromRef(GetActiveAccentRef(), 0.28f) : D2D1::ColorF(1, 1, 1, 0.06f));
    FillRR(modeTodo, 7.0f, (g_notesMode == 1) ? ColorFromRef(GetActiveAccentRef(), 0.28f) : D2D1::ColorF(1, 1, 1, 0.06f));
    g_pRT->DrawText(L"Заметки", 7, g_pTextFormatTime, modeNotes, (g_notesMode == 0) ? g_pBrushAccent : g_pBrushText);
    g_pRT->DrawText(L"To-Do", 5, g_pTextFormatTime, modeTodo, (g_notesMode == 1) ? g_pBrushAccent : g_pBrushText);

    if (g_notesMode == 0) {
        int count = std::min(5, static_cast<int>(g_notePages.size()));
        float px = left + 170.0f;
        for (int i = 0; i < count; ++i) {
            D2D1_RECT_F pr = D2D1::RectF(px, top + 40.0f, px + 28.0f, top + 62.0f);
            bool act = (i == g_activeNoteIdx);
            FillRR(pr, 6.0f, act ? D2D1::ColorF(1, 1, 1, 0.18f) : D2D1::ColorF(1, 1, 1, 0.06f));
            if (act) g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(pr, 6.0f, 6.0f), g_pBrushAccent, 1.0f);
            std::wstring lbl = L"#" + std::to_wstring(i + 1);
            g_pRT->DrawText(lbl.c_str(), (UINT32)lbl.size(), g_pTextFormatTime, pr, act ? g_pBrushAccent : g_pBrushText);
            px += 31.0f;
        }
        if (g_notePages.size() < 5) {
            D2D1_RECT_F addP = D2D1::RectF(px, top + 40.0f, px + 24.0f, top + 62.0f);
            FillRR(addP, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(addP, mp.x, mp.y) ? 0.16f : 0.07f));
            g_pRT->DrawText(L"+", 1, g_pTextFormatTitle, addP, g_pBrushAccent);
        }

        D2D1_RECT_F bCopy = D2D1::RectF(right - 134.0f, top + 40.0f, right - 64.0f, top + 62.0f);
        D2D1_RECT_F bDel = D2D1::RectF(right - 60.0f, top + 40.0f, right - 14.0f, top + 62.0f);
        if (PtInRectF(bCopy, mp.x, mp.y)) FillRR(bCopy, 7.0f, D2D1::ColorF(1, 1, 1, 0.10f));
        if (PtInRectF(bDel, mp.x, mp.y))  FillRR(bDel, 7.0f, D2D1::ColorF(1, 1, 1, 0.10f));
        g_pRT->DrawText(L"Копир.", 6, g_pTextFormatTime, bCopy, g_pBrushText);
        g_pRT->DrawText((g_notePages.size() > 1) ? L"Удалить" : L"Очист.", 6, g_pTextFormatTime, bDel, g_pBrushTextMuted);

        D2D1_RECT_F ed = D2D1::RectF(left + 14.0f, top + 66.0f, right - 14.0f, top + 224.0f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(ed, 14.0f, 14.0f), g_pBrushInputBg);
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(ed, 14.0f, 14.0f),
            g_state.isTypingNote ? g_pBrushAccent : g_pBrushBorder, g_state.isTypingNote ? 1.4f : 1.0f);

        std::wstring& curText = GetActiveNotePage();
        std::wstring disp = curText.empty()
            ? (g_state.isTypingNote ? L"|" : L"Кликните, чтобы писать заметку...")
            : (curText + (g_state.isTypingNote ? L"|" : L""));
        D2D1_RECT_F tr = D2D1::RectF(ed.left + 12.0f, ed.top + 10.0f, ed.right - 12.0f, ed.bottom - 10.0f);
        g_pRT->PushAxisAlignedClip(tr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        g_pRT->DrawText(disp.c_str(), (UINT32)disp.size(), g_pTextFormatSmallMuted, tr,
            curText.empty() ? g_pBrushTextMuted : g_pBrushText);
        g_pRT->PopAxisAlignedClip();
    }
    else {
        D2D1_RECT_F inBox = D2D1::RectF(left + 14.0f, top + 68.0f, right - 48.0f, top + 96.0f);
        D2D1_RECT_F addBtn = D2D1::RectF(right - 42.0f, top + 68.0f, right - 14.0f, top + 96.0f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(inBox, 8.0f, 8.0f), g_pBrushInputBg);
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(inBox, 8.0f, 8.0f),
            g_state.isTypingNote ? g_pBrushAccent : g_pBrushBorder, 1.2f);
        FillRR(addBtn, 8.0f, ColorFromRef(GetActiveAccentRef(), PtInRectF(addBtn, mp.x, mp.y) ? 0.45f : 0.25f));
        g_pRT->DrawText(L"+", 1, g_pTextFormatTitle, addBtn, g_pBrushText);

        std::wstring tShow = g_todoInput.empty()
            ? (g_state.isTypingNote ? L"|" : L"Новая задача (Enter — добавить)...")
            : (g_todoInput + (g_state.isTypingNote ? L"|" : L""));
        DrawClippedText(tShow, g_pTextFormatLeftSub,
            D2D1::RectF(inBox.left + 10.0f, inBox.top, inBox.right - 8.0f, inBox.bottom),
            g_todoInput.empty() ? g_pBrushTextMuted : g_pBrushText);

        int maxSc = std::max(0, static_cast<int>(g_todoList.size()) - 3);
        g_todoScroll = std::clamp(g_todoScroll, 0, maxSc);

        for (int s = 0; s < 3; ++s) {
            int idx = g_todoScroll + s;
            if (idx >= static_cast<int>(g_todoList.size())) break;
            const auto& item = g_todoList[idx];

            float ry = top + 104.0f + s * 39.0f;
            D2D1_RECT_F rowR = D2D1::RectF(left + 14.0f, ry, right - 14.0f, ry + 34.0f);
            bool hot = PtInRectF(rowR, mp.x, mp.y);
            FillRR(rowR, 9.0f, D2D1::ColorF(1, 1, 1, hot ? 0.12f : 0.06f));
            g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(rowR, 9.0f, 9.0f), g_pBrushBorder, 1.0f);

            float cy = (rowR.top + rowR.bottom) * 0.5f;
            D2D1_RECT_F chk = D2D1::RectF(rowR.left + 10.0f, cy - 8.0f, rowR.left + 26.0f, cy + 8.0f);
            if (item.done) {
                FillRR(chk, 4.5f, ColorFromRef(GetActiveAccentRef(), 0.9f));
                g_pRT->DrawText(L"\u2713", 1, g_pTextFormatTime, chk, g_pBrushText);
            }
            else {
                g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(chk, 4.5f, 4.5f), g_pBrushTextMuted, 1.2f);
            }

            D2D1_RECT_F txtR = D2D1::RectF(rowR.left + 34.0f, rowR.top, rowR.right - 30.0f, rowR.bottom);
            DrawClippedText(item.text, g_pTextFormatLeftSub, txtR, item.done ? g_pBrushTextMuted : g_pBrushText);
            if (item.done) {
                g_pRT->DrawLine(D2D1::Point2F(txtR.left, cy), D2D1::Point2F(std::min(txtR.right, txtR.left + item.text.size() * 7.2f), cy), g_pBrushTextMuted, 1.0f);
            }

            D2D1_RECT_F delR = D2D1::RectF(rowR.right - 26.0f, cy - 10.0f, rowR.right - 6.0f, cy + 10.0f);
            g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatSub, delR, PtInRectF(delR, mp.x, mp.y) ? g_pBrushText : g_pBrushTextMuted);
        }
    }
}

inline bool HandleNotesTabClick(HWND hwnd, int mouseX, int mouseY) {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    float right = left + g_state.curW;

    D2D1_RECT_F modeNotes = D2D1::RectF(left + 14.0f, top + 40.0f, left + 86.0f, top + 62.0f);
    D2D1_RECT_F modeTodo = D2D1::RectF(left + 90.0f, top + 40.0f, left + 162.0f, top + 62.0f);
    if (PtInRectF(modeNotes, mouseX, mouseY)) {
        g_notesMode = 0;
        SetWindowInputFocus(hwnd, false);
        return true;
    }
    if (PtInRectF(modeTodo, mouseX, mouseY)) {
        g_notesMode = 1;
        SetWindowInputFocus(hwnd, false);
        return true;
    }

    if (g_notesMode == 0) {
        int count = std::min(5, static_cast<int>(g_notePages.size()));
        float px = left + 170.0f;
        for (int i = 0; i < count; ++i) {
            D2D1_RECT_F pr = D2D1::RectF(px, top + 40.0f, px + 28.0f, top + 62.0f);
            if (PtInRectF(pr, mouseX, mouseY)) {
                g_activeNoteIdx = i;
                return true;
            }
            px += 31.0f;
        }
        if (g_notePages.size() < 5) {
            D2D1_RECT_F addP = D2D1::RectF(px, top + 40.0f, px + 24.0f, top + 62.0f);
            if (PtInRectF(addP, mouseX, mouseY)) {
                g_notePages.push_back(L"");
                g_activeNoteIdx = static_cast<int>(g_notePages.size()) - 1;
                g_state.isTypingNote = true;
                SetWindowInputFocus(hwnd, true);
                SaveExtraTabsData();
                return true;
            }
        }

        D2D1_RECT_F bCopy = D2D1::RectF(right - 134.0f, top + 40.0f, right - 64.0f, top + 62.0f);
        D2D1_RECT_F bDel = D2D1::RectF(right - 60.0f, top + 40.0f, right - 14.0f, top + 62.0f);
        if (PtInRectF(bCopy, mouseX, mouseY)) {
            CopyWideToClipboard(hwnd, GetActiveNotePage());
            TriggerMiniBadge(L"Скопировано", true, 1400);
            return true;
        }
        if (PtInRectF(bDel, mouseX, mouseY)) {
            if (g_notePages.size() > 1) {
                g_notePages.erase(g_notePages.begin() + g_activeNoteIdx);
                g_activeNoteIdx = std::clamp(g_activeNoteIdx, 0, static_cast<int>(g_notePages.size()) - 1);
            }
            else {
                GetActiveNotePage().clear();
            }
            SaveExtraTabsData();
            return true;
        }

        D2D1_RECT_F ed = D2D1::RectF(left + 14.0f, top + 66.0f, right - 14.0f, top + 224.0f);
        if (PtInRectF(ed, mouseX, mouseY)) {
            g_state.isTypingNote = true;
            g_state.isTypingCalNote = false;
            g_state.isTypingCity = false;
            SetWindowInputFocus(hwnd, true);
            return true;
        }
    }
    else {
        D2D1_RECT_F inBox = D2D1::RectF(left + 14.0f, top + 68.0f, right - 48.0f, top + 96.0f);
        D2D1_RECT_F addBtn = D2D1::RectF(right - 42.0f, top + 68.0f, right - 14.0f, top + 96.0f);
        if (PtInRectF(inBox, mouseX, mouseY)) {
            g_state.isTypingNote = true;
            g_state.isTypingCalNote = false;
            g_state.isTypingCity = false;
            SetWindowInputFocus(hwnd, true);
            return true;
        }
        if (PtInRectF(addBtn, mouseX, mouseY)) {
            if (g_todoInput.empty()) {
                g_state.isTypingNote = true;
                SetWindowInputFocus(hwnd, true);
            }
            else {
                AddTodoFromInput();
            }
            return true;
        }

        for (int s = 0; s < 3; ++s) {
            int idx = g_todoScroll + s;
            if (idx >= static_cast<int>(g_todoList.size())) break;
            float ry = top + 104.0f + s * 39.0f;
            D2D1_RECT_F rowR = D2D1::RectF(left + 14.0f, ry, right - 14.0f, ry + 34.0f);
            float cy = (rowR.top + rowR.bottom) * 0.5f;
            D2D1_RECT_F delR = D2D1::RectF(rowR.right - 26.0f, cy - 10.0f, rowR.right - 6.0f, cy + 10.0f);
            if (PtInRectF(delR, mouseX, mouseY)) {
                g_todoList.erase(g_todoList.begin() + idx);
                SaveExtraTabsData();
                return true;
            }
            if (PtInRectF(rowR, mouseX, mouseY)) {
                g_todoList[idx].done = !g_todoList[idx].done;
                SaveExtraTabsData();
                return true;
            }
        }
    }
    return false;
}

// --- 4. Вкладка «Игры» (Игровой монитор) ---
inline D2D1_RECT_F GetGameModeBtnRect() {
    float left = IslandLeft();
    return D2D1::RectF(left + 14.0f, ISLAND_TOP + 190.0f, left + 162.0f, ISLAND_TOP + 222.0f);
}

inline D2D1_RECT_F GetGameCleanRamBtnRect() {
    float left = IslandLeft();
    return D2D1::RectF(left + 168.0f, ISLAND_TOP + 190.0f, left + 306.0f, ISLAND_TOP + 222.0f);
}

inline D2D1_RECT_F GetGameGhostBtnRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    return D2D1::RectF(left + 312.0f, ISLAND_TOP + 190.0f, right - 14.0f, ISLAND_TOP + 222.0f);
}

inline void DrawGameTab(float left, float top, float right, const POINT& mp) {
    int fps = g_game.fps.load();
    float ft = g_game.frameTimeMs.load();
    int cpu = g_game.cpuPct.load();
    int gpu = g_game.gpuPct.load();
    int ram = g_game.ramPct.load();
    int ping = g_game.pingMs.load();

    static ULONGLONG s_lastHistTick = 0;
    ULONGLONG now = GetTickCount64();
    if (now - s_lastHistTick > 250) {
        s_lastHistTick = now;
        g_game.fpsHistory.push_back(static_cast<float>(fps));
        if (g_game.fpsHistory.size() > 36) g_game.fpsHistory.erase(g_game.fpsHistory.begin());
    }

    D2D1_RECT_F fpsCard = D2D1::RectF(left + 14.0f, top + 44.0f, left + 184.0f, top + 182.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(fpsCard, 14.0f, 14.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(fpsCard, 14.0f, 14.0f), g_pBrushBorder, 1.0f);

    g_pRT->DrawText(L"FPS / ВРЕМЯ КАДРА", 17, g_pTextFormatBadge,
        D2D1::RectF(fpsCard.left + 12.0f, fpsCard.top + 8.0f, fpsCard.right - 12.0f, fpsCard.top + 24.0f), g_pBrushTextMuted);

    std::wstring fpsStr = std::to_wstring(fps);
    g_pRT->DrawText(fpsStr.c_str(), (UINT32)fpsStr.size(), g_pTextFormatBig,
        D2D1::RectF(fpsCard.left + 14.0f, fpsCard.top + 24.0f, fpsCard.left + 100.0f, fpsCard.top + 74.0f), g_pBrushAccent);

    wchar_t ftBuf[32];
    swprintf_s(ftBuf, L"%.1f мс\n\u23F1 %s", ft, GetSessionDurationString().c_str());
    g_pRT->DrawText(ftBuf, (UINT32)wcslen(ftBuf), g_pTextFormatSmallMuted,
        D2D1::RectF(fpsCard.left + 98.0f, fpsCard.top + 30.0f, fpsCard.right - 8.0f, fpsCard.top + 74.0f), g_pBrushTextMuted);

    D2D1_RECT_F graphBox = D2D1::RectF(fpsCard.left + 10.0f, fpsCard.top + 82.0f, fpsCard.right - 10.0f, fpsCard.bottom - 10.0f);
    FillRR(graphBox, 8.0f, D2D1::ColorF(1, 1, 1, 0.04f));
    if (g_game.fpsHistory.size() > 2) {
        float gw = graphBox.right - graphBox.left - 8.0f;
        float gh = graphBox.bottom - graphBox.top - 10.0f;
        float maxF = 165.0f;
        for (float v : g_game.fpsHistory) if (v > maxF) maxF = v;
        for (size_t i = 1; i < g_game.fpsHistory.size(); ++i) {
            float x1 = graphBox.left + 4.0f + gw * static_cast<float>(i - 1) / 35.0f;
            float x2 = graphBox.left + 4.0f + gw * static_cast<float>(i) / 35.0f;
            float y1 = graphBox.bottom - 5.0f - gh * std::clamp(g_game.fpsHistory[i - 1] / maxF, 0.05f, 1.0f);
            float y2 = graphBox.bottom - 5.0f - gh * std::clamp(g_game.fpsHistory[i] / maxF, 0.05f, 1.0f);
            g_pRT->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), g_pBrushAccent, 1.8f);
        }
    }

    D2D1_RECT_F hwCard = D2D1::RectF(left + 192.0f, top + 44.0f, right - 14.0f, top + 182.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(hwCard, 14.0f, 14.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(hwCard, 14.0f, 14.0f), g_pBrushBorder, 1.0f);

    auto drawMetricBar = [&](int row, const std::wstring& name, const std::wstring& valText, float norm, D2D1_COLOR_F col) {
        float y = hwCard.top + 10.0f + row * 32.0f;
        g_pRT->DrawText(name.c_str(), (UINT32)name.size(), g_pTextFormatLeftSub,
            D2D1::RectF(hwCard.left + 12.0f, y, hwCard.left + 110.0f, y + 16.0f), g_pBrushText);
        g_pRT->DrawText(valText.c_str(), (UINT32)valText.size(), g_pTextFormatRightSub,
            D2D1::RectF(hwCard.right - 130.0f, y, hwCard.right - 12.0f, y + 16.0f), g_pBrushTextMuted);

        D2D1_RECT_F track = D2D1::RectF(hwCard.left + 12.0f, y + 19.0f, hwCard.right - 12.0f, y + 25.0f);
        FillRR(track, 3.0f, D2D1::ColorF(1, 1, 1, 0.08f));
        float fillW = (track.right - track.left) * std::clamp(norm, 0.02f, 1.0f);
        FillRR(D2D1::RectF(track.left, track.top, track.left + fillW, track.bottom), 3.0f, col);
        };

    wchar_t gpuStr[48], ramStr[48];
    swprintf_s(gpuStr, L"%d%% (%.1f / %.0f ГБ)", gpu, g_game.vramUsedGb.load(), g_game.vramTotalGb.load());
    swprintf_s(ramStr, L"%d%% (%.1f / %.0f ГБ)", ram, g_game.ramUsedGb.load(), g_game.ramTotalGb.load());

    drawMetricBar(0, L"Процессор (CPU)", std::to_wstring(cpu) + L"%", cpu / 100.0f, D2D1::ColorF(0.42f, 0.75f, 1.0f));
    drawMetricBar(1, L"Видеокарта (GPU)", gpuStr, gpu / 100.0f, D2D1::ColorF(0.72f, 0.52f, 1.0f));
    drawMetricBar(2, L"Память (RAM)", ramStr, ram / 100.0f, D2D1::ColorF(0.35f, 0.88f, 0.82f));
    drawMetricBar(3, L"Сеть (Пинг)", std::to_wstring(ping) + L" мс", std::min(1.0f, ping / 150.0f),
        (ping < 60) ? D2D1::ColorF(0.30f, 0.92f, 0.52f) : D2D1::ColorF(1.0f, 0.65f, 0.25f));

    auto drawBottomBtn = [&](const D2D1_RECT_F& r, const std::wstring& txt, bool active) {
        bool hot = PtInRectF(r, mp.x, mp.y);
        FillRR(r, 10.0f, active ? ColorFromRef(GetActiveAccentRef(), 0.28f)
            : D2D1::ColorF(1, 1, 1, hot ? 0.14f : 0.07f));
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(r, 10.0f, 10.0f), active ? g_pBrushAccent : g_pBrushBorder, 1.0f);
        g_pRT->DrawText(txt.c_str(), (UINT32)txt.size(), g_pTextFormatTime, r, active ? g_pBrushAccent : g_pBrushText);
        };

    drawBottomBtn(GetGameModeBtnRect(), g_cfg.gameModeActive ? L"Игровой HUD: ВКЛ" : L"Игровой HUD: ВЫКЛ", g_cfg.gameModeActive);
    drawBottomBtn(GetGameCleanRamBtnRect(), (now < g_game.ramCleanStatusUntil) ? g_game.ramCleanStatus : L"\U0001F680 Очистить ОЗУ", false);
    drawBottomBtn(GetGameGhostBtnRect(), g_cfg.gameClickThrough ? L"Сквозной клик: ДА" : L"Сквозной клик: НЕТ", g_cfg.gameClickThrough);
}

#include "SettingsWindow.h"

inline void ApplyOpenTabPolicy() {
    if (g_cfg.gameModeActive && g_cfg.tabEnabled[static_cast<int>(Tab::Game)]) {
        g_state.activeTab = Tab::Game;
    }
    else if (g_cfg.openTabMode == 2) {
        g_state.activeTab = Tab::Music;
    }
    else if (g_cfg.openTabMode == 1 && g_music.isPlaying) {
        g_state.activeTab = Tab::Music;
    }
    EnsureActiveTabEnabled();
}

void RenderIsland(HWND hwnd) {
    g_pRT->BeginDraw();
    g_hasScrolling = false;

    const double livePos = GetLivePosition();
    const LyricView curLyric = GetCurrentLyric(livePos);
    g_lastLyricIdx = curLyric.index;

    POINT mp = { -10000, -10000 };
    if (GetCursorPos(&mp)) ScreenToClient(hwnd, &mp);
    else mp = { -10000, -10000 };
    g_pRT->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    const bool wantExpanded = g_state.isHovered || IsAnyTypingActive() ||
        g_state.isDraggingVolume || g_state.isDraggingTimeline || g_state.isDragOver ||
        g_phoneEvent.hasActiveEvent || g_state.isDraggingWindow ||
        g_state.isDraggingTabOnIsland || g_discord.isDraggingVol;

    float left = IslandLeft();
    float top = ISLAND_TOP;
    float right = left + g_state.curW;
    float bottom = top + g_state.curH;

    DrawIslandCapsule(left, top, right, bottom, g_state.curR);

    const bool hasMiniBadge = (GetTickCount64() < g_extras.miniBadgeUntil && !g_extras.miniBadgeText.empty());
    // 1. Учитываем тумблер showMicMuteCompact из настроек
    const bool showMuteDot = (g_cfg.showMicMuteCompact && g_discord.micMuted && !hasMiniBadge);

    if (!wantExpanded && std::fabs(g_state.curH - (g_cfg.dockToTop ? 32.0f : 34.0f)) < 12.0f) {
        float compactBottom = top + (g_cfg.dockToTop ? 32.0f : 34.0f);
        float cy = (top + compactBottom) * 0.5f;

        float rightContentLimit = right - 10.0f;
        if (hasMiniBadge) {
            D2D1_RECT_F pillR = D2D1::RectF(right - 68.0f, top + 5.0f, right - 6.0f, compactBottom - 5.0f);
            FillRR(pillR, 9.0f, g_extras.miniBadgeAccent
                ? ColorFromRef(GetActiveAccentRef(), 0.28f)
                : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f));
            g_pRT->DrawText(g_extras.miniBadgeText.c_str(), (UINT32)g_extras.miniBadgeText.size(),
                g_pTextFormatBadge, pillR, g_extras.miniBadgeAccent ? g_pBrushAccent : g_pBrushText);
            rightContentLimit = right - 72.0f;
        }
        else if (showMuteDot) {
            // Красный индикатор выключенного микрофона в свёрнутом острове
            D2D1_RECT_F muteR = D2D1::RectF(right - 26.0f, cy - 9.0f, right - 8.0f, cy + 9.0f);
            FillRR(muteR, 9.0f, D2D1::ColorF(0.95f, 0.25f, 0.28f, 0.32f));
            g_pRT->DrawText(L"\U0001F507", 2, g_pTextFormatBadge, muteR, g_pBrushText);
            rightContentLimit = right - 30.0f;
        }

        float extraRightW = hasMiniBadge ? 62.0f : (showMuteDot ? 22.0f : 0.0f);

        if (g_cfg.gameModeActive) {
            float hudW = CalcGameHudCompactWidth() + extraRightW;
            if (std::fabs(g_state.targetW - hudW) > 0.5f) {
                g_state.targetW = hudW;
                g_state.isAnimating = true;
            }
            DrawGameModeCompactHud(left, top, (hasMiniBadge || showMuteDot) ? rightContentLimit : right, compactBottom);
        }
        else if (g_music.isPlaying && g_cfg.showMusicCompact) {
            g_state.targetW = std::max(185.0f, g_cfg.compactW) + extraRightW;

            float textLeft = left + 12.0f;
            if (g_pAlbumBitmap) {
                D2D1_RECT_F miniCover = D2D1::RectF(left + 7.0f, top + 5.0f, left + 29.0f, compactBottom - 5.0f);
                DrawRoundedBitmap(g_pAlbumBitmap, miniCover, 6.0f);
                textLeft = left + 34.0f;
            }
            else {
                D2D1_RECT_F noteRect = D2D1::RectF(left + 8.0f, top, left + 26.0f, compactBottom);
                g_pRT->DrawText(L"\u266B", 1, g_pTextFormatTitle, noteRect, g_pBrushAccent);
                textLeft = left + 28.0f;
            }

            float eqReserve = (g_cfg.showEqualizer && !hasMiniBadge && !showMuteDot) ? 22.0f : 0.0f;
            if (g_cfg.showEqualizer && !hasMiniBadge && !showMuteDot) {
                DrawMiniEqualizer(right - 8.0f, cy, true);
            }

            D2D1_RECT_F textRect = D2D1::RectF(textLeft, top, rightContentLimit - eqReserve, compactBottom);
            if (curLyric.valid) {
                DrawHighlightedLyric(SLOT_COMPACT, curLyric, g_pTextFormatLyrics, textRect);
            }
            else {
                // Трек без слов (LRC) отображается ровно посередине
                DrawScrollingText(SLOT_COMPACT, g_music.title, g_pTextFormatSub, textRect, g_pBrushText);
            }
        }
        else if (g_cfg.minimalCompact) {
            std::wstring clockStr = GetClockString();
            std::wstring degStr = g_weather.info.loaded ? FormatTemp(g_weather.info.temp) : L"...";

            float newTargetW = std::clamp(g_cfg.compactW, 114.0f, 185.0f) + extraRightW;
            if (std::fabs(g_state.targetW - newTargetW) > 0.5f) {
                g_state.targetW = newTargetW;
                g_state.isAnimating = true;
            }

            D2D1_RECT_F timeRect = D2D1::RectF(left + 12.0f, top, left + 58.0f, compactBottom);
            g_pRT->DrawText(clockStr.c_str(), (UINT32)clockStr.size(), g_pTextFormatLeftTitle, timeRect, g_pBrushText);

            if (!hasMiniBadge) {
                float wxRight = showMuteDot ? rightContentLimit : right;
                if (g_weather.info.loaded) {
                    DrawWeatherIcon(WxKindFromCode(g_weather.info.code), g_weather.info.isDay,
                        wxRight - 38.0f, cy, 14.0f);
                }
                D2D1_RECT_F tempRect = D2D1::RectF(wxRight - 29.0f, top, wxRight - 6.0f, compactBottom);
                g_pRT->DrawText(degStr.c_str(), (UINT32)degStr.size(), g_pTextFormatLeftTitle, tempRect, g_pBrushText);
            }
        }
        else {
            std::wstring timeAndWeather = GetClockString() + L"  |  " + g_weather.city + L" " + g_weather.tempCompact;
            if (g_cfg.showCpuCompact) {
                timeAndWeather += L"  |  CPU " + std::to_wstring(g_game.cpuPct.load()) + L"%";
            }

            float contentWidth = g_cfg.compactW;
            IDWriteTextLayout* pTempLayout = nullptr;
            if (SUCCEEDED(g_pDWriteFactory->CreateTextLayout(
                timeAndWeather.c_str(), (UINT32)timeAndWeather.size(), g_pTextFormatLeftSub, 500.0f, 30.0f, &pTempLayout))) {
                DWRITE_TEXT_METRICS metrics = {};
                pTempLayout->GetMetrics(&metrics);
                contentWidth = metrics.width + 52.0f;
                pTempLayout->Release();
            }

            float newTargetW = std::clamp(contentWidth, g_cfg.compactW, 275.0f) + extraRightW;
            if (std::fabs(g_state.targetW - newTargetW) > 0.5f) {
                g_state.targetW = newTargetW;
                g_state.isAnimating = true;
            }

            if (g_weather.info.loaded && !hasMiniBadge && !showMuteDot) {
                D2D1_RECT_F textRect = D2D1::RectF(left + 12.0f, top, right - 32.0f, compactBottom);
                DrawScrollingText(SLOT_COMPACT, timeAndWeather, g_pTextFormatLeftSub, textRect, g_pBrushText);

                DrawWeatherIcon(WxKindFromCode(g_weather.info.code), g_weather.info.isDay,
                    right - 20.0f, cy, 16.0f);
            }
            else {
                D2D1_RECT_F textRect = D2D1::RectF(left + 10.0f, top, rightContentLimit, compactBottom);
                DrawScrollingText(SLOT_COMPACT, timeAndWeather, g_pTextFormatLeftSub, textRect, g_pBrushText);
            }
        }
    }
    else if (g_state.curH > 90.0f) {
        g_pRT->PushAxisAlignedClip(D2D1::RectF(left, top, right, bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

        std::vector<Tab> visTabs = GetVisibleTabs();
        int visCount = static_cast<int>(visTabs.size());

        // 2. Плавное скольжение плашки активной вкладки
        for (int i = 0; i < visCount; ++i) {
            if (visTabs[i] == g_state.activeTab) {
                D2D1_RECT_F targetR = GetTabRect(i, visCount);
                if (!g_extras.tabAnimInit || !g_cfg.fxSmoothTabs) {
                    g_extras.smoothTabX = targetR.left;
                    g_extras.smoothTabW = targetR.right - targetR.left;
                    g_extras.tabAnimInit = true;
                }
                else {
                    g_extras.smoothTabX = Lerp(g_extras.smoothTabX, targetR.left, 0.32f);
                    g_extras.smoothTabW = Lerp(g_extras.smoothTabW, targetR.right - targetR.left, 0.32f);
                    if (std::fabs(g_extras.smoothTabX - targetR.left) > 0.5f) {
                        g_hasScrolling = true;
                    }
                }
                D2D1_RECT_F animPill = D2D1::RectF(
                    g_extras.smoothTabX, targetR.top,
                    g_extras.smoothTabX + g_extras.smoothTabW, targetR.bottom);
                FillRR(animPill, 8.0f, ColorFromRef(GetActiveAccentRef(), 0.22f));
                g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(animPill, 8.0f, 8.0f), g_pBrushBorder, 1.0f);
                break;
            }
        }

        for (int i = 0; i < visCount; ++i) {
            Tab t = visTabs[i];
            D2D1_RECT_F tabRect = GetTabRect(i, visCount);
            bool isActive = (g_state.activeTab == t);

            const wchar_t* tName = GetTabShortName(t);
            g_pRT->DrawText(
                tName,
                (UINT32)wcslen(tName),
                (visCount >= 6) ? g_pTextFormatTime : g_pTextFormatTitle,
                tabRect,
                isActive ? g_pBrushAccent : g_pBrushText
            );

            if (g_cfg.showTabBadges) {
                if (t == Tab::Phone && !g_phoneList.empty()) {
                    D2D1_RECT_F badgeRect = D2D1::RectF(tabRect.right - 15.0f, tabRect.top + 2.0f, tabRect.right - 1.0f, tabRect.top + 15.0f);
                    FillRR(badgeRect, 6.5f, D2D1::ColorF(0.95f, 0.3f, 0.3f, 1.0f));
                    std::wstring countStr = std::to_wstring(g_phoneList.size());
                    g_pRT->DrawText(countStr.c_str(), (UINT32)countStr.size(), g_pTextFormatBadge, badgeRect, g_pBrushText);
                }
                else if (t == Tab::Discord && g_discord.micMuted) {
                    D2D1_RECT_F dot = D2D1::RectF(tabRect.right - 8.0f, tabRect.top + 4.0f, tabRect.right - 2.0f, tabRect.top + 10.0f);
                    FillRR(dot, 3.0f, D2D1::ColorF(0.95f, 0.30f, 0.32f, 1.0f));
                }
                else if (t == Tab::Game && g_cfg.gameModeActive) {
                    D2D1_RECT_F dot = D2D1::RectF(tabRect.right - 8.0f, tabRect.top + 4.0f, tabRect.right - 2.0f, tabRect.top + 10.0f);
                    FillRR(dot, 3.0f, D2D1::ColorF(0.30f, 0.92f, 0.52f, 1.0f));
                }
                else if (t == Tab::Calendar && !g_calEvents.empty()) {
                    D2D1_RECT_F dot = D2D1::RectF(tabRect.right - 8.0f, tabRect.top + 4.0f, tabRect.right - 2.0f, tabRect.top + 10.0f);
                    FillRR(dot, 3.0f, D2D1::ColorF(0.95f, 0.35f, 0.35f, 1.0f));
                }
            }
        }

        if (g_phoneEvent.hasActiveEvent) {
            DrawPhoneBanner(mp);
        }
        else {
            switch (g_state.activeTab) {
            case Tab::Music:
                DrawMusicTab(left, top, right, livePos, curLyric, mp);
                break;
            case Tab::Game:
                DrawGameTab(left, top, right, mp);
                break;
            case Tab::Discord:
                DrawDiscordTab(left, top, right, mp);
                break;
            case Tab::Clipboard:
                DrawClipboardTab(left, top, right, mp);
                break;
            case Tab::Calendar:
                DrawCalendarTab(left, top, right, mp);
                break;
            case Tab::Notes:
                DrawNotesTab(left, top, right, mp);
                break;
            case Tab::Weather:
                DrawWeatherTab(left, top, right, mp);
                break;
            case Tab::Shelf:
                DrawShelfTab(left, top, right, mp);
                break;
            case Tab::Phone:
                DrawPhoneTab(left, top, right, mp);
                break;
            }
        }

        g_pRT->PopAxisAlignedClip();
    }

    DrawIslandPet(left, top, right, bottom, g_music.isPlaying);

    g_pRT->EndDraw();

    HDC screenDC = GetDC(nullptr);
    POINT ptSrc = { 0, 0 };
    SIZE winSize = { CANVAS_W, CANVAS_H };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    RECT winRect;
    GetWindowRect(hwnd, &winRect);
    POINT ptWinPos = { winRect.left, winRect.top };

    UpdateLayeredWindow(hwnd, screenDC, &ptWinPos, &winSize, g_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_HOTKEY: {
        if (wParam == HOTKEY_GAMEMODE_ID) {
            g_cfg.gameModeActive = !g_cfg.gameModeActive;
            if (g_cfg.gameModeActive) g_game.sessionStartTick = GetTickCount64();
            TriggerMiniBadge(g_cfg.gameModeActive ? L"GAME ON" : L"GAME OFF", g_cfg.gameModeActive, 1800);
            NotifyIslandChanged();
            if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        OpenSettingsWindow();
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (GetAsyncKeyState(VK_MENU) & 0x8000) {
            g_state.isDraggingWindow = true;
            GetCursorPos(&g_state.dragStartCursor);
            g_state.dragStartOffsetX = g_cfg.offsetX;
            g_state.dragStartPosY = g_cfg.posY;
            SetCapture(hwnd);
            return 0;
        }

        if (g_state.curH < 80.0f) {
            if (!g_state.isHovered) {
                g_state.isHovered = true;
                ApplyOpenTabPolicy();
                UpdateClickThroughState(hwnd);
                UpdateTargets();
                RenderIsland(hwnd);
            }
            return 0;
        }

        if (g_state.curH >= 80.0f) {
            int mouseX = GET_X_LPARAM(lParam);
            int mouseY = GET_Y_LPARAM(lParam);

            std::vector<Tab> visTabs = GetVisibleTabs();
            int visCount = static_cast<int>(visTabs.size());
            for (int i = 0; i < visCount; ++i) {
                if (PtInRectF(GetTabRect(i, visCount), mouseX, mouseY)) {
                    SetWindowInputFocus(hwnd, false);
                    g_phoneEvent.hasActiveEvent = false;
                    g_state.activeTab = visTabs[i];
                    if (i > 0) {
                        g_state.isDraggingTabOnIsland = true;
                        g_state.draggedVisTabIdx = i;
                        g_state.tabDragStartX = mouseX;
                        SetCapture(hwnd);
                    }
                    UpdateTargets();
                    if (g_state.activeTab == Tab::Music) {
                        g_music.volume = GetSystemVolume();
                        g_forceMusicPoll = true;
                    }
                    else if (g_state.activeTab == Tab::Discord) {
                        SyncDiscordAppVolume(false);
                    }
                    RenderIsland(hwnd);
                    return 0;
                }
            }

            if (g_phoneEvent.hasActiveEvent) {
                if (PtInRectF(GetPhoneBannerCloseRect(), mouseX, mouseY)) {
                    g_phoneEvent.hasActiveEvent = false;
                    UpdateTargets();
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetPhoneBannerRect(), mouseX, mouseY)) {
                    g_phoneEvent.hasActiveEvent = false;
                    g_state.activeTab = Tab::Phone;
                    UpdateTargets();
                    RenderIsland(hwnd);
                    return 0;
                }
            }

            if (g_state.activeTab == Tab::Music) {
                if (PtInRectF(GetTimelineSliderRect(), mouseX, mouseY) && g_music.durationSec > 0.1) {
                    g_state.isDraggingTimeline = true;
                    SetCapture(hwnd);
                    UpdateTimelineFromMouse(hwnd, mouseX);
                    return 0;
                }
                if (PtInRectF(GetPrevBtnRect(), mouseX, mouseY)) {
                    SendMediaKey(VK_MEDIA_PREV_TRACK);
                    return 0;
                }
                if (PtInRectF(GetPlayBtnRect(), mouseX, mouseY)) {
                    SendMediaKey(VK_MEDIA_PLAY_PAUSE);
                    g_music.positionSec = GetLivePosition();
                    g_music.positionTick = GetTickCount64();
                    g_music.isPlaying = !g_music.isPlaying;
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetNextBtnRect(), mouseX, mouseY)) {
                    SendMediaKey(VK_MEDIA_NEXT_TRACK);
                    return 0;
                }
                if (PtInRectF(GetVolumeSliderRect(), mouseX, mouseY)) {
                    g_state.isDraggingVolume = true;
                    SetCapture(hwnd);
                    UpdateVolumeFromMouse(hwnd, mouseX);
                    return 0;
                }
            }
            else if (g_state.activeTab == Tab::Discord) {
                if (PtInRectF(GetDiscordBtnRect(0), mouseX, mouseY)) {
                    ToggleDiscordMic();
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetDiscordBtnRect(1), mouseX, mouseY)) {
                    ToggleDiscordDeafen();
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetDiscordBtnRect(2), mouseX, mouseY)) {
                    LeaveDiscordCall();
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetDiscordVolSliderRect(), mouseX, mouseY)) {
                    g_discord.isDraggingVol = true;
                    SetCapture(hwnd);
                    UpdateDiscordVolFromMouse(hwnd, mouseX);
                    return 0;
                }
            }
            else if (g_state.activeTab == Tab::Game) {
                if (PtInRectF(GetGameModeBtnRect(), mouseX, mouseY)) {
                    g_cfg.gameModeActive = !g_cfg.gameModeActive;
                    if (g_cfg.gameModeActive) g_game.sessionStartTick = GetTickCount64();
                    NotifyIslandChanged();
                    if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
                    return 0;
                }
                if (PtInRectF(GetGameCleanRamBtnRect(), mouseX, mouseY)) {
                    CleanSystemRam();
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetGameGhostBtnRect(), mouseX, mouseY)) {
                    g_cfg.gameClickThrough = !g_cfg.gameClickThrough;
                    NotifyIslandChanged();
                    if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
                    return 0;
                }
            }
            else if (g_state.activeTab == Tab::Clipboard) {
                if (!g_clipHistory.empty() && PtInRectF(GetClipClearBtnRect(), mouseX, mouseY)) {
                    g_clipHistory.clear();
                    g_clipScroll = 0;
                    SaveExtraTabsData();
                    RenderIsland(hwnd);
                    return 0;
                }
                for (int slot = 0; slot < 3; ++slot) {
                    int idx = g_clipScroll + slot;
                    if (idx >= static_cast<int>(g_clipHistory.size())) break;
                    D2D1_RECT_F card = GetClipCardRect(slot);
                    if (PtInRectF(card, mouseX, mouseY)) {
                        if (PtInRectF(GetClipCardCloseRect(card), mouseX, mouseY)) {
                            g_clipHistory.erase(g_clipHistory.begin() + idx);
                            ClampClipScroll();
                            SaveExtraTabsData();
                        }
                        else {
                            CopyWideToClipboard(hwnd, g_clipHistory[idx].text);
                            g_clipNotice = L"Скопировано в буфер обмена!";
                            g_clipNoticeUntil = GetTickCount64() + 2000;
                        }
                        RenderIsland(hwnd);
                        return 0;
                    }
                }
            }
            else if (g_state.activeTab == Tab::Calendar) {
                if (HandleCalendarTabClick(hwnd, mouseX, mouseY)) {
                    RenderIsland(hwnd);
                    return 0;
                }
            }
            else if (g_state.activeTab == Tab::Notes) {
                if (HandleNotesTabClick(hwnd, mouseX, mouseY)) {
                    RenderIsland(hwnd);
                    return 0;
                }
            }
            else if (g_state.activeTab == Tab::Weather) {
                if (g_state.isTypingCity) {
                    if (PtInRectF(GetCityInputRect(), mouseX, mouseY)) return 0;
                    SetTypingMode(hwnd, false);
                }
                D2D1_RECT_F startTyping = g_weather.info.loaded ? GetWxCityRect() : GetCityInputRect();
                if (PtInRectF(startTyping, mouseX, mouseY)) {
                    g_weather.inputBuffer.clear();
                    SetTypingMode(hwnd, true);
                    RenderIsland(hwnd);
                    return 0;
                }
                if (g_weather.info.loaded) {
                    for (int i = 0; i < 2; ++i) {
                        if (PtInRectF(GetWxSegRect(i), mouseX, mouseY)) {
                            g_weather.viewWeek = (i == 1);
                            RenderIsland(hwnd);
                            return 0;
                        }
                    }
                }
            }
            else if (g_state.activeTab == Tab::Shelf) {
                if (PtInRectF(GetShelfLinkBtnRect(), mouseX, mouseY)) {
                    AddLinkFromClipboard(hwnd);
                    RenderIsland(hwnd);
                    return 0;
                }
                if (!g_shelf.empty() && PtInRectF(GetShelfClearBtnRect(), mouseX, mouseY)) {
                    g_shelf.clear();
                    g_shelfScroll = 0;
                    RenderIsland(hwnd);
                    return 0;
                }
                int idx = HitShelfChip(mouseX, mouseY);
                if (idx >= 0) {
                    D2D1_RECT_F chip = GetShelfChipRect(idx - g_shelfScroll * 2);
                    if (PtInRectF(GetShelfChipCloseRect(chip), mouseX, mouseY)) {
                        g_shelf.erase(g_shelf.begin() + idx);
                        ClampShelfScroll();
                        RenderIsland(hwnd);
                        return 0;
                    }
                    g_pressedItem = idx;
                    g_pressPt.x = mouseX;
                    g_pressPt.y = mouseY;
                    SetCapture(hwnd);
                    return 0;
                }
            }
            else if (g_state.activeTab == Tab::Phone) {
                if (!g_phoneList.empty() && PtInRectF(GetPhoneClearBtnRect(), mouseX, mouseY)) {
                    g_phoneList.clear();
                    g_phoneScroll = 0;
                    RenderIsland(hwnd);
                    return 0;
                }
                int idx = HitPhoneCard(mouseX, mouseY);
                if (idx >= 0) {
                    D2D1_RECT_F card = GetPhoneCardRect(idx - g_phoneScroll);
                    if (PtInRectF(GetPhoneCardCloseRect(card), mouseX, mouseY)) {
                        g_phoneList.erase(g_phoneList.begin() + idx);
                        ClampPhoneScroll();
                        RenderIsland(hwnd);
                        return 0;
                    }
                }
            }

            if (IsAnyTypingActive()) {
                SetWindowInputFocus(hwnd, false);
                RenderIsland(hwnd);
            }
        }
        return 0;
    }

    case WM_PHONE_NOTIFICATION: {
        auto* item = reinterpret_cast<PhoneNotifItem*>(lParam);
        if (item) {
            g_phoneList.insert(g_phoneList.begin(), *item);
            ClampPhoneScroll();

            if (item->showPopup) {
                g_phoneEvent.hasActiveEvent = true;
                g_phoneEvent.isCall = item->isCall;
                g_phoneEvent.appName = item->appName;
                g_phoneEvent.title = item->title;
                g_phoneEvent.body = item->body;
                g_phoneEvent.timestamp = GetTickCount64();
                if (g_cfg.soundOnNotify) MessageBeep(MB_OK);
                UpdateTargets();
            }
            delete item;
        }
        RenderIsland(hwnd);
        return 0;
    }

    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Настройки Island...");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_GAMEMODE, g_cfg.gameModeActive ? L"Выключить Игровой режим (Alt+G)" : L"Включить Игровой режим (Alt+G)");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"Показать / Скрыть остров");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
        }
        else if (lParam == WM_LBUTTONDBLCLK) {
            OpenSettingsWindow();
        }
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_TRAY_SETTINGS:
            OpenSettingsWindow();
            break;
        case ID_TRAY_GAMEMODE:
            g_cfg.gameModeActive = !g_cfg.gameModeActive;
            if (g_cfg.gameModeActive) g_game.sessionStartTick = GetTickCount64();
            NotifyIslandChanged();
            break;
        case ID_TRAY_TOGGLE: {
            bool isVisible = IsWindowVisible(hwnd);
            ShowWindow(hwnd, isVisible ? SW_HIDE : SW_SHOW);
            if (!isVisible) RenderIsland(hwnd);
            break;
        }
        case ID_TRAY_EXIT: {
            g_running = false;
            PostQuitMessage(0);
            break;
        }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_discord.isDraggingVol) {
            g_discord.isDraggingVol = false;
            ReleaseCapture();
            UpdateTargets();
            return 0;
        }
        if (g_state.isDraggingTabOnIsland) {
            g_state.isDraggingTabOnIsland = false;
            g_state.draggedVisTabIdx = -1;
            ReleaseCapture();
            SaveConfig();
            if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
            return 0;
        }
        if (g_state.isDraggingWindow) {
            g_state.isDraggingWindow = false;
            ReleaseCapture();
            SaveConfig();
            if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
            UpdateTargets();
            return 0;
        }
        if (g_pressedItem >= 0) {
            int idx = g_pressedItem;
            g_pressedItem = -1;
            ReleaseCapture();
            if (HitShelfChip(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)) == idx) {
                OpenShelfItem(idx);
            }
        }
        if (g_state.isDraggingVolume) {
            g_state.isDraggingVolume = false;
            ReleaseCapture();
            UpdateTargets();
        }
        if (g_state.isDraggingTimeline) {
            g_state.isDraggingTimeline = false;
            ReleaseCapture();
            g_music.positionTick = GetTickCount64();
            g_seekTargetSeconds = g_music.positionSec;
            g_hasSeekRequest = true;
            g_forceMusicPoll = true;
            UpdateTargets();
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (g_state.curH < 80.0f && g_cfg.wheelVolumeCompact) {
            float step = (delta > 0) ? 0.04f : -0.04f;
            SetSystemVolume(GetSystemVolume() + step);
            int pct = static_cast<int>(std::round(g_music.volume * 100.0f));
            TriggerMiniBadge(L"\U0001F50A " + std::to_wstring(pct) + L"%", true, 1600);
            UpdateTargets();
            RenderIsland(hwnd);
            return 0;
        }

        if (g_state.curH >= 80.0f) {
            if (g_state.activeTab == Tab::Music) {
                float step = (delta > 0) ? 0.05f : -0.05f;
                SetSystemVolume(GetSystemVolume() + step);
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Discord) {
                float step = (delta > 0) ? 0.05f : -0.05f;
                SyncDiscordAppVolume(true, g_discord.discordVolume + step);
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Clipboard) {
                g_clipScroll += (delta > 0) ? -1 : 1;
                ClampClipScroll();
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Notes && g_notesMode == 1) {
                int maxTodoSc = std::max(0, static_cast<int>(g_todoList.size()) - 3);
                g_todoScroll = std::clamp(g_todoScroll + (delta > 0 ? -1 : 1), 0, maxTodoSc);
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Calendar) {
                auto dayEvs = GetEventsForSelectedDate();
                int maxCalSc = std::max(0, static_cast<int>(dayEvs.size()) - 2);
                g_calEventScroll = std::clamp(g_calEventScroll + (delta > 0 ? -1 : 1), 0, maxCalSc);
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Weather && g_weather.info.loaded && !g_weather.viewWeek) {
                int maxOff = std::max(0, static_cast<int>(g_weather.info.hours.size()) - 7);
                g_weather.hourOffset = std::clamp(g_weather.hourOffset + (delta > 0 ? -1 : 1), 0, maxOff);
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Shelf) {
                g_shelfScroll += (delta > 0) ? -1 : 1;
                ClampShelfScroll();
                RenderIsland(hwnd);
            }
            else if (g_state.activeTab == Tab::Phone) {
                g_phoneScroll += (delta > 0) ? -1 : 1;
                ClampPhoneScroll();
                RenderIsland(hwnd);
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_discord.isDraggingVol && (wParam & MK_LBUTTON)) {
            UpdateDiscordVolFromMouse(hwnd, GET_X_LPARAM(lParam));
            return 0;
        }
        if (g_state.isDraggingTabOnIsland && (wParam & MK_LBUTTON)) {
            int mx = GET_X_LPARAM(lParam);
            std::vector<Tab> visTabs = GetVisibleTabs();
            int visCount = static_cast<int>(visTabs.size());
            for (int i = 1; i < visCount; ++i) {
                D2D1_RECT_F r = GetTabRect(i, visCount);
                if (mx >= r.left && mx <= r.right && i != g_state.draggedVisTabIdx) {
                    int idA = static_cast<int>(visTabs[g_state.draggedVisTabIdx]);
                    int idB = static_cast<int>(visTabs[i]);
                    int posA = -1, posB = -1;
                    for (int k = 0; k < kCustomTabCount; ++k) {
                        if (g_cfg.tabOrder[k] == idA) posA = k;
                        if (g_cfg.tabOrder[k] == idB) posB = k;
                    }
                    if (posA >= 0 && posB >= 0) {
                        std::swap(g_cfg.tabOrder[posA], g_cfg.tabOrder[posB]);
                        g_state.draggedVisTabIdx = i;
                        RenderIsland(hwnd);
                        if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
                    }
                    break;
                }
            }
            return 0;
        }
        if (g_state.isDraggingWindow && (wParam & MK_LBUTTON)) {
            POINT pt;
            GetCursorPos(&pt);
            g_cfg.offsetX = g_state.dragStartOffsetX + (pt.x - g_state.dragStartCursor.x);
            if (!g_cfg.dockToTop) {
                g_cfg.posY = g_state.dragStartPosY + (pt.y - g_state.dragStartCursor.y);
            }
            ApplyWindowPosition(hwnd);
            if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
            return 0;
        }
        if (g_pressedItem >= 0 && (wParam & MK_LBUTTON)) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            if (std::abs(x - g_pressPt.x) > GetSystemMetrics(SM_CXDRAG) ||
                std::abs(y - g_pressPt.y) > GetSystemMetrics(SM_CYDRAG)) {
                int idx = g_pressedItem;
                g_pressedItem = -1;
                ReleaseCapture();
                BeginShelfDragOut(hwnd, idx);
            }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (g_state.isTypingCity) {
            if (wParam == VK_RETURN) {
                std::wstring query = g_weather.inputBuffer;
                SetTypingMode(hwnd, false);
                if (!query.empty()) FetchWeatherAsync(hwnd, query);
                RenderIsland(hwnd);
            }
            else if (wParam == VK_ESCAPE) {
                SetTypingMode(hwnd, false);
                RenderIsland(hwnd);
            }
        }
        else if (g_state.isTypingNote || g_state.isTypingCalNote) {
            if (wParam == VK_ESCAPE) {
                SetWindowInputFocus(hwnd, false);
                SaveExtraTabsData();
                RenderIsland(hwnd);
            }
            else if (wParam == VK_RETURN && g_state.isTypingCalNote) {
                AddCalendarEventFromInput();
                SetWindowInputFocus(hwnd, false);
                RenderIsland(hwnd);
            }
            else if (wParam == VK_RETURN && g_state.isTypingNote && g_notesMode == 1) {
                AddTodoFromInput();
                RenderIsland(hwnd);
            }
            else if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                std::wstring clip = ReadClipboardText(hwnd);
                if (g_state.isTypingNote) {
                    if (g_notesMode == 0) GetActiveNotePage() += clip;
                    else g_todoInput += clip;
                }
                else if (g_state.isTypingCalNote) {
                    g_calEventInput += clip;
                }
                SaveExtraTabsData();
                RenderIsland(hwnd);
            }
        }
        return 0;
    }
    case WM_CHAR: {
        wchar_t ch = (wchar_t)wParam;
        if (g_state.isTypingCity) {
            if (ch == L'\b') {
                if (!g_weather.inputBuffer.empty()) g_weather.inputBuffer.pop_back();
            }
            else if (ch >= 32 && ch != 127 && g_weather.inputBuffer.size() < 28) {
                g_weather.inputBuffer.push_back(ch);
            }
            RenderIsland(hwnd);
        }
        else if (g_state.isTypingNote) {
            if (g_notesMode == 0) {
                std::wstring& curPage = GetActiveNotePage();
                if (ch == L'\b') {
                    if (!curPage.empty()) curPage.pop_back();
                }
                else if (ch == L'\r' || ch == L'\n') {
                    if (curPage.size() < 600) curPage.push_back(L'\n');
                }
                else if (ch >= 32 && ch != 127 && curPage.size() < 600) {
                    curPage.push_back(ch);
                }
            }
            else {
                if (ch == L'\b') {
                    if (!g_todoInput.empty()) g_todoInput.pop_back();
                }
                else if (ch >= 32 && ch != 127 && g_todoInput.size() < 60) {
                    g_todoInput.push_back(ch);
                }
            }
            SaveExtraTabsData();
            RenderIsland(hwnd);
        }
        else if (g_state.isTypingCalNote) {
            if (ch == L'\b') {
                if (!g_calEventInput.empty()) g_calEventInput.pop_back();
            }
            else if (ch >= 32 && ch != 127 && g_calEventInput.size() < 48) {
                g_calEventInput.push_back(ch);
            }
            RenderIsland(hwnd);
        }
        return 0;
    }
    case WM_WEATHER_UPDATED: {
        auto* res = reinterpret_cast<WeatherResult*>(lParam);
        g_weatherBusy = false;
        if (res) {
            if (res->success) {
                bool stale = res->silent && res->query != g_weather.lastQuery;
                if (!stale) {
                    g_weather.info = res->info;
                    g_weather.city = res->info.city;
                    if (!g_state.isTypingCity) {
                        g_weather.inputBuffer = res->info.city;
                    }
                    g_weather.tempCompact = FormatTemp(res->info.temp) + L"C";
                    g_weather.statusLine.clear();
                    int maxOff = std::max(0, static_cast<int>(res->info.hours.size()) - 7);
                    g_weather.hourOffset = std::clamp(g_weather.hourOffset, 0, maxOff);
                }
            }
            else if (!res->silent) {
                g_weather.info.loaded = false;
                g_weather.statusLine = res->errorText;
            }
            delete res;
        }
        RenderIsland(hwnd);
        if (g_hSettingsWnd) RenderSettingsWindow(g_hSettingsWnd);
        return 0;
    }
    case WM_MUSIC_UPDATED: {
        auto* res = reinterpret_cast<MusicResult*>(lParam);
        if (res) {
            g_music.title = res->title;
            g_music.artist = res->artist;
            g_music.isPlaying = res->isPlaying;
            g_music.durationSec = res->durationSec;
            if (!g_state.isDraggingTimeline) {
                g_music.positionSec = res->positionSec;
                g_music.positionTick = GetTickCount64();
            }
            if (res->hasLyrics) {
                g_music.lyrics = std::move(res->lyrics);
            }

            if (res->clearCover || res->hasNewCover) {
                if (g_pAlbumBitmap) {
                    g_pAlbumBitmap->Release();
                    g_pAlbumBitmap = nullptr;
                }
                g_extras.hasAlbumColor = false;

                if (res->hasNewCover && res->coverW > 0 && res->coverH > 0) {
                    g_extras.albumColor = ExtractVibrantColor(res->coverPixels, res->coverW, res->coverH);
                    g_extras.hasAlbumColor = true;

                    D2D1_BITMAP_PROPERTIES bmpProps = D2D1::BitmapProperties(
                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
                    );
                    g_pRT->CreateBitmap(
                        D2D1::SizeU(res->coverW, res->coverH),
                        res->coverPixels.data(),
                        res->coverW * 4,
                        &bmpProps,
                        &g_pAlbumBitmap
                    );
                }
                ApplyConfigBrushes();
            }
            delete res;
        }
        if (!g_state.isDraggingVolume) {
            g_music.volume = GetSystemVolume();
        }
        RenderIsland(hwnd);
        return 0;
    }
    case WM_MBUTTONDOWN: {
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == ANIM_TIMER_ID) {
            static bool s_hiddenByFs = false;
            bool fsActive = IsFullscreenWindowFocused(hwnd);

            if (g_cfg.fullscreenBehavior == 0) {
                if (fsActive && !s_hiddenByFs) {
                    ShowWindow(hwnd, SW_HIDE);
                    s_hiddenByFs = true;
                    return 0;
                }
                else if (!fsActive && s_hiddenByFs) {
                    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                    s_hiddenByFs = false;
                    RenderIsland(hwnd);
                }
                if (s_hiddenByFs) return 0;
            }
            else if (g_cfg.fullscreenBehavior == 1) {
                if (fsActive && !g_cfg.gameModeActive) {
                    g_cfg.gameModeActive = true;
                    g_game.autoTriggeredByFs = true;
                    g_game.sessionStartTick = GetTickCount64();
                    UpdateTargets();
                }
                else if (!fsActive && g_game.autoTriggeredByFs) {
                    g_cfg.gameModeActive = false;
                    g_game.autoTriggeredByFs = false;
                    UpdateTargets();
                }
            }

            UpdateClickThroughState(hwnd);

            const ULONGLONG now = GetTickCount64();
            PollClipboardChanges(hwnd);
            PollDiscordMicLevel();
            UpdateBeatPhysics(GetAudioPeakLevel(), g_music.isPlaying);

            if (g_cfg.capsLockAlert) {
                bool capsNow = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
                if (!g_extras.capsInitialized) {
                    g_extras.lastCapsState = capsNow;
                    g_extras.capsInitialized = true;
                }
                else if (capsNow != g_extras.lastCapsState) {
                    g_extras.lastCapsState = capsNow;
                    TriggerMiniBadge(capsNow ? L"CAPS ON" : L"CAPS OFF", capsNow, 2000);
                    UpdateTargets();
                }
            }

            static bool s_hadMiniBadge = false;
            bool hasMiniNow = (now < g_extras.miniBadgeUntil);
            if (s_hadMiniBadge && !hasMiniNow) {
                UpdateTargets();
            }
            s_hadMiniBadge = hasMiniNow;

            POINT pt;
            GetCursorPos(&pt);
            RECT winRect;
            GetWindowRect(hwnd, &winRect);

            float currentLeft = winRect.left + (CANVAS_W - g_state.curW) * 0.5f;
            float currentRight = currentLeft + g_state.curW;
            float currentTop = static_cast<float>(winRect.top + ISLAND_TOP);
            float currentBottom = winRect.top + ISLAND_TOP + g_state.curH;

            bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            bool blockHoverByClickThrough = (g_cfg.gameModeActive && g_cfg.gameClickThrough && !altHeld && !g_state.isHovered);

            bool isInside = !blockHoverByClickThrough &&
                (pt.x >= currentLeft && pt.x <= currentRight &&
                    pt.y >= currentTop && pt.y <= currentBottom);

            if (!g_state.isDraggingVolume && !g_state.isDraggingTimeline && !g_discord.isDraggingVol &&
                !IsAnyTypingActive() && !g_state.isDragOver &&
                !g_state.isDraggingWindow && !g_state.isDraggingTabOnIsland) {

                if (isInside) {
                    if (!g_state.rawCursorInside) {
                        g_state.rawCursorInside = true;
                        g_state.hoverEnterTick = now;
                    }
                    if (!g_cfg.openByClick && !g_state.isHovered &&
                        (now - g_state.hoverEnterTick >= static_cast<ULONGLONG>(g_cfg.hoverDelayMs))) {
                        g_state.isHovered = true;
                        ApplyOpenTabPolicy();
                        UpdateTargets();
                    }
                }
                else {
                    g_state.rawCursorInside = false;
                    if (g_state.isHovered) {
                        g_state.isHovered = false;
                        UpdateTargets();
                    }
                }
            }

            if (g_phoneEvent.hasActiveEvent &&
                now - g_phoneEvent.timestamp >= PHONE_POPUP_DURATION_MS) {
                g_phoneEvent.hasActiveEvent = false;
                UpdateTargets();
            }

            if (g_state.isDraggingVolume) {
                POINT cur;
                GetCursorPos(&cur);
                ScreenToClient(hwnd, &cur);
                UpdateVolumeFromMouse(hwnd, cur.x);
            }
            else if (g_state.isDraggingTimeline) {
                POINT cur;
                GetCursorPos(&cur);
                ScreenToClient(hwnd, &cur);
                UpdateTimelineFromMouse(hwnd, cur.x);
            }

            if (g_weather.info.loaded && !g_weatherBusy &&
                now - g_lastWeatherTick > 10ull * 60ull * 1000ull) {
                FetchWeatherAsync(hwnd, g_weather.lastQuery, true);
            }
            else if (!g_weather.info.loaded && !g_weatherBusy && !g_state.isTypingCity &&
                now - g_lastWeatherTick > 20000) {
                FetchWeatherAsync(hwnd, g_weather.lastQuery, true);
            }

            static WORD s_lastMinute = 0xFFFF;
            SYSTEMTIME stNow;
            GetLocalTime(&stNow);
            bool minuteChanged = (stNow.wMinute != s_lastMinute);
            s_lastMinute = stNow.wMinute;

            static ULONGLONG s_lastGameHudRedraw = 0;
            bool needGameTick = (g_cfg.gameModeActive || (g_state.isHovered && g_state.activeTab == Tab::Game)) &&
                (now - s_lastGameHudRedraw > 250);
            if (needGameTick) s_lastGameHudRedraw = now;

            bool needDiscordTick = (g_state.isHovered && g_state.activeTab == Tab::Discord && g_discord.micPeak > 0.02f);

            bool wasAnimating = g_state.isAnimating;
            const bool lyricChanged =
                (GetLyricIndex(GetLivePosition() + LYRIC_LOOKAHEAD_SEC) != g_lastLyricIdx);
            StepAnimation();

            bool needMusicTick = g_music.isPlaying &&
                ((g_state.curH > 80.0f && g_state.activeTab == Tab::Music) ||
                    (g_state.curH < 80.0f && (!g_music.lyrics.empty() || g_cfg.showEqualizer)) ||
                    g_cfg.beatCirclesMode != 0);

            if (wasAnimating || lyricChanged || g_hasScrolling || needMusicTick || needGameTick || needDiscordTick ||
                minuteChanged || isInside || g_phoneEvent.hasActiveEvent || g_cfg.petType != 0) {
                RenderIsland(hwnd);
            }
        }
        return 0;
    }
    case WM_DESTROY: {
        UnregisterHotKey(hwnd, HOTKEY_GAMEMODE_ID);
        SaveExtraTabsData();
        g_running = false;
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Cleanup() {
    g_running = false;
    if (g_pCfgRT) {
        g_pCfgRT->Release();
        g_pCfgRT = nullptr;
    }
    RemoveTrayIcon();
    CleanupDiscordAudio();
    CleanupMusic();
    CleanupGraphics();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    OleInitialize(nullptr);

    LoadConfig();
    LoadExtraTabsData();
    if (!InitGraphics()) return -1;
    ApplyConfigBrushes();
    InitAudio();
    InitDiscordAudio();

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    wc.lpszClassName = L"DynamicIslandOverlayClass";
    RegisterClassEx(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int posX = (screenW - CANVAS_W) / 2 + g_cfg.offsetX;
    int posY = g_cfg.dockToTop ? 0 : g_cfg.posY;

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"IslandOverlay",
        WS_POPUP | WS_VISIBLE,
        posX, posY, CANVAS_W, CANVAS_H,
        nullptr, nullptr, hInstance, nullptr
    );

    g_hwnd = hwnd;
    ApplyWindowPosition(hwnd);
    UpdateTargets();

    RegisterHotKey(hwnd, HOTKEY_GAMEMODE_ID, MOD_ALT, 'G');

    g_dropTarget = new ShelfDropTarget();
    RegisterDragDrop(hwnd, g_dropTarget);
    AddTrayIcon(hwnd);

    RenderIsland(hwnd);
    SetTimer(hwnd, ANIM_TIMER_ID, 16, nullptr);

    FetchWeatherAsync(hwnd, L"Пермь");
    StartMusicWorker(hwnd);
    StartPhoneListener(hwnd);
    StartGameMetricsWorker();

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_dropTarget) {
        RevokeDragDrop(hwnd);
        g_dropTarget->Release();
        g_dropTarget = nullptr;
    }
    Cleanup();
    OleUninitialize();
    return 0;
}