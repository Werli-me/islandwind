#pragma once
#include "Common.h"
#include "Graphics.h"
#include "WeatherTab.h"
#include "MusicTab.h"
#include "ShelfTab.h"

constexpr int CFG_W = 860;
constexpr int CFG_H = 650;
constexpr float SIDEBAR_W = 216.0f;

inline ID2D1HwndRenderTarget* g_pCfgRT = nullptr;
// 0=Остров, 1=Внешний вид, 2=Музыка, 3=Игровой режим, 4=Уведомления, 5=Вкладки, 6=Питомец, 7=Система
inline int g_cfgPage = 0;
inline int g_cfgDragSlider = -1;
inline int g_cfgDragTabRow = -1;

struct CfgHitZone {
    int id = 0;
    D2D1_RECT_F rect = {};
};
inline std::vector<CfgHitZone> g_cfgHits;

constexpr COLORREF kAccentPresets[7] = {
    RGB(114, 137, 250),
    RGB(107, 184, 255),
    RGB(80, 220, 130),
    RGB(180, 115, 255),
    RGB(255, 105, 155),
    RGB(255, 175, 65),
    RGB(240, 244, 250)
};

inline void EnsureActiveTabEnabled() {
    auto vis = GetVisibleTabs();
    if (std::find(vis.begin(), vis.end(), g_state.activeTab) == vis.end()) {
        g_state.activeTab = Tab::Music;
    }
}

inline void NotifyIslandChanged() {
    EnsureActiveTabEnabled();
    ApplyConfigBrushes();
    ApplyWindowPosition(g_hwnd);
    UpdateClickThroughState(g_hwnd);
    UpdateTargets();
    if (g_hwnd) RenderIsland(g_hwnd);
    SaveConfig();
    SaveExtraTabsData();
}

inline D2D1_RECT_F SidebarItemRect(int i) {
    float y = 78.0f + i * 38.0f;
    return D2D1::RectF(12.0f, y, SIDEBAR_W - 12.0f, y + 34.0f);
}

inline D2D1_RECT_F TabOrderRowRect(int rowIdx) {
    float CL = SIDEBAR_W + 34.0f;
    float CR = CFG_W - 34.0f;
    float y = 72.0f + rowIdx * 38.0f;
    return D2D1::RectF(CL + 12.0f, y, CR - 12.0f, y + 34.0f);
}

inline void AddHit(int id, const D2D1_RECT_F& r) {
    g_cfgHits.push_back({ id, r });
}

