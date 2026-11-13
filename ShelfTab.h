#pragma once
#include "Common.h"
#include "Graphics.h"

struct ShelfItem {
    bool isUrl = false;
    std::wstring path;
    std::wstring label;
    ID2D1Bitmap* icon = nullptr;
};

std::vector<ShelfItem> g_shelf;
int g_shelfScroll = 0;
int g_pressedItem = -1;
POINT g_pressPt = { 0, 0 };

inline D2D1_RECT_F GetShelfZoneRect() {
    float left = IslandLeft();
    float right = left + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(left + 14.0f, top + 66.0f, right - 14.0f, top + 226.0f);
}

inline D2D1_RECT_F GetShelfLinkBtnRect() {
    float right = IslandLeft() + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(right - 14.0f - 82.0f - 6.0f - 96.0f, top + 40.0f, right - 14.0f - 82.0f - 6.0f, top + 62.0f);
}

inline D2D1_RECT_F GetShelfClearBtnRect() {
    float right = IslandLeft() + g_state.curW;
    float top = ISLAND_TOP;
    return D2D1::RectF(right - 14.0f - 82.0f, top + 40.0f, right - 14.0f, top + 62.0f);
}

inline D2D1_RECT_F GetShelfChipRect(int visIdx) {
    D2D1_RECT_F z = GetShelfZoneRect();
    int col = visIdx % 2;
    int row = visIdx / 2;
    float gap = 8.0f;
    float pad = 8.0f;
    float cellW = ((z.right - z.left) - 2.0f * pad - gap) * 0.5f;
    float cellH = 44.0f;
    float x = z.left + pad + col * (cellW + gap);
    float y = z.top + pad + row * (cellH + 6.0f);
    return D2D1::RectF(x, y, x + cellW, y + cellH);
}

inline D2D1_RECT_F GetShelfChipCloseRect(const D2D1_RECT_F& chip) {
    float cy = (chip.top + chip.bottom) * 0.5f;
    return D2D1::RectF(chip.right - 24.0f, cy - 10.0f, chip.right - 4.0f, cy + 10.0f);
}

inline void ClampShelfScroll() {
    int rows = (static_cast<int>(g_shelf.size()) + 1) / 2;
    int maxScroll = std::max(0, rows - 3);
    g_shelfScroll = std::clamp(g_shelfScroll, 0, maxScroll);
}

inline int HitShelfChip(int x, int y) {
    for (int v = 0; v < 6; ++v) {
        int idx = g_shelfScroll * 2 + v;
        if (idx >= static_cast<int>(g_shelf.size())) break;
        if (PtInRectF(GetShelfChipRect(v), x, y)) return idx;
    }
    return -1;
}

