#pragma once
#include "Common.h"

ID2D1Factory* g_pD2DFactory = nullptr;
IDWriteFactory* g_pDWriteFactory = nullptr;
ID2D1DCRenderTarget* g_pRT = nullptr;

IDWriteTextFormat* g_pTextFormatTitle = nullptr;
IDWriteTextFormat* g_pTextFormatSub = nullptr;
IDWriteTextFormat* g_pTextFormatLeftTitle = nullptr;
IDWriteTextFormat* g_pTextFormatLeftSub = nullptr;
IDWriteTextFormat* g_pTextFormatTime = nullptr;
IDWriteTextFormat* g_pTextFormatLyrics = nullptr;
IDWriteTextFormat* g_pTextFormatBig = nullptr;
IDWriteTextFormat* g_pTextFormatBadge = nullptr;
IDWriteTextFormat* g_pTextFormatRightSub = nullptr;
IDWriteTextFormat* g_pTextFormatRightTitle = nullptr;
IDWriteTextFormat* g_pTextFormatHeader = nullptr;
IDWriteTextFormat* g_pTextFormatSmallMuted = nullptr;

ID2D1SolidColorBrush* g_pBrushBg = nullptr;
ID2D1SolidColorBrush* g_pBrushBorder = nullptr;
ID2D1SolidColorBrush* g_pBrushInputBg = nullptr;
ID2D1SolidColorBrush* g_pBrushText = nullptr;
ID2D1SolidColorBrush* g_pBrushTextMuted = nullptr;
ID2D1SolidColorBrush* g_pBrushAccent = nullptr;
ID2D1SolidColorBrush* g_pBrushIcon = nullptr;
ID2D1StrokeStyle* g_pDashStyle = nullptr;
ID2D1StrokeStyle* g_pRoundStrokeStyle = nullptr;

HDC g_memDC = nullptr;
HBITMAP g_hBitmap = nullptr;
HBITMAP g_hOldBitmap = nullptr;
bool g_hasScrolling = false;

enum ScrollSlot {
    SLOT_COMPACT = 0,
    SLOT_TITLE,
    SLOT_ARTIST,
    SLOT_LYRICS,
    SLOT_CITY_INPUT,
    SLOT_WEATHER,
    SLOT_TABBODY,
    SLOT_WX_COND,
    SLOT_WX_CITY,
    SLOT_WX_FEELS,
    SLOT_WX_STATUS,
    SLOT_SHELF_HOVER,
    SLOT_COUNT
};

struct ScrollSlotState {
    std::wstring text;
    ULONGLONG startTick = 0;
    ULONGLONG lastDrawTick = 0;
};

struct ScrollTiming {
    bool enabled = false;
    double elapsedSec = 0.0;
    double durationSec = 0.0;
    bool isPlaying = false;
};

inline void IconColor(float r, float g, float b, float a = 1.0f) {
    g_pBrushIcon->SetColor(D2D1::ColorF(r, g, b, a));
}

inline void ApplyConfigBrushes() {
    if (g_pBrushBg) g_pBrushBg->SetColor(ColorFromRef(g_cfg.bgColor, g_cfg.bgOpacity));
    if (g_pBrushAccent) g_pBrushAccent->SetColor(ColorFromRef(GetActiveAccentRef(), 1.0f));
}

inline void FillRR(const D2D1_RECT_F& r, float radius, const D2D1_COLOR_F& c) {
    g_pBrushIcon->SetColor(c);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), g_pBrushIcon);
}

