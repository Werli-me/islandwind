#pragma once
#include "Common.h"
#include "Graphics.h"
#include "HttpUtils.h"

struct LyricLine {
    double timeSec = 0.0;
    std::wstring text;
};

struct MusicData {
#if USE_SMTC
    std::wstring title = L"Нет активного трека";
    std::wstring artist = L"Запустите музыку в браузере или плеере";
#else
    std::wstring title = L"Управление медиа и звуком";
    std::wstring artist = L"Включите /std:c++17 для названия и обложки";
#endif
    bool isPlaying = false;
    float volume = 0.5f;
    double positionSec = 0.0;
    ULONGLONG positionTick = 0;
    double durationSec = 0.0;
    std::vector<LyricLine> lyrics;
} g_music;

struct MusicResult {
    std::wstring title;
    std::wstring artist;
    bool isPlaying = false;
    double positionSec = 0.0;
    double durationSec = 0.0;
    bool clearCover = false;
    bool hasNewCover = false;
    UINT coverW = 0;
    UINT coverH = 0;
    std::vector<uint8_t> coverPixels;
    bool hasLyrics = false;
    std::vector<LyricLine> lyrics;
};

std::atomic<bool> g_forceMusicPoll{ false };
std::atomic<bool> g_hasSeekRequest{ false };
std::atomic<double> g_seekTargetSeconds{ 0.0 };

ID2D1Bitmap* g_pAlbumBitmap = nullptr;
IAudioEndpointVolume* g_pEndpointVolume = nullptr;
IAudioMeterInformation* g_pAudioMeter = nullptr;
int g_lastLyricIdx = -1;

// --- Геометрия вкладки Музыки ---
inline D2D1_RECT_F GetCoverRect() {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 20.0f, top + 44.0f, left + 108.0f, top + 132.0f);
}

inline D2D1_RECT_F GetTimelineSliderRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 156.0f, top + 84.0f, right - 54.0f, top + 98.0f);
}

inline D2D1_RECT_F GetPrevBtnRect() {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 120.0f, top + 102.0f, left + 156.0f, top + 130.0f);
}

inline D2D1_RECT_F GetPlayBtnRect() {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 162.0f, top + 102.0f, left + 204.0f, top + 130.0f);
}

inline D2D1_RECT_F GetNextBtnRect() {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 210.0f, top + 102.0f, left + 246.0f, top + 130.0f);
}

inline D2D1_RECT_F GetVolumeSliderRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 260.0f, top + 102.0f, right - 54.0f, top + 130.0f);
}

// --- Звук и медиа-клавиши ---
inline void InitAudio() {
    IMMDeviceEnumerator* pEnumerator = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator))) {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))) {
            pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER,
                nullptr, (void**)&g_pEndpointVolume);
            pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_INPROC_SERVER,
                nullptr, (void**)&g_pAudioMeter);
            pDevice->Release();
        }
        pEnumerator->Release();
    }
    if (g_pEndpointVolume) {
        float vol = 0.5f;
        if (SUCCEEDED(g_pEndpointVolume->GetMasterVolumeLevelScalar(&vol))) {
            g_music.volume = vol;
        }
    }
}

inline void CleanupMusic() {
    if (g_pAudioMeter) g_pAudioMeter->Release();
    if (g_pEndpointVolume) g_pEndpointVolume->Release();
    if (g_pAlbumBitmap) g_pAlbumBitmap->Release();
}

inline float GetAudioPeakLevel() {
    if (!g_pAudioMeter) return 0.0f;
    float peak = 0.0f;
    if (SUCCEEDED(g_pAudioMeter->GetPeakValue(&peak))) {
        return std::clamp(peak, 0.0f, 1.0f);
    }
    return 0.0f;
}

inline float GetSystemVolume() {
    if (!g_pEndpointVolume) return g_music.volume;
    float vol = g_music.volume;
    g_pEndpointVolume->GetMasterVolumeLevelScalar(&vol);
    return vol;
}

inline void SetSystemVolume(float vol) {
    vol = std::clamp(vol, 0.0f, 1.0f);
    g_music.volume = vol;
    if (g_pEndpointVolume) {
        g_pEndpointVolume->SetMasterVolumeLevelScalar(vol, nullptr);
    }
}

inline void SendMediaKey(WORD vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
    g_forceMusicPoll = true;
}

