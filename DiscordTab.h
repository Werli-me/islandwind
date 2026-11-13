#pragma once
#include "Common.h"
#include "Graphics.h"
#include <audiopolicy.h>

struct DiscordState {
    bool isRunning = false;
    bool micMuted = false;
    bool deafened = false;
    float discordVolume = 1.0f; // Громкость процесса Discord.exe (0.0 .. 1.0)
    float micPeak = 0.0f;       // Уровень голоса в микрофоне (0.0 .. 1.0)
    bool isDraggingVol = false;
    ULONGLONG lastSessionCheck = 0;
} g_discord;

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
        if (SUCCEEDED(g_pMicVolume->GetMute(&muted))) {
            g_discord.micMuted = (muted != FALSE);
        }
    }
}

inline void CleanupDiscordAudio() {
    if (g_pMicMeter) { g_pMicMeter->Release(); g_pMicMeter = nullptr; }
    if (g_pMicVolume) { g_pMicVolume->Release(); g_pMicVolume = nullptr; }
}

inline void SendDiscordChordKey(WORD vkMain, bool ctrl = true, bool shift = true, bool alt = false) {
    INPUT inputs[8] = {};
    int n = 0;
    auto pushKey = [&](WORD vk, bool up) {
        inputs[n].type = INPUT_KEYBOARD;
        inputs[n].ki.wVk = vk;
        inputs[n].ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
        ++n;
        };
    if (ctrl)  pushKey(VK_CONTROL, false);
    if (shift) pushKey(VK_SHIFT, false);
    if (alt)   pushKey(VK_MENU, false);
    pushKey(vkMain, false);
    pushKey(vkMain, true);
    if (alt)   pushKey(VK_MENU, true);
    if (shift) pushKey(VK_SHIFT, true);
    if (ctrl)  pushKey(VK_CONTROL, true);
    SendInput(n, inputs, sizeof(INPUT));
}