// --- Отрисовка компактного Игрового HUD ---
inline void DrawGameModeCompactHud(float left, float top, float right, float bottom) {
    struct HudChip {
        std::wstring label;
        std::wstring val;
        D2D1_COLOR_F dotColor;
        float width;
    };
    std::vector<HudChip> chips;

    // 1. FPS + Время кадра (мс)
    if (g_cfg.hudShowFps) {
        int fps = g_game.fps.load();
        wchar_t buf[48];
        if (g_cfg.hudShowFrameTime) {
            swprintf_s(buf, L"%d (%.1fмс)", fps, g_game.frameTimeMs.load());
        }
        else {
            swprintf_s(buf, L"%d", fps);
        }
        D2D1_COLOR_F col = (fps >= 58) ? D2D1::ColorF(0.30f, 0.92f, 0.52f)
            : (fps >= 30) ? D2D1::ColorF(1.0f, 0.78f, 0.25f)
            : D2D1::ColorF(1.0f, 0.35f, 0.35f);
        chips.push_back({ L"FPS", buf, col, g_cfg.hudShowFrameTime ? 98.0f : 58.0f });
    }

    // 2. CPU %
    if (g_cfg.hudShowCpu) {
        int cpu = g_game.cpuPct.load();
        D2D1_COLOR_F col = (cpu < 70) ? D2D1::ColorF(0.42f, 0.75f, 1.0f)
            : (cpu < 90) ? D2D1::ColorF(1.0f, 0.75f, 0.25f)
            : D2D1::ColorF(1.0f, 0.35f, 0.35f);
        chips.push_back({ L"CPU", std::to_wstring(cpu) + L"%", col, 62.0f });
    }

    // 3. GPU % и VRAM
    if (g_cfg.hudShowGpu || g_cfg.hudShowVram) {
        int gpu = g_game.gpuPct.load();
        wchar_t buf[48];
        if (g_cfg.hudShowGpu && g_cfg.hudShowVram) {
            swprintf_s(buf, L"%d%% %.1fГБ", gpu, g_game.vramUsedGb.load());
        }
        else if (g_cfg.hudShowGpu) {
            swprintf_s(buf, L"%d%%", gpu);
        }
        else {
            swprintf_s(buf, L"%.1fГБ", g_game.vramUsedGb.load());
        }
        D2D1_COLOR_F col = (gpu < 80) ? D2D1::ColorF(0.72f, 0.52f, 1.0f)
            : D2D1::ColorF(1.0f, 0.55f, 0.30f);
        float cw = (g_cfg.hudShowGpu && g_cfg.hudShowVram) ? 100.0f : 62.0f;
        chips.push_back({ g_cfg.hudShowGpu ? L"GPU" : L"VRAM", buf, col, cw });
    }

    // 4. RAM
    if (g_cfg.hudShowRam) {
        int ram = g_game.ramPct.load();
        D2D1_COLOR_F col = (ram < 80) ? D2D1::ColorF(0.35f, 0.88f, 0.82f)
            : D2D1::ColorF(1.0f, 0.45f, 0.35f);
        chips.push_back({ L"RAM", std::to_wstring(ram) + L"%", col, 64.0f });
    }

    // 5. Пинг (мс)
    if (g_cfg.hudShowPing) {
        int ping = g_game.pingMs.load();
        D2D1_COLOR_F col = (ping < 55) ? D2D1::ColorF(0.30f, 0.92f, 0.52f)
            : (ping < 100) ? D2D1::ColorF(1.0f, 0.78f, 0.25f)
            : D2D1::ColorF(1.0f, 0.35f, 0.35f);
        chips.push_back({ L"PING", std::to_wstring(ping) + L"мс", col, 68.0f });
    }

    // 6. Таймер игровой сессии
    if (g_cfg.hudShowTimer) {
        chips.push_back({ L"\u23F1", GetSessionDurationString(), ColorFromRef(GetActiveAccentRef()), 56.0f });
    }

    if (chips.empty()) {
        chips.push_back({ L"GAME HUD", GetClockString(), ColorFromRef(GetActiveAccentRef()), 100.0f });
    }

    float totalW = 0.0f;
    for (const auto& c : chips) totalW += c.width;
    float curX = std::max(left + 10.0f, (left + right - totalW) * 0.5f);
    float cy = (top + bottom) * 0.5f;

    g_pRT->PushAxisAlignedClip(D2D1::RectF(left + 6.0f, top, right - 6.0f, bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (size_t i = 0; i < chips.size(); ++i) {
        const auto& c = chips[i];

        // Цветная точка статуса датчика
        g_pBrushIcon->SetColor(c.dotColor);
        g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(curX + 4.0f, cy), 2.8f, 2.8f), g_pBrushIcon);

        std::wstring fullTxt = c.label + L" " + c.val;
        D2D1_RECT_F tr = D2D1::RectF(curX + 10.0f, top, curX + c.width, bottom);
        g_pRT->DrawText(fullTxt.c_str(), (UINT32)fullTxt.size(), g_pTextFormatTime, tr, g_pBrushText);

        curX += c.width;

        // Тонкий вертикальный разделитель между датчиками
        if (i + 1 < chips.size()) {
            g_pRT->DrawLine(D2D1::Point2F(curX - 2.0f, cy - 6.5f), D2D1::Point2F(curX - 2.0f, cy + 6.5f), g_pBrushBorder, 1.0f);
        }
    }

    g_pRT->PopAxisAlignedClip();
}

