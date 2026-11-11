#pragma once
#include "Common.h"
#include "Graphics.h"
#include "HttpUtils.h"

struct HourForecast {
    std::wstring label;
    int temp = 0;
    int code = 0;
    bool isDay = true;
};

struct DayForecast {
    std::wstring label;
    int tMax = 0;
    int tMin = 0;
    int code = 0;
};

struct WeatherInfo {
    bool loaded = false;
    std::wstring city;
    std::wstring region;
    int temp = 0;
    int feels = 0;
    int code = 0;
    int humidity = 0;
    bool isDay = true;
    double windMs = 0.0;
    int tMax = 0;
    int tMin = 0;
    std::vector<HourForecast> hours;
    std::vector<DayForecast> days;
};

struct WeatherData {
    std::wstring city = L"Пермь";
    std::wstring inputBuffer = L"Пермь";
    std::wstring tempCompact = L"...\u00B0C";
    std::wstring statusLine = L"Загрузка погоды...";
    std::wstring lastQuery = L"Пермь";
    WeatherInfo info;
    bool viewWeek = false;
    int hourOffset = 0;
} g_weather;

struct WeatherResult {
    bool success = false;
    bool silent = false;
    std::wstring query;
    std::wstring errorText;
    WeatherInfo info;
};

std::atomic<bool> g_weatherBusy{ false };
ULONGLONG g_lastWeatherTick = 0;

inline D2D1_RECT_F GetCityInputRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 14.0f, top + 52.0f, right - 14.0f, top + 84.0f);
}

inline D2D1_RECT_F GetWxCityRect() {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 146.0f, top + 46.0f, left + 278.0f, top + 100.0f);
}

inline D2D1_RECT_F GetWxSegRect(int i) {
    float left = IslandLeft();
    float top = ISLAND_TOP;
    float x = left + 16.0f + i * 82.0f;
    return D2D1::RectF(x, top + 106.0f, x + 80.0f, top + 128.0f);
}

inline D2D1_RECT_F GetWxCardRect(int i) {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    float inner = (right - 14.0f) - (left + 14.0f);
    float gap = 4.0f;
    float w = (inner - 6.0f * gap) / 7.0f;
    float x = left + 14.0f + i * (w + gap);
    return D2D1::RectF(x, top + 138.0f, x + w, top + 226.0f);
}

inline long DaysFromCivil(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long>(doe) - 719468;
}

inline bool ParseIsoDate(const std::string& s, long& days) {
    if (s.size() < 10) return false;
    int y = atoi(s.substr(0, 4).c_str());
    int m = atoi(s.substr(5, 2).c_str());
    int d = atoi(s.substr(8, 2).c_str());
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    days = DaysFromCivil(y, m, d);
    return true;
}

inline std::wstring DayLabel(const std::string& date, const std::string& today) {
    static const wchar_t* kWeekdayShort[7] = { L"Вс", L"Пн", L"Вт", L"Ср", L"Чт", L"Пт", L"Сб" };
    long d = 0, t = 0;
    if (!ParseIsoDate(date, d) || !ParseIsoDate(today, t)) return Utf8ToWide(date.substr(0, 10));
    long diff = d - t;
    if (diff == 0) return L"Сегодня";
    if (diff == 1) return L"Завтра";
    long w = (d + 4) % 7;
    if (w < 0) w += 7;
    return kWeekdayShort[w];
}

inline std::wstring FormatTemp(int t) {
    std::wstring r = (t < 0) ? L"\u2212" : L"";
    r += std::to_wstring(t < 0 ? -t : t);
    r += L"\u00B0";
    return r;
}

enum class WxKind { Clear, PartlyCloudy, Cloudy, Fog, Rain, Snow, Thunder };

inline WxKind WxKindFromCode(int c) {
    if (c == 0 || c == 1) return WxKind::Clear;
    if (c == 2) return WxKind::PartlyCloudy;
    if (c == 3) return WxKind::Cloudy;
    if (c == 45 || c == 48) return WxKind::Fog;
    if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return WxKind::Rain;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return WxKind::Snow;
    if (c >= 95) return WxKind::Thunder;
    return WxKind::Cloudy;
}