// Поиск аудиосессии Discord.exe в микшере Windows и управление громкостью собеседников
inline void SyncDiscordAppVolume(bool setNewVol, float newVol = 1.0f, int muteAction = -1) {
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
                                        if (pathStr.find(L"discord") != std::wstring::npos) {
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
                                                if (muteAction == 1) pVol->SetMute(TRUE, nullptr);
                                                else if (muteAction == 0) pVol->SetMute(FALSE, nullptr);
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

// Периодическое обновление состояния микрофона и пика голоса
inline void PollDiscordAudioState() {
    if (g_pMicVolume) {
        BOOL muted = FALSE;
        if (SUCCEEDED(g_pMicVolume->GetMute(&muted))) {
            g_discord.micMuted = (muted != FALSE);
        }
    }
    if (g_pMicMeter && !g_discord.micMuted) {
        float peak = 0.0f;
        if (SUCCEEDED(g_pMicMeter->GetPeakValue(&peak))) {
            g_discord.micPeak = Lerp(g_discord.micPeak, std::clamp(peak * 2.2f, 0.0f, 1.0f), 0.45f);
            if (g_discord.micPeak > 0.04f) g_hasScrolling = true;
        }
    }
    else {
        g_discord.micPeak = 0.0f;
    }

    ULONGLONG now = GetTickCount64();
    if (now - g_discord.lastSessionCheck > 2000) {
        g_discord.lastSessionCheck = now;
        SyncDiscordAppVolume(false);
    }
}

inline void ToggleDiscordMic() {
    g_discord.micMuted = !g_discord.micMuted;
    if (g_pMicVolume) {
        g_pMicVolume->SetMute(g_discord.micMuted ? TRUE : FALSE, nullptr);
    }
    SendDiscordChordKey('M', true, true, false); // Ctrl + Shift + M
    TriggerMiniBadge(g_discord.micMuted ? L"\U0001F399 MIC OFF" : L"\U0001F399 MIC ON", !g_discord.micMuted, 1800);
}

inline void ToggleDiscordDeafen() {
    g_discord.deafened = !g_discord.deafened;
    if (g_discord.deafened && !g_discord.micMuted) {
        g_discord.micMuted = true;
        if (g_pMicVolume) g_pMicVolume->SetMute(TRUE, nullptr);
    }
    SyncDiscordAppVolume(false, 1.0f, g_discord.deafened ? 1 : 0);
    SendDiscordChordKey('D', true, true, false); // Ctrl + Shift + D
    TriggerMiniBadge(g_discord.deafened ? L"\U0001F507 ЗВУК ВЫКЛ" : L"\U0001F50A ЗВУК ВКЛ", !g_discord.deafened, 1800);
}

inline void LeaveDiscordCall() {
    SendDiscordChordKey('H', true, true, false); // Ctrl + Shift + H (настраиваемый хоткей выхода в Discord)
    TriggerMiniBadge(L"\U0001F4DE ОТКЛЮЧЕНО", false, 1800);
}

// Геометрия кнопок и ползунка во вкладке Discord
inline D2D1_RECT_F GetDiscordMicBtnRect() {
    float left = IslandLeft();
    float w = (g_state.curW - 28.0f - 16.0f) / 3.0f;
    return D2D1::RectF(left + 14.0f, ISLAND_TOP + 100.0f, left + 14.0f + w, ISLAND_TOP + 158.0f);
}

inline D2D1_RECT_F GetDiscordDeafBtnRect() {
    float left = IslandLeft();
    float w = (g_state.curW - 28.0f - 16.0f) / 3.0f;
    float x = left + 14.0f + w + 8.0f;
    return D2D1::RectF(x, ISLAND_TOP + 100.0f, x + w, ISLAND_TOP + 158.0f);
}

inline D2D1_RECT_F GetDiscordLeaveBtnRect() {
    float left = IslandLeft();
    float w = (g_state.curW - 28.0f - 16.0f) / 3.0f;
    float x = left + 14.0f + 2.0f * (w + 8.0f);
    return D2D1::RectF(x, ISLAND_TOP + 100.0f, x + w, ISLAND_TOP + 158.0f);
}

inline D2D1_RECT_F GetDiscordVolSliderRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    return D2D1::RectF(left + 14.0f, ISLAND_TOP + 170.0f, right - 14.0f, ISLAND_TOP + 222.0f);
}

inline void UpdateDiscordVolFromMouse(HWND hwnd, int mouseX) {
    D2D1_RECT_F sr = GetDiscordVolSliderRect();
    float trackLeft = sr.left + 16.0f;
    float trackRight = sr.right - 56.0f;
    float t = std::clamp((mouseX - trackLeft) / std::max(1.0f, trackRight - trackLeft), 0.0f, 1.0f);
    SyncDiscordAppVolume(true, t);
    RenderIsland(hwnd);
}

inline void DrawDiscordTab(float left, float top, float right, const POINT& mp) {
    // 1. Верхняя карточка: Статус голосового канала и живой индикатор нашего микрофона
    D2D1_RECT_F statusCard = D2D1::RectF(left + 14.0f, top + 44.0f, right - 14.0f, top + 90.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(statusCard, 13.0f, 13.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(statusCard, 13.0f, 13.0f), g_pBrushBorder, 1.0f);

    float avatarCx = statusCard.left + 26.0f;
    float avatarCy = (statusCard.top + statusCard.bottom) * 0.5f;

    // Зеленое кольцо вокруг иконки, когда мы говорим (как в Discord)
    if (!g_discord.micMuted && g_discord.micPeak > 0.04f) {
        float ringR = 15.0f + g_discord.micPeak * 4.5f;
        g_pBrushIcon->SetColor(D2D1::ColorF(0.26f, 0.86f, 0.48f, 0.85f));
        g_pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(avatarCx, avatarCy), ringR, ringR), g_pBrushIcon, 2.2f);
    }

    D2D1_COLOR_F badgeCol = g_discord.micMuted
        ? D2D1::ColorF(0.95f, 0.28f, 0.28f, 0.25f)
        : D2D1::ColorF(0.35f, 0.40f, 0.95f, 0.28f);
    FillRR(D2D1::RectF(avatarCx - 14.0f, avatarCy - 14.0f, avatarCx + 14.0f, avatarCy + 14.0f), 14.0f, badgeCol);
    g_pRT->DrawText(g_discord.micMuted ? L"\U0001F507" : L"\U0001F399", 2, g_pTextFormatSub,
        D2D1::RectF(avatarCx - 14.0f, avatarCy - 14.0f, avatarCx + 14.0f, avatarCy + 14.0f), g_pBrushText);

    std::wstring statusTitle = g_discord.isRunning
        ? (g_discord.micMuted ? L"Микрофон выключен (Muted)" : L"Голосовой канал активен")
        : L"Управление микрофоном и Discord";
    std::wstring statusSub = g_discord.deafened
        ? L"Звук собеседников заглушен (Deafened)"
        : (g_discord.micMuted ? L"Нажмите кнопку ниже, чтобы включить микрофон" : L"Зеленое кольцо светится, когда вы говорите");

    g_pRT->DrawText(statusTitle.c_str(), (UINT32)statusTitle.size(), g_pTextFormatLeftTitle,
        D2D1::RectF(statusCard.left + 50.0f, statusCard.top + 6.0f, statusCard.right - 12.0f, statusCard.top + 26.0f),
        g_discord.micMuted ? g_pBrushAccent : g_pBrushText);
    g_pRT->DrawText(statusSub.c_str(), (UINT32)statusSub.size(), g_pTextFormatSmallMuted,
        D2D1::RectF(statusCard.left + 50.0f, statusCard.top + 25.0f, statusCard.right - 12.0f, statusCard.bottom - 5.0f),
        g_pBrushTextMuted);

    // 2. Три большие кнопки: Микрофон, Наушники, Ливнуть
    auto drawActionCard = [&](const D2D1_RECT_F& r, const wchar_t* icon, const wchar_t* label, bool isAlertActive, bool isDangerBtn) {
        bool hot = PtInRectF(r, mp.x, mp.y);
        D2D1_COLOR_F bg = isAlertActive
            ? D2D1::ColorF(0.92f, 0.26f, 0.28f, hot ? 0.38f : 0.26f)
            : (isDangerBtn
                ? D2D1::ColorF(0.92f, 0.26f, 0.28f, hot ? 0.30f : 0.14f)
                : D2D1::ColorF(1.0f, 1.0f, 1.0f, hot ? 0.14f : 0.07f));

        FillRR(r, 12.0f, bg);

        if (isAlertActive || isDangerBtn) {
            g_pBrushIcon->SetColor(D2D1::ColorF(0.95f, 0.32f, 0.32f, isAlertActive ? 0.85f : 0.45f));
            g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(r, 12.0f, 12.0f), g_pBrushIcon, 1.2f);
        }
        else {
            g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(r, 12.0f, 12.0f), g_pBrushBorder, 1.0f);
        }

        g_pRT->DrawText(icon, (UINT32)wcslen(icon), g_pTextFormatTitle,
            D2D1::RectF(r.left, r.top + 6.0f, r.right, r.top + 32.0f), g_pBrushText);
        g_pRT->DrawText(label, (UINT32)wcslen(label), g_pTextFormatTime,
            D2D1::RectF(r.left, r.top + 32.0f, r.right, r.bottom - 4.0f),
            isAlertActive ? g_pBrushText : g_pBrushTextMuted);
        };

    drawActionCard(GetDiscordMicBtnRect(),
        g_discord.micMuted ? L"\U0001F507" : L"\U0001F399",
        g_discord.micMuted ? L"Размутить" : L"Микрофон",
        g_discord.micMuted, false);

    drawActionCard(GetDiscordDeafBtnRect(),
        g_discord.deafened ? L"\U0001F507" : L"\U0001F3A7",
        g_discord.deafened ? L"Вкл. звук" : L"Наушники",
        g_discord.deafened, false);

    drawActionCard(GetDiscordLeaveBtnRect(),
        L"\U0001F4DE", L"Ливнуть",
        false, true);

    // 3. Нижняя карточка: Ползунок громкости собеседников (Discord.exe)
    D2D1_RECT_F volCard = GetDiscordVolSliderRect();
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(volCard, 13.0f, 13.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(volCard, 13.0f, 13.0f), g_pBrushBorder, 1.0f);

    int pct = static_cast<int>(std::round(g_discord.discordVolume * 100.0f));
    std::wstring volTitle = L"Громкость собеседников (Discord)";
    std::wstring volPctStr = std::to_wstring(pct) + L"%";

    g_pRT->DrawText(volTitle.c_str(), (UINT32)volTitle.size(), g_pTextFormatSmallMuted,
        D2D1::RectF(volCard.left + 16.0f, volCard.top + 6.0f, volCard.right - 64.0f, volCard.top + 24.0f), g_pBrushTextMuted);
    g_pRT->DrawText(volPctStr.c_str(), (UINT32)volPctStr.size(), g_pTextFormatRightSub,
        D2D1::RectF(volCard.right - 68.0f, volCard.top + 6.0f, volCard.right - 16.0f, volCard.top + 24.0f), g_pBrushAccent);

    float trackY = volCard.top + 36.0f;
    float trackLeft = volCard.left + 16.0f;
    float trackRight = volCard.right - 16.0f;
    D2D1_RECT_F track = D2D1::RectF(trackLeft, trackY - 3.0f, trackRight, trackY + 3.0f);
    FillRR(track, 3.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f));

    float fillX = trackLeft + (trackRight - trackLeft) * std::clamp(g_discord.discordVolume, 0.0f, 1.0f);
    if (fillX > trackLeft + 2.0f) {
        FillRR(D2D1::RectF(trackLeft, trackY - 3.0f, fillX, trackY + 3.0f), 3.0f, ColorFromRef(GetActiveAccentRef(), 0.95f));
    }
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(fillX, trackY), 6.5f, 6.5f), g_pBrushText);
}