// --- Физика и отрисовка кругов под биты музыки ---
inline void UpdateBeatPhysics(float rawAudioPeak, bool isPlaying) {
    if (g_cfg.beatCirclesMode == 0 || (!isPlaying && rawAudioPeak < 0.015f)) {
        g_extras.smoothLevel = Lerp(g_extras.smoothLevel, 0.0f, 0.15f);
        for (auto& r : g_extras.beatRings) r.progress += 0.04f;
        g_extras.beatRings.erase(
            std::remove_if(g_extras.beatRings.begin(), g_extras.beatRings.end(),
                [](const BeatRing& b) { return b.progress >= 1.0f; }),
            g_extras.beatRings.end());
        return;
    }

    g_hasScrolling = true;
    ULONGLONG now = GetTickCount64();

    float level = rawAudioPeak;
    if (isPlaying && level < 0.02f) {
        float t = static_cast<float>(now % 100000) / 1000.0f;
        float pulse = std::pow(std::max(0.0f, std::sin(t * 6.8f)), 4.0f);
        level = 0.25f + 0.55f * pulse;
    }

    float delta = level - g_extras.prevPeak;
    g_extras.prevPeak = level;
    g_extras.smoothLevel = Lerp(g_extras.smoothLevel, level, 0.35f);

    bool isBeat = (delta > 0.09f && level > 0.18f && (now - g_extras.lastBeatTick > 150)) ||
        (level > 0.30f && (now - g_extras.lastBeatTick > 440));

    if (isBeat && g_extras.beatRings.size() < 8) {
        g_extras.lastBeatTick = now;
        g_extras.beatRings.push_back({ 0.02f, std::clamp(level * 1.35f, 0.45f, 1.0f) });
    }

    float speed = (g_cfg.beatCirclesMode == 3) ? 0.036f : 0.024f;
    for (auto& r : g_extras.beatRings) {
        r.progress += speed * (0.85f + 0.5f * g_extras.smoothLevel);
    }
    g_extras.beatRings.erase(
        std::remove_if(g_extras.beatRings.begin(), g_extras.beatRings.end(),
            [](const BeatRing& b) { return b.progress >= 1.0f; }),
        g_extras.beatRings.end());
}

inline void DrawBeatCircles(float left, float top, float right, float bottom) {
    if (g_cfg.beatCirclesMode == 0) return;
    if (g_extras.smoothLevel < 0.01f && g_extras.beatRings.empty()) return;

    float cx = (left + right) * 0.5f;
    float cy = g_cfg.dockToTop ? top : (top + bottom) * 0.5f;

    float w = right - left;
    float h = bottom - top;
    float baseR = 6.0f;
    // Максимальный радиус ограничен размерами самой капсулы
    float maxR = g_cfg.dockToTop
        ? std::max(w * 0.48f, h * 0.92f)
        : std::max(w * 0.46f, h * 0.75f);

    COLORREF acc = GetActiveAccentRef();

    if (g_cfg.beatCirclesMode == 1 || g_cfg.beatCirclesMode == 3) {
        for (const auto& ring : g_extras.beatRings) {
            float rad = Lerp(baseR, maxR, ring.progress);
            float fade = (1.0f - ring.progress) * (1.0f - ring.progress) * ring.strength;
            float alpha = fade * 0.55f;

            g_pBrushIcon->SetColor(ColorFromRef(acc, alpha));
            float strokeW = (g_cfg.beatCirclesMode == 3 ? 2.4f : 1.8f) * (1.0f - 0.35f * ring.progress);
            g_pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), g_pBrushIcon, strokeW);

            if (g_cfg.beatCirclesMode == 3 && rad > 12.0f) {
                g_pBrushIcon->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.45f));
                g_pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad * 0.75f, rad * 0.75f), g_pBrushIcon, 1.1f);
            }
        }
    }

    if (g_cfg.beatCirclesMode == 2 || g_cfg.beatCirclesMode == 3) {
        float pulse = g_extras.smoothLevel;
        for (int i = 1; i <= 3; ++i) {
            float rad = baseR + (maxR - baseR) * (0.18f * i + 0.30f * pulse);
            float alpha = (0.38f - i * 0.09f) * std::min(1.0f, pulse * 1.5f);
            if (alpha <= 0.01f) continue;

            g_pBrushIcon->SetColor(ColorFromRef(acc, alpha));
            g_pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), g_pBrushIcon, 2.0f);
            if (i == 1) {
                g_pBrushIcon->SetColor(ColorFromRef(acc, alpha * 0.22f));
                g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), g_pBrushIcon);
            }
        }
    }
}