inline const wchar_t* WxDescription(int c) {
    switch (c) {
    case 0: return L"Ясно";
    case 1: return L"Преимущественно ясно";
    case 2: return L"Переменная облачность";
    case 3: return L"Пасмурно";
    case 45: case 48: return L"Туман";
    case 51: case 53: case 55: return L"Морось";
    case 56: case 57: return L"Ледяная морось";
    case 61: return L"Небольшой дождь";
    case 63: return L"Дождь";
    case 65: return L"Сильный дождь";
    case 66: case 67: return L"Ледяной дождь";
    case 71: return L"Небольшой снег";
    case 73: return L"Снег";
    case 75: return L"Сильный снег";
    case 77: return L"Снежные зёрна";
    case 80: return L"Небольшой ливень";
    case 81: return L"Ливень";
    case 82: return L"Сильный ливень";
    case 85: case 86: return L"Снегопад";
    case 95: return L"Гроза";
    case 96: case 99: return L"Гроза с градом";
    default: return L"Облачно";
    }
}

inline void IconCloud(float cx, float cy, float s, float r, float g, float b) {
    IconColor(r, g, b);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(cx - 0.5f * s, cy - 0.02f * s, cx + 0.5f * s, cy + 0.26f * s), 0.14f * s, 0.14f * s), g_pBrushIcon);
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 0.22f * s, cy), 0.2f * s, 0.2f * s), g_pBrushIcon);
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 0.04f * s, cy - 0.1f * s), 0.26f * s, 0.26f * s), g_pBrushIcon);
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 0.3f * s, cy + 0.04f * s), 0.18f * s, 0.18f * s), g_pBrushIcon);
}

inline void IconSun(float cx, float cy, float s) {
    IconColor(1.0f, 0.78f, 0.22f);
    const float r = 0.22f * s;
    g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), g_pBrushIcon);
    const float w = std::max(1.2f, 0.055f * s);
    for (int i = 0; i < 8; ++i) {
        float a = static_cast<float>(i) * 0.7853982f;
        float c = std::cos(a), sn = std::sin(a);
        g_pRT->DrawLine(D2D1::Point2F(cx + c * 0.33f * s, cy + sn * 0.33f * s),
            D2D1::Point2F(cx + c * 0.47f * s, cy + sn * 0.47f * s), g_pBrushIcon, w);
    }
}

inline void IconMoon(float cx, float cy, float s) {
    const float r = 0.32f * s;
    ID2D1EllipseGeometry* ga = nullptr;
    ID2D1EllipseGeometry* gb = nullptr;
    ID2D1PathGeometry* path = nullptr;
    g_pD2DFactory->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), &ga);
    g_pD2DFactory->CreateEllipseGeometry(
        D2D1::Ellipse(D2D1::Point2F(cx + 0.5f * r, cy - 0.35f * r), r * 0.88f, r * 0.88f), &gb);
    g_pD2DFactory->CreatePathGeometry(&path);
    ID2D1GeometrySink* sink = nullptr;
    if (ga && gb && path && SUCCEEDED(path->Open(&sink))) {
        ga->CombineWithGeometry(gb, D2D1_COMBINE_MODE_EXCLUDE, nullptr, sink);
        sink->Close();
        sink->Release();
        IconColor(0.90f, 0.93f, 1.0f);
        g_pRT->FillGeometry(path, g_pBrushIcon);
    }
    if (path) path->Release();
    if (gb) gb->Release();
    if (ga) ga->Release();
}

