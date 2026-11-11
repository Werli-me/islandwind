#pragma once
#include "Common.h"
#include "Graphics.h"

struct PhoneNotifItem {
    uint32_t id = 0;
    bool isCall = false;
    bool showPopup = true;
    std::wstring appName;
    std::wstring title;
    std::wstring body;
    std::wstring timeStr;
};

inline std::vector<PhoneNotifItem> g_phoneList;
inline int g_phoneScroll = 0;

inline D2D1_RECT_F GetPhoneBannerRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 16.0f, top + 44.0f, right - 16.0f, top + ISLAND_H_SHORT - 12.0f);
}

inline D2D1_RECT_F GetPhoneBannerCloseRect() {
    D2D1_RECT_F b = GetPhoneBannerRect();
    return D2D1::RectF(b.right - 28.0f, b.top + 8.0f, b.right - 8.0f, b.top + 28.0f);
}

inline D2D1_RECT_F GetPhoneZoneRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 14.0f, top + 66.0f, right - 14.0f, top + 226.0f);
}

inline D2D1_RECT_F GetPhoneClearBtnRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(right - 14.0f - 76.0f, top + 40.0f, right - 14.0f, top + 62.0f);
}

inline D2D1_RECT_F GetPhoneCardRect(int slot) {
    D2D1_RECT_F z = GetPhoneZoneRect();
    const float h = 48.0f;
    const float gap = 6.0f;
    float y = z.top + slot * (h + gap);
    return D2D1::RectF(z.left, y, z.right, y + h);
}

inline D2D1_RECT_F GetPhoneCardCloseRect(const D2D1_RECT_F& card) {
    float cy = (card.top + card.bottom) * 0.5f;
    return D2D1::RectF(card.right - 28.0f, cy - 10.0f, card.right - 8.0f, cy + 10.0f);
}

inline void ClampPhoneScroll() {
    int maxScroll = std::max(0, static_cast<int>(g_phoneList.size()) - 3);
    g_phoneScroll = std::clamp(g_phoneScroll, 0, maxScroll);
}

inline int HitPhoneCard(int x, int y) {
    for (int slot = 0; slot < 3; ++slot) {
        int idx = g_phoneScroll + slot;
        if (idx >= static_cast<int>(g_phoneList.size())) break;
        if (PtInRectF(GetPhoneCardRect(slot), x, y)) return idx;
    }
    return -1;
}

inline void DrawPhoneBanner(const POINT& mp) {
    D2D1_RECT_F bannerRect = GetPhoneBannerRect();
    D2D1_ROUNDED_RECT bannerPill = D2D1::RoundedRect(bannerRect, 14.0f, 14.0f);

    g_pRT->FillRoundedRectangle(bannerPill, g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(bannerPill, g_phoneEvent.isCall ? g_pBrushAccent : g_pBrushBorder, 1.2f);

    D2D1_RECT_F iconBox = D2D1::RectF(bannerRect.left + 14.0f, bannerRect.top + 14.0f, bannerRect.left + 54.0f, bannerRect.top + 54.0f);
    D2D1_ROUNDED_RECT iconCircle = D2D1::RoundedRect(iconBox, 20.0f, 20.0f);

    g_pBrushIcon->SetColor(g_phoneEvent.isCall ? D2D1::ColorF(0.3f, 0.85f, 0.45f, 0.25f) : D2D1::ColorF(0.42f, 0.72f, 1.0f, 0.2f));
    g_pRT->FillRoundedRectangle(iconCircle, g_pBrushIcon);
    g_pRT->DrawText(g_phoneEvent.isCall ? L"\U0001F4DE" : L"\U0001F4AC", 2, g_pTextFormatTitle, iconBox, g_pBrushText);

    D2D1_RECT_F nameRect = D2D1::RectF(bannerRect.left + 64.0f, bannerRect.top + 12.0f, bannerRect.right - 34.0f, bannerRect.top + 36.0f);
    D2D1_RECT_F textRect = D2D1::RectF(bannerRect.left + 64.0f, bannerRect.top + 38.0f, bannerRect.right - 16.0f, bannerRect.bottom - 10.0f);

    std::wstring header = g_phoneEvent.isCall ? (L"Входящий вызов: " + g_phoneEvent.title) : g_phoneEvent.title;
    DrawScrollingText(SLOT_TITLE, header, g_pTextFormatLeftTitle, nameRect, g_pBrushText);
    DrawScrollingText(SLOT_ARTIST, g_phoneEvent.body, g_pTextFormatLeftSub, textRect, g_pBrushTextMuted);

    D2D1_RECT_F cr = GetPhoneBannerCloseRect();
    bool hotClose = PtInRectF(cr, mp.x, mp.y);
    if (hotClose) FillRR(cr, 8.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f));
    g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatSub, cr, hotClose ? g_pBrushText : g_pBrushTextMuted);
}