inline ID2D1PathGeometry* BuildDockedGeometry(float left, float top, float right, float bottom, float radius, bool closedTop) {
    float r = std::min({ radius, (right - left) * 0.4f, (bottom - top) * 0.65f });
    float earR = 6.0f;

    ID2D1PathGeometry* pGeo = nullptr;
    if (FAILED(g_pD2DFactory->CreatePathGeometry(&pGeo))) return nullptr;
    ID2D1GeometrySink* pSink = nullptr;
    if (SUCCEEDED(pGeo->Open(&pSink))) {
        pSink->BeginFigure(D2D1::Point2F(left - earR, top),
            closedTop ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);

        pSink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(left, top + earR),
            D2D1::SizeF(earR, earR), 0.0f,
            D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

        pSink->AddLine(D2D1::Point2F(left, bottom - r));
        pSink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(left + r, bottom),
            D2D1::SizeF(r, r), 0.0f,
            D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

        pSink->AddLine(D2D1::Point2F(right - r, bottom));
        pSink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(right, bottom - r),
            D2D1::SizeF(r, r), 0.0f,
            D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

        pSink->AddLine(D2D1::Point2F(right, top + earR));
        pSink->AddArc(D2D1::ArcSegment(
            D2D1::Point2F(right + earR, top),
            D2D1::SizeF(earR, earR), 0.0f,
            D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));

        pSink->EndFigure(closedTop ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
        pSink->Close();
        pSink->Release();
    }
    return pGeo;
}

inline void DrawIslandCapsule(float left, float top, float right, float bottom, float radius) {
    ApplyConfigBrushes();

    // 1. Внешнее неоновое свечение (Ambient Glow) вокруг капсулы
    if (g_cfg.fxAmbientGlow) {
        float glowPower = (g_extras.smoothLevel > 0.02f) ? (0.20f + g_extras.smoothLevel * 0.48f) : 0.12f;
        COLORREF glowRef = GetActiveAccentRef();
        for (int layer = 3; layer >= 1; --layer) {
            float expand = layer * 2.6f + g_extras.smoothLevel * 3.8f;
            float alpha = (glowPower / layer) * 0.40f;
            float gTop = g_cfg.dockToTop ? top : (top - expand * 0.6f);
            D2D1_RECT_F gr = D2D1::RectF(left - expand, gTop, right + expand, bottom + expand * 0.8f);
            FillRR(gr, radius + expand, ColorFromRef(glowRef, alpha));
        }
    }

    // Вспомогательная лямбда для создания градиентной кисти 3D-стекла
    auto createGlassRimBrush = [&]() -> ID2D1LinearGradientBrush* {
        if (!g_cfg.fxGlassRim) return nullptr;
        D2D1_GRADIENT_STOP stops[3] = {
            { 0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.36f) },
            { 0.45f, ColorFromRef(GetActiveAccentRef(), 0.18f) },
            { 1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.04f) }
        };
        ID2D1GradientStopCollection* pStops = nullptr;
        ID2D1LinearGradientBrush* pRim = nullptr;
        if (SUCCEEDED(g_pRT->CreateGradientStopCollection(stops, 3, &pStops))) {
            D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(left, top), D2D1::Point2F(left, bottom));
            g_pRT->CreateLinearGradientBrush(props, pStops, &pRim);
            pStops->Release();
        }
        return pRim;
        };

    if (!g_cfg.dockToTop) {
        D2D1_ROUNDED_RECT capsule = D2D1::RoundedRect(D2D1::RectF(left, top, right, bottom), radius, radius);
        g_pRT->FillRoundedRectangle(capsule, g_pBrushBg);

        if (g_cfg.beatCirclesMode != 0 && (g_extras.smoothLevel >= 0.01f || !g_extras.beatRings.empty())) {
            ID2D1RoundedRectangleGeometry* pRoundGeo = nullptr;
            ID2D1Layer* pLayer = nullptr;
            if (SUCCEEDED(g_pD2DFactory->CreateRoundedRectangleGeometry(capsule, &pRoundGeo)) &&
                SUCCEEDED(g_pRT->CreateLayer(&pLayer))) {
                g_pRT->PushLayer(D2D1::LayerParameters(D2D1::RectF(left, top, right, bottom), pRoundGeo), pLayer);
                DrawBeatCircles(left, top, right, bottom);
                g_pRT->PopLayer();
            }
            if (pLayer) pLayer->Release();
            if (pRoundGeo) pRoundGeo->Release();
        }

        if (ID2D1LinearGradientBrush* pRim = createGlassRimBrush()) {
            g_pRT->DrawRoundedRectangle(capsule, pRim, 1.3f);
            pRim->Release();
        }
        else {
            g_pRT->DrawRoundedRectangle(capsule, g_pBrushBorder, 1.0f);
        }
        return;
    }

    ID2D1PathGeometry* pFillGeo = BuildDockedGeometry(left, top, right, bottom, radius, true);
    ID2D1PathGeometry* pBorderGeo = BuildDockedGeometry(left, top, right, bottom, radius, false);

    if (pFillGeo) {
        g_pRT->FillGeometry(pFillGeo, g_pBrushBg);

        if (g_cfg.beatCirclesMode != 0 && (g_extras.smoothLevel >= 0.01f || !g_extras.beatRings.empty())) {
            ID2D1Layer* pLayer = nullptr;
            if (SUCCEEDED(g_pRT->CreateLayer(&pLayer))) {
                g_pRT->PushLayer(D2D1::LayerParameters(D2D1::RectF(left, top, right, bottom), pFillGeo), pLayer);
                DrawBeatCircles(left, top, right, bottom);
                g_pRT->PopLayer();
                pLayer->Release();
            }
        }
        pFillGeo->Release();
    }

    if (pBorderGeo) {
        if (ID2D1LinearGradientBrush* pRim = createGlassRimBrush()) {
            g_pRT->DrawGeometry(pBorderGeo, pRim, 1.3f);
            pRim->Release();
        }
        else {
            g_pRT->DrawGeometry(pBorderGeo, g_pBrushBorder, 1.0f);
        }
        pBorderGeo->Release();
    }
}