inline void DrawWeatherIcon(WxKind kind, bool isDay, float cx, float cy, float s) {
    const float lw = std::max(1.3f, 0.06f * s);
    switch (kind) {
    case WxKind::Clear:
        if (isDay) IconSun(cx, cy, s); else IconMoon(cx, cy, s);
        break;
    case WxKind::PartlyCloudy:
        if (isDay) IconSun(cx - 0.18f * s, cy - 0.18f * s, 0.66f * s);
        else IconMoon(cx - 0.18f * s, cy - 0.18f * s, 0.7f * s);
        IconCloud(cx + 0.06f * s, cy + 0.16f * s, 0.84f * s, 0.86f, 0.88f, 0.92f);
        break;
    case WxKind::Cloudy:
        IconCloud(cx + 0.14f * s, cy - 0.10f * s, 0.66f * s, 0.58f, 0.62f, 0.70f);
        IconCloud(cx - 0.04f * s, cy + 0.10f * s, 0.86f * s, 0.84f, 0.86f, 0.90f);
        break;
    case WxKind::Fog:
        IconCloud(cx, cy - 0.14f * s, 0.8f * s, 0.70f, 0.73f, 0.80f);
        IconColor(0.72f, 0.75f, 0.82f);
        for (int i = 0; i < 3; ++i) {
            float y = cy + (0.22f + 0.13f * i) * s;
            float half = (0.34f - 0.04f * i) * s;
            g_pRT->DrawLine(D2D1::Point2F(cx - half, y), D2D1::Point2F(cx + half, y), g_pBrushIcon, lw);
        }
        break;
    case WxKind::Rain:
        IconCloud(cx, cy - 0.16f * s, 0.88f * s, 0.66f, 0.70f, 0.80f);
        IconColor(0.40f, 0.68f, 1.0f);
        for (int k = -1; k <= 1; ++k) {
            float x = cx + k * 0.22f * s;
            g_pRT->DrawLine(D2D1::Point2F(x + 0.05f * s, cy + 0.16f * s),
                D2D1::Point2F(x - 0.02f * s, cy + 0.36f * s), g_pBrushIcon, lw);
        }
        break;
    case WxKind::Snow:
        IconCloud(cx, cy - 0.16f * s, 0.88f * s, 0.66f, 0.70f, 0.80f);
        IconColor(0.95f, 0.97f, 1.0f);
        for (int k = -1; k <= 1; ++k) {
            float x = cx + k * 0.22f * s;
            float y = cy + ((k == 0) ? 0.34f : 0.24f) * s;
            float r = std::max(1.2f, 0.05f * s);
            g_pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), r, r), g_pBrushIcon);
        }
        break;
    case WxKind::Thunder: {
        IconCloud(cx, cy - 0.16f * s, 0.88f * s, 0.56f, 0.60f, 0.70f);
        IconColor(1.0f, 0.84f, 0.25f);
        const float bw = std::max(1.5f, 0.07f * s);
        D2D1_POINT_2F p0 = D2D1::Point2F(cx + 0.06f * s, cy + 0.10f * s);
        D2D1_POINT_2F p1 = D2D1::Point2F(cx - 0.06f * s, cy + 0.28f * s);
        D2D1_POINT_2F p2 = D2D1::Point2F(cx + 0.04f * s, cy + 0.28f * s);
        D2D1_POINT_2F p3 = D2D1::Point2F(cx - 0.08f * s, cy + 0.50f * s);
        g_pRT->DrawLine(p0, p1, g_pBrushIcon, bw);
        g_pRT->DrawLine(p1, p2, g_pBrushIcon, bw);
        g_pRT->DrawLine(p2, p3, g_pBrushIcon, bw);
        break;
    }
    }
}