inline void RenderSettingsWindow(HWND hwnd) {
    if (!g_pCfgRT) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        if (FAILED(g_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &g_pCfgRT))) return;
    }

    POINT mp = { -1000, -1000 };
    if (GetCursorPos(&mp)) ScreenToClient(hwnd, &mp);

    ID2D1SolidColorBrush* br = nullptr;
    g_pCfgRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &br);
    if (!br) return;

    g_cfgHits.clear();
    g_pCfgRT->BeginDraw();
    g_pCfgRT->Clear(D2D1::ColorF(0.07f, 0.07f, 0.085f, 1.0f));

    auto fillRR = [&](const D2D1_RECT_F& r, float rad, D2D1_COLOR_F c) {
        br->SetColor(c);
        g_pCfgRT->FillRoundedRectangle(D2D1::RoundedRect(r, rad, rad), br);
        };
    auto drawRR = [&](const D2D1_RECT_F& r, float rad, D2D1_COLOR_F c, float w = 1.0f) {
        br->SetColor(c);
        g_pCfgRT->DrawRoundedRectangle(D2D1::RoundedRect(r, rad, rad), br, w);
        };
    auto drawLine = [&](float x1, float y1, float x2, float y2, D2D1_COLOR_F c) {
        br->SetColor(c);
        g_pCfgRT->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), br, 1.0f);
        };
    auto drawText = [&](const std::wstring& txt, IDWriteTextFormat* fmt, const D2D1_RECT_F& r, D2D1_COLOR_F c) {
        br->SetColor(c);
        g_pCfgRT->DrawText(txt.c_str(), (UINT32)txt.size(), fmt, r, br);
        };

    D2D1_COLOR_F cText = D2D1::ColorF(0.95f, 0.96f, 0.98f);
    D2D1_COLOR_F cMuted = D2D1::ColorF(0.56f, 0.58f, 0.64f);
    D2D1_COLOR_F cAccent = ColorFromRef(GetActiveAccentRef(), 1.0f);
    D2D1_COLOR_F cAccentSoft = ColorFromRef(GetActiveAccentRef(), 0.22f);
    D2D1_COLOR_F cCard = D2D1::ColorF(0.115f, 0.118f, 0.135f, 1.0f);
    D2D1_COLOR_F cPillBg = D2D1::ColorF(0.16f, 0.165f, 0.19f, 1.0f);
    D2D1_COLOR_F cBorder = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.07f);

    // ================= ЛЕВЫЙ САЙДБАР (8 РАЗДЕЛОВ) =================
    br->SetColor(D2D1::ColorF(0.085f, 0.088f, 0.102f, 1.0f));
    g_pCfgRT->FillRectangle(D2D1::RectF(0, 0, SIDEBAR_W, (float)CFG_H), br);
    drawLine(SIDEBAR_W, 0, SIDEBAR_W, (float)CFG_H, cBorder);

    D2D1_RECT_F logoBox = D2D1::RectF(18, 16, 56, 54);
    fillRR(logoBox, 11.0f, D2D1::ColorF(0.14f, 0.16f, 0.24f));
    drawRR(logoBox, 11.0f, cBorder);
    fillRR(D2D1::RectF(25, 20, 49, 29), 4.5f, D2D1::ColorF(0.05f, 0.05f, 0.06f));
    br->SetColor(cText);
    g_pCfgRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(31, 24.5f), 2.0f, 2.0f), br);
    br->SetColor(cAccent);
    g_pCfgRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(43, 24.5f), 2.0f, 2.0f), br);

    drawText(L"Island", g_pTextFormatLeftTitle, D2D1::RectF(66, 16, 200, 36), cText);
    drawText(L"Настройки", g_pTextFormatSmallMuted, D2D1::RectF(66, 34, 200, 52), cMuted);

    const wchar_t* navNames[8] = {
        L"\U0001F5A5   Остров",
        L"\U0001F3A8   Внешний вид",
        L"\u266B   Музыка",
        L"\U0001F3AE   Игровой режим",
        L"\U0001F514   Уведомления",
        L"\u25A6   Вкладки",
        L"\u2661   Питомец",
        L"\u2699   Система"
    };

    for (int i = 0; i < 8; ++i) {
        D2D1_RECT_F nr = SidebarItemRect(i);
        bool active = (g_cfgPage == i);
        bool hot = PtInRectF(nr, mp.x, mp.y);
        if (active || hot) {
            fillRR(nr, 8.0f, active ? D2D1::ColorF(1, 1, 1, 0.09f) : D2D1::ColorF(1, 1, 1, 0.04f));
        }
        if (active) {
            fillRR(D2D1::RectF(nr.left, nr.top + 7.0f, nr.left + 3.5f, nr.bottom - 7.0f), 1.8f, cAccent);
        }
        drawText(navNames[i], g_pTextFormatLeftSub, D2D1::RectF(nr.left + 14.0f, nr.top, nr.right, nr.bottom),
            active ? cText : cMuted);
    }

    D2D1_RECT_F proCard = D2D1::RectF(12.0f, CFG_H - 94.0f, SIDEBAR_W - 12.0f, CFG_H - 42.0f);
    fillRR(proCard, 10.0f, D2D1::ColorF(0.13f, 0.14f, 0.20f, 0.9f));
    drawRR(proCard, 10.0f, ColorFromRef(GetActiveAccentRef(), 0.35f));
    drawText(L"\u2605  Island Gaming", g_pTextFormatLeftTitle, D2D1::RectF(26.0f, proCard.top + 6.0f, 200.0f, proCard.top + 26.0f), cText);
    drawText(L"Alt + G — Игровой HUD", g_pTextFormatSmallMuted, D2D1::RectF(26.0f, proCard.top + 26.0f, 200.0f, proCard.bottom - 4.0f), cMuted);

    D2D1_RECT_F resetBtn = D2D1::RectF(12.0f, CFG_H - 36.0f, SIDEBAR_W - 12.0f, CFG_H - 8.0f);
    bool hotReset = PtInRectF(resetBtn, mp.x, mp.y);
    if (hotReset) fillRR(resetBtn, 7.0f, D2D1::ColorF(1, 1, 1, 0.06f));
    drawText(L"\u21BA   Сбросить по умолчанию", g_pTextFormatSmallMuted, D2D1::RectF(24.0f, resetBtn.top, resetBtn.right, resetBtn.bottom), hotReset ? cText : cMuted);
    AddHit(999, resetBtn);

    // ================= ПРАВАЯ ЧАСТЬ =================
    const float CL = SIDEBAR_W + 34.0f;
    const float CR = CFG_W - 34.0f;

    auto drawIconBadge = [&](float x, float y, const wchar_t* iconStr) {
        D2D1_RECT_F ib = D2D1::RectF(x, y, x + 32.0f, y + 32.0f);
        fillRR(ib, 8.0f, D2D1::ColorF(1, 1, 1, 0.05f));
        drawText(iconStr, g_pTextFormatSub, ib, cMuted);
        };

    auto drawPillChoice = [&](int hitId, float x, float y, float w, const wchar_t* label, bool selected) {
        D2D1_RECT_F pr = D2D1::RectF(x, y, x + w, y + 30.0f);
        bool hot = PtInRectF(pr, mp.x, mp.y);
        fillRR(pr, 15.0f, selected ? cAccentSoft : (hot ? D2D1::ColorF(1, 1, 1, 0.10f) : cPillBg));
        if (selected) drawRR(pr, 15.0f, cAccent, 1.5f);
        drawText(label, g_pTextFormatTime, pr, cText);
        AddHit(hitId, pr);
        };

    auto drawSwitch = [&](int hitId, float rightX, float centerY, bool on) {
        D2D1_RECT_F sw = D2D1::RectF(rightX - 44.0f, centerY - 12.0f, rightX, centerY + 12.0f);
        fillRR(sw, 12.0f, on ? cAccent : cPillBg);
        float kx = on ? (sw.right - 12.0f) : (sw.left + 12.0f);
        br->SetColor(cText);
        g_pCfgRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, centerY), 8.5f, 8.5f), br);
        AddHit(hitId, sw);
        };

    auto drawSliderControl = [&](int sliderId, float leftX, float rightX, float centerY, float norm, const std::wstring& valStr) {
        D2D1_RECT_F hitR = D2D1::RectF(leftX, centerY - 10.0f, rightX - 58.0f, centerY + 10.0f);
        D2D1_RECT_F track = D2D1::RectF(hitR.left, centerY - 2.5f, hitR.right, centerY + 2.5f);
        fillRR(track, 2.5f, cPillBg);
        float t = std::clamp(norm, 0.0f, 1.0f);
        float fx = track.left + (track.right - track.left) * t;
        if (fx > track.left + 2.0f) {
            fillRR(D2D1::RectF(track.left, centerY - 2.5f, fx, centerY + 2.5f), 2.5f, cAccent);
        }
        br->SetColor(cText);
        g_pCfgRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(fx, centerY), 8.0f, 8.0f), br);
        drawText(valStr, g_pTextFormatRightSub, D2D1::RectF(hitR.right + 6.0f, centerY - 12.0f, rightX, centerY + 12.0f), cMuted);
        AddHit(sliderId, hitR);
        };

    // ---------------- СТРАНИЦА 0: ОСТРОВ ----------------
    if (g_cfgPage == 0) {
        drawText(L"Остров", g_pTextFormatHeader, D2D1::RectF(CL, 20, CR, 52), cText);
        drawText(L"Раскрытие", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 62, CR, 80), cMuted);

        D2D1_RECT_F card1 = D2D1::RectF(CL, 86, CR, 336);
        fillRR(card1, 14.0f, cCard);
        drawRR(card1, 14.0f, cBorder);

        drawIconBadge(CL + 18, 112, L"\U0001F5B1");
        drawText(L"Раскрывать", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 100, CR - 20, 122), cText);
        drawText(L"Как открыть остров", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 120, CR - 20, 138), cMuted);
        drawPillChoice(101, CL + 64, 144, 104, L"Наведением", !g_cfg.openByClick);
        drawPillChoice(102, CL + 176, 144, 80, L"Кликом", g_cfg.openByClick);

        drawLine(CL + 18, 188, CR - 18, 188, cBorder);

        drawIconBadge(CL + 18, 202, L"\u23F1");
        drawText(L"Задержка раскрытия", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 198, CL + 270, 220), cText);
        drawText(L"Сколько держать курсор до раскрытия", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 218, CL + 300, 236), cMuted);
        drawSliderControl(500, CL + 290, CR - 18, 218, g_cfg.hoverDelayMs / 400.0f, std::to_wstring(g_cfg.hoverDelayMs) + L" мс");

        drawLine(CL + 18, 248, CR - 18, 248, cBorder);

        drawIconBadge(CL + 18, 268, L"\u25A6");
        drawText(L"При раскрытии открывать", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 256, CR - 20, 278), cText);
        drawText(L"С какой вкладки начинать", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 276, CR - 20, 294), cMuted);
        drawPillChoice(103, CL + 64, 298, 142, L"Последнюю вкладку", g_cfg.openTabMode == 0);
        drawPillChoice(104, CL + 214, 298, 184, L"Плеер, если играет музыка", g_cfg.openTabMode == 1);
        drawPillChoice(105, CL + 406, 298, 106, L"Всегда плеер", g_cfg.openTabMode == 2);

        drawText(L"Свёрнутый остров", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 348, CR, 366), cMuted);

        D2D1_RECT_F prevBox = D2D1::RectF(CL, 372, CR, 444);
        fillRR(prevBox, 14.0f, D2D1::ColorF(0.11f, 0.13f, 0.19f, 1.0f));
        drawRR(prevBox, 14.0f, cBorder);

        float pw = g_cfg.gameModeActive ? 260.0f : (g_cfg.minimalCompact ? 156.0f : 210.0f);
        float pcx = (prevBox.left + prevBox.right) * 0.5f;
        float pTop = g_cfg.dockToTop ? prevBox.top : (prevBox.top + 8.0f);
        D2D1_RECT_F pCap = D2D1::RectF(pcx - pw * 0.5f, pTop, pcx + pw * 0.5f, pTop + 30.0f);
        fillRR(pCap, 11.0f, ColorFromRef(g_cfg.bgColor, 0.98f));
        drawRR(pCap, 11.0f, cBorder);

        if (g_cfg.gameModeActive) {
            std::wstring gPrev = L"FPS " + std::to_wstring(g_game.fps.load()) +
                L"  |  CPU " + std::to_wstring(g_game.cpuPct.load()) +
                L"%  |  PING " + std::to_wstring(g_game.pingMs.load()) + L"мс";
            drawText(gPrev, g_pTextFormatTime, pCap, cAccent);
        }
        else {
            std::wstring pClock = GetClockString();
            std::wstring pTemp = g_weather.info.loaded ? FormatTemp(g_weather.info.temp) : L"14\u00B0";
            drawText(pClock, g_pTextFormatLeftTitle, D2D1::RectF(pCap.left + 12.0f, pCap.top, pCap.left + 64.0f, pCap.bottom), cText);
            drawText(L"\u2601 " + pTemp, g_pTextFormatRightTitle, D2D1::RectF(pCap.right - 66.0f, pCap.top, pCap.right - 12.0f, pCap.bottom), cText);
        }

        D2D1_RECT_F card2 = D2D1::RectF(CL, 454, CR, 582);
        fillRR(card2, 14.0f, cCard);
        drawRR(card2, 14.0f, cBorder);

        drawIconBadge(CL + 18, 472, L"\u25A3");
        drawText(L"Режим", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 460, CR - 20, 480), cText);
        drawText(L"Мини: часы и погода. Caps Lock и громкость появляются сбоку по необходимости", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 480, CR - 20, 498), cMuted);
        drawPillChoice(106, CL + 64, 502, 86, L"Обычный", !g_cfg.minimalCompact);
        drawPillChoice(107, CL + 158, 502, 108, L"Мини-остров", g_cfg.minimalCompact);

        drawLine(CL + 18, 538, CR - 18, 538, cBorder);

        drawIconBadge(CL + 18, 544, L"\u266B");
        drawText(L"Музыка в свёрнутом режиме", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 542, CR - 80, 562), cText);
        drawText(L"Обложка и название трека (или караоке строки песни)", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 560, CR - 80, 578), cMuted);
        drawSwitch(108, CR - 20, 560, g_cfg.showMusicCompact);
    }

    // ---------------- СТРАНИЦА 1: ВНЕШНИЙ ВИД ----------------
    else if (g_cfgPage == 1) {
        drawText(L"Внешний вид", g_pTextFormatHeader, D2D1::RectF(CL, 20, CR, 52), cText);
        drawText(L"Дизайн и цвета", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 62, CR, 80), cMuted);

        D2D1_RECT_F card1 = D2D1::RectF(CL, 86, CR, 266);
        fillRR(card1, 14.0f, cCard);
        drawRR(card1, 14.0f, cBorder);

        drawIconBadge(CL + 18, 106, L"\u25B2");
        drawText(L"Форма капсулы", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 96, CR - 20, 118), cText);
        drawText(L"В режиме прилипания круги битов идут из верхнего центра, в парящем — из центра", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 116, CR - 20, 134), cMuted);
        drawPillChoice(201, CL + 64, 138, 165, L"Прилипающий к верху", g_cfg.dockToTop);
        drawPillChoice(202, CL + 237, 138, 130, L"Парящий остров", !g_cfg.dockToTop);

        drawLine(CL + 18, 178, CR - 18, 178, cBorder);

        drawIconBadge(CL + 18, 198, L"\U0001F3A8");
        drawText(L"Акцентный цвет и палитра", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 186, CR - 20, 208), cText);
        for (int i = 0; i < 7; ++i) {
            D2D1_RECT_F cr = D2D1::RectF(CL + 64.0f + i * 36.0f, 214.0f, CL + 90.0f + i * 36.0f, 240.0f);
            fillRR(cr, 13.0f, ColorFromRef(kAccentPresets[i]));
            if (g_cfg.accentColor == kAccentPresets[i]) {
                drawRR(D2D1::RectF(cr.left - 2, cr.top - 2, cr.right + 2, cr.bottom + 2), 15.0f, cText, 2.0f);
            }
            AddHit(210 + i, cr);
        }
        drawPillChoice(220, CL + 324, 212, 118, L"Свой RGB...", false);

        drawText(L"Размеры и позиция на экране", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 280, CR, 298), cMuted);
        D2D1_RECT_F card2 = D2D1::RectF(CL, 304, CR, 576);
        fillRR(card2, 14.0f, cCard);
        drawRR(card2, 14.0f, cBorder);

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int halfRange = std::max(100, (screenW - static_cast<int>(g_cfg.expandedW)) / 2);

        struct SRow {
            int id;
            const wchar_t* title;
            float norm;
            std::wstring val;
        } rows[5] = {
            { 501, L"Позиция по горизонтали (или Alt + ЛКМ)", (g_cfg.offsetX + halfRange) / (2.0f * halfRange), std::to_wstring(g_cfg.offsetX) + L" px" },
            { 502, L"Ширина раскрытого острова", (g_cfg.expandedW - 360.0f) / 140.0f, std::to_wstring((int)g_cfg.expandedW) + L" px" },
            { 503, L"Ширина свёрнутого острова", (g_cfg.compactW - 114.0f) / 126.0f, std::to_wstring((int)g_cfg.compactW) + L" px" },
            { 504, L"Непрозрачность фона", (g_cfg.bgOpacity - 0.40f) / 0.60f, std::to_wstring((int)std::round(g_cfg.bgOpacity * 100)) + L"%" },
            { 505, L"Скорость анимации", (g_cfg.animSpeed - 0.08f) / 0.32f, std::to_wstring((int)std::round(g_cfg.animSpeed * 100)) + L"%" }
        };

        for (int i = 0; i < 5; ++i) {
            float ry = 330.0f + i * 48.0f;
            drawText(rows[i].title, g_pTextFormatLeftSub, D2D1::RectF(CL + 22, ry - 12, CL + 280, ry + 12), cText);
            drawSliderControl(rows[i].id, CL + 290, CR - 18, ry, rows[i].norm, rows[i].val);
            if (i < 4) drawLine(CL + 18, ry + 24, CR - 18, ry + 24, cBorder);
        }
    }

    // ---------------- СТРАНИЦА 2: МУЗЫКА ----------------
    else if (g_cfgPage == 2) {
        drawText(L"Музыка", g_pTextFormatHeader, D2D1::RectF(CL, 20, CR, 52), cText);
        drawText(L"Круги под биты и визуализация", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 62, CR, 80), cMuted);

        D2D1_RECT_F card = D2D1::RectF(CL, 86, CR, 396);
        fillRR(card, 14.0f, cCard);
        drawRR(card, 14.0f, cBorder);

        drawIconBadge(CL + 18, 112, L"\u25CE");
        drawText(L"Круги под биты музыки", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 98, CR - 20, 120), cText);
        drawText(L"Аудио-реактивные волны: из верхнего центра при прилипании и из центра в парящем режиме", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 118, CR - 20, 138), cMuted);

        drawPillChoice(310, CL + 64, 144, 82, L"Выкл", g_cfg.beatCirclesMode == 0);
        drawPillChoice(311, CL + 154, 144, 132, L"Импульсные волны", g_cfg.beatCirclesMode == 1);
        drawPillChoice(312, CL + 294, 144, 126, L"Неоновый ореол", g_cfg.beatCirclesMode == 2);
        drawPillChoice(313, CL + 428, 144, 116, L"Каскад (Строб)", g_cfg.beatCirclesMode == 3);

        drawLine(CL + 18, 190, CR - 18, 190, cBorder);

        drawIconBadge(CL + 18, 210, L"\u266B");
        drawText(L"Анимированный эквалайзер", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 202, CR - 80, 224), cText);
        drawText(L"Живые звуковые полоски рядом с текстом песни в компактном режиме", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 222, CR - 80, 242), cMuted);
        drawSwitch(301, CR - 20, 222, g_cfg.showEqualizer);

        drawLine(CL + 18, 258, CR - 18, 258, cBorder);

        drawIconBadge(CL + 18, 278, L"\u2728");
        drawText(L"Адаптивный цвет под обложку трека", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 270, CR - 80, 292), cText);
        drawText(L"Окрашивать круги битов, акцент и караоке в цвет обложки текущей песни", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 290, CR - 80, 310), cMuted);
        drawSwitch(302, CR - 20, 290, g_cfg.dynamicAlbumAccent);

        drawLine(CL + 18, 326, CR - 18, 326, cBorder);

        drawIconBadge(CL + 18, 346, L"\U0001F4BF");
        drawText(L"Отображать музыку в свёрнутом острове", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 338, CR - 80, 360), cText);
        drawText(L"Показывать мини-обложку и синхронизированный текст песни (LRC)", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 358, CR - 80, 378), cMuted);
        drawSwitch(108, CR - 20, 358, g_cfg.showMusicCompact);
    }

    // ---------------- СТРАНИЦА 3: ИГРОВОЙ РЕЖИМ ----------------
    else if (g_cfgPage == 3) {
        drawText(L"Игровой режим", g_pTextFormatHeader, D2D1::RectF(CL, 18, CR, 50), cText);
        drawText(L"Мониторинг FPS, железа и защита от случайных кликов в играх", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 58, CR, 76), cMuted);

        D2D1_RECT_F card1 = D2D1::RectF(CL, 82, CR, 318);
        fillRR(card1, 14.0f, cCard);
        drawRR(card1, 14.0f, cBorder);

        // 1. Включить Игровой HUD
        drawIconBadge(CL + 18, 100, L"\U0001F3AE");
        drawText(L"Активный Игровой режим (Alt + G)", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 92, CR - 80, 114), cText);
        drawText(L"Превращает свёрнутый остров в компактную игровую панель с датчиками", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 112, CR - 80, 132), cMuted);
        drawSwitch(801, CR - 20, 112, g_cfg.gameModeActive);

        drawLine(CL + 18, 146, CR - 18, 146, cBorder);

        // 2. Поведение при запуске полноэкранной игры
        drawIconBadge(CL + 18, 168, L"\U0001F5A5");
        drawText(L"При запуске полноэкранной игры", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 156, CR - 20, 178), cText);
        drawText(L"Как вести себя острову, когда открыта 3D-игра во весь экран", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 176, CR - 20, 194), cMuted);
        drawPillChoice(802, CL + 64, 200, 172, L"Включать Игровой HUD", g_cfg.fullscreenBehavior == 1);
        drawPillChoice(803, CL + 244, 200, 138, L"Скрывать остров", g_cfg.fullscreenBehavior == 0);
        drawPillChoice(804, CL + 390, 200, 126, L"Обычный режим", g_cfg.fullscreenBehavior == 2);

        drawLine(CL + 18, 242, CR - 18, 242, cBorder);

        // 3. Прозрачность для кликов мыши (Click-Through)
        drawIconBadge(CL + 18, 262, L"\U0001F5B1");
        drawText(L"Пропускать клики сквозь свёрнутый HUD (Click-Through)", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 254, CR - 80, 276), cText);
        drawText(L"В игре вы не заденете остров мышью. Зажмите Alt, чтобы кликнуть по острову", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 274, CR - 80, 304), cMuted);
        drawSwitch(805, CR - 20, 276, g_cfg.gameClickThrough);

        // Карточка выбора датчиков по важности
        drawText(L"Датчики на компактной панели (По важности)", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 332, CR, 350), cMuted);

        D2D1_RECT_F card2 = D2D1::RectF(CL, 356, CR, 610);
        fillRR(card2, 14.0f, cCard);
        drawRR(card2, 14.0f, cBorder);

        drawIconBadge(CL + 18, 376, L"\u26A1");
        drawText(L"Выберите показатели для вывода в Игровом HUD", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 368, CR - 20, 390), cText);
        drawText(L"Остров сам подстроит ширину и подсветит критические значения цветом", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 388, CR - 20, 408), cMuted);

        // Ряд 1 пилюль датчиков
        drawPillChoice(810, CL + 64, 420, 108, L"FPS (Кадры/с)", g_cfg.hudShowFps);
        drawPillChoice(811, CL + 180, 420, 144, L"Время кадра (мс)", g_cfg.hudShowFrameTime);
        drawPillChoice(812, CL + 332, 420, 114, L"Процессор CPU", g_cfg.hudShowCpu);

        // Ряд 2 пилюль датчиков
        drawPillChoice(813, CL + 64, 460, 122, L"Видеокарта GPU", g_cfg.hudShowGpu);
        drawPillChoice(814, CL + 194, 460, 142, L"Видеопамять VRAM", g_cfg.hudShowVram);
        drawPillChoice(815, CL + 344, 460, 116, L"Память ОЗУ %", g_cfg.hudShowRam);

        // Ряд 3 пилюль датчиков
        drawPillChoice(816, CL + 64, 500, 134, L"Сетевой Пинг (мс)", g_cfg.hudShowPing);
        drawPillChoice(817, CL + 206, 500, 136, L"Таймер сессии \u23F1", g_cfg.hudShowTimer);

        drawLine(CL + 18, 544, CR - 18, 544, cBorder);

        // Кнопка быстрой очистки ОЗУ прямо из настроек
        drawIconBadge(CL + 18, 560, L"\U0001F680");
        std::wstring ramInfo = L"Занято ОЗУ: " + std::to_wstring(g_game.ramPct.load()) + L"%  ·  Пинг: " + std::to_wstring(g_game.pingMs.load()) + L" мс";
        drawText(ramInfo, g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 554, CL + 340, 576), cText);
        drawText(L"Освободить кэш фоновых программ перед запуском игры", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 574, CL + 360, 596), cMuted);
        drawPillChoice(820, CR - 156, 562, 138, L"Очистить ОЗУ", false);
    }

    // ---------------- СТРАНИЦА 4: УВЕДОМЛЕНИЯ ----------------
    else if (g_cfgPage == 4) {
        drawText(L"Уведомления и Телефон", g_pTextFormatHeader, D2D1::RectF(CL, 20, CR, 52), cText);
        drawText(L"Интеграция через «Связь с телефоном»", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 62, CR, 80), cMuted);

        D2D1_RECT_F card = D2D1::RectF(CL, 86, CR, 230);
        fillRR(card, 14.0f, cCard);
        drawRR(card, 14.0f, cBorder);

        drawIconBadge(CL + 18, 108, L"\u23F1");
        drawText(L"Длительность всплывающего баннера", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 100, CL + 310, 122), cText);
        drawText(L"Через сколько секунд скрывать пуш", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 120, CL + 310, 140), cMuted);
        drawSliderControl(506, CL + 310, CR - 18, 120, (g_cfg.popupDurationSec - 2.0f) / 6.0f, std::to_wstring(g_cfg.popupDurationSec) + L" сек");

        drawLine(CL + 18, 158, CR - 18, 158, cBorder);

        drawIconBadge(CL + 18, 176, L"\U0001F514");
        drawText(L"Звуковой сигнал при уведомлении", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 170, CR - 80, 192), cText);
        drawText(L"Короткий системный сигнал при входящем звонке или сообщении", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 190, CR - 80, 210), cMuted);
        drawSwitch(401, CR - 20, 190, g_cfg.soundOnNotify);
    }

    // ---------------- СТРАНИЦА 5: ВКЛАДКИ ----------------
    else if (g_cfgPage == 5) {
        drawText(L"Вкладки", g_pTextFormatHeader, D2D1::RectF(CL, 14, CR, 44), cText);

        D2D1_RECT_F listCard = D2D1::RectF(CL, 62, CR, 382);
        fillRR(listCard, 14.0f, cCard);
        drawRR(listCard, 14.0f, cBorder);

        for (int i = 0; i < kCustomTabCount; ++i) {
            int tabId = g_cfg.tabOrder[i];
            Tab t = static_cast<Tab>(tabId);
            D2D1_RECT_F row = TabOrderRowRect(i);
            float cy = (row.top + row.bottom) * 0.5f;

            bool isDragged = (g_cfgDragTabRow == i);
            bool hotRow = PtInRectF(row, mp.x, mp.y);
            if (isDragged || hotRow) {
                fillRR(row, 8.0f, isDragged ? cAccentSoft : D2D1::ColorF(1, 1, 1, 0.04f));
            }

            D2D1_RECT_F handleR = D2D1::RectF(row.left + 8.0f, cy - 12.0f, row.left + 32.0f, cy + 12.0f);
            drawText(L"\u2261", g_pTextFormatTitle, handleR, cMuted);
            AddHit(700 + i, handleR);

            drawSwitch(710 + tabId, row.left + 86.0f, cy, g_cfg.tabEnabled[tabId]);

            drawText(GetTabFullName(t), g_pTextFormatLeftTitle,
                D2D1::RectF(row.left + 104.0f, row.top, row.right - 48.0f, row.bottom), cText);

            drawText(L"\u2699", g_pTextFormatSub,
                D2D1::RectF(row.right - 36.0f, cy - 12.0f, row.right - 8.0f, cy + 12.0f), cMuted);
        }

        D2D1_RECT_F rstOrder = D2D1::RectF(CL + 22.0f, 344.0f, CL + 168.0f, 374.0f);
        bool hotRst = PtInRectF(rstOrder, mp.x, mp.y);
        if (hotRst) fillRR(rstOrder, 7.0f, D2D1::ColorF(1, 1, 1, 0.07f));
        drawText(L"Сбросить порядок", g_pTextFormatLeftSub, D2D1::RectF(rstOrder.left + 6.0f, rstOrder.top, rstOrder.right, rstOrder.bottom),
            hotRst ? cText : cMuted);
        AddHit(750, rstOrder);

        drawText(L"Порядок: тяните за \u2261 или зажмите вкладку на острове. «Плеер» всегда на первом месте.",
            g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 388, CR, 412), cMuted);

        D2D1_RECT_F badgeCard = D2D1::RectF(CL, 418, CR, 492);
        fillRR(badgeCard, 14.0f, cCard);
        drawRR(badgeCard, 14.0f, cBorder);
        drawIconBadge(CL + 18, 438, L"\U0001F514");
        drawText(L"Отметки на вкладках", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 428, CR - 80, 450), cText);
        drawText(L"Красная точка или число, если что-то ждёт: уведомления, записи буфера, событие календаря",
            g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 450, CR - 80, 480), cMuted);
        drawSwitch(751, CR - 20, 454, g_cfg.showTabBadges);

        drawText(L"Буфер обмена", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 502, CR, 520), cMuted);

        D2D1_RECT_F clipCard = D2D1::RectF(CL, 526, CR, 604);
        fillRR(clipCard, 14.0f, cCard);
        drawRR(clipCard, 14.0f, cBorder);
        drawIconBadge(CL + 18, 548, L"\u21BA");
        drawText(L"Хранить историю после перезапуска", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 536, CR - 80, 558), cText);
        drawText(L"Тексты и ссылки сохраняются в папке данных между перезапусками программы",
            g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 558, CR - 80, 590), cMuted);
        drawSwitch(752, CR - 20, 564, g_cfg.persistClipboard);
    }

    // ---------------- СТРАНИЦА 6: ПИТОМЕЦ ----------------
    else if (g_cfgPage == 6) {
        drawText(L"Питомец на острове", g_pTextFormatHeader, D2D1::RectF(CL, 20, CR, 52), cText);
        drawText(L"Житель вашего динамического островка", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 62, CR, 80), cMuted);

        D2D1_RECT_F card = D2D1::RectF(CL, 86, CR, 210);
        fillRR(card, 14.0f, cCard);
        drawRR(card, 14.0f, cBorder);

        drawIconBadge(CL + 18, 114, L"\u2661");
        drawText(L"Выберите питомца", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 102, CR - 20, 124), cText);
        drawText(L"Гуляет по островку и танцует в такт, когда играет музыка", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 122, CR - 20, 142), cMuted);

        drawPillChoice(450, CL + 64, 154, 100, L"Выключен", g_cfg.petType == 0);
        drawPillChoice(451, CL + 172, 154, 110, L"Котик", g_cfg.petType == 1);
        drawPillChoice(452, CL + 290, 154, 120, L"Геккончик", g_cfg.petType == 2);
    }

    // ---------------- СТРАНИЦА 7: СИСТЕМА ----------------
    else if (g_cfgPage == 7) {
        drawText(L"Система", g_pTextFormatHeader, D2D1::RectF(CL, 20, CR, 52), cText);
        drawText(L"Системные индикаторы и поведение в Windows", g_pTextFormatSmallMuted, D2D1::RectF(CL + 2, 62, CR, 80), cMuted);

        D2D1_RECT_F card = D2D1::RectF(CL, 86, CR, 356);
        fillRR(card, 14.0f, cCard);
        drawRR(card, 14.0f, cBorder);

        drawIconBadge(CL + 18, 106, L"A");
        drawText(L"Индикатор Caps Lock", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 98, CR - 80, 120), cText);
        drawText(L"Показывать сбоку капсулы статус нажатия клавиши Caps Lock", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 118, CR - 80, 138), cMuted);
        drawSwitch(601, CR - 20, 118, g_cfg.capsLockAlert);

        drawLine(CL + 18, 152, CR - 18, 152, cBorder);

        drawIconBadge(CL + 18, 172, L"\U0001F50A");
        drawText(L"Громкость колесиком по свёрнутому острову", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 164, CR - 80, 186), cText);
        drawText(L"Менять системный звук прокруткой колеса мыши без раскрытия острова", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 184, CR - 80, 204), cMuted);
        drawSwitch(602, CR - 20, 184, g_cfg.wheelVolumeCompact);

        drawLine(CL + 18, 218, CR - 18, 218, cBorder);

        drawIconBadge(CL + 18, 238, L"%");
        drawText(L"Монитор нагрузки процессора (CPU)", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 230, CR - 80, 252), cText);
        drawText(L"Отображать процент загрузки CPU в обычном режиме капсулы", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 250, CR - 80, 270), cMuted);
        drawSwitch(603, CR - 20, 250, g_cfg.showCpuCompact);

        drawLine(CL + 18, 284, CR - 18, 284, cBorder);

        bool autoStart = IsAutoStartEnabled();
        drawIconBadge(CL + 18, 304, L"\U0001F680");
        drawText(L"Запускать вместе с Windows", g_pTextFormatLeftTitle, D2D1::RectF(CL + 64, 296, CR - 80, 318), cText);
        drawText(L"Автоматический старт программы при входе в систему", g_pTextFormatSmallMuted, D2D1::RectF(CL + 64, 316, CR - 80, 336), cMuted);
        drawSwitch(605, CR - 20, 316, autoStart);
    }

    g_pCfgRT->EndDraw();
    br->Release();
}