inline void DrawMiniEqualizer(float rightEdge, float centerY, bool isPlaying) {
    if (!g_cfg.showEqualizer) return;
    float t = static_cast<float>(GetTickCount64()) / 140.0f;
    for (int i = 0; i < 4; ++i) {
        float h = isPlaying
            ? (2.5f + 4.2f * std::fabs(std::sin(t + i * 1.15f)))
            : 2.0f;
        float x = (rightEdge - 4.0f) - (4 - i) * 4.2f;
        D2D1_RECT_F bar = D2D1::RectF(x, centerY - h, x + 2.2f, centerY + h);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(bar, 1.1f, 1.1f), g_pBrushAccent);
    }
    if (isPlaying) g_hasScrolling = true;
}

inline void DrawIslandPet(float left, float top, float right, float bottom, bool isPlaying) {
    if (g_cfg.petType == 0) return;
    g_hasScrolling = true;

    float minX = left + 22.0f;
    float maxX = right - 22.0f;
    if (maxX <= minX) return;

    float speed = isPlaying ? 0.55f : 0.22f;
    g_extras.petX += g_extras.petDir * speed;
    g_extras.petPhase += isPlaying ? 0.22f : 0.08f;

    if (g_extras.petX < minX) { g_extras.petX = minX; g_extras.petDir = 1.0f; }
    if (g_extras.petX > maxX) { g_extras.petX = maxX; g_extras.petDir = -1.0f; }

    float px = g_extras.petX;
    float bounce = isPlaying ? std::fabs(std::sin(g_extras.petPhase)) * 2.2f : 0.0f;
    float py = bottom - 5.0f - bounce;

    D2D1_COLOR_F cPet = ColorFromRef(GetActiveAccentRef(), 0.95f);
    g_pBrushIcon->SetColor(cPet);

    if (g_cfg.petType == 1) {
        D2D1_RECT_F body = D2D1::RectF(px - 6.0f, py - 5.0f, px + 6.0f, py + 2.0f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(body, 3.0f, 3.0f), g_pBrushIcon);

        float headX = px + g_extras.petDir * 5.0f;
        g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX, py - 4.5f), 4.2f, 3.8f), g_pBrushIcon);
        g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX - 2.4f, py - 8.2f), 1.5f, 2.2f), g_pBrushIcon);
        g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + 2.4f, py - 8.2f), 1.5f, 2.2f), g_pBrushIcon);

        float tailWave = std::sin(g_extras.petPhase * 1.5f) * 3.0f;
        g_pRT->DrawLine(
            D2D1::Point2F(px - g_extras.petDir * 5.5f, py - 2.0f),
            D2D1::Point2F(px - g_extras.petDir * 10.5f, py - 7.0f + tailWave),
            g_pBrushIcon, 1.8f);
    }
    else if (g_cfg.petType == 2) {
        D2D1_COLOR_F cGecko = D2D1::ColorF(0.35f, 0.90f, 0.55f, 0.95f);
        g_pBrushIcon->SetColor(cGecko);

        D2D1_RECT_F body = D2D1::RectF(px - 7.0f, py - 3.5f, px + 6.0f, py + 1.5f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(body, 2.5f, 2.5f), g_pBrushIcon);

        float headX = px + g_extras.petDir * 6.5f;
        g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX, py - 1.5f), 3.8f, 3.0f), g_pBrushIcon);

        float tailWave = std::sin(g_extras.petPhase * 1.8f) * 2.5f;
        g_pRT->DrawLine(
            D2D1::Point2F(px - g_extras.petDir * 6.5f, py - 1.0f),
            D2D1::Point2F(px - g_extras.petDir * 13.0f, py - 2.0f + tailWave),
            g_pBrushIcon, 2.0f);

        g_pBrushIcon->SetColor(D2D1::ColorF(0.08f, 0.08f, 0.10f, 1.0f));
        g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(headX + g_extras.petDir * 1.4f, py - 2.2f), 1.0f, 1.0f), g_pBrushIcon);
    }
}