inline bool FetchWeatherOpenMeteo(const std::wstring& cityQuery, WeatherInfo& w, std::wstring& err) {
    std::string geo = HttpGet(L"geocoding-api.open-meteo.com",
        L"/v1/search?name=" + UrlEncodeWide(cityQuery) + L"&count=1&language=ru&format=json");
    if (geo.empty() || t_httpError != 0) {
        err = HttpDiag();
        return false;
    }
    double lat = 0.0, lon = 0.0;
    if (!JsonGetNumber(geo, "latitude", lat) || !JsonGetNumber(geo, "longitude", lon)) {
        err = (t_httpStatus == 200) ? (L"Город не найден: " + cityQuery) : HttpDiag();
        return false;
    }
    w.city = JsonGetString(geo, "name");
    if (w.city.empty()) w.city = cityQuery;
    w.region = JsonGetString(geo, "admin1");

    wchar_t path[512];
    swprintf_s(path,
        L"/v1/forecast?latitude=%.4f&longitude=%.4f"
        L"&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,is_day"
        L"&hourly=temperature_2m,weather_code,is_day"
        L"&daily=weather_code,temperature_2m_max,temperature_2m_min"
        L"&timezone=auto&forecast_days=7&wind_speed_unit=ms", lat, lon);
    std::string fc = HttpGet(L"api.open-meteo.com", path);
    if (fc.empty() || t_httpError != 0) {
        err = HttpDiag();
        return false;
    }

    std::string cur = JsonObjectSlice(fc, "current");
    std::string hr = JsonObjectSlice(fc, "hourly");
    std::string dy = JsonObjectSlice(fc, "daily");

    double v = 0.0;
    if (cur.empty() || hr.empty() || dy.empty() || !JsonGetNumber(cur, "temperature_2m", v)) {
        err = L"некорректный ответ прогноза";
        return false;
    }
    w.temp = static_cast<int>(std::lround(v));
    w.feels = JsonGetNumber(cur, "apparent_temperature", v) ? static_cast<int>(std::lround(v)) : w.temp;
    w.humidity = JsonGetNumber(cur, "relative_humidity_2m", v) ? static_cast<int>(std::lround(v)) : 0;
    w.code = JsonGetNumber(cur, "weather_code", v) ? static_cast<int>(v) : 3;
    w.windMs = JsonGetNumber(cur, "wind_speed_10m", v) ? v : 0.0;
    w.isDay = JsonGetNumber(cur, "is_day", v) ? (v > 0.5) : true;

    SYSTEMTIME stNow;
    GetLocalTime(&stNow);
    wchar_t todayBuf[16];
    swprintf_s(todayBuf, L"%04d-%02d-%02d", stNow.wYear, stNow.wMonth, stNow.wDay);
    std::string today = WideToUtf8(todayBuf);

    auto hTimes = JsonGetStrArray(hr, "time");
    auto hTemps = JsonGetNumArray(hr, "temperature_2m");
    auto hCodes = JsonGetNumArray(hr, "weather_code");
    auto hDay = JsonGetNumArray(hr, "is_day");
    size_t n = std::min({ hTimes.size(), hTemps.size(), hCodes.size() });

    size_t start = 0;
    wchar_t bufKey[32];
    swprintf_s(bufKey, L"%04d-%02d-%02dT%02d", stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour);
    std::string targetHourPrefix = WideToUtf8(bufKey);

    for (size_t i = 0; i < n; ++i) {
        if (hTimes[i].rfind(targetHourPrefix, 0) == 0) {
            start = i;
            break;
        }
    }

    for (size_t i = start; i < n && w.hours.size() < 36; ++i) {
        HourForecast h;
        h.temp = static_cast<int>(std::lround(hTemps[i]));
        h.code = static_cast<int>(hCodes[i]);
        h.isDay = (i < hDay.size()) ? (hDay[i] > 0.5) : true;
        const std::string& t = hTimes[i];
        std::string hh = t.size() >= 13 ? t.substr(11, 2) : "00";
        if (w.hours.empty()) {
            h.label = L"Сейчас";
            h.temp = w.temp;
            h.code = w.code;
            h.isDay = w.isDay;
        }
        else if (hh == "00") {
            h.label = DayLabel(t.substr(0, 10), today);
        }
        else {
            h.label = Utf8ToWide(hh) + L":00";
        }
        w.hours.push_back(h);
    }

    auto dTimes = JsonGetStrArray(dy, "time");
    auto dCodes = JsonGetNumArray(dy, "weather_code");
    auto dMax = JsonGetNumArray(dy, "temperature_2m_max");
    auto dMin = JsonGetNumArray(dy, "temperature_2m_min");
    size_t dn = std::min({ dTimes.size(), dCodes.size(), dMax.size(), dMin.size() });
    for (size_t i = 0; i < dn && i < 7; ++i) {
        DayForecast d;
        d.label = DayLabel(dTimes[i], today);
        d.code = static_cast<int>(dCodes[i]);
        d.tMax = static_cast<int>(std::lround(dMax[i]));
        d.tMin = static_cast<int>(std::lround(dMin[i]));
        w.days.push_back(d);
    }
    if (!w.days.empty()) {
        w.tMax = w.days[0].tMax;
        w.tMin = w.days[0].tMin;
    }
    else {
        w.tMax = w.tMin = w.temp;
    }

    if (w.hours.empty()) {
        err = L"не удалось разобрать прогноз";
        return false;
    }
    w.loaded = true;
    return true;
}