inline void DrawPhoneTab(float left, float top, float right, const POINT& mp) {
    std::wstring status = g_phoneList.empty()
        ? L"Связь с телефоном активна"
        : (L"Уведомления: " + std::to_wstring(g_phoneList.size()));
    D2D1_RECT_F sr = D2D1::RectF(left + 18.0f, top + 40.0f, right - 100.0f, top + 62.0f);
    g_pRT->DrawText(status.c_str(), static_cast<UINT32>(status.size()), g_pTextFormatLeftSub, sr, g_pBrushTextMuted);

    if (!g_phoneList.empty()) {
        D2D1_RECT_F clrRect = GetPhoneClearBtnRect();
        bool hotClr = PtInRectF(clrRect, mp.x, mp.y);
        if (hotClr) FillRR(clrRect, 9.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f));
        const wchar_t* clrLabel = L"Очистить";
        g_pRT->DrawText(clrLabel, static_cast<UINT32>(wcslen(clrLabel)), g_pTextFormatSub, clrRect,
            hotClr ? g_pBrushText : g_pBrushTextMuted);
    }

    D2D1_RECT_F zone = GetPhoneZoneRect();
    if (g_phoneList.empty()) {
        D2D1_ROUNDED_RECT zoneRR = D2D1::RoundedRect(zone, 16.0f, 16.0f);
        g_pRT->FillRoundedRectangle(zoneRR, g_pBrushInputBg);
        g_pRT->DrawRoundedRectangle(zoneRR, g_pBrushBorder, 1.0f);

        const wchar_t* emptyMsg = L"Новых уведомлений нет\nСообщения и звонки со смартфона появятся здесь";
        D2D1_RECT_F tr = D2D1::RectF(zone.left + 16.0f, zone.top + 20.0f, zone.right - 16.0f, zone.bottom - 20.0f);
        g_pRT->DrawText(emptyMsg, static_cast<UINT32>(wcslen(emptyMsg)), g_pTextFormatSub, tr, g_pBrushTextMuted);
        return;
    }

    for (int slot = 0; slot < 3; ++slot) {
        int idx = g_phoneScroll + slot;
        if (idx >= static_cast<int>(g_phoneList.size())) break;
        const PhoneNotifItem& it = g_phoneList[idx];

        D2D1_RECT_F card = GetPhoneCardRect(slot);
        bool hot = PtInRectF(card, mp.x, mp.y);

        FillRR(card, 12.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, hot ? 0.13f : 0.07f));
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(card, 12.0f, 12.0f), g_pBrushBorder, 1.0f);

        D2D1_RECT_F badge = D2D1::RectF(card.left + 8.0f, card.top + 8.0f, card.left + 40.0f, card.bottom - 8.0f);
        D2D1_COLOR_F bc = it.isCall ? D2D1::ColorF(0.30f, 0.85f, 0.45f, 0.22f) : D2D1::ColorF(0.42f, 0.72f, 1.0f, 0.20f);
        FillRR(badge, 10.0f, bc);
        g_pRT->DrawText(it.isCall ? L"\U0001F4DE" : L"\U0001F4AC", 2, g_pTextFormatSub, badge, g_pBrushText);

        D2D1_RECT_F timeRect = D2D1::RectF(card.right - 68.0f, card.top + 5.0f, card.right - 32.0f, card.top + 23.0f);
        g_pRT->DrawText(it.timeStr.c_str(), static_cast<UINT32>(it.timeStr.size()), g_pTextFormatTime, timeRect, g_pBrushTextMuted);

        D2D1_RECT_F titleRect = D2D1::RectF(card.left + 48.0f, card.top + 4.0f, card.right - 72.0f, card.top + 25.0f);
        D2D1_RECT_F bodyRect = D2D1::RectF(card.left + 48.0f, card.top + 24.0f, card.right - 34.0f, card.bottom - 4.0f);

        if (hot) {
            DrawScrollingText(SLOT_TITLE, it.title, g_pTextFormatLeftTitle, titleRect, g_pBrushText);
            DrawScrollingText(SLOT_ARTIST, it.body, g_pTextFormatLeftSub, bodyRect, g_pBrushTextMuted);
        }
        else {
            DrawClippedText(it.title, g_pTextFormatLeftTitle, titleRect, g_pBrushText);
            DrawClippedText(it.body, g_pTextFormatLeftSub, bodyRect, g_pBrushTextMuted);
        }

        D2D1_RECT_F cr = GetPhoneCardCloseRect(card);
        bool hotClose = PtInRectF(cr, mp.x, mp.y);
        if (hotClose) FillRR(cr, 8.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f));
        g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatSub, cr, hotClose ? g_pBrushText : g_pBrushTextMuted);
    }
}