inline void DrawRoundedBitmap(ID2D1Bitmap* pBitmap, const D2D1_RECT_F& rect, float radius) {
    if (!pBitmap) return;
    D2D1_SIZE_F bmpSize = pBitmap->GetSize();
    if (bmpSize.width <= 0 || bmpSize.height <= 0) return;

    ID2D1BitmapBrush* pBmpBrush = nullptr;
    if (SUCCEEDED(g_pRT->CreateBitmapBrush(pBitmap, &pBmpBrush))) {
        float scaleX = (rect.right - rect.left) / bmpSize.width;
        float scaleY = (rect.bottom - rect.top) / bmpSize.height;
        pBmpBrush->SetTransform(
            D2D1::Matrix3x2F::Scale(scaleX, scaleY) *
            D2D1::Matrix3x2F::Translation(rect.left, rect.top)
        );
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(rect, radius, radius), pBmpBrush);
        pBmpBrush->Release();
    }
}

inline void DrawClippedText(const std::wstring& text, IDWriteTextFormat* fmt,
    const D2D1_RECT_F& rect, ID2D1Brush* brush) {
    if (text.empty()) return;
    const float w = rect.right - rect.left;
    const float h = rect.bottom - rect.top;
    IDWriteTextLayout* layout = nullptr;
    if (FAILED(g_pDWriteFactory->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), fmt, w, h, &layout)))
        return;
    layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    g_pRT->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_pRT->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), layout, brush);
    g_pRT->PopAxisAlignedClip();
    layout->Release();
}