inline int WwoToWmo(int c) {
    switch (c) {
    case 113: return 0;
    case 116: return 2;
    case 119: case 122: return 3;
    case 143: case 248: case 260: return 45;
    case 176: case 263: case 266: case 293: case 296: return 61;
    case 299: case 302: return 63;
    case 305: case 308: return 65;
    case 353: return 80;
    case 356: return 81;
    case 359: return 82;
    case 281: case 284: case 185: return 56;
    case 311: case 314: case 182: case 317: case 320: case 362: case 365: case 350: case 374: case 377: return 66;
    case 179: case 227: case 323: case 326: return 71;
    case 329: case 332: return 73;
    case 230: case 335: case 338: return 75;
    case 368: return 85;
    case 371: case 395: return 86;
    case 200: case 386: case 389: case 392: return 95;
    default: return 3;
    }
}

inline bool FetchWeatherWttr(const std::wstring& cityQuery, WeatherInfo& w, std::wstring& err) {
    std::string js = HttpGet(L"wttr.in", L"/" + UrlEncodeWide(cityQuery) + L"?format=j1&lang=ru");
    if (js.empty() || t_httpError != 0) {
        err = HttpDiag();
        return false;
    }
    auto cc = JsonArrayObjects(js, "current_condition");
    auto days = JsonArrayObjects(js, "weather");
    double v = 0.0;
    if (cc.empty() || days.empty() || !JsonGetNumQ(cc[0], "temp_C", v)) {
        err = (t_httpStatus == 200) ? (L"Город не найден: " + cityQuery) : HttpDiag();
        return false;
    }
    const std::string& cur = cc[0];

    w.city = cityQuery;
    w.temp = static_cast<int>(std::lround(v));
    w.feels = JsonGetNumQ(cur, "FeelsLikeC", v) ? static_cast<int>(std::lround(v)) : w.temp;
    w.humidity = JsonGetNumQ(cur, "humidity", v) ? static_cast<int>(std::lround(v)) : 0;
    w.windMs = JsonGetNumQ(cur, "windspeedKmph", v) ? v / 3.6 : 0.0;
    w.code = WwoToWmo(JsonGetNumQ(cur, "weatherCode", v) ? static_cast<int>(v) : 122);

    std::string today = JsonGetStringA(days[0], "date");
    SYSTEMTIME stNow;
    GetLocalTime(&stNow);
    int nowMin = stNow.wHour * 60 + stNow.wMinute;
    w.isDay = (nowMin >= 7 * 60 && nowMin < 19 * 60);

    HourForecast nowCard;
    nowCard.label = L"Сейчас";
    nowCard.temp = w.temp;
    nowCard.code = w.code;
    nowCard.isDay = w.isDay;
    w.hours.push_back(nowCard);

    for (size_t d = 0; d < days.size() && w.hours.size() < 25; ++d) {
        std::string date = JsonGetStringA(days[d], "date");
        auto hrs = JsonArrayObjects(days[d], "hourly");

        DayForecast df;
        df.label = DayLabel(date, today);
        df.tMax = JsonGetNumQ(days[d], "maxtempC", v) ? static_cast<int>(std::lround(v)) : w.temp;
        df.tMin = JsonGetNumQ(days[d], "mintempC", v) ? static_cast<int>(std::lround(v)) : w.temp;
        df.code = w.code;
        bool codeFromNoon = false;

        for (const auto& h : hrs) {
            double tv = 0.0, tc = 0.0, cd = 0.0;
            if (!JsonGetNumQ(h, "time", tv) || !JsonGetNumQ(h, "tempC", tc)) continue;
            int hh = static_cast<int>(tv) / 100;
            int code = WwoToWmo(JsonGetNumQ(h, "weatherCode", cd) ? static_cast<int>(cd) : 122);

            if (hh == 12 && !codeFromNoon) {
                df.code = code;
                codeFromNoon = true;
            }

            long absMin = static_cast<long>(d) * 1440 + hh * 60;
            if (absMin <= nowMin || w.hours.size() >= 25) continue;

            HourForecast hf;
            hf.temp = static_cast<int>(std::lround(tc));
            hf.code = code;
            hf.isDay = (hh >= 7 && hh < 19);
            if (hh == 0) {
                hf.label = DayLabel(date, today);
            }
            else {
                wchar_t b[16];
                swprintf_s(b, L"%02d:00", hh);
                hf.label = b;
            }
            w.hours.push_back(hf);
        }
        w.days.push_back(df);
    }

    if (!w.days.empty()) {
        w.tMax = w.days[0].tMax;
        w.tMin = w.days[0].tMin;
    }
    else {
        w.tMax = w.tMin = w.temp;
    }
    w.loaded = true;
    return true;
}

