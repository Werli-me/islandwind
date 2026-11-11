#include "Common.h"
#include "Graphics.h"
#include "HttpUtils.h"
#include "WeatherTab.h"
#include "MusicTab.h"
#include "ShelfTab.h"
#include "PhoneListener.h"
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
        g_phoneEvent.hasActiveEvent || g_state.isDraggingWindow || g_state.isDraggingTabOnIsland;

    float left = IslandLeft();
    float top = ISLAND_TOP;
    float right = left + g_state.curW;
    float bottom = top + g_state.curH;

    DrawIslandCapsule(left, top, right, bottom, g_state.curR);

    const bool hasMiniBadge = (GetTickCount64() < g_extras.miniBadgeUntil && !g_extras.miniBadgeText.empty());

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

        // 1. Приоритет: если активен Игровой режим — рисуем компактный Игровой HUD
        if (g_cfg.gameModeActive) {
            float hudW = CalcGameHudCompactWidth() + (hasMiniBadge ? 62.0f : 0.0f);
            if (std::fabs(g_state.targetW - hudW) > 0.5f) {
                g_state.targetW = hudW;
                g_state.isAnimating = true;
            }
            DrawGameModeCompactHud(left, top, hasMiniBadge ? rightContentLimit : right, compactBottom);
        }
        // 2. Иначе если играет музыка
        else if (g_music.isPlaying && g_cfg.showMusicCompact) {
            g_state.targetW = std::max(185.0f, g_cfg.compactW) + (hasMiniBadge ? 62.0f : 0.0f);

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

            float eqReserve = (g_cfg.showEqualizer && !hasMiniBadge) ? 22.0f : 0.0f;
            if (g_cfg.showEqualizer && !hasMiniBadge) {
                DrawMiniEqualizer(right - 8.0f, cy, true);
            }

            D2D1_RECT_F textRect = D2D1::RectF(textLeft, top, rightContentLimit - eqReserve, compactBottom);
            if (curLyric.valid) {
                DrawHighlightedLyric(SLOT_COMPACT, curLyric, g_pTextFormatLyrics, textRect);
            }
            else {
                DrawScrollingText(SLOT_COMPACT, g_music.title, g_pTextFormatLeftSub, textRect, g_pBrushText);
            }
        }
        // 3. Мини-остров (12:34 ☁ 14°)
        else if (g_cfg.minimalCompact) {
            std::wstring clockStr = GetClockString();
            std::wstring degStr = g_weather.info.loaded ? FormatTemp(g_weather.info.temp) : L"...";

            float newTargetW = std::clamp(g_cfg.compactW, 114.0f, 185.0f) + (hasMiniBadge ? 62.0f : 0.0f);
            if (std::fabs(g_state.targetW - newTargetW) > 0.5f) {
                g_state.targetW = newTargetW;
                g_state.isAnimating = true;
            }

            D2D1_RECT_F timeRect = D2D1::RectF(left + 12.0f, top, left + 58.0f, compactBottom);
            g_pRT->DrawText(clockStr.c_str(), (UINT32)clockStr.size(), g_pTextFormatLeftTitle, timeRect, g_pBrushText);

            if (!hasMiniBadge) {
                if (g_weather.info.loaded) {
                    DrawWeatherIcon(WxKindFromCode(g_weather.info.code), g_weather.info.isDay,
                        right - 38.0f, cy, 14.0f);
                }
                D2D1_RECT_F tempRect = D2D1::RectF(right - 29.0f, top, right - 8.0f, compactBottom);
                g_pRT->DrawText(degStr.c_str(), (UINT32)degStr.size(), g_pTextFormatLeftTitle, tempRect, g_pBrushText);
            }
        }
        // 4. Обычный компактный вид
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

            float newTargetW = std::clamp(contentWidth, g_cfg.compactW, 275.0f) + (hasMiniBadge ? 62.0f : 0.0f);
            if (std::fabs(g_state.targetW - newTargetW) > 0.5f) {
                g_state.targetW = newTargetW;
                g_state.isAnimating = true;
            }

            if (g_weather.info.loaded && !hasMiniBadge) {
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

        for (int i = 0; i < visCount; ++i) {
            Tab t = visTabs[i];
            D2D1_RECT_F tabRect = GetTabRect(i, visCount);
            bool isActive = (g_state.activeTab == t);

            if (isActive) {
                D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(tabRect, 8.0f, 8.0f);
                g_pRT->FillRoundedRectangle(pill, g_pBrushBorder);
            }

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
                else if (t == Tab::Game && g_cfg.gameModeActive) {
                    D2D1_RECT_F dot = D2D1::RectF(tabRect.right - 8.0f, tabRect.top + 4.0f, tabRect.right - 2.0f, tabRect.top + 10.0f);
                    FillRR(dot, 3.0f, D2D1::ColorF(0.30f, 0.92f, 0.52f, 1.0f));
                }
                else if (t == Tab::Calendar && !g_calDayNote.empty()) {
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
                float left = IslandLeft();
                float top = ISLAND_TOP;
                float right = left + g_state.curW;
                D2D1_RECT_F leftCard = D2D1::RectF(left + 14.0f, top + 44.0f, left + 168.0f, top + 224.0f);
                D2D1_RECT_F bPrev = D2D1::RectF(leftCard.left + 10.0f, leftCard.top + 86.0f, leftCard.left + 38.0f, leftCard.top + 110.0f);
                D2D1_RECT_F bToday = D2D1::RectF(leftCard.left + 42.0f, leftCard.top + 86.0f, leftCard.right - 42.0f, leftCard.top + 110.0f);
                D2D1_RECT_F bNext = D2D1::RectF(leftCard.right - 38.0f, leftCard.top + 86.0f, leftCard.right - 10.0f, leftCard.top + 110.0f);
                D2D1_RECT_F noteBox = D2D1::RectF(leftCard.left + 10.0f, leftCard.top + 118.0f, leftCard.right - 10.0f, leftCard.bottom - 10.0f);

                if (PtInRectF(bPrev, mouseX, mouseY)) {
                    if (--g_calMonth < 1) { g_calMonth = 12; --g_calYear; }
                    g_calSelectedDay = std::min(g_calSelectedDay, DaysInMonth(g_calYear, g_calMonth));
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(bNext, mouseX, mouseY)) {
                    if (++g_calMonth > 12) { g_calMonth = 1; ++g_calYear; }
                    g_calSelectedDay = std::min(g_calSelectedDay, DaysInMonth(g_calYear, g_calMonth));
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(bToday, mouseX, mouseY)) {
                    SYSTEMTIME st; GetLocalTime(&st);
                    g_calYear = st.wYear; g_calMonth = st.wMonth; g_calSelectedDay = st.wDay;
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(noteBox, mouseX, mouseY)) {
                    g_state.isTypingCalNote = true;
                    SetWindowInputFocus(hwnd, true);
                    RenderIsland(hwnd);
                    return 0;
                }

                float gx0 = left + 178.0f;
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
                        SetWindowInputFocus(hwnd, false);
                        RenderIsland(hwnd);
                        return 0;
                    }
                }
            }
            else if (g_state.activeTab == Tab::Notes) {
                if (PtInRectF(GetNotesCopyBtnRect(), mouseX, mouseY)) {
                    CopyWideToClipboard(hwnd, g_notesText);
                    TriggerMiniBadge(L"Скопировано", true, 1400);
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetNotesClearBtnRect(), mouseX, mouseY)) {
                    g_notesText.clear();
                    SaveExtraTabsData();
                    RenderIsland(hwnd);
                    return 0;
                }
                if (PtInRectF(GetNotesEditorRect(), mouseX, mouseY)) {
                    g_state.isTypingNote = true;
                    SetWindowInputFocus(hwnd, true);
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
            else if (g_state.activeTab == Tab::Clipboard) {
                g_clipScroll += (delta > 0) ? -1 : 1;
                ClampClipScroll();
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
            else if (wParam == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                std::wstring clip = ReadClipboardText(hwnd);
                if (g_state.isTypingNote) g_notesText += clip;
                else g_calDayNote += clip;
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
            if (ch == L'\b') {
                if (!g_notesText.empty()) g_notesText.pop_back();
            }
            else if (ch == L'\r' || ch == L'\n') {
                if (g_notesText.size() < 600) g_notesText.push_back(L'\n');
            }
            else if (ch >= 32 && ch != 127 && g_notesText.size() < 600) {
                g_notesText.push_back(ch);
            }
            SaveExtraTabsData();
            RenderIsland(hwnd);
        }
        else if (g_state.isTypingCalNote) {
            if (ch == L'\b') {
                if (!g_calDayNote.empty()) g_calDayNote.pop_back();
            }
            else if (ch == L'\r') {
                SetWindowInputFocus(hwnd, false);
            }
            else if (ch >= 32 && ch != 127 && g_calDayNote.size() < 42) {
                g_calDayNote.push_back(ch);
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

            // Реакция на запуск полноэкранной игры (0 = Скрывать, 1 = Авто-Игровой HUD, 2 = Обычный)
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

            if (!g_state.isDraggingVolume && !g_state.isDraggingTimeline &&
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

            bool wasAnimating = g_state.isAnimating;
            const bool lyricChanged =
                (GetLyricIndex(GetLivePosition() + LYRIC_LOOKAHEAD_SEC) != g_lastLyricIdx);
            StepAnimation();

            bool needMusicTick = g_music.isPlaying &&
                ((g_state.curH > 80.0f && g_state.activeTab == Tab::Music) ||
                    (g_state.curH < 80.0f && (!g_music.lyrics.empty() || g_cfg.showEqualizer)) ||
                    g_cfg.beatCirclesMode != 0);

            if (wasAnimating || lyricChanged || g_hasScrolling || needMusicTick || needGameTick ||
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

    // Регистрируем глобальную горячую клавишу Alt + G для Игрового режима
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