inline void UpdateVolumeFromMouse(HWND hwnd, int mouseX) {
    D2D1_RECT_F sr = GetVolumeSliderRect();
    if (sr.right > sr.left) {
        float ratio = (mouseX - sr.left) / (sr.right - sr.left);
        SetSystemVolume(ratio);
        RenderIsland(hwnd);
    }
}

inline void UpdateTimelineFromMouse(HWND hwnd, int mouseX) {
    if (g_music.durationSec <= 0.1) return;
    D2D1_RECT_F tr = GetTimelineSliderRect();
    if (tr.right > tr.left) {
        double ratio = std::clamp(static_cast<double>(mouseX - tr.left) / (tr.right - tr.left), 0.0, 1.0);
        g_music.positionSec = ratio * g_music.durationSec;
        g_music.positionTick = GetTickCount64();
        RenderIsland(hwnd);
    }
}

// --- Парсинг LRC и воркер SMTC ---
inline std::vector<LyricLine> ParseLrc(const std::string& lrcString) {
    std::vector<LyricLine> result;
    std::istringstream stream(lrcString);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.size() > 10 && line[0] == '[' && line[3] == ':' && line[6] == '.') {
            try {
                int mins = std::stoi(line.substr(1, 2));
                int secs = std::stoi(line.substr(4, 2));
                int millis = std::stoi(line.substr(7, 2));
                double totalSec = mins * 60.0 + secs + millis / 100.0;

                size_t bracketEnd = line.find(']');
                if (bracketEnd != std::string::npos && bracketEnd + 1 < line.size()) {
                    std::string textPart = line.substr(bracketEnd + 1);
                    while (!textPart.empty() && (textPart.front() == ' ' || textPart.front() == '\r')) textPart.erase(textPart.begin());
                    while (!textPart.empty() && (textPart.back() == ' ' || textPart.back() == '\r')) textPart.pop_back();

                    if (!textPart.empty()) {
                        result.push_back({ totalSec, Utf8ToWide(textPart) });
                    }
                }
            }
            catch (...) {}
        }
    }
    return result;
}

inline std::string ExtractSyncedLyricsFromJson(const std::string& json) {
    std::string key = "\"syncedLyrics\":\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos += key.size();

    std::string lyrics;
    bool escaped = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) {
            if (c == 'n') lyrics.push_back('\n');
            else if (c == 't') lyrics.push_back('\t');
            else if (c == 'r') {}
            else { lyrics.push_back(c); }
            escaped = false;
        }
        else if (c == '\\') {
            escaped = true;
        }
        else if (c == '"') {
            break;
        }
        else {
            lyrics.push_back(c);
        }
    }
    return lyrics;
}