inline void FetchWeatherAsync(HWND hwnd, std::wstring cityQuery, bool silent = false) {
    if (!silent) {
        g_weather.lastQuery = cityQuery;
        g_weather.statusLine = L"Загрузка погоды...";
        g_weather.info.loaded = false;
        RenderIsland(hwnd);
    }
    g_weatherBusy = true;
    g_lastWeatherTick = GetTickCount64();

    std::thread([hwnd, cityQuery, silent]() {
        auto* res = new WeatherResult();
        res->silent = silent;
        res->query = cityQuery;

        std::wstring errOM, errWT;
        WeatherInfo wOM, wWT;
        if (FetchWeatherOpenMeteo(cityQuery, wOM, errOM)) {
            res->info = wOM;
            res->success = true;
        }
        else {
            bool notFound = errOM.rfind(L"Город не найден", 0) == 0;
            if (!notFound && FetchWeatherWttr(cityQuery, wWT, errWT)) {
                res->info = wWT;
                res->success = true;
            }
            else if (notFound) {
                res->errorText = errOM;
            }
            else {
                res->errorText = L"Нет связи с серверами погоды. Open-Meteo: " + errOM + L"; wttr.in: " + errWT;
            }
        }

        if (!PostMessage(hwnd, WM_WEATHER_UPDATED, 0, reinterpret_cast<LPARAM>(res))) {
            g_weatherBusy = false;
            delete res;
        }
        }).detach();
}

