#pragma once
#include "Common.h"
#include "Graphics.h"
#include "HttpUtils.h"

struct ShelfItem {
    std::wstring path;     // путь к файлу/папке или URL
    std::wstring display;  // имя для показа
    std::wstring ext;      // расширение (верхний регистр) для значка
    bool isLink = false;
    bool isFolder = false;
};

std::vector<ShelfItem> g_shelf;
int g_shelfScroll = 0;            // прокрутка полки, в строках (по 2 элемента в строке)
int g_pressedItem = -1;           // элемент полки, на котором зажата кнопка мыши
POINT g_pressPt = { 0, 0 };
std::wstring g_shelfNotice;       // короткое уведомление над полкой
ULONGLONG g_shelfNoticeUntil = 0;

// --- Геометрия вкладки Полки ---
inline D2D1_RECT_F GetShelfZoneRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 14.0f, top + 66.0f, right - 14.0f, top + 226.0f);
}

inline D2D1_RECT_F GetShelfLinkBtnRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(right - 14.0f - 76.0f, top + 40.0f, right - 14.0f, top + 62.0f);
}

inline D2D1_RECT_F GetShelfClearBtnRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(right - 14.0f - 76.0f - 6.0f - 76.0f, top + 40.0f, right - 14.0f - 76.0f - 6.0f, top + 62.0f);
}

// slot = 0..7 (2 колонки x 4 ряда видимых элементов)
inline D2D1_RECT_F GetShelfChipRect(int slot) {
    D2D1_RECT_F z = GetShelfZoneRect();
    const float padX = 8.0f, padY = 6.0f, gap = 6.0f, h = 32.0f;
    float w = ((z.right - z.left) - 2.0f * padX - gap) * 0.5f;
    int col = slot % 2;
    int row = slot / 2;
    float x = z.left + padX + col * (w + gap);
    float y = z.top + padY + row * (h + gap);
    return D2D1::RectF(x, y, x + w, y + h);
}

inline D2D1_RECT_F GetShelfChipCloseRect(const D2D1_RECT_F& chip) {
    return D2D1::RectF(chip.right - 24.0f, chip.top + 7.0f, chip.right - 6.0f, chip.bottom - 7.0f);
}

// --- Логика элементов полки ---
inline std::wstring TrimW(const std::wstring& t) {
    size_t b = 0, e = t.size();
    while (b < e && (t[b] == L' ' || t[b] == L'\t' || t[b] == L'\r' || t[b] == L'\n')) ++b;
    while (e > b && (t[e - 1] == L' ' || t[e - 1] == L'\t' || t[e - 1] == L'\r' || t[e - 1] == L'\n')) --e;
    return t.substr(b, e - b);
}

inline void SetShelfNotice(const std::wstring& text) {
    g_shelfNotice = text;
    g_shelfNoticeUntil = GetTickCount64() + 2500;
}

inline void ClampShelfScroll() {
    int rows = (static_cast<int>(g_shelf.size()) + 1) / 2;
    int maxScroll = std::max(0, rows - 4);
    g_shelfScroll = std::clamp(g_shelfScroll, 0, maxScroll);
}

inline bool LooksLikeUrl(const std::wstring& t) {
    if (t.size() < 4) return false;
    if (t.find_first_of(L" \t\r\n") != std::wstring::npos) return false;
    std::wstring low = t;
    for (auto& ch : low) ch = static_cast<wchar_t>(towlower(ch));
    return low.rfind(L"http://", 0) == 0 || low.rfind(L"https://", 0) == 0 ||
        low.rfind(L"ftp://", 0) == 0 || low.rfind(L"www.", 0) == 0;
}