inline uint64_t HashBytes(const uint8_t* data, size_t size) {
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline void StartMusicWorker(HWND hwnd) {
#if USE_SMTC
    std::thread([hwnd]() {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        IWICImagingFactory* pWicFactory = nullptr;
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&pWicFactory));

        std::wstring lastTitle;
        std::wstring lastArtist;
        bool lastPlaying = false;
        int lastPosInt = -1;
        int lastDurInt = -1;
        uint64_t lastCoverHash = 0;
        int coverRecheckTicks = 0;

        while (g_running) {
            try {
                using namespace winrt::Windows::Media::Control;
                using namespace winrt::Windows::Storage::Streams;

                auto manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                auto session = manager ? manager.GetCurrentSession() : nullptr;

                if (session) {
                    auto timeline = session.GetTimelineProperties();

                    if (g_hasSeekRequest.exchange(false)) {
                        double targetSec = g_seekTargetSeconds.load();
                        int64_t startTicks = timeline.StartTime().count();
                        int64_t targetTicks = startTicks + static_cast<int64_t>(targetSec * 10000000.0);
                        session.TryChangePlaybackPositionAsync(targetTicks).get();
                        timeline = session.GetTimelineProperties();
                    }

                    auto info = session.GetPlaybackInfo();
                    bool isPlaying = (info.PlaybackStatus() ==
                        GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);

                    double durSec = std::chrono::duration<double>(timeline.EndTime() - timeline.StartTime()).count();
                    double posSec = std::chrono::duration<double>(timeline.Position() - timeline.StartTime()).count();
                    if (durSec < 0.0) durSec = 0.0;

                    auto lastUpdated = timeline.LastUpdatedTime();
                    if (isPlaying && durSec > 0.0 && lastUpdated.time_since_epoch().count() > 0) {
                        auto now = winrt::clock::now();
                        if (now > lastUpdated) {
                            double elapsed = std::chrono::duration<double>(now - lastUpdated).count();
                            if (elapsed >= 0.0 && elapsed < 3600.0) {
                                posSec += elapsed;
                            }
                        }
                    }
                    posSec = (durSec > 0.0) ? std::clamp(posSec, 0.0, durSec) : 0.0;

                    auto props = session.TryGetMediaPropertiesAsync().get();
                    std::wstring title = props.Title().c_str();
                    std::wstring artist = props.Artist().c_str();

                    if (title.empty()) title = L"Неизвестный трек";
                    if (artist.empty()) artist = L"Активное медиа";

                    bool trackChanged = (title != lastTitle || artist != lastArtist);
                    if (trackChanged) {
                        coverRecheckTicks = 12;
                    }

                    bool stateChanged = (isPlaying != lastPlaying);
                    bool timeChanged = (static_cast<int>(posSec) != lastPosInt ||
                        static_cast<int>(durSec) != lastDurInt);
                    bool shouldCheckCover = trackChanged || (coverRecheckTicks > 0) || (lastCoverHash == 0);

                    bool coverUpdated = false;
                    bool coverCleared = false;
                    UINT decodedW = 0, decodedH = 0;
                    std::vector<uint8_t> decodedPixels;

                    if (shouldCheckCover && pWicFactory) {
                        if (coverRecheckTicks > 0) --coverRecheckTicks;

                        auto thumbRef = props.Thumbnail();
                        if (thumbRef) {
                            auto stream = thumbRef.OpenReadAsync().get();
                            uint32_t size = static_cast<uint32_t>(stream.Size());
                            if (size > 0 && size < 8 * 1024 * 1024) {
                                DataReader reader(stream);
                                reader.LoadAsync(size).get();
                                std::vector<uint8_t> rawBytes(size);
                                reader.ReadBytes(rawBytes);

                                uint64_t newHash = HashBytes(rawBytes.data(), rawBytes.size());
                                if (newHash != lastCoverHash) {
                                    IWICStream* pWicStream = nullptr;
                                    if (SUCCEEDED(pWicFactory->CreateStream(&pWicStream))) {
                                        if (SUCCEEDED(pWicStream->InitializeFromMemory(rawBytes.data(), (DWORD)rawBytes.size()))) {
                                            IWICBitmapDecoder* pDecoder = nullptr;
                                            if (SUCCEEDED(pWicFactory->CreateDecoderFromStream(pWicStream, nullptr, WICDecodeMetadataCacheOnDemand, &pDecoder))) {
                                                IWICBitmapFrameDecode* pFrame = nullptr;
                                                if (SUCCEEDED(pDecoder->GetFrame(0, &pFrame))) {
                                                    IWICFormatConverter* pConverter = nullptr;
                                                    if (SUCCEEDED(pWicFactory->CreateFormatConverter(&pConverter))) {
                                                        if (SUCCEEDED(pConverter->Initialize(
                                                            pFrame,
                                                            GUID_WICPixelFormat32bppPBGRA,
                                                            WICBitmapDitherTypeNone,
                                                            nullptr, 0.0f,
                                                            WICBitmapPaletteTypeCustom))) {
                                                            UINT w = 0, h = 0;
                                                            pConverter->GetSize(&w, &h);
                                                            if (w > 0 && h > 0) {
                                                                decodedW = w;
                                                                decodedH = h;
                                                                decodedPixels.resize(w * h * 4);
                                                                pConverter->CopyPixels(nullptr, w * 4, (UINT)decodedPixels.size(), decodedPixels.data());
                                                                coverUpdated = true;
                                                                lastCoverHash = newHash;
                                                            }
                                                        }
                                                        pConverter->Release();
                                                    }
                                                    pFrame->Release();
                                                }
                                                pDecoder->Release();
                                            }
                                        }
                                        pWicStream->Release();
                                    }
                                }
                            }
                        }
                        else if (lastCoverHash != 0) {
                            lastCoverHash = 0;
                            coverCleared = true;
                        }
                    }

                    bool lyricsFetched = false;
                    std::vector<LyricLine> newLyrics;
                    if (trackChanged) {
                        std::wstring lrcPath = L"/api/get?artist_name=" + UrlEncodeWide(artist) + L"&track_name=" + UrlEncodeWide(title);
                        std::string jsonResp = HttpGet(L"lrclib.net", lrcPath);
                        std::string syncedStr = ExtractSyncedLyricsFromJson(jsonResp);
                        if (!syncedStr.empty()) {
                            newLyrics = ParseLrc(syncedStr);
                        }
                        lyricsFetched = true;
                    }

                    if (trackChanged || stateChanged || timeChanged || coverUpdated || coverCleared || lyricsFetched) {
                        auto* res = new MusicResult();
                        res->title = title;
                        res->artist = artist;
                        res->isPlaying = isPlaying;
                        res->positionSec = posSec;
                        res->durationSec = durSec;
                        res->clearCover = coverCleared;
                        res->hasNewCover = coverUpdated;
                        if (coverUpdated) {
                            res->coverW = decodedW;
                            res->coverH = decodedH;
                            res->coverPixels = std::move(decodedPixels);
                        }
                        res->hasLyrics = lyricsFetched;
                        res->lyrics = std::move(newLyrics);

                        lastTitle = title;
                        lastArtist = artist;
                        lastPlaying = isPlaying;
                        lastPosInt = static_cast<int>(posSec);
                        lastDurInt = static_cast<int>(durSec);

                        if (!PostMessage(hwnd, WM_MUSIC_UPDATED, 0, reinterpret_cast<LPARAM>(res))) {
                            delete res;
                        }
                    }
                }
                else if (!lastTitle.empty()) {
                    lastTitle.clear();
                    lastArtist.clear();
                    lastPlaying = false;
                    lastPosInt = -1;
                    lastDurInt = -1;
                    lastCoverHash = 0;
                    coverRecheckTicks = 0;

                    auto* res = new MusicResult();
                    res->title = L"Нет активного трека";
                    res->artist = L"Запустите музыку в браузере или плеере";
                    res->isPlaying = false;
                    res->positionSec = 0.0;
                    res->durationSec = 0.0;
                    res->clearCover = true;
                    res->hasLyrics = true;
                    if (!PostMessage(hwnd, WM_MUSIC_UPDATED, 0, reinterpret_cast<LPARAM>(res))) {
                        delete res;
                    }
                }
            }
            catch (...) {
            }

            for (int i = 0; i < 5 && g_running; ++i) {
                if (g_forceMusicPoll.exchange(false)) break;
                Sleep(50);
            }
        }

        if (pWicFactory) pWicFactory->Release();
        }).detach();