inline void HandleCfgSliderDrag(int sliderId, int mx) {
    for (const auto& h : g_cfgHits) {
        if (h.id == sliderId) {
            float t = std::clamp((mx - h.rect.left) / (h.rect.right - h.rect.left), 0.0f, 1.0f);
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int halfRange = std::max(100, (screenW - static_cast<int>(g_cfg.expandedW)) / 2);

            switch (sliderId) {
            case 500: g_cfg.hoverDelayMs = static_cast<int>(std::round(t * 40.0f)) * 10; break;
            case 501: g_cfg.offsetX = static_cast<int>(std::round(-halfRange + t * (2.0f * halfRange))); break;
            case 502: g_cfg.expandedW = std::round(360.0f + t * 140.0f); break;
            case 503: g_cfg.compactW = std::round(114.0f + t * 126.0f); break;
            case 504: g_cfg.bgOpacity = 0.40f + t * 0.60f; break;
            case 505: g_cfg.animSpeed = 0.08f + t * 0.32f; break;
            case 506: g_cfg.popupDurationSec = static_cast<int>(std::round(2.0f + t * 6.0f)); break;
            }
            NotifyIslandChanged();
            break;
        }
    }
}

inline LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        RenderSettingsWindow(hwnd);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);

        if (g_cfgDragSlider >= 500 && (wParam & MK_LBUTTON)) {
            HandleCfgSliderDrag(g_cfgDragSlider, mx);
        }
        else if (g_cfgDragTabRow >= 0 && (wParam & MK_LBUTTON)) {
            int targetRow = std::clamp(static_cast<int>((my - 72.0f) / 38.0f), 0, kCustomTabCount - 1);
            if (targetRow != g_cfgDragTabRow) {
                std::swap(g_cfg.tabOrder[g_cfgDragTabRow], g_cfg.tabOrder[targetRow]);
                g_cfgDragTabRow = targetRow;
                NotifyIslandChanged();
            }
        }
        RenderSettingsWindow(hwnd);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);

        for (int i = 0; i < 8; ++i) {
            if (PtInRectF(SidebarItemRect(i), mx, my)) {
                g_cfgPage = i;
                RenderSettingsWindow(hwnd);
                return 0;
            }
        }

        for (const auto& h : g_cfgHits) {
            if (PtInRectF(h.rect, mx, my)) {
                if (h.id >= 500 && h.id <= 510) {
                    g_cfgDragSlider = h.id;
                    SetCapture(hwnd);
                    HandleCfgSliderDrag(h.id, mx);
                    RenderSettingsWindow(hwnd);
                    return 0;
                }
                if (h.id >= 700 && h.id < 700 + kCustomTabCount) {
                    g_cfgDragTabRow = h.id - 700;
                    SetCapture(hwnd);
                    RenderSettingsWindow(hwnd);
                    return 0;
                }
                if (h.id >= 711 && h.id <= 710 + kCustomTabCount) {
                    int tid = h.id - 710;
                    g_cfg.tabEnabled[tid] = !g_cfg.tabEnabled[tid];
                    NotifyIslandChanged();
                    RenderSettingsWindow(hwnd);
                    return 0;
                }

                switch (h.id) {
                case 101: g_cfg.openByClick = false; break;
                case 102: g_cfg.openByClick = true; break;
                case 103: g_cfg.openTabMode = 0; break;
                case 104: g_cfg.openTabMode = 1; break;
                case 105: g_cfg.openTabMode = 2; break;
                case 106: g_cfg.minimalCompact = false; break;
                case 107: g_cfg.minimalCompact = true; break;
                case 108: g_cfg.showMusicCompact = !g_cfg.showMusicCompact; break;

                case 201: g_cfg.dockToTop = true; break;
                case 202: g_cfg.dockToTop = false; break;
                case 220: {
                    static COLORREF acrCustClr[16] = {};
                    CHOOSECOLORW cc = { sizeof(cc) };
                    cc.hwndOwner = hwnd;
                    cc.lpCustColors = acrCustClr;
                    cc.rgbResult = g_cfg.accentColor;
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColorW(&cc)) g_cfg.accentColor = cc.rgbResult;
                    break;
                }

                case 301: g_cfg.showEqualizer = !g_cfg.showEqualizer; break;
                case 302: g_cfg.dynamicAlbumAccent = !g_cfg.dynamicAlbumAccent; break;
                case 310: g_cfg.beatCirclesMode = 0; break;
                case 311: g_cfg.beatCirclesMode = 1; break;
                case 312: g_cfg.beatCirclesMode = 2; break;
                case 313: g_cfg.beatCirclesMode = 3; break;

                case 401: g_cfg.soundOnNotify = !g_cfg.soundOnNotify; break;

                case 450: g_cfg.petType = 0; break;
                case 451: g_cfg.petType = 1; break;
                case 452: g_cfg.petType = 2; break;

                case 601: g_cfg.capsLockAlert = !g_cfg.capsLockAlert; break;
                case 602: g_cfg.wheelVolumeCompact = !g_cfg.wheelVolumeCompact; break;
                case 603: g_cfg.showCpuCompact = !g_cfg.showCpuCompact; break;
                case 605: SetAutoStart(!IsAutoStartEnabled()); break;

                case 750: ResetTabOrder(); break;
                case 751: g_cfg.showTabBadges = !g_cfg.showTabBadges; break;
                case 752: g_cfg.persistClipboard = !g_cfg.persistClipboard; break;

                    // Настройки Игрового режима
                case 801:
                    g_cfg.gameModeActive = !g_cfg.gameModeActive;
                    if (g_cfg.gameModeActive) g_game.sessionStartTick = GetTickCount64();
                    break;
                case 802: g_cfg.fullscreenBehavior = 1; break;
                case 803: g_cfg.fullscreenBehavior = 0; break;
                case 804: g_cfg.fullscreenBehavior = 2; break;
                case 805: g_cfg.gameClickThrough = !g_cfg.gameClickThrough; break;
                case 810: g_cfg.hudShowFps = !g_cfg.hudShowFps; break;
                case 811: g_cfg.hudShowFrameTime = !g_cfg.hudShowFrameTime; break;
                case 812: g_cfg.hudShowCpu = !g_cfg.hudShowCpu; break;
                case 813: g_cfg.hudShowGpu = !g_cfg.hudShowGpu; break;
                case 814: g_cfg.hudShowVram = !g_cfg.hudShowVram; break;
                case 815: g_cfg.hudShowRam = !g_cfg.hudShowRam; break;
                case 816: g_cfg.hudShowPing = !g_cfg.hudShowPing; break;
                case 817: g_cfg.hudShowTimer = !g_cfg.hudShowTimer; break;
                case 820: CleanSystemRam(); break;

                case 999: g_cfg = IslandConfig{}; break;
                }

                if (h.id >= 210 && h.id < 217) {
                    g_cfg.accentColor = kAccentPresets[h.id - 210];
                }

                NotifyIslandChanged();
                RenderSettingsWindow(hwnd);
                return 0;
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_cfgDragSlider >= 0 || g_cfgDragTabRow >= 0) {
            g_cfgDragSlider = -1;
            g_cfgDragTabRow = -1;
            ReleaseCapture();
            RenderSettingsWindow(hwnd);
        }
        return 0;
    }
    case WM_CLOSE: {
        if (g_pCfgRT) {
            g_pCfgRT->Release();
            g_pCfgRT = nullptr;
        }
        DestroyWindow(hwnd);
        g_hSettingsWnd = nullptr;
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

inline void OpenSettingsWindow() {
    if (g_hSettingsWnd && IsWindow(g_hSettingsWnd)) {
        ShowWindow(g_hSettingsWnd, SW_SHOW);
        SetForegroundWindow(g_hSettingsWnd);
        return;
    }

    static bool registered = false;
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.lpszClassName = L"IslandSettingsWndClass";
        RegisterClassExW(&wc);
        registered = true;
    }

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    RECT rc = { 0, 0, CFG_W, CFG_H };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, style, FALSE);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    g_hSettingsWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"IslandSettingsWndClass",
        L"Island — Настройки",
        style | WS_VISIBLE,
        (scrW - w) / 2, (scrH - h) / 2, w, h,
        nullptr, nullptr, hInst, nullptr
    );
}