inline bool AddShelfFile(const std::wstring& pathIn) {
    std::wstring path = pathIn;
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    if (path.empty()) return false;
    for (const auto& it : g_shelf) {
        if (!it.isLink && _wcsicmp(it.path.c_str(), path.c_str()) == 0) return false;
    }

    ShelfItem item;
    item.path = path;
    DWORD attr = GetFileAttributesW(path.c_str());
    item.isFolder = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);

    size_t slash = path.find_last_of(L"\\/");
    item.display = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    if (item.display.empty()) item.display = path;

    if (!item.isFolder) {
        size_t dot = item.display.find_last_of(L'.');
        if (dot != std::wstring::npos && dot + 1 < item.display.size()) {
            std::wstring ext = item.display.substr(dot + 1);
            for (auto& ch : ext) ch = static_cast<wchar_t>(towupper(ch));
            if (ext.size() > 4) ext = ext.substr(0, 4);
            item.ext = ext;
        }
    }

    g_shelf.insert(g_shelf.begin(), std::move(item));
    g_shelfScroll = 0;
    return true;
}

inline bool AddShelfLink(const std::wstring& urlIn) {
    std::wstring url = TrimW(urlIn);
    if (url.empty()) return false;

    std::wstring low = url;
    for (auto& ch : low) ch = static_cast<wchar_t>(towlower(ch));
    if (low.rfind(L"www.", 0) == 0) url = L"https://" + url;

    for (const auto& it : g_shelf) {
        if (it.isLink && it.path == url) return false;
    }

    ShelfItem item;
    item.isLink = true;
    item.path = url;
    size_t scheme = url.find(L"://");
    item.display = (scheme == std::wstring::npos) ? url : url.substr(scheme + 3);
    if (!item.display.empty() && item.display.back() == L'/') item.display.pop_back();

    g_shelf.insert(g_shelf.begin(), std::move(item));
    g_shelfScroll = 0;
    return true;
}

inline int AddLinksFromText(const std::wstring& text) {
    int added = 0;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find(L'\n', pos);
        std::wstring line = TrimW(text.substr(pos, nl == std::wstring::npos ? std::wstring::npos : nl - pos));
        if (LooksLikeUrl(line) && AddShelfLink(line)) ++added;
        if (nl == std::wstring::npos) break;
        pos = nl + 1;
    }
    return added;
}

inline std::wstring ReadClipboardText(HWND hwnd) {
    std::wstring r;
    if (!OpenClipboard(hwnd)) return r;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
        if (p) {
            r = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return r;
}

inline void AddLinkFromClipboard(HWND hwnd) {
    int added = AddLinksFromText(ReadClipboardText(hwnd));
    SetShelfNotice(added > 0 ? L"Ссылка добавлена" : L"В буфере обмена нет ссылки");
}

inline void OpenShelfItem(int idx) {
    if (idx < 0 || idx >= static_cast<int>(g_shelf.size())) return;
    ShellExecuteW(nullptr, L"open", g_shelf[idx].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

inline int HitShelfChip(int x, int y) {
    for (int slot = 0; slot < 8; ++slot) {
        int idx = g_shelfScroll * 2 + slot;
        if (idx >= static_cast<int>(g_shelf.size())) break;
        if (PtInRectF(GetShelfChipRect(slot), x, y)) return idx;
    }
    return -1;
}

// --- OLE Drag & Drop ---
inline FORMATETC MakeFmt(CLIPFORMAT cf) {
    FORMATETC f = {};
    f.cfFormat = cf;
    f.ptd = nullptr;
    f.dwAspect = DVASPECT_CONTENT;
    f.lindex = -1;
    f.tymed = TYMED_HGLOBAL;
    return f;
}

inline CLIPFORMAT UrlFormat() {
    static CLIPFORMAT cf = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"UniformResourceLocatorW"));
    return cf;
}

inline bool DataHasShelfContent(IDataObject* pData) {
    FORMATETC f = MakeFmt(CF_HDROP);
    if (pData->QueryGetData(&f) == S_OK) return true;
    f = MakeFmt(UrlFormat());
    if (pData->QueryGetData(&f) == S_OK) return true;
    f = MakeFmt(CF_UNICODETEXT);
    return pData->QueryGetData(&f) == S_OK;
}

inline int ExtractIntoShelf(IDataObject* pData) {
    int added = 0;

    FORMATETC f = MakeFmt(CF_HDROP);
    STGMEDIUM stg = {};
    if (SUCCEEDED(pData->GetData(&f, &stg))) {
        HDROP hDrop = reinterpret_cast<HDROP>(stg.hGlobal);
        UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i) {
            UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
            std::wstring path(len, L'\0');
            DragQueryFileW(hDrop, i, &path[0], len + 1);
            if (AddShelfFile(path)) ++added;
        }
        ReleaseStgMedium(&stg);
        return added;
    }

    std::wstring text;
    auto readText = [&](CLIPFORMAT cf) -> bool {
        FORMATETC ft = MakeFmt(cf);
        STGMEDIUM sm = {};
        if (FAILED(pData->GetData(&ft, &sm))) return false;
        bool ok = false;
        if (sm.hGlobal) {
            const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(sm.hGlobal));
            if (p) {
                text = p;
                GlobalUnlock(sm.hGlobal);
                ok = true;
            }
        }
        ReleaseStgMedium(&sm);
        return ok;
        };
    if (readText(UrlFormat()) || readText(CF_UNICODETEXT)) {
        added += AddLinksFromText(text);
    }
    return added;
}