#else
    (void)hwnd;
#endif
}

// --- Живая интерполяция таймлайна и пословное караоке со свечением ---
inline double GetLivePosition() {
    double pos = g_music.positionSec;
    if (g_music.isPlaying && !g_state.isDraggingTimeline && g_music.positionTick != 0) {
        pos += static_cast<double>(GetTickCount64() - g_music.positionTick) / 1000.0;
    }
    if (g_music.durationSec > 0.0) pos = std::clamp(pos, 0.0, g_music.durationSec);
    return pos < 0.0 ? 0.0 : pos;
}

inline int GetLyricIndex(double lyricPos) {
    int idx = -1;
    for (size_t i = 0; i < g_music.lyrics.size(); ++i) {
        if (lyricPos >= g_music.lyrics[i].timeSec) idx = static_cast<int>(i);
        else break;
    }
    return idx;
}

struct LyricView {
    bool valid = false;
    int index = -1;
    std::wstring text;
    double elapsed = 0.0;
    double duration = 0.0;
};

inline LyricView GetCurrentLyric(double livePos) {
    LyricView v;
    const double lyricPos = livePos + LYRIC_LOOKAHEAD_SEC;
    v.index = GetLyricIndex(lyricPos);
    if (v.index < 0) return v;

    const size_t i = static_cast<size_t>(v.index);
    const double start = g_music.lyrics[i].timeSec;
    double end;
    if (i + 1 < g_music.lyrics.size()) end = g_music.lyrics[i + 1].timeSec;
    else if (g_music.durationSec > start) end = g_music.durationSec;
    else end = start + 5.0;

    v.valid = true;
    v.text = g_music.lyrics[i].text;
    v.elapsed = std::max(0.0, lyricPos - start);
    v.duration = std::max(0.3, end - start);
    return v;
}