#if USE_SMTC
using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::UI::Notifications::Management;

inline void StartPhoneListener(HWND hwnd) {
    std::thread([hwnd]() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);

            UserNotificationListener listener = UserNotificationListener::Current();
            UserNotificationListenerAccessStatus access = listener.RequestAccessAsync().get();

            if (access != UserNotificationListenerAccessStatus::Allowed) {
                return;
            }

            std::vector<uint32_t> seenIds;
            bool firstPass = true;

            while (g_running) {
                auto notifications = listener.GetNotificationsAsync(NotificationKinds::Toast).get();

                for (const auto& n : notifications) {
                    uint32_t id = n.Id();
                    if (std::find(seenIds.begin(), seenIds.end(), id) != seenIds.end()) {
                        continue;
                    }
                    seenIds.push_back(id);

                    std::wstring appDisplayName = L"";
                    try {
                        appDisplayName = n.AppInfo().DisplayInfo().DisplayName().c_str();
                    }
                    catch (...) {}

                    if (appDisplayName.find(L"Связь с телефоном") != std::wstring::npos ||
                        appDisplayName.find(L"Phone Link") != std::wstring::npos ||
                        appDisplayName.find(L"Ваш телефон") != std::wstring::npos) {

                        NotificationBinding binding = n.Notification().Visual().GetBinding(KnownNotificationBindings::ToastGeneric());
                        if (binding) {
                            auto textElements = binding.GetTextElements();
                            std::wstring titleText = L"";
                            std::wstring bodyText = L"";

                            auto it = textElements.First();
                            if (it.HasCurrent()) {
                                titleText = it.Current().Text().c_str();
                                it.MoveNext();
                            }
                            if (it.HasCurrent()) {
                                bodyText = it.Current().Text().c_str();
                            }

                            if (titleText.empty() && bodyText.empty()) continue;

                            bool isCall = false;
                            std::wstring lowTitle = titleText;
                            std::wstring lowBody = bodyText;
                            for (auto& c : lowTitle) c = towlower(c);
                            for (auto& c : lowBody) c = towlower(c);

                            if (lowTitle.find(L"вызов") != std::wstring::npos ||
                                lowTitle.find(L"звонок") != std::wstring::npos ||
                                lowTitle.find(L"calling") != std::wstring::npos ||
                                lowBody.find(L"входящий") != std::wstring::npos ||
                                lowBody.find(L"incoming") != std::wstring::npos) {
                                isCall = true;
                            }

                            auto* item = new PhoneNotifItem();
                            item->id = id;
                            item->isCall = isCall;
                            item->showPopup = !firstPass;
                            item->appName = appDisplayName;
                            item->title = titleText.empty() ? L"Уведомление" : titleText;
                            item->body = bodyText;
                            item->timeStr = GetClockString();

                            if (!PostMessage(hwnd, WM_PHONE_NOTIFICATION, 0, reinterpret_cast<LPARAM>(item))) {
                                delete item;
                            }
                        }
                    }
                }

                firstPass = false;
                Sleep(250);
            }
        }
        catch (...) {}
        }).detach();
}
#else
inline void StartPhoneListener(HWND) {}
#endif