inline void DrawScrollingText(int slot, const std::wstring& text, IDWriteTextFormat* textFormat,
    const D2D1_RECT_F& rect, ID2D1Brush* brush,
    float speedPxPerSec = 35.0f, bool stickToEnd = false,
    const ScrollTiming* timing = nullptr) {
    if (text.empty()) return;

    const float boxWidth = rect.right - rect.left;
    const float boxHeight = rect.bottom - rect.top;

    IDWriteTextLayout* pLayout = nullptr;
    if (FAILED(g_pDWriteFactory->CreateTextLayout(
        text.c_str(), (UINT32)text.size(), textFormat, 10000.0f, boxHeight, &pLayout))) {
        return;
    }
    pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    DWRITE_TEXT_METRICS metrics;
    pLayout->GetMetrics(&metrics);
    const float textWidth = metrics.width;

    static ScrollSlotState slots[SLOT_COUNT];
    ScrollSlotState& st = slots[slot];
    ULONGLONG now = GetTickCount64();
    if (st.text != text || now - st.lastDrawTick > 500) {
        st.text = text;
        st.startTick = now;
    }
    st.lastDrawTick = now;

    if (textWidth <= boxWidth) {
        pLayout->SetMaxWidth(boxWidth);
        pLayout->SetMaxHeight(boxHeight);
        g_pRT->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), pLayout, brush);
    }
    else {
        const float overflow = textWidth - boxWidth;
        float offsetX = 0.0f;

        if (stickToEnd) {
            offsetX = overflow;
        }
        else if (timing && timing->enabled) {
            if (timing->isPlaying) g_hasScrolling = true;

            const double window = std::clamp(timing->durationSec, 0.3, LYRIC_MAX_SCROLL_WINDOW);
            double pauseIn = std::clamp(window * 0.15, 0.2, 1.0);
            double pauseOut = std::clamp(window * 0.10, 0.15, 0.6);
            double scrollTime = window - pauseIn - pauseOut;
            if (scrollTime < 0.2) {
                pauseIn = pauseOut = window * 0.1;
                scrollTime = window * 0.8;
            }
            scrollTime = std::min(scrollTime, static_cast<double>(overflow) / LYRIC_MIN_SCROLL_SPEED);

            const double progress = std::clamp((timing->elapsedSec - pauseIn) / scrollTime, 0.0, 1.0);
            offsetX = static_cast<float>(overflow * progress);
        }
        else {
            g_hasScrolling = true;

            const float pause = 1.0f;
            const float scrollTime = overflow / speedPxPerSec;
            const float cycle = scrollTime + 2.0f * pause;
            const float elapsed = static_cast<float>(now - st.startTick) / 1000.0f;
            const float t = std::fmod(elapsed, cycle);

            if (t >= pause + scrollTime) offsetX = overflow;
            else if (t > pause) offsetX = (t - pause) * speedPxPerSec;
        }

        pLayout->SetMaxWidth(textWidth + 2.0f);
        pLayout->SetMaxHeight(boxHeight);

        g_pRT->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        g_pRT->DrawTextLayout(D2D1::Point2F(rect.left - offsetX, rect.top), pLayout, brush);
        g_pRT->PopAxisAlignedClip();
    }

    pLayout->Release();
}