struct LyricWordSpan {
    UINT32 start = 0;
    UINT32 length = 0;
    double weight = 1.0;
};

inline void DrawHighlightedLyric(int slot, const LyricView& lyric, IDWriteTextFormat* textFormat, const D2D1_RECT_F& rect) {
    if (!lyric.valid || lyric.text.empty()) return;

    const float boxWidth = rect.right - rect.left;
    const float boxHeight = rect.bottom - rect.top;
    if (boxWidth <= 2.0f || boxHeight <= 2.0f) return;

    std::vector<LyricWordSpan> words;
    UINT32 n = static_cast<UINT32>(lyric.text.size());
    UINT32 i = 0;
    while (i < n) {
        while (i < n && iswspace(lyric.text[i])) ++i;
        if (i >= n) break;
        UINT32 start = i;
        while (i < n && !iswspace(lyric.text[i])) ++i;
        UINT32 len = i - start;
        words.push_back({ start, len, static_cast<double>(len) + 1.8 });
    }

    if (words.empty()) {
        DrawScrollingText(slot, lyric.text, textFormat, rect, g_pBrushAccent);
        return;
    }

    double totalWeight = 0.0;
    for (const auto& w : words) totalWeight += w.weight;

    double vocalSpan = std::clamp(lyric.duration * 0.85, 0.35, std::max(1.2, words.size() * 0.62));
    double progress = std::clamp(lyric.elapsed / vocalSpan, 0.0, 0.9999);
    double targetWeight = progress * totalWeight;

    size_t activeIdx = words.size() - 1;
    double accum = 0.0;
    for (size_t k = 0; k < words.size(); ++k) {
        if (targetWeight <= accum + words[k].weight) {
            activeIdx = k;
            break;
        }
        accum += words[k].weight;
    }

    IDWriteTextLayout* pLayout = nullptr;
    if (FAILED(g_pDWriteFactory->CreateTextLayout(
        lyric.text.c_str(), n, textFormat, 10000.0f, boxHeight, &pLayout))) {
        return;
    }
    pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    const LyricWordSpan& activeWord = words[activeIdx];
    DWRITE_TEXT_RANGE activeRange = { activeWord.start, activeWord.length };

    // Активное слово выделяется жирным шрифтом и ярким акцентным цветом
    pLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, activeRange);
    pLayout->SetDrawingEffect(g_pBrushAccent, activeRange);

    DWRITE_TEXT_METRICS metrics = {};
    pLayout->GetMetrics(&metrics);
    const float textWidth = metrics.width;

    // Плавное центрирование строки за счет смещения макета (без использования хит-тестов)
    static int s_lastLineIdx[2] = { -1, -1 };
    static float s_curScroll[2] = { 0.0f, 0.0f };
    int slotIdx = (slot == SLOT_COMPACT) ? 0 : 1;

    float targetScroll = 0.0f;
    if (textWidth > boxWidth) {
        // Приблизительное смещение пропорционально прогрессу песни в строке
        float approxWordPos = textWidth * static_cast<float>(activeIdx + 0.5) / static_cast<float>(words.size());
        float anchor = boxWidth * 0.5f;
        targetScroll = std::clamp(approxWordPos - anchor, 0.0f, textWidth - boxWidth);
    }

    if (s_lastLineIdx[slotIdx] != lyric.index) {
        s_lastLineIdx[slotIdx] = lyric.index;
        s_curScroll[slotIdx] = targetScroll;
    }
    else {
        s_curScroll[slotIdx] = Lerp(s_curScroll[slotIdx], targetScroll, 0.20f);
    }

    if (g_music.isPlaying) {
        g_hasScrolling = true;
    }

    float drawX = rect.left - s_curScroll[slotIdx];
    pLayout->SetMaxWidth(textWidth + 16.0f);
    pLayout->SetMaxHeight(boxHeight);

    g_pRT->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Неоновое свечение шрифта: рисуем мягкие размытые тени-дубликаты позади текста
    IDWriteTextLayout* pGlowLayout = nullptr;
    if (SUCCEEDED(g_pDWriteFactory->CreateTextLayout(
        lyric.text.c_str(), n, textFormat, 10000.0f, boxHeight, &pGlowLayout))) {

        pGlowLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        pGlowLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, activeRange);

        ID2D1SolidColorBrush* pGlowBrush = nullptr;
        g_pRT->CreateSolidColorBrush(D2D1::ColorF(0.28f, 0.65f, 1.0f, 0.28f), &pGlowBrush);
        if (pGlowBrush) {
            pGlowLayout->SetDrawingEffect(pGlowBrush, activeRange);

            // Смещенные копии создают красивый ореол свечения вокруг букв активного слова
            g_pRT->DrawTextLayout(D2D1::Point2F(drawX - 1.2f, rect.top), pGlowLayout, pGlowBrush);
            g_pRT->DrawTextLayout(D2D1::Point2F(drawX + 1.2f, rect.top), pGlowLayout, pGlowBrush);
            g_pRT->DrawTextLayout(D2D1::Point2F(drawX, rect.top - 1.2f), pGlowLayout, pGlowBrush);
            g_pRT->DrawTextLayout(D2D1::Point2F(drawX, rect.top + 1.2f), pGlowLayout, pGlowBrush);

            pGlowBrush->Release();
        }
        pGlowLayout->Release();
    }

    // Основной текст строки поверх свечения
    g_pRT->DrawTextLayout(D2D1::Point2F(drawX, rect.top), pLayout, g_pBrushText);

    g_pRT->PopAxisAlignedClip();
    pLayout->Release();
}