inline ID2D1Bitmap* IconBitmapFromHICON(HICON hIcon) {
    if (!hIcon || !g_pRT) return nullptr;
    IWICImagingFactory* wic = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
        return nullptr;

    IWICBitmap* wicBmp = nullptr;
    ID2D1Bitmap* d2dBmp = nullptr;
    if (SUCCEEDED(wic->CreateBitmapFromHICON(hIcon, &wicBmp))) {
        IWICFormatConverter* conv = nullptr;
        if (SUCCEEDED(wic->CreateFormatConverter(&conv))) {
            if (SUCCEEDED(conv->Initialize(wicBmp, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
                g_pRT->CreateBitmapFromWicBitmap(conv, nullptr, &d2dBmp);
            }
            conv->Release();
        }
        wicBmp->Release();
    }
    wic->Release();
    return d2dBmp;
}

inline ID2D1Bitmap* GetFileIconBitmap(const std::wstring& path, bool isUrl) {
    SHFILEINFOW sfi = {};
    UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
    LPCWSTR query = path.c_str();
    DWORD attrs = 0;
    if (isUrl) {
        query = L".html";
        attrs = FILE_ATTRIBUTE_NORMAL;
        flags |= SHGFI_USEFILEATTRIBUTES;
    }
    if (!SHGetFileInfoW(query, attrs, &sfi, sizeof(sfi), flags) || !sfi.hIcon)
        return nullptr;
    ID2D1Bitmap* bmp = IconBitmapFromHICON(sfi.hIcon);
    DestroyIcon(sfi.hIcon);
    return bmp;
}

inline std::wstring TrimW(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

inline bool LooksLikeUrl(const std::wstring& s) {
    return s.rfind(L"http://", 0) == 0 || s.rfind(L"https://", 0) == 0 || s.rfind(L"www.", 0) == 0;
}

inline std::wstring MakeShelfLabel(const std::wstring& path, bool isUrl) {
    if (isUrl) {
        std::wstring u = path;
        size_t p = u.find(L"://");
        if (p != std::wstring::npos) u = u.substr(p + 3);
        if (u.rfind(L"www.", 0) == 0) u = u.substr(4);
        while (!u.empty() && u.back() == L'/') u.pop_back();
        return u.empty() ? path : u;
    }
    std::wstring p = path;
    while (!p.empty() && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    size_t slash = p.find_last_of(L"\\/");
    return (slash != std::wstring::npos && slash + 1 < p.size()) ? p.substr(slash + 1) : p;
}

inline void AddShelfEntry(const std::wstring& raw, bool isUrl) {
    std::wstring path = TrimW(raw);
    if (path.empty()) return;
    if (isUrl && path.rfind(L"www.", 0) == 0) path = L"https://" + path;

    for (auto it = g_shelf.begin(); it != g_shelf.end(); ++it) {
        if (_wcsicmp(it->path.c_str(), path.c_str()) == 0) {
            ShelfItem moved = *it;
            g_shelf.erase(it);
            g_shelf.insert(g_shelf.begin(), moved);
            g_shelfScroll = 0;
            return;
        }
    }

    ShelfItem item;
    item.isUrl = isUrl;
    item.path = path;
    item.label = MakeShelfLabel(path, isUrl);
    item.icon = GetFileIconBitmap(path, isUrl);
    g_shelf.insert(g_shelf.begin(), item);
    if (g_shelf.size() > 24) {
        if (g_shelf.back().icon) g_shelf.back().icon->Release();
        g_shelf.pop_back();
    }
    g_shelfScroll = 0;
}

inline std::wstring ReadClipboardText(HWND hwnd) {
    std::wstring out;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return out;
    if (!OpenClipboard(hwnd)) return out;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
        if (p) {
            out = p;
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

inline void AddLinkFromClipboard(HWND hwnd) {
    std::wstring txt = TrimW(ReadClipboardText(hwnd));
    if (txt.empty()) return;
    DWORD attr = GetFileAttributesW(txt.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        AddShelfEntry(txt, false);
    }
    else {
        if (!LooksLikeUrl(txt) && txt.find(L'.') != std::wstring::npos && txt.find(L' ') == std::wstring::npos) {
            txt = L"https://" + txt;
        }
        AddShelfEntry(txt, true);
    }
}

inline void OpenShelfItem(int idx) {
    if (idx < 0 || idx >= static_cast<int>(g_shelf.size())) return;
    ShellExecuteW(nullptr, L"open", g_shelf[idx].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

class SimpleDropSource : public IDropSource {
    LONG m_ref = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) { *ppv = static_cast<IDropSource*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&m_ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEsc, DWORD keyState) override {
        if (fEsc) return DRAGDROP_S_CANCEL;
        if (!(keyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

class FileDropObject : public IDataObject {
    LONG m_ref = 1;
    std::wstring m_text;
    bool m_isFile = false;

    static HGLOBAL MakeDropFiles(const std::wstring& path) {
        size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GHND | GMEM_SHARE, bytes);
        if (!h) return nullptr;
        auto* df = static_cast<DROPFILES*>(GlobalLock(h));
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        wchar_t* dst = reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>(df) + sizeof(DROPFILES));
        memcpy(dst, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
        dst[path.size() + 1] = 0;
        GlobalUnlock(h);
        return h;
    }
    static HGLOBAL MakeUnicodeText(const std::wstring& s) {
        size_t bytes = (s.size() + 1) * sizeof(wchar_t);
        HGLOBAL h = GlobalAlloc(GHND | GMEM_SHARE, bytes);
        if (!h) return nullptr;
        void* p = GlobalLock(h);
        memcpy(p, s.c_str(), bytes);
        GlobalUnlock(h);
        return h;
    }
public:
    FileDropObject(const std::wstring& s, bool isFile) : m_text(s), m_isFile(isFile) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) { *ppv = static_cast<IDataObject*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&m_ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* fmt, STGMEDIUM* med) override {
        if (!fmt || !med) return E_POINTER;
        if (!(fmt->tymed & TYMED_HGLOBAL)) return DV_E_TYMED;
        ZeroMemory(med, sizeof(*med));
        med->tymed = TYMED_HGLOBAL;
        if (fmt->cfFormat == CF_HDROP && m_isFile) {
            med->hGlobal = MakeDropFiles(m_text);
            return med->hGlobal ? S_OK : E_OUTOFMEMORY;
        }
        if (fmt->cfFormat == CF_UNICODETEXT) {
            med->hGlobal = MakeUnicodeText(m_text);
            return med->hGlobal ? S_OK : E_OUTOFMEMORY;
        }
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* fmt) override {
        if (!fmt || !(fmt->tymed & TYMED_HGLOBAL)) return DV_E_FORMATETC;
        if (fmt->cfFormat == CF_HDROP && m_isFile) return S_OK;
        if (fmt->cfFormat == CF_UNICODETEXT) return S_OK;
        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override { if (out) out->ptd = nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC** ppEnum) override {
        if (dir != DATADIR_GET) return E_NOTIMPL;
        FORMATETC fmts[2] = {
            { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL },
            { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }
        };
        return SHCreateStdEnumFmtEtc(m_isFile ? 2 : 1, m_isFile ? fmts : &fmts[1], ppEnum);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
};

inline void BeginShelfDragOut(HWND hwnd, int idx) {
    if (idx < 0 || idx >= static_cast<int>(g_shelf.size())) return;
    const ShelfItem item = g_shelf[idx];
    g_state.isDraggingOut = true;
    IDataObject* dobj = new FileDropObject(item.path, !item.isUrl);
    IDropSource* dsrc = new SimpleDropSource();
    DWORD effect = 0;
    DoDragDrop(dobj, dsrc, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
    dsrc->Release();
    dobj->Release();
    g_state.isDraggingOut = false;
    UpdateTargets();
    RenderIsland(hwnd);
}

class ShelfDropTarget : public IDropTarget {
    LONG m_ref = 1;
    static bool HasSupportedFormat(IDataObject* d) {
        if (!d) return false;
        FORMATETC f1 = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        FORMATETC f2 = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return SUCCEEDED(d->QueryGetData(&f1)) || SUCCEEDED(d->QueryGetData(&f2));
    }
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) { *ppv = static_cast<IDropTarget*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&m_ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* d, DWORD, POINTL, DWORD* eff) override {
        if (g_state.isDraggingOut || !HasSupportedFormat(d)) { *eff = DROPEFFECT_NONE; return S_OK; }
        g_state.isDragOver = true;
        g_state.isHovered = true;
        g_state.activeTab = Tab::Shelf;
        UpdateTargets();
        if (g_hwnd) RenderIsland(g_hwnd);
        *eff = DROPEFFECT_COPY;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* eff) override {
        *eff = g_state.isDragOver ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        g_state.isDragOver = false;
        UpdateTargets();
        if (g_hwnd) RenderIsland(g_hwnd);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* d, DWORD, POINTL, DWORD* eff) override {
        g_state.isDragOver = false;
        *eff = DROPEFFECT_NONE;
        if (!d || g_state.isDraggingOut) return S_OK;

        FORMATETC fmtDrop = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM med = {};
        if (SUCCEEDED(d->GetData(&fmtDrop, &med))) {
            HDROP hDrop = static_cast<HDROP>(GlobalLock(med.hGlobal));
            if (hDrop) {
                UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i) {
                    UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
                    std::wstring path(len, L'\0');
                    DragQueryFileW(hDrop, i, path.data(), len + 1);
                    AddShelfEntry(path, false);
                }
                GlobalUnlock(med.hGlobal);
                *eff = DROPEFFECT_COPY;
            }
            ReleaseStgMedium(&med);
        }
        else {
            FORMATETC fmtTxt = { CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            if (SUCCEEDED(d->GetData(&fmtTxt, &med))) {
                const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(med.hGlobal));
                if (p) {
                    std::wstring s = TrimW(p);
                    GlobalUnlock(med.hGlobal);
                    DWORD attr = GetFileAttributesW(s.c_str());
                    AddShelfEntry(s, attr == INVALID_FILE_ATTRIBUTES);
                    *eff = DROPEFFECT_COPY;
                }
                ReleaseStgMedium(&med);
            }
        }
        UpdateTargets();
        if (g_hwnd) RenderIsland(g_hwnd);
        return S_OK;
    }
};

ShelfDropTarget* g_dropTarget = nullptr;

inline void DrawShelfTab(float left, float top, float right, const POINT& mp) {
    std::wstring countTxt = g_shelf.empty()
        ? L"Полка пуста"
        : (L"На полке: " + std::to_wstring(g_shelf.size()));
    D2D1_RECT_F cntRect = D2D1::RectF(left + 18.0f, top + 40.0f, left + 190.0f, top + 62.0f);
    g_pRT->DrawText(countTxt.c_str(), static_cast<UINT32>(countTxt.size()), g_pTextFormatLeftSub, cntRect, g_pBrushTextMuted);

    D2D1_RECT_F linkBtn = GetShelfLinkBtnRect();
    bool hotLink = PtInRectF(linkBtn, mp.x, mp.y);
    FillRR(linkBtn, 9.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, hotLink ? 0.14f : 0.07f));
    const wchar_t* linkLbl = L"+ Из буфера";
    g_pRT->DrawText(linkLbl, static_cast<UINT32>(wcslen(linkLbl)), g_pTextFormatSub, linkBtn,
        hotLink ? g_pBrushAccent : g_pBrushText);

    if (!g_shelf.empty()) {
        D2D1_RECT_F clrBtn = GetShelfClearBtnRect();
        bool hotClr = PtInRectF(clrBtn, mp.x, mp.y);
        if (hotClr) FillRR(clrBtn, 9.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.10f));
        const wchar_t* clrLbl = L"Очистить";
        g_pRT->DrawText(clrLbl, static_cast<UINT32>(wcslen(clrLbl)), g_pTextFormatSub, clrBtn,
            hotClr ? g_pBrushText : g_pBrushTextMuted);
    }

    D2D1_RECT_F zone = GetShelfZoneRect();
    D2D1_ROUNDED_RECT zoneRR = D2D1::RoundedRect(zone, 16.0f, 16.0f);
    g_pRT->FillRoundedRectangle(zoneRR, g_pBrushInputBg);

    if (g_shelf.empty() || g_state.isDragOver) {
        ID2D1Brush* borderBr = g_state.isDragOver ? g_pBrushAccent : g_pBrushBorder;
        g_pRT->DrawRoundedRectangle(zoneRR, borderBr, g_state.isDragOver ? 1.6f : 1.2f, g_pDashStyle);
    }
    else {
        g_pRT->DrawRoundedRectangle(zoneRR, g_pBrushBorder, 1.0f);
    }

    if (g_shelf.empty()) {
        const wchar_t* hint = g_state.isDragOver
            ? L"Отпустите, чтобы положить на полку"
            : L"Перетащите сюда файлы, папки или ссылки\nили нажмите «+ Из буфера»";
        D2D1_RECT_F hr = D2D1::RectF(zone.left + 16.0f, zone.top + 20.0f, zone.right - 16.0f, zone.bottom - 20.0f);
        g_pRT->DrawText(hint, static_cast<UINT32>(wcslen(hint)), g_pTextFormatSub, hr,
            g_state.isDragOver ? g_pBrushAccent : g_pBrushTextMuted);
        return;
    }

    g_pRT->PushAxisAlignedClip(zone, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (int v = 0; v < 6; ++v) {
        int idx = g_shelfScroll * 2 + v;
        if (idx >= static_cast<int>(g_shelf.size())) break;
        const ShelfItem& it = g_shelf[idx];

        D2D1_RECT_F chip = GetShelfChipRect(v);
        bool hot = PtInRectF(chip, mp.x, mp.y);
        FillRR(chip, 11.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, hot ? 0.14f : 0.07f));
        g_pRT->DrawRoundedRectangle(D2D1::RoundedRect(chip, 11.0f, 11.0f), g_pBrushBorder, 1.0f);

        float cy = (chip.top + chip.bottom) * 0.5f;
        D2D1_RECT_F icoRect = D2D1::RectF(chip.left + 8.0f, cy - 13.0f, chip.left + 34.0f, cy + 13.0f);
        if (it.icon) {
            g_pRT->DrawBitmap(it.icon, icoRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        else {
            FillRR(icoRect, 6.0f, D2D1::ColorF(0.42f, 0.72f, 1.0f, 0.22f));
            const wchar_t* sym = it.isUrl ? L"URL" : L"FILE";
            g_pRT->DrawText(sym, static_cast<UINT32>(wcslen(sym)), g_pTextFormatBadge, icoRect, g_pBrushAccent);
        }

        D2D1_RECT_F closeRect = GetShelfChipCloseRect(chip);
        bool hotClose = PtInRectF(closeRect, mp.x, mp.y);

        D2D1_RECT_F txtRect = D2D1::RectF(chip.left + 40.0f, chip.top + 4.0f, closeRect.left - 2.0f, chip.bottom - 4.0f);
        if (hot && !hotClose) {
            DrawScrollingText(SLOT_SHELF_HOVER, it.label, g_pTextFormatLeftSub, txtRect, g_pBrushText, 40.0f);
        }
        else {
            DrawClippedText(it.label, g_pTextFormatLeftSub, txtRect, g_pBrushText);
        }

        if (hotClose) {
            FillRR(closeRect, 8.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f));
        }
        g_pRT->DrawText(L"\u00D7", 1, g_pTextFormatSub, closeRect, hotClose ? g_pBrushText : g_pBrushTextMuted);
    }

    g_pRT->PopAxisAlignedClip();
}