inline bool InitGraphics() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory)))
        return false;

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&g_pDWriteFactory))))
        return false;

    HDC screenDC = GetDC(nullptr);
    g_memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = CANVAS_W;
    bmi.bmiHeader.biHeight = -CANVAS_H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    g_hBitmap = CreateDIBSection(g_memDC, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    g_hOldBitmap = (HBITMAP)SelectObject(g_memDC, g_hBitmap);
    ReleaseDC(nullptr, screenDC);

    D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0, 0, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
    );

    if (FAILED(g_pD2DFactory->CreateDCRenderTarget(&props, &g_pRT)))
        return false;

    RECT rc = { 0, 0, CANVAS_W, CANVAS_H };
    g_pRT->BindDC(g_memDC, &rc);
    g_pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.14f, 0.15f, 0.98f), &g_pBrushBg);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f), &g_pBrushBorder);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.09f, 0.10f, 0.95f), &g_pBrushInputBg);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.0f), &g_pBrushText);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.65f, 0.66f, 0.70f, 1.0f), &g_pBrushTextMuted);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.45f, 0.54f, 0.98f, 1.0f), &g_pBrushAccent);
    g_pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &g_pBrushIcon);
    ApplyConfigBrushes();

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"ru-ru", &g_pTextFormatTitle);
    g_pTextFormatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_pTextFormatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ru-ru", &g_pTextFormatSub);
    g_pTextFormatSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_pTextFormatSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.8f, L"ru-ru", &g_pTextFormatLeftTitle);
    g_pTextFormatLeftTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pTextFormatLeftTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatLeftTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ru-ru", &g_pTextFormatLeftSub);
    g_pTextFormatLeftSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pTextFormatLeftSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatLeftSub->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"ru-ru", &g_pTextFormatTime);
    g_pTextFormatTime->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_pTextFormatTime->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_MEDIUM,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ru-ru", &g_pTextFormatLyrics);
    g_pTextFormatLyrics->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pTextFormatLyrics->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatLyrics->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_LIGHT,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 34.0f, L"ru-ru", &g_pTextFormatBig);
    g_pTextFormatBig->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pTextFormatBig->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatBig->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.0f, L"ru-ru", &g_pTextFormatBadge);
    g_pTextFormatBadge->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    g_pTextFormatBadge->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatBadge->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"ru-ru", &g_pTextFormatRightSub);
    g_pTextFormatRightSub->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    g_pTextFormatRightSub->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatRightSub->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"ru-ru", &g_pTextFormatRightTitle);
    g_pTextFormatRightTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    g_pTextFormatRightTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    g_pTextFormatRightTitle->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.0f, L"ru-ru", &g_pTextFormatHeader);
    g_pTextFormatHeader->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pTextFormatHeader->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    g_pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.2f, L"ru-ru", &g_pTextFormatSmallMuted);
    g_pTextFormatSmallMuted->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    g_pTextFormatSmallMuted->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    D2D1_STROKE_STYLE_PROPERTIES dashProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT,
        D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_DASH, 0.0f);
    g_pD2DFactory->CreateStrokeStyle(dashProps, nullptr, 0, &g_pDashStyle);

    D2D1_STROKE_STYLE_PROPERTIES roundProps = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f);
    g_pD2DFactory->CreateStrokeStyle(roundProps, nullptr, 0, &g_pRoundStrokeStyle);

    return true;
}

inline void CleanupGraphics() {
    if (g_pBrushAccent) g_pBrushAccent->Release();
    if (g_pBrushTextMuted) g_pBrushTextMuted->Release();
    if (g_pBrushText) g_pBrushText->Release();
    if (g_pBrushInputBg) g_pBrushInputBg->Release();
    if (g_pBrushBorder) g_pBrushBorder->Release();
    if (g_pBrushBg) g_pBrushBg->Release();
    if (g_pBrushIcon) g_pBrushIcon->Release();

    if (g_pTextFormatSmallMuted) g_pTextFormatSmallMuted->Release();
    if (g_pTextFormatHeader) g_pTextFormatHeader->Release();
    if (g_pTextFormatLyrics) g_pTextFormatLyrics->Release();
    if (g_pTextFormatTime) g_pTextFormatTime->Release();
    if (g_pTextFormatLeftSub) g_pTextFormatLeftSub->Release();
    if (g_pTextFormatLeftTitle) g_pTextFormatLeftTitle->Release();
    if (g_pTextFormatSub) g_pTextFormatSub->Release();
    if (g_pTextFormatTitle) g_pTextFormatTitle->Release();
    if (g_pTextFormatRightTitle) g_pTextFormatRightTitle->Release();
    if (g_pTextFormatRightSub) g_pTextFormatRightSub->Release();
    if (g_pTextFormatBadge) g_pTextFormatBadge->Release();
    if (g_pTextFormatBig) g_pTextFormatBig->Release();

    if (g_pRoundStrokeStyle) g_pRoundStrokeStyle->Release();
    if (g_pDashStyle) g_pDashStyle->Release();

    if (g_pRT) g_pRT->Release();
    if (g_pDWriteFactory) g_pDWriteFactory->Release();
    if (g_pD2DFactory) g_pD2DFactory->Release();

    if (g_memDC) {
        SelectObject(g_memDC, g_hOldBitmap);
        DeleteObject(g_hBitmap);
        DeleteDC(g_memDC);
    }
}