// --- Векторные кнопки плеера и отрисовка вкладки Музыки ---
inline void DrawRoundedTriangle(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_POINT_2F p3, ID2D1Brush* brush) {
    ID2D1PathGeometry* pGeo = nullptr;
    if (SUCCEEDED(g_pD2DFactory->CreatePathGeometry(&pGeo))) {
        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pGeo->Open(&pSink))) {
            pSink->BeginFigure(p1, D2D1_FIGURE_BEGIN_FILLED);
            pSink->AddLine(p2);
            pSink->AddLine(p3);
            pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
            pSink->Close();
            pSink->Release();
            g_pRT->FillGeometry(pGeo, brush);
            if (g_pRoundStrokeStyle) {
                g_pRT->DrawGeometry(pGeo, brush, 1.6f, g_pRoundStrokeStyle);
            }
        }
        pGeo->Release();
    }
}

inline void DrawMediaTransportButtons(const D2D1_RECT_F& prevRect, const D2D1_RECT_F& playRect,
    const D2D1_RECT_F& nextRect, const POINT& mp) {
    // 1. Prev
        {
            bool hot = PtInRectF(prevRect, mp.x, mp.y);
            float rad = (prevRect.bottom - prevRect.top) * 0.42f;
            D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(prevRect, rad, rad);

            g_pBrushIcon->SetColor(hot ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f) : D2D1::ColorF(0.09f, 0.09f, 0.10f, 0.95f));
            g_pRT->FillRoundedRectangle(rr, g_pBrushIcon);
            g_pRT->DrawRoundedRectangle(rr, hot ? g_pBrushAccent : g_pBrushBorder, 1.0f);

            float cx = (prevRect.left + prevRect.right) * 0.5f;
            float cy = (prevRect.top + prevRect.bottom) * 0.5f;
            ID2D1Brush* fg = hot ? g_pBrushAccent : g_pBrushText;

            D2D1_RECT_F bar = D2D1::RectF(cx - 5.5f, cy - 5.0f, cx - 3.5f, cy + 5.0f);
            g_pRT->FillRoundedRectangle(D2D1::RoundedRect(bar, 1.0f, 1.0f), fg);
            DrawRoundedTriangle(
                D2D1::Point2F(cx + 4.8f, cy - 4.5f),
                D2D1::Point2F(cx - 2.0f, cy),
                D2D1::Point2F(cx + 4.8f, cy + 4.5f),
                fg
            );
        }

        // 2. Play / Pause
        {
            bool hot = PtInRectF(playRect, mp.x, mp.y);
            float rad = (playRect.bottom - playRect.top) * 0.5f;
            D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(playRect, rad, rad);

            if (g_music.isPlaying) {
                g_pBrushIcon->SetColor(D2D1::ColorF(0.42f, 0.72f, 1.0f, hot ? 0.26f : 0.16f));
                g_pRT->FillRoundedRectangle(rr, g_pBrushIcon);
                g_pRT->DrawRoundedRectangle(rr, g_pBrushAccent, 1.3f);
            }
            else {
                g_pBrushIcon->SetColor(hot ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f) : D2D1::ColorF(0.09f, 0.09f, 0.10f, 0.95f));
                g_pRT->FillRoundedRectangle(rr, g_pBrushIcon);
                g_pRT->DrawRoundedRectangle(rr, hot ? g_pBrushAccent : g_pBrushBorder, 1.1f);
            }

            float cx = (playRect.left + playRect.right) * 0.5f;
            float cy = (playRect.top + playRect.bottom) * 0.5f;
            ID2D1Brush* fg = (g_music.isPlaying || hot) ? g_pBrushAccent : g_pBrushText;

            if (g_music.isPlaying) {
                D2D1_RECT_F bar1 = D2D1::RectF(cx - 4.6f, cy - 5.6f, cx - 1.6f, cy + 5.6f);
                D2D1_RECT_F bar2 = D2D1::RectF(cx + 1.6f, cy - 5.6f, cx + 4.6f, cy + 5.6f);
                g_pRT->FillRoundedRectangle(D2D1::RoundedRect(bar1, 1.5f, 1.5f), fg);
                g_pRT->FillRoundedRectangle(D2D1::RoundedRect(bar2, 1.5f, 1.5f), fg);
            }
            else {
                DrawRoundedTriangle(
                    D2D1::Point2F(cx - 3.4f, cy - 5.2f),
                    D2D1::Point2F(cx + 5.6f, cy),
                    D2D1::Point2F(cx - 3.4f, cy + 5.2f),
                    fg
                );
            }
        }

        // 3. Next
        {
            bool hot = PtInRectF(nextRect, mp.x, mp.y);
            float rad = (nextRect.bottom - nextRect.top) * 0.42f;
            D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(nextRect, rad, rad);

            g_pBrushIcon->SetColor(hot ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.14f) : D2D1::ColorF(0.09f, 0.09f, 0.10f, 0.95f));
            g_pRT->FillRoundedRectangle(rr, g_pBrushIcon);
            g_pRT->DrawRoundedRectangle(rr, hot ? g_pBrushAccent : g_pBrushBorder, 1.0f);

            float cx = (nextRect.left + nextRect.right) * 0.5f;
            float cy = (nextRect.top + nextRect.bottom) * 0.5f;
            ID2D1Brush* fg = hot ? g_pBrushAccent : g_pBrushText;

            DrawRoundedTriangle(
                D2D1::Point2F(cx - 4.8f, cy - 4.5f),
                D2D1::Point2F(cx + 2.0f, cy),
                D2D1::Point2F(cx - 4.8f, cy + 4.5f),
                fg
            );
            D2D1_RECT_F bar = D2D1::RectF(cx + 3.5f, cy - 5.0f, cx + 5.5f, cy + 5.0f);
            g_pRT->FillRoundedRectangle(D2D1::RoundedRect(bar, 1.0f, 1.0f), fg);
        }
}