class ShelfDropTarget : public IDropTarget {
    LONG m_ref = 1;
    bool m_accept = false;

    DWORD ChooseEffect(DWORD allowed) const {
        if (!m_accept) return DROPEFFECT_NONE;
        if (allowed & DROPEFFECT_COPY) return DROPEFFECT_COPY;
        if (allowed & DROPEFFECT_LINK) return DROPEFFECT_LINK;
        return DROPEFFECT_NONE;
    }

public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropTarget)) {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_ref)); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = static_cast<ULONG>(InterlockedDecrement(&m_ref));
        if (r == 0) delete this;
        return r;
    }

    STDMETHODIMP DragEnter(IDataObject* pData, DWORD, POINTL, DWORD* pdwEffect) override {
        m_accept = !g_state.isDraggingOut && pData && DataHasShelfContent(pData);
        if (m_accept) {
            if (g_state.isTypingCity && g_hwnd) SetTypingMode(g_hwnd, false);
            g_state.isDragOver = true;
            g_state.isHovered = true;
            g_state.activeTab = Tab::Shelf;
            UpdateTargets();
            if (g_hwnd) RenderIsland(g_hwnd);
        }
        *pdwEffect = ChooseEffect(*pdwEffect);
        return S_OK;
    }

    STDMETHODIMP DragOver(DWORD, POINTL, DWORD* pdwEffect) override {
        *pdwEffect = ChooseEffect(*pdwEffect);
        return S_OK;
    }

    STDMETHODIMP DragLeave() override {
        g_state.isDragOver = false;
        UpdateTargets();
        return S_OK;
    }

    STDMETHODIMP Drop(IDataObject* pData, DWORD, POINTL, DWORD* pdwEffect) override {
        g_state.isDragOver = false;
        int added = 0;
        if (m_accept && pData) {
            added = ExtractIntoShelf(pData);
            if (added == 0) SetShelfNotice(L"Нужны файлы или ссылка");
        }
        *pdwEffect = (added > 0) ? ChooseEffect(*pdwEffect) : DROPEFFECT_NONE;
        ClampShelfScroll();
        UpdateTargets();
        if (g_hwnd) RenderIsland(g_hwnd);
        return S_OK;
    }
};

ShelfDropTarget* g_dropTarget = nullptr;