inline void DrawWeatherTab(float left, float top, float right, const POINT& mp) {
    const WeatherInfo& w = g_weather.info;

    if (g_state.isTypingCity) {
        D2D1_RECT_F inputRect = GetCityInputRect();
        D2D1_ROUNDED_RECT pill = D2D1::RoundedRect(inputRect, 10.0f, 10.0f);
        g_pRT->FillRoundedRectangle(pill, g_pBrushInputBg);
        g_pRT->DrawRoundedRectangle(pill, g_pBrushAccent, 1.2f);
        std::wstring txt = L"Город (Enter \u2014 применить): " + g_weather.inputBuffer + L"|";
        D2D1_RECT_F tr = D2D1::RectF(inputRect.left + 12.0f, inputRect.top, inputRect.right - 12.0f, inputRect.bottom);
        DrawScrollingText(SLOT_CITY_INPUT, txt, g_pTextFormatLeftSub, tr, g_pBrushAccent, 35.0f, true);
    }
    else if (!w.loaded) {
        D2D1_RECT_F sr = D2D1::RectF(left + 20.0f, top + 50.0f, right - 20.0f, top + 82.0f);
        DrawScrollingText(SLOT_WX_STATUS, g_weather.statusLine, g_pTextFormatTitle, sr, g_pBrushText);
        const wchar_t* hint = L"Нажмите, чтобы ввести город";
        D2D1_RECT_F hr = D2D1::RectF(left + 20.0f, top + 84.0f, right - 20.0f, top + 104.0f);
        g_pRT->DrawText(hint, static_cast<UINT32>(wcslen(hint)), g_pTextFormatTime, hr, g_pBrushTextMuted);
    }
    else {
        D2D1_RECT_F cityBlock = GetWxCityRect();
        if (PtInRectF(cityBlock, mp.x, mp.y)) {
            FillRR(cityBlock, 10.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.07f));
        }

        DrawWeatherIcon(WxKindFromCode(w.code), w.isDay, left + 38.0f, top + 74.0f, 46.0f);
        std::wstring tempStr = FormatTemp(w.temp);
        D2D1_RECT_F tempRect = D2D1::RectF(left + 64.0f, top + 48.0f, left + 150.0f, top + 100.0f);
        g_pRT->DrawText(tempStr.c_str(), static_cast<UINT32>(tempStr.size()), g_pTextFormatBig, tempRect, g_pBrushText);

        D2D1_RECT_F condRect = D2D1::RectF(left + 152.0f, top + 50.0f, left + 274.0f, top + 68.0f);
        D2D1_RECT_F cityRect = D2D1::RectF(left + 152.0f, top + 68.0f, left + 274.0f, top + 84.0f);
        D2D1_RECT_F feelsRect = D2D1::RectF(left + 152.0f, top + 84.0f, left + 274.0f, top + 100.0f);
        DrawScrollingText(SLOT_WX_COND, WxDescription(w.code), g_pTextFormatLeftTitle, condRect, g_pBrushText);
        DrawScrollingText(SLOT_WX_CITY, w.city, g_pTextFormatLeftSub, cityRect, g_pBrushText);
        DrawScrollingText(SLOT_WX_FEELS, L"Ощущается как " + FormatTemp(w.feels),
            g_pTextFormatLeftSub, feelsRect, g_pBrushTextMuted);

        std::wstring hiLo = L"\u2191 " + FormatTemp(w.tMax) + L"  \u2193 " + FormatTemp(w.tMin);
        std::wstring wind = L"Ветер " + std::to_wstring(static_cast<int>(std::lround(w.windMs))) + L" м/с";
        std::wstring hum = L"Влажность " + std::to_wstring(w.humidity) + L"%";
        D2D1_RECT_F r1 = D2D1::RectF(left + 280.0f, top + 50.0f, right - 14.0f, top + 68.0f);
        D2D1_RECT_F r2 = D2D1::RectF(left + 280.0f, top + 68.0f, right - 14.0f, top + 84.0f);
        D2D1_RECT_F r3 = D2D1::RectF(left + 280.0f, top + 84.0f, right - 14.0f, top + 100.0f);
        g_pRT->DrawText(hiLo.c_str(), static_cast<UINT32>(hiLo.size()), g_pTextFormatRightTitle, r1, g_pBrushText);
        g_pRT->DrawText(wind.c_str(), static_cast<UINT32>(wind.size()), g_pTextFormatRightSub, r2, g_pBrushText);
        g_pRT->DrawText(hum.c_str(), static_cast<UINT32>(hum.size()), g_pTextFormatRightSub, r3, g_pBrushText);
    }

    if (!w.loaded) return;

    D2D1_RECT_F segBox = D2D1::RectF(left + 14.0f, top + 104.0f, left + 182.0f, top + 130.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(segBox, 13.0f, 13.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(segBox, 13.0f, 13.0f), g_pBrushBorder, 1.0f);
    const wchar_t* segNames[2] = { L"По часам", L"На неделю" };
    for (int i = 0; i < 2; ++i) {
        D2D1_RECT_F sr = GetWxSegRect(i);
        bool active = (g_weather.viewWeek == (i == 1));
        if (active) FillRR(sr, 11.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f));
        g_pRT->DrawText(segNames[i], static_cast<UINT32>(wcslen(segNames[i])), g_pTextFormatSub, sr,
            active ? g_pBrushText : g_pBrushTextMuted);
    }

    if (!g_weather.viewWeek) {
        int maxOff = std::max(0, static_cast<int>(w.hours.size()) - 7);
        int off = std::clamp(g_weather.hourOffset, 0, maxOff);
        for (int i = 0; i < 7; ++i) {
            int idx = off + i;
            if (idx >= static_cast<int>(w.hours.size())) break;
            const HourForecast& h = w.hours[idx];
            D2D1_RECT_F r = GetWxCardRect(i);
            bool isNow = (idx == 0);
            FillRR(r, 12.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, isNow ? 0.15f : 0.06f));
            if (isNow) g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(r, 12.0f, 12.0f), g_pBrushBorder, 1.0f);

            D2D1_RECT_F lr = D2D1::RectF(r.left, r.top + 6.0f, r.right, r.top + 22.0f);
            g_pRT->DrawText(h.label.c_str(), static_cast<UINT32>(h.label.size()), g_pTextFormatTime, lr,
                isNow ? g_pBrushText : g_pBrushTextMuted);

            DrawWeatherIcon(WxKindFromCode(h.code), h.isDay, (r.left + r.right) * 0.5f, r.top + 44.0f, 26.0f);

            std::wstring t = FormatTemp(h.temp);
            D2D1_RECT_F tr = D2D1::RectF(r.left, r.top + 62.0f, r.right, r.top + 82.0f);
            g_pRT->DrawText(t.c_str(), static_cast<UINT32>(t.size()), g_pTextFormatTitle, tr, g_pBrushText);
        }
    }
    else {
        for (int i = 0; i < 7 && i < static_cast<int>(w.days.size()); ++i) {
            const DayForecast& d = w.days[i];
            D2D1_RECT_F r = GetWxCardRect(i);
            bool isToday = (i == 0);
            FillRR(r, 12.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, isToday ? 0.15f : 0.06f));
            if (isToday) g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(r, 12.0f, 12.0f), g_pBrushBorder, 1.0f);

            D2D1_RECT_F lr = D2D1::RectF(r.left, r.top + 6.0f, r.right, r.top + 22.0f);
            g_pRT->DrawText(d.label.c_str(), static_cast<UINT32>(d.label.size()), g_pTextFormatTime, lr,
                isToday ? g_pBrushText : g_pBrushTextMuted);

            DrawWeatherIcon(WxKindFromCode(d.code), true, (r.left + r.right) * 0.5f, r.top + 40.0f, 26.0f);

            std::wstring hi = FormatTemp(d.tMax);
            std::wstring lo = FormatTemp(d.tMin);
            D2D1_RECT_F hr = D2D1::RectF(r.left, r.top + 56.0f, r.right, r.top + 72.0f);
            D2D1_RECT_F lor = D2D1::RectF(r.left, r.top + 70.0f, r.right, r.top + 86.0f);
            g_pRT->DrawText(hi.c_str(), static_cast<UINT32>(hi.size()), g_pTextFormatTitle, hr, g_pBrushText);
            g_pRT->DrawText(lo.c_str(), static_cast<UINT32>(lo.size()), g_pTextFormatTime, lor, g_pBrushTextMuted);
        }
    }
}