inline void DrawMusicTab(float left, float top, float right, double livePos, const LyricView& curLyric, const POINT& mp) {
    D2D1_RECT_F coverRect = GetCoverRect();
    D2D1_ROUNDED_RECT coverRound = D2D1::RoundedRect(coverRect, 14.0f, 14.0f);
    g_pRT->FillRoundedRectangle(coverRound, g_pBrushInputBg);

    if (g_pAlbumBitmap) {
        DrawRoundedBitmap(g_pAlbumBitmap, coverRect, 14.0f);
    }
    else {
        const wchar_t* noteIcon = L"\u266B";
        g_pRT->DrawText(noteIcon, 1, g_pTextFormatTitle, coverRect, g_pBrushAccent);
    }
    g_pRT->DrawRoundedRectangle(coverRound, g_pBrushBorder, 1.0f);

    D2D1_RECT_F titleRect = D2D1::RectF(left + 120.0f, top + 42.0f, right - 20.0f, top + 60.0f);
    D2D1_RECT_F artistRect = D2D1::RectF(left + 120.0f, top + 58.0f, right - 20.0f, top + 72.0f);

    DrawScrollingText(SLOT_TITLE, g_music.title, g_pTextFormatLeftTitle, titleRect, g_pBrushText);
    DrawScrollingText(SLOT_ARTIST, g_music.artist, g_pTextFormatLeftSub, artistRect, g_pBrushTextMuted);

    D2D1_RECT_F lyricsRect = D2D1::RectF(left + 120.0f, top + 70.0f, right - 20.0f, top + 84.0f);
    if (curLyric.valid) {
        DrawHighlightedLyric(SLOT_LYRICS, curLyric, g_pTextFormatLyrics, lyricsRect);
    }
    else {
        std::wstring fallback = g_music.lyrics.empty() ? L"Текст песни не найден" : L"\u266A ...";
        DrawScrollingText(SLOT_LYRICS, fallback, g_pTextFormatLyrics, lyricsRect, g_pBrushTextMuted);
    }

    D2D1_RECT_F tlRect = GetTimelineSliderRect();
    float tlY = (tlRect.top + tlRect.bottom) * 0.5f;

    std::wstring curTimeStr = FormatTime(livePos);
    std::wstring durTimeStr = FormatTime(g_music.durationSec);

    D2D1_RECT_F curTimeRect = D2D1::RectF(left + 116.0f, tlRect.top, tlRect.left - 3.0f, tlRect.bottom);
    D2D1_RECT_F durTimeRect = D2D1::RectF(tlRect.right + 3.0f, tlRect.top, right - 14.0f, tlRect.bottom);

    g_pRT->DrawText(curTimeStr.c_str(), (UINT32)curTimeStr.size(), g_pTextFormatTime, curTimeRect, g_pBrushTextMuted);
    g_pRT->DrawText(durTimeStr.c_str(), (UINT32)durTimeStr.size(), g_pTextFormatTime, durTimeRect, g_pBrushTextMuted);

    D2D1_RECT_F tlTrackBg = D2D1::RectF(tlRect.left, tlY - 2.5f, tlRect.right, tlY + 2.5f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(tlTrackBg, 2.5f, 2.5f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(tlTrackBg, 2.5f, 2.5f), g_pBrushBorder, 1.0f);

    float progress = (g_music.durationSec > 0.1)
        ? static_cast<float>(std::clamp(livePos / g_music.durationSec, 0.0, 1.0))
        : 0.0f;
    float tlFillRight = tlRect.left + (tlRect.right - tlRect.left) * progress;

    if (tlFillRight > tlRect.left + 1.5f) {
        D2D1_RECT_F tlTrackFill = D2D1::RectF(tlRect.left, tlY - 2.5f, tlFillRight, tlY + 2.5f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(tlTrackFill, 2.5f, 2.5f), g_pBrushAccent);
    }

    float thumbRadius = g_state.isDraggingTimeline ? 6.0f : 4.5f;
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(tlFillRight, tlY), thumbRadius, thumbRadius), g_pBrushText);

    DrawMediaTransportButtons(GetPrevBtnRect(), GetPlayBtnRect(), GetNextBtnRect(), mp);

    D2D1_RECT_F sliderHit = GetVolumeSliderRect();
    float trackY = (sliderHit.top + sliderHit.bottom) * 0.5f;
    D2D1_RECT_F trackBg = D2D1::RectF(sliderHit.left, trackY - 3.0f, sliderHit.right, trackY + 3.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(trackBg, 3.0f, 3.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(trackBg, 3.0f, 3.0f), g_pBrushBorder, 1.0f);

    float fillRight = sliderHit.left + (sliderHit.right - sliderHit.left) * g_music.volume;
    if (fillRight > sliderHit.left + 2.0f) {
        D2D1_RECT_F trackFill = D2D1::RectF(sliderHit.left, trackY - 3.0f, fillRight, trackY + 3.0f);
        g_pRT->FillRoundedRectangle(D2D1::RoundedRect(trackFill, 3.0f, 3.0f), g_pBrushAccent);
    }

    D2D1_ELLIPSE thumb = D2D1::Ellipse(D2D1::Point2F(fillRight, trackY), 6.0f, 6.0f);
    g_pRT->FillEllipse(thumb, g_pBrushText);

    int volPct = static_cast<int>(std::round(g_music.volume * 100.0f));
    std::wstring volStr = std::to_wstring(volPct) + L"%";
    D2D1_RECT_F volTextRect = D2D1::RectF(sliderHit.right + 4.0f, sliderHit.top, right - 14.0f, sliderHit.bottom);
    g_pRT->DrawText(volStr.c_str(), (UINT32)volStr.size(), g_pTextFormatTime, volTextRect, g_pBrushTextMuted);
}