class ShelfDragSource : public IDropSource {
    LONG m_ref = 1;

public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDropSource)) {
            *ppv = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_ref)); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = static_cast<ULONG>(InterlockedDecrement(&m_ref));
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) override {
        if (fEscapePressed) return DRAGDROP_S_CANCEL;
        if (!(grfKeyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    STDMETHODIMP GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

inline void BeginShelfDragOut(HWND hwnd, int idx) {
    if (idx < 0 || idx >= static_cast<int>(g_shelf.size()) || g_shelf[idx].isLink) return;
    const std::wstring path = g_shelf[idx].path;

    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(SHParseDisplayName(path.c_str(), nullptr, &pidl, 0, nullptr)) || !pidl) {
        SetShelfNotice(L"Файл не найден");
        return;
    }

    IDataObject* pdo = nullptr;
    IShellFolder* psf = nullptr;
    PCUITEMID_CHILD child = nullptr;
    if (SUCCEEDED(SHBindToParent(pidl, IID_PPV_ARGS(&psf), &child))) {
        psf->GetUIObjectOf(nullptr, 1, &child, __uuidof(IDataObject), nullptr, reinterpret_cast<void**>(&pdo));
        psf->Release();
    }
    CoTaskMemFree(pidl);

    if (!pdo) {
        SetShelfNotice(L"Не удалось начать перетаскивание");
        return;
    }

    auto* src = new ShelfDragSource();
    DWORD effect = DROPEFFECT_NONE;
    g_state.isDraggingOut = true;
    HRESULT hr = DoDragDrop(pdo, src, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
    g_state.isDraggingOut = false;
    src->Release();
    pdo->Release();

    if (hr == DRAGDROP_S_DROP && effect != DROPEFFECT_NONE) {
        for (size_t i = 0; i < g_shelf.size(); ++i) {
            if (!g_shelf[i].isLink && g_shelf[i].path == path) {
                g_shelf.erase(g_shelf.begin() + static_cast<ptrdiff_t>(i));
                break;
            }
        }
        ClampShelfScroll();
    }
    UpdateTargets();
    RenderIsland(hwnd);
}

inline D2D1_COLOR_F ExtColor(const std::wstring& e) {
    auto in = [&](std::initializer_list<const wchar_t*> list) {
        for (const wchar_t* x : list) {
            if (e == x) return true;
        }
        return false;
        };
    if (in({ L"PNG", L"JPG", L"JPEG", L"GIF", L"WEBP", L"BMP", L"SVG", L"ICO", L"HEIC" }))
        return D2D1::ColorF(0.30f, 0.80f, 0.55f);
    if (in({ L"PDF", L"DOC", L"DOCX", L"TXT", L"RTF", L"XLS", L"XLSX", L"CSV", L"PPT", L"PPTX", L"MD", L"ODT" }))
        return D2D1::ColorF(0.42f, 0.65f, 1.0f);
    if (in({ L"ZIP", L"RAR", L"7Z", L"TAR", L"GZ", L"ISO" }))
        return D2D1::ColorF(1.0f, 0.66f, 0.30f);
    if (in({ L"MP3", L"WAV", L"FLAC", L"OGG", L"MP4", L"MKV", L"AVI", L"MOV", L"WEBM" }))
        return D2D1::ColorF(0.72f, 0.52f, 1.0f);
    if (in({ L"CPP", L"H", L"PY", L"JS", L"TS", L"JSON", L"HTML", L"CSS", L"CS" }))
        return D2D1::ColorF(0.35f, 0.80f, 0.85f);
    if (in({ L"EXE", L"MSI", L"BAT" }))
        return D2D1::ColorF(1.0f, 0.45f, 0.45f);
    return D2D1::ColorF(0.70f, 0.72f, 0.78f);
}

inline void DrawShelfTab(float left, float top, float right, const POINT& mp) {
    const ULONGLONG now = GetTickCount64();

    std::wstring status;
    bool isNotice = false;
    if (!g_shelfNotice.empty() && now < g_shelfNoticeUntil) {
        status = g_shelfNotice;
        isNotice = true;
    }
    else if (!g_shelf.empty()) {
        status = L"На полке: " + std::to_wstring(g_shelf.size());
    }
    if (!status.empty()) {
        D2D1_RECT_F sr = D2D1::RectF(left + 18.0f, top + 40.0f, right - 170.0f, top + 62.0f);
        g_pRT->DrawText(status.c_str(), static_cast<UINT32>(status.size()), g_pTextFormatLeftSub, sr,
            isNotice ? g_pBrushAccent : g_pBrushTextMuted);
    }

    auto drawButton = [&](const D2D1_RECT_F& r, const wchar_t* label) {
        bool hot = PtInRectF(r, mp.x, mp.y);
        if (hot) FillRR(r, 9.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f));
        g_pRT->DrawText(label, static_cast<UINT32>(wcslen(label)), g_pTextFormatSub, r,
            hot ? g_pBrushText : g_pBrushTextMuted);
        };
    drawButton(GetShelfLinkBtnRect(), L"+ Ссылка");
    if (!g_shelf.empty()) drawButton(GetShelfClearBtnRect(), L"Очистить");

    D2D1_RECT_F zone = GetShelfZoneRect();
    D2D1_ROUNDED_RECT zoneRR = D2D1::RoundedRect(zone, 18.0f, 18.0f);
    const bool over = g_state.isDragOver;
    if (over) {
        g_pBrushIcon->SetColor(D2D1::ColorF(0.42f, 0.72f, 1.0f, 0.10f));
        g_pRT->FillRoundedRectangle(zoneRR, g_pBrushIcon);
    }
    g_pBrushIcon->SetColor(over ? D2D1::ColorF(0.42f, 0.72f, 1.0f, 0.95f) : D2D1::ColorF(0.72f, 0.74f, 0.80f, 0.45f));
    g_pRT->DrawRoundedRectangle(zoneRR, g_pBrushIcon, 1.5f, g_pDashStyle);

    if (g_shelf.empty()) {
        float cx = (zone.left + zone.right) * 0.5f;
        float y0 = zone.top + 26.0f;
        g_pBrushIcon->SetColor(D2D1::ColorF(0.78f, 0.80f, 0.86f, over ? 1.0f : 0.85f));
        g_pRT->DrawLine(D2D1::Point2F(cx, y0), D2D1::Point2F(cx, y0 + 22.0f), g_pBrushIcon, 1.8f);
        g_pRT->DrawLine(D2D1::Point2F(cx - 7.0f, y0 + 15.0f), D2D1::Point2F(cx, y0 + 22.0f), g_pBrushIcon, 1.8f);
        g_pRT->DrawLine(D2D1::Point2F(cx + 7.0f, y0 + 15.0f), D2D1::Point2F(cx, y0 + 22.0f), g_pBrushIcon, 1.8f);
        g_pRT->DrawLine(D2D1::Point2F(cx - 11.0f, y0 + 30.0f), D2D1::Point2F(cx + 11.0f, y0 + 30.0f), g_pBrushIcon, 1.8f);

        const wchar_t* msg = over ? L"Отпустите, чтобы добавить на полку"
            : L"Перетащите файлы или ссылки на остров \u2014\nони подождут здесь";
        D2D1_RECT_F tr = D2D1::RectF(zone.left + 10.0f, zone.top + 68.0f, zone.right - 10.0f, zone.top + 118.0f);
        g_pRT->DrawText(msg, static_cast<UINT32>(wcslen(msg)), g_pTextFormatSub, tr, g_pBrushText);
        return;
    }

    for (int slot = 0; slot < 8; ++slot) {
        int idx = g_shelfScroll * 2 + slot;
        if (idx >= static_cast<int>(g_shelf.size())) break;
        const ShelfItem& it = g_shelf[idx];
        D2D1_RECT_F chip = GetShelfChipRect(slot);
        bool hot = PtInRectF(chip, mp.x, mp.y);
        FillRR(chip, 10.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, hot ? 0.13f : 0.07f));

        D2D1_RECT_F badge = D2D1::RectF(chip.left + 6.0f, chip.top + 5.0f, chip.left + 36.0f, chip.bottom - 5.0f);
        if (it.isFolder) {
            D2D1_COLOR_F fc = D2D1::ColorF(1.0f, 0.78f, 0.30f);
            FillRR(D2D1::RectF(badge.left + 2.0f, badge.top + 1.0f, badge.left + 13.0f, badge.top + 8.0f), 2.0f, fc);
            FillRR(D2D1::RectF(badge.left + 2.0f, badge.top + 4.0f, badge.right - 2.0f, badge.bottom), 3.0f, fc);
        }
        else {
            D2D1_COLOR_F c = it.isLink ? D2D1::ColorF(0.42f, 0.72f, 1.0f) : ExtColor(it.ext);
            FillRR(badge, 6.0f, D2D1::ColorF(c.r, c.g, c.b, 0.22f));
            g_pBrushIcon->SetColor(D2D1::ColorF(c.r, c.g, c.b, 1.0f));
            std::wstring label = it.isLink ? std::wstring(L"\u2197") : (it.ext.empty() ? std::wstring(L"FILE") : it.ext);
            g_pRT->DrawText(label.c_str(), static_cast<UINT32>(label.size()), g_pTextFormatBadge, badge, g_pBrushIcon);
        }

        D2D1_RECT_F nameRect = D2D1::RectF(chip.left + 42.0f, chip.top, chip.right - (hot ? 28.0f : 10.0f), chip.bottom);
        if (hot) DrawScrollingText(SLOT_SHELF_HOVER, it.display, g_pTextFormatLeftSub, nameRect, g_pBrushText);
        else DrawClippedText(it.display, g_pTextFormatLeftSub, nameRect, g_pBrushText);

        if (hot) {
            D2D1_RECT_F cr = GetShelfChipCloseRect(chip);
            bool hotClose = PtInRectF(cr, mp.x, mp.y);
            if (hotClose) FillRR(cr, 8.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.16f));
            g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatSub, cr, hotClose ? g_pBrushText : g_pBrushTextMuted);
        }
    }
}

struct ClipHistoryItem {
    std::wstring text;
    std::wstring timeStr;
};

inline std::vector<ClipHistoryItem> g_clipHistory;
inline int g_clipScroll = 0;
inline DWORD g_lastClipSeq = 0;
inline std::wstring g_clipNotice;
inline ULONGLONG g_clipNoticeUntil = 0;

inline std::wstring g_notesText = L"Быстрые заметки прямо в Острове.\nКликните сюда, чтобы печатать текст...";
inline std::wstring g_calDayNote = L"";
inline int g_calYear = 0;
inline int g_calMonth = 0;
inline int g_calSelectedDay = 0;

inline void SaveExtraTabsData() {
    // Сохраняем заметку вкладки «Текст»
    std::wstring notesFile = GetAppDirPath() + L"island_notes.txt";
    std::ofstream fn(notesFile, std::ios::binary | std::ios::trunc);
    if (fn.is_open()) {
        std::string u8 = WideToUtf8(g_notesText);
        fn.write(u8.data(), u8.size());
    }

    // Сохраняем историю буфера обмена, если включено «Хранить историю после перезапуска»
    std::wstring clipFile = GetAppDirPath() + L"island_clipboard.txt";
    if (!g_cfg.persistClipboard) {
        DeleteFileW(clipFile.c_str());
        return;
    }
    std::ofstream fc(clipFile, std::ios::binary | std::ios::trunc);
    if (fc.is_open()) {
        for (const auto& it : g_clipHistory) {
            std::wstring flat = it.text;
            for (auto& c : flat) if (c == L'\n' || c == L'\r') c = L' ';
            std::string line = WideToUtf8(it.timeStr + L"\t" + flat) + "\n";
            fc.write(line.data(), line.size());
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
        if (!content.empty()) g_notesText = Utf8ToWide(content);
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

    // Не дублируем подряд одинаковые строки
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

// --- 1. Отрисовка и клики вкладки «Буфер обмена» ---
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

// --- 2. Отрисовка и логика вкладки «Календарь» ---
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

inline void DrawCalendarTab(float left, float top, float right, const POINT& mp) {
    static const wchar_t* kMonths[12] = {
        L"Январь", L"Февраль", L"Март", L"Апрель", L"Май", L"Июнь",
        L"Июль", L"Август", L"Сентябрь", L"Октябрь", L"Ноябрь", L"Декабрь"
    };
    SYSTEMTIME stNow;
    GetLocalTime(&stNow);
    if (g_calYear == 0) { g_calYear = stNow.wYear; g_calMonth = stNow.wMonth; g_calSelectedDay = stNow.wDay; }

    // Левая карточка: крупное число и заметка на день
    D2D1_RECT_F leftCard = D2D1::RectF(left + 14.0f, top + 44.0f, left + 168.0f, top + 224.0f);
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(leftCard, 14.0f, 14.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(leftCard, 14.0f, 14.0f), g_pBrushBorder, 1.0f);

    std::wstring mTitle = std::wstring(kMonths[std::clamp(g_calMonth - 1, 0, 11)]) + L" " + std::to_wstring(g_calYear);
    g_pRT->DrawText(mTitle.c_str(), (UINT32)mTitle.size(), g_pTextFormatLeftTitle,
        D2D1::RectF(leftCard.left + 12.0f, leftCard.top + 8.0f, leftCard.right - 8.0f, leftCard.top + 30.0f), g_pBrushAccent);

    std::wstring bigDay = std::to_wstring(g_calSelectedDay);
    g_pRT->DrawText(bigDay.c_str(), (UINT32)bigDay.size(), g_pTextFormatBig,
        D2D1::RectF(leftCard.left + 12.0f, leftCard.top + 32.0f, leftCard.right - 12.0f, leftCard.top + 82.0f), g_pBrushText);

    // Кнопки переключения месяца < Сегодня >
    D2D1_RECT_F bPrev = D2D1::RectF(leftCard.left + 10.0f, leftCard.top + 86.0f, leftCard.left + 38.0f, leftCard.top + 110.0f);
    D2D1_RECT_F bToday = D2D1::RectF(leftCard.left + 42.0f, leftCard.top + 86.0f, leftCard.right - 42.0f, leftCard.top + 110.0f);
    D2D1_RECT_F bNext = D2D1::RectF(leftCard.right - 38.0f, leftCard.top + 86.0f, leftCard.right - 10.0f, leftCard.top + 110.0f);
    FillRR(bPrev, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(bPrev, mp.x, mp.y) ? 0.16f : 0.07f));
    FillRR(bToday, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(bToday, mp.x, mp.y) ? 0.16f : 0.07f));
    FillRR(bNext, 6.0f, D2D1::ColorF(1, 1, 1, PtInRectF(bNext, mp.x, mp.y) ? 0.16f : 0.07f));
    g_pRT->DrawText(L"<", 1, g_pTextFormatSub, bPrev, g_pBrushText);
    g_pRT->DrawText(L"Сегодня", 7, g_pTextFormatTime, bToday, g_pBrushText);
    g_pRT->DrawText(L">", 1, g_pTextFormatSub, bNext, g_pBrushText);

    // Поле быстрой заметки на день
    D2D1_RECT_F noteBox = D2D1::RectF(leftCard.left + 10.0f, leftCard.top + 118.0f, leftCard.right - 10.0f, leftCard.bottom - 10.0f);
    FillRR(noteBox, 8.0f, D2D1::ColorF(1, 1, 1, 0.05f));
    if (g_state.isTypingCalNote) {
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(noteBox, 8.0f, 8.0f), g_pBrushAccent, 1.2f);
    }
    std::wstring noteShow = g_calDayNote.empty()
        ? (g_state.isTypingCalNote ? L"|" : L"+ Событие дня...")
        : (g_calDayNote + (g_state.isTypingCalNote ? L"|" : L""));
    g_pRT->DrawText(noteShow.c_str(), (UINT32)noteShow.size(), g_pTextFormatSmallMuted,
        D2D1::RectF(noteBox.left + 6.0f, noteBox.top + 4.0f, noteBox.right - 6.0f, noteBox.bottom - 4.0f),
        g_calDayNote.empty() ? g_pBrushTextMuted : g_pBrushText);

    // Правая часть: сетка дней месяца 7 колонок
    float gx0 = left + 178.0f;
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
    }
}

// --- 3. Отрисовка вкладки «Текст» (Быстрый блокнот) ---
inline D2D1_RECT_F GetNotesCopyBtnRect() {
    float right = IslandLeft() + g_state.curW;
    return D2D1::RectF(right - 178.0f, ISLAND_TOP + 40.0f, right - 96.0f, ISLAND_TOP + 62.0f);
}

inline D2D1_RECT_F GetNotesClearBtnRect() {
    float right = IslandLeft() + g_state.curW;
    return D2D1::RectF(right - 90.0f, ISLAND_TOP + 40.0f, right - 14.0f, ISLAND_TOP + 62.0f);
}

inline D2D1_RECT_F GetNotesEditorRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    return D2D1::RectF(left + 14.0f, ISLAND_TOP + 66.0f, right - 14.0f, ISLAND_TOP + 224.0f);
}

inline void DrawNotesTab(float left, float top, float right, const POINT& mp) {
    std::wstring status = g_state.isTypingNote
        ? L"Режим ввода активен (Esc — готово, Ctrl+V — вставить)"
        : L"Кликните по полю ниже для редактирования";
    g_pRT->DrawText(status.c_str(), (UINT32)status.size(), g_pTextFormatLeftSub,
        D2D1::RectF(left + 18.0f, top + 40.0f, right - 184.0f, top + 62.0f),
        g_state.isTypingNote ? g_pBrushAccent : g_pBrushTextMuted);

    D2D1_RECT_F bCopy = GetNotesCopyBtnRect();
    D2D1_RECT_F bClr = GetNotesClearBtnRect();
    if (PtInRectF(bCopy, mp.x, mp.y)) FillRR(bCopy, 8.0f, D2D1::ColorF(1, 1, 1, 0.10f));
    if (PtInRectF(bClr, mp.x, mp.y)) FillRR(bClr, 8.0f, D2D1::ColorF(1, 1, 1, 0.10f));
    g_pRT->DrawText(L"Копировать", 10, g_pTextFormatSub, bCopy, g_pBrushText);
    g_pRT->DrawText(L"Очистить", 8, g_pTextFormatSub, bClr, g_pBrushTextMuted);

    D2D1_RECT_F ed = GetNotesEditorRect();
    g_pRT->FillRoundedRectangle(D2D1::RoundedRect(ed, 14.0f, 14.0f), g_pBrushInputBg);
    g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(ed, 14.0f, 14.0f),
        g_state.isTypingNote ? g_pBrushAccent : g_pBrushBorder, g_state.isTypingNote ? 1.4f : 1.0f);

    std::wstring disp = g_notesText + (g_state.isTypingNote ? L"|" : L"");
    D2D1_RECT_F tr = D2D1::RectF(ed.left + 12.0f, ed.top + 10.0f, ed.right - 12.0f, ed.bottom - 10.0f);
    g_pRT->PushAxisAlignedClip(tr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_pRT->DrawText(disp.c_str(), (UINT32)disp.size(), g_pTextFormatSmallMuted, tr, g_pBrushText);
    g_pRT->PopAxisAlignedClip();
}

// --- 4. Отрисовка и кнопки вкладки «Игры» (Игровой монитор) ---
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

    // Обновляем историю FPS для спарклайн-графика
    static ULONGLONG s_lastHistTick = 0;
    ULONGLONG now = GetTickCount64();
    if (now - s_lastHistTick > 250) {
        s_lastHistTick = now;
        g_game.fpsHistory.push_back(static_cast<float>(fps));
        if (g_game.fpsHistory.size() > 36) g_game.fpsHistory.erase(g_game.fpsHistory.begin());
    }

    // Карточка 1 (Слева): Крупный FPS + Frametime + живой график
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

    // Живой спарклайн-график стабильности FPS
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

    // Карточка 2 (Справа): Полоски CPU, GPU + VRAM, RAM и PING
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

    // Нижние кнопки управления Игровым режимом
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