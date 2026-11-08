#include "renderer.h"
#include "init.h"
#include "document.h"
#include "store.h"

#include <objidl.h>
#include <gdiplus.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shobjidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

#define SLIDE_KEY RGB(255, 0, 255)

#define LAYOUT_WIDTH 800
#define LAYOUT_HEIGHT 480
#define SLIDE_FALLBACK_WIDTH 1920
#define SLIDE_FALLBACK_HEIGHT 1080
#define MAX_SLIDES 64
#define DEMO_SLIDE_COUNT 8
#define MAX_ELEMENTS 32
#define DOCUMENT_WIDTH 1920
#define DOCUMENT_HEIGHT 1080
#define SLIDE_TEXT 4
#define SLIDE_PHOTO 5
#define SLIDE_FREEFORM 6
#define SLIDE_VIDEO 7

enum ElementKind
{
    ELEMENT_TEXT,
    ELEMENT_RECT,
    ELEMENT_ELLIPSE,
    ELEMENT_IMAGE,
    ELEMENT_VIDEO
};

enum SlideRole
{
    SLIDE_ROLE_SLIDE = 0,
    SLIDE_ROLE_BACKGROUND = 1
};

struct SlideElement
{
    ElementKind kind;
    int x;
    int y;
    int w;
    int h;
    float rotation;
    COLORREF fill;
    COLORREF textColor;
    int fontSize;
    char text[1024];
    char fontName[64];
    bool fontBold;
    bool fontItalic;
    bool fontUnderline;
    int align;
    char slot[32];
    Gdiplus::Image* image;
    bool placeholderIfMissing;
    ImageDisplayMode displayMode;
    wchar_t videoPath[MAX_PATH];
    bool locked;
};

struct Slide
{
    char id[32];
    int width;
    int height;
    COLORREF background;
    int role;
    int transition;
    int transitionDir;
    int transitionMs;
    bool transitionOverride;
    SlideElement elements[MAX_ELEMENTS];
    int elementCount;
};

static Slide g_slides[MAX_SLIDES] = {};
static HBITMAP g_frames[MAX_SLIDES] = {};
static int g_defaultSlideTransition = SLIDE_TRANSITION_CROSSFADE;
static int g_defaultSlideTransitionDir = SLIDE_TRANSITION_DIR_LEFT;
static int g_defaultSlideTransitionMs = 300;
static int g_defaultBgTransition = SLIDE_TRANSITION_CROSSFADE;
static int g_defaultBgTransitionDir = SLIDE_TRANSITION_DIR_LEFT;
static int g_defaultBgTransitionMs = 300;
static int g_slideCount = 0;
static int g_currentSlide = 0;
static int g_editSlide = 0;
static int g_backgroundSlide = -1;
static int g_nextSlideSerial = 1;
static Gdiplus::Image* g_photo = nullptr;
static Gdiplus::Image* g_slide5Background = nullptr;
static Gdiplus::Image* g_slide6Photo = nullptr;
static bool g_started = false;
static void EnsureSlideId(Slide* slide);
static int ClampSlideTransition(int transition);
static int ClampSlideTransitionDir(int direction);
static int ClampTransitionMs(int milliseconds);
static HBITMAP g_editorCache = nullptr;
static int g_editorCacheW = 0;
static int g_editorCacheH = 0;
static bool g_editorDragging = false;
static HBITMAP g_liveOverlay = nullptr;
static int g_liveOverlayW = 0;
static int g_liveOverlayH = 0;
static SlideElement g_elementClipboard = {};
static bool g_hasElementClipboard = false;
static int g_canvasW = SLIDE_FALLBACK_WIDTH;
static int g_canvasH = SLIDE_FALLBACK_HEIGHT;

static BOOL CALLBACK FindPrimaryMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoA(monitor, &info))
    {
        return TRUE;
    }

    RECT* rect = reinterpret_cast<RECT*>(data);
    if ((info.dwFlags & MONITORINFOF_PRIMARY) != 0)
    {
        *rect = info.rcMonitor;
        return FALSE;
    }

    if (rect->right == rect->left && rect->bottom == rect->top)
    {
        *rect = info.rcMonitor;
    }
    return TRUE;
}

static void ReadDefaultDisplaySize(int* width, int* height)
{
    RECT monitor = {};
    EnumDisplayMonitors(nullptr, nullptr, FindPrimaryMonitor, reinterpret_cast<LPARAM>(&monitor));
    int w = monitor.right - monitor.left;
    int h = monitor.bottom - monitor.top;
    if (w < 320 || h < 240)
    {
        w = SLIDE_FALLBACK_WIDTH;
        h = SLIDE_FALLBACK_HEIGHT;
    }
    *width = w;
    *height = h;
}

static int LayoutX(int x)
{
    return MulDiv(x, g_canvasW, LAYOUT_WIDTH);
}

static int LayoutY(int y)
{
    return MulDiv(y, g_canvasH, LAYOUT_HEIGHT);
}

static int LayoutS(int size)
{
    return MulDiv(size, g_canvasH, LAYOUT_HEIGHT);
}

static int MapValue(int value, int source, int dest)
{
    if (source == 0)
    {
        return 0;
    }
    return MulDiv(value, dest, source);
}

static bool g_posterMf = false;

static bool EnsurePosterMf()
{
    if (g_posterMf)
    {
        return true;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr) && hr != MF_E_ALREADY_INITIALIZED)
    {
        return false;
    }
    g_posterMf = true;
    return true;
}

static void PathToFileUrl(const wchar_t* path, wchar_t* url, int urlChars)
{
    if (path == nullptr || url == nullptr || urlChars <= 0)
    {
        return;
    }
    if (wcsstr(path, L"://") != nullptr)
    {
        lstrcpynW(url, path, urlChars);
        return;
    }

    wchar_t full[MAX_PATH] = {};
    const DWORD got = GetFullPathNameW(path, MAX_PATH, full, nullptr);
    if (got == 0 || got >= MAX_PATH)
    {
        lstrcpynW(full, path, MAX_PATH);
    }
    for (wchar_t* p = full; *p != L'\0'; ++p)
    {
        if (*p == L'\\')
        {
            *p = L'/';
        }
    }
    wchar_t prefixed[MAX_PATH * 2] = {};
    wsprintfW(prefixed, L"file:///%s", full);
    lstrcpynW(url, prefixed, urlChars);
}

static Gdiplus::Image* PosterFromHbitmap(HBITMAP hbmp)
{
    if (hbmp == nullptr)
    {
        return nullptr;
    }
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hbmp, nullptr);
    DeleteObject(hbmp);
    if (bitmap == nullptr || bitmap->GetLastStatus() != Gdiplus::Ok)
    {
        delete bitmap;
        return nullptr;
    }
    return bitmap;
}

static Gdiplus::Image* LoadVideoPosterShell(const wchar_t* path)
{
    IShellItem* item = nullptr;
    if (FAILED(SHCreateItemFromParsingName(path, nullptr, IID_PPV_ARGS(&item))) || item == nullptr)
    {
        return nullptr;
    }

    IShellItemImageFactory* factory = nullptr;
    const HRESULT queried = item->QueryInterface(IID_PPV_ARGS(&factory));
    item->Release();
    if (FAILED(queried) || factory == nullptr)
    {
        return nullptr;
    }

    SIZE size = { 640, 360 };
    HBITMAP hbmp = nullptr;
    HRESULT hr = factory->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
    if (FAILED(hr) || hbmp == nullptr)
    {
        hr = factory->GetImage(size, SIIGBF_RESIZETOFIT | SIIGBF_BIGGERSIZEOK, &hbmp);
    }
    factory->Release();
    if (FAILED(hr) || hbmp == nullptr)
    {
        return nullptr;
    }
    return PosterFromHbitmap(hbmp);
}

static bool SetReaderRgbType(IMFSourceReader* reader, const GUID& subtype)
{
    IMFMediaType* want = nullptr;
    if (FAILED(MFCreateMediaType(&want)) || want == nullptr)
    {
        return false;
    }
    want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    want->SetGUID(MF_MT_SUBTYPE, subtype);
    const HRESULT typed = reader->SetCurrentMediaType(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
        nullptr,
        want
    );
    want->Release();
    return SUCCEEDED(typed);
}

static Gdiplus::Image* LoadVideoPosterMf(const wchar_t* path)
{
    if (!EnsurePosterMf())
    {
        return nullptr;
    }

    wchar_t url[MAX_PATH * 2] = {};
    PathToFileUrl(path, url, MAX_PATH * 2);

    IMFAttributes* attrs = nullptr;
    MFCreateAttributes(&attrs, 2);
    if (attrs != nullptr)
    {
        attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }

    IMFSourceReader* reader = nullptr;
    HRESULT opened = MFCreateSourceReaderFromURL(url, attrs, &reader);
    if (FAILED(opened) || reader == nullptr)
    {
        opened = MFCreateSourceReaderFromURL(path, attrs, &reader);
    }
    if (attrs != nullptr)
    {
        attrs->Release();
    }
    if (FAILED(opened) || reader == nullptr)
    {
        return nullptr;
    }

    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);
    if (!SetReaderRgbType(reader, MFVideoFormat_RGB32) &&
        !SetReaderRgbType(reader, MFVideoFormat_RGB24))
    {
        reader->Release();
        return nullptr;
    }

    IMFMediaType* current = nullptr;
    if (FAILED(reader->GetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            &current)) ||
        current == nullptr)
    {
        reader->Release();
        return nullptr;
    }

    UINT32 width = 0;
    UINT32 height = 0;
    const HRESULT sized = MFGetAttributeSize(current, MF_MT_FRAME_SIZE, &width, &height);
    UINT32 strideAttr = 0;
    const bool haveStride = SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideAttr));
    GUID subtype = {};
    current->GetGUID(MF_MT_SUBTYPE, &subtype);
    current->Release();
    if (FAILED(sized) || width < 2 || height < 2)
    {
        reader->Release();
        return nullptr;
    }
    const UINT32 bpp = (subtype == MFVideoFormat_RGB24) ? 3u : 4u;

    IMFSample* sample = nullptr;
    for (int attempt = 0; attempt < 48 && sample == nullptr; ++attempt)
    {
        DWORD flags = 0;
        IMFSample* next = nullptr;
        const HRESULT read = reader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            nullptr,
            &flags,
            nullptr,
            &next
        );
        if (FAILED(read) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0)
        {
            if (next != nullptr)
            {
                next->Release();
            }
            break;
        }
        if (next == nullptr)
        {
            continue;
        }
        sample = next;
    }
    reader->Release();
    if (sample == nullptr)
    {
        return nullptr;
    }

    IMFMediaBuffer* buffer = nullptr;
    if (FAILED(sample->ConvertToContiguousBuffer(&buffer)) || buffer == nullptr)
    {
        sample->Release();
        return nullptr;
    }
    sample->Release();

    BYTE* data = nullptr;
    DWORD maxLen = 0;
    DWORD curLen = 0;
    if (FAILED(buffer->Lock(&data, &maxLen, &curLen)) || data == nullptr || curLen == 0)
    {
        buffer->Release();
        return nullptr;
    }

    INT stride = haveStride ? static_cast<INT>(strideAttr) : static_cast<INT>(width * bpp);
    if (stride == 0)
    {
        stride = static_cast<INT>(width * bpp);
    }
    const UINT32 absStride = stride < 0 ? static_cast<UINT32>(-stride) : static_cast<UINT32>(stride);
    const BYTE* firstRow = (stride < 0) ? (data + ((height - 1) * absStride)) : data;
    const INT copyStride = (stride < 0) ? -static_cast<INT>(absStride) : static_cast<INT>(absStride);

    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(
        static_cast<INT>(width),
        static_cast<INT>(height),
        PixelFormat32bppRGB
    );
    Gdiplus::Image* poster = nullptr;
    if (bitmap != nullptr && bitmap->GetLastStatus() == Gdiplus::Ok)
    {
        Gdiplus::BitmapData locked = {};
        Gdiplus::Rect area(0, 0, static_cast<INT>(width), static_cast<INT>(height));
        if (bitmap->LockBits(&area, Gdiplus::ImageLockModeWrite, PixelFormat32bppRGB, &locked) == Gdiplus::Ok)
        {
            const BYTE* src = firstRow;
            BYTE* dst = static_cast<BYTE*>(locked.Scan0);
            for (UINT32 row = 0; row < height; ++row)
            {
                if (bpp == 4)
                {
                    memcpy(dst, src, static_cast<size_t>(width) * 4);
                }
                else
                {
                    for (UINT32 col = 0; col < width; ++col)
                    {
                        dst[col * 4 + 0] = src[col * 3 + 0];
                        dst[col * 4 + 1] = src[col * 3 + 1];
                        dst[col * 4 + 2] = src[col * 3 + 2];
                        dst[col * 4 + 3] = 255;
                    }
                }
                src += copyStride;
                dst += locked.Stride;
            }
            bitmap->UnlockBits(&locked);
            poster = bitmap->Clone(area, PixelFormat32bppRGB);
        }
    }
    buffer->Unlock();
    buffer->Release();
    delete bitmap;
    if (poster == nullptr || poster->GetLastStatus() != Gdiplus::Ok)
    {
        delete poster;
        return nullptr;
    }
    return poster;
}

static Gdiplus::Image* LoadVideoPoster(const wchar_t* path)
{
    if (!App_HasFullGraphics() || path == nullptr || path[0] == L'\0')
    {
        return nullptr;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::Image* poster = LoadVideoPosterShell(path);
    if (poster != nullptr)
    {
        return poster;
    }
    return LoadVideoPosterMf(path);
}

static void AttachVideoPoster(SlideElement* element, const wchar_t* path)
{
    if (element == nullptr)
    {
        return;
    }
    if (element->image != nullptr &&
        element->image != g_photo &&
        element->image != g_slide5Background &&
        element->image != g_slide6Photo)
    {
        delete element->image;
        element->image = nullptr;
    }

    wchar_t resolved[MAX_PATH] = {};
    const wchar_t* loadPath = path;
    if (path != nullptr && path[0] != L'\0' && Document_ResolveAsset(path, resolved, MAX_PATH) && resolved[0] != L'\0')
    {
        loadPath = resolved;
    }
    element->image = LoadVideoPoster(loadPath);
    element->placeholderIfMissing = true;
}

static Gdiplus::Image* LoadPhoto(const wchar_t* path)
{
    if (!App_HasFullGraphics() || path == nullptr)
    {
        return nullptr;
    }

    Gdiplus::Image* image = Gdiplus::Image::FromFile(path);
    if (image == nullptr || image->GetLastStatus() != Gdiplus::Ok)
    {
        delete image;
        return nullptr;
    }
    return image;
}

static SlideElement* AddElement(Slide* slide, ElementKind kind, int x, int y, int w, int h)
{
    if (slide->elementCount >= MAX_ELEMENTS)
    {
        return nullptr;
    }

    SlideElement* element = &slide->elements[slide->elementCount];
    slide->elementCount += 1;
    ZeroMemory(element, sizeof(*element));
    element->kind = kind;
    element->x = x;
    element->y = y;
    element->w = w;
    element->h = h;
    return element;
}

static void GetNativeSize(const Slide* slide, int* width, int* height)
{
    *width = slide->width > 0 ? slide->width : g_canvasW;
    *height = slide->height > 0 ? slide->height : g_canvasH;
}

static void RotateAround(float cx, float cy, float deg, float* x, float* y)
{
    const float rad = deg * 3.14159265f / 180.0f;
    const float c = cosf(rad);
    const float s = sinf(rad);
    const float dx = *x - cx;
    const float dy = *y - cy;
    *x = cx + dx * c - dy * s;
    *y = cy + dx * s + dy * c;
}

static void LocalFromSlide(const SlideElement* element, int slideX, int slideY, int* localX, int* localY)
{
    float x = static_cast<float>(slideX);
    float y = static_cast<float>(slideY);
    const float cx = element->x + (element->w * 0.5f);
    const float cy = element->y + (element->h * 0.5f);
    RotateAround(cx, cy, -element->rotation, &x, &y);
    *localX = static_cast<int>(x + 0.5f);
    *localY = static_cast<int>(y + 0.5f);
}

static bool HitElementLocal(const SlideElement* element, int slideX, int slideY, int pad)
{
    int localX = 0;
    int localY = 0;
    LocalFromSlide(element, slideX, slideY, &localX, &localY);
    return localX >= element->x - pad &&
        localY >= element->y - pad &&
        localX < element->x + element->w + pad &&
        localY < element->y + element->h + pad;
}

static void BeginElementSpin(HDC hdc, int x, int y, int w, int h, float deg, int* oldMode, XFORM* oldXform)
{
    *oldMode = SetGraphicsMode(hdc, GM_ADVANCED);
    GetWorldTransform(hdc, oldXform);
    if (deg > -0.05f && deg < 0.05f)
    {
        return;
    }
    const float rad = deg * 3.14159265f / 180.0f;
    const float c = cosf(rad);
    const float s = sinf(rad);
    const float cx = x + (w * 0.5f);
    const float cy = y + (h * 0.5f);
    XFORM spin = {
        c, s,
        -s, c,
        cx - (c * cx) + (s * cy),
        cy - (s * cx) - (c * cy)
    };
    ModifyWorldTransform(hdc, &spin, MWT_LEFTMULTIPLY);
}

static void EndElementSpin(HDC hdc, int oldMode, const XFORM* oldXform)
{
    SetWorldTransform(hdc, oldXform);
    SetGraphicsMode(hdc, oldMode);
}

static void DrawMediaImage(
    HDC hdc,
    const SlideElement* element,
    int x,
    int y,
    int w,
    int h,
    bool fast,
    const char* missingLabel,
    float rotation,
    const XFORM* oldXform
)
{
    RECT box = { x, y, x + w, y + h };
    if (App_HasFullGraphics() && element->image != nullptr)
    {
        const UINT imageW = element->image->GetWidth();
        const UINT imageH = element->image->GetHeight();
        Gdiplus::Rect dest(x, y, w, h);

        if (imageW > 0 && imageH > 0 && element->displayMode != IMAGE_STRETCH)
        {
            const float boxW = static_cast<float>(w);
            const float boxH = static_cast<float>(h);
            const float srcImageW = static_cast<float>(imageW);
            const float srcImageH = static_cast<float>(imageH);
            const float scaleWidth = boxW / srcImageW;
            const float scaleHeight = boxH / srcImageH;
            const float scale = (element->displayMode == IMAGE_FIT)
                ? (scaleWidth < scaleHeight ? scaleWidth : scaleHeight)
                : (scaleWidth > scaleHeight ? scaleWidth : scaleHeight);
            dest.Width = static_cast<INT>(srcImageW * scale + 0.5f);
            dest.Height = static_cast<INT>(srcImageH * scale + 0.5f);
            dest.X = x + ((w - dest.Width) / 2);
            dest.Y = y + ((h - dest.Height) / 2);
        }

        Gdiplus::Graphics graphics(hdc);
        graphics.SetInterpolationMode(
            fast
                ? Gdiplus::InterpolationModeLowQuality
                : Gdiplus::InterpolationModeHighQualityBicubic
        );
        graphics.SetPixelOffsetMode(
            fast ? Gdiplus::PixelOffsetModeDefault : Gdiplus::PixelOffsetModeHighQuality
        );
        graphics.SetSmoothingMode(
            fast ? Gdiplus::SmoothingModeNone : Gdiplus::SmoothingModeHighQuality
        );
        if (oldXform != nullptr)
        {
            SetWorldTransform(hdc, oldXform);
        }
        if (rotation > 0.05f || rotation < -0.05f)
        {
            const float cx = x + (w * 0.5f);
            const float cy = y + (h * 0.5f);
            graphics.TranslateTransform(cx, cy);
            graphics.RotateTransform(-rotation);
            graphics.TranslateTransform(-cx, -cy);
        }
        if (element->displayMode == IMAGE_FILL)
        {
            graphics.SetClip(Gdiplus::Rect(x, y, w, h));
        }
        graphics.DrawImage(element->image, dest);
        return;
    }

    if (element->placeholderIfMissing)
    {
        HBRUSH brush = CreateSolidBrush(RGB(60, 60, 60));
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
        HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        Rectangle(hdc, x, y, x + w, y + h);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
        SetTextColor(hdc, RGB(220, 220, 220));
        DrawTextA(hdc, missingLabel != nullptr ? missingLabel : "Photo", -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static HFONT CreateElementFont(const SlideElement* element, int pixelHeight)
{
    const char* face = element->fontName[0] != '\0' ? element->fontName : "Segoe UI";
    return CreateFontA(
        -pixelHeight,
        0, 0, 0,
        element->fontBold ? FW_BOLD : FW_NORMAL,
        element->fontItalic ? TRUE : FALSE,
        element->fontUnderline ? TRUE : FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        face
    );
}

#define TEXT_FIT_PAD 8

// Text draws from the top of its box, so the box is sized to its wrapped lines
// around the box's old vertical center. One line and several lines then both
// sit where the box was placed.
static void FitTextElementHeight(SlideElement* element)
{
    if (element == nullptr || element->kind != ELEMENT_TEXT || element->text[0] == '\0' ||
        element->w <= 0 || element->fontSize <= 0)
    {
        return;
    }
    HDC dc = CreateCompatibleDC(nullptr);
    if (dc == nullptr)
    {
        return;
    }
    HFONT font = CreateElementFont(element, element->fontSize);
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    RECT measure = { 0, 0, element->w, 0 };
    DrawTextA(dc, element->text, -1, &measure, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);

    const int height = (measure.bottom - measure.top) + TEXT_FIT_PAD;
    if (height <= TEXT_FIT_PAD)
    {
        return;
    }
    const int center = element->y + element->h / 2;
    element->h = height;
    element->y = center - height / 2;
}

static void ComposeOne(
    HDC hdc,
    int destX,
    int destY,
    int destW,
    int destH,
    const Slide* slide,
    const SlideElement* element,
    bool fast,
    bool uiPreview
)
{
    int srcW = 0;
    int srcH = 0;
    GetNativeSize(slide, &srcW, &srcH);

    const int x = destX + MapValue(element->x, srcW, destW);
    const int y = destY + MapValue(element->y, srcH, destH);
    const int w = MapValue(element->w, srcW, destW);
    const int h = MapValue(element->h, srcH, destH);
    RECT box = { x, y, x + w, y + h };
    int oldMode = 0;
    XFORM oldXform = { 1, 0, 0, 1, 0, 0 };
    BeginElementSpin(hdc, x, y, w, h, element->rotation, &oldMode, &oldXform);

    switch (element->kind)
    {
        case ELEMENT_RECT:
        {
            HBRUSH brush = CreateSolidBrush(element->fill);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
            HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
            Rectangle(hdc, x, y, x + w, y + h);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(brush);
            break;
        }

        case ELEMENT_ELLIPSE:
        {
            HBRUSH brush = CreateSolidBrush(element->fill);
            HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
            HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
            Ellipse(hdc, x, y, x + w, y + h);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(brush);
            break;
        }

        case ELEMENT_TEXT:
        {
            HFONT font = CreateElementFont(element, MapValue(element->fontSize, srcH, destH));
            HFONT oldFont = (HFONT)SelectObject(hdc, font);
            SetBkMode(hdc, TRANSPARENT);
            UINT flags = DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;
            if (element->align == 2)
            {
                flags |= DT_RIGHT;
            }
            else if (element->align == 1)
            {
                flags |= DT_CENTER;
            }
            else
            {
                flags |= DT_LEFT;
            }
            if (element->text[0] == '\0')
            {
                if (uiPreview)
                {
                    SetTextColor(hdc, RGB(0xA0, 0xA0, 0xA0));
                    DrawTextA(hdc, "Text", -1, &box, flags);
                }
            }
            else
            {
                SetTextColor(hdc, element->textColor);
                DrawTextA(hdc, element->text, -1, &box, flags);
            }
            SelectObject(hdc, oldFont);
            DeleteObject(font);
            break;
        }

        case ELEMENT_IMAGE:
            DrawMediaImage(hdc, element, x, y, w, h, fast, "Photo", element->rotation, &oldXform);
            break;

        case ELEMENT_VIDEO:
            if (uiPreview)
            {
                DrawMediaImage(hdc, element, x, y, w, h, fast, "Video", element->rotation, &oldXform);
            }
            else
            {
                HBRUSH brush = CreateSolidBrush(SLIDE_KEY);
                FillRect(hdc, &box, brush);
                DeleteObject(brush);
            }
            break;
    }

    EndElementSpin(hdc, oldMode, &oldXform);
}

static void FillCheckerboard(HDC hdc, int x, int y, int width, int height)
{
    RECT well = { x, y, x + width, y + height };
    HBRUSH back = CreateSolidBrush(RGB(0x1A, 0x1A, 0x1A));
    FillRect(hdc, &well, back);
    DeleteObject(back);

    HBRUSH tile = CreateSolidBrush(RGB(0x2A, 0x2A, 0x2A));
    const int size = 12;
    for (int row = 0; row * size < height; ++row)
    {
        for (int col = 0; col * size < width; ++col)
        {
            if (((row + col) & 1) == 0)
            {
                continue;
            }
            RECT cell = {
                x + (col * size),
                y + (row * size),
                x + ((col + 1) * size),
                y + ((row + 1) * size)
            };
            if (cell.right > well.right)
            {
                cell.right = well.right;
            }
            if (cell.bottom > well.bottom)
            {
                cell.bottom = well.bottom;
            }
            FillRect(hdc, &cell, tile);
        }
    }
    DeleteObject(tile);
}

static void ComposeSlideElements(
    HDC hdc,
    int destX,
    int destY,
    int destW,
    int destH,
    const Slide* slide,
    int skipIndex,
    bool uiPreview
)
{
    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < slide->elementCount; ++i)
    {
        if (i == skipIndex)
        {
            continue;
        }
        ComposeOne(hdc, destX, destY, destW, destH, slide, &slide->elements[i], false, uiPreview);
    }
}

// The locked picture or video on a background slide is the bed. Everything
// else on that slide is its slide half and goes live like any other slide.
static bool IsBedElement(const Slide* slide, const SlideElement* element)
{
    return slide->role == SLIDE_ROLE_BACKGROUND &&
        element->locked &&
        (element->kind == ELEMENT_IMAGE || element->kind == ELEMENT_VIDEO);
}

static void ComposeSlide(HDC hdc, int destX, int destY, int destW, int destH, const Slide* slide, bool bed)
{
    RECT background = { destX, destY, destX + destW, destY + destH };
    HBRUSH backgroundBrush = CreateSolidBrush(SLIDE_KEY);
    FillRect(hdc, &background, backgroundBrush);
    DeleteObject(backgroundBrush);
    SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < slide->elementCount; ++i)
    {
        if (IsBedElement(slide, &slide->elements[i]) != bed)
        {
            continue;
        }
        ComposeOne(hdc, destX, destY, destW, destH, slide, &slide->elements[i], false, false);
    }
}

static HBITMAP CreateFrameBitmap(HDC reference, int width, int height)
{
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    return CreateDIBSection(reference, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
}

static void SealFrameAlpha(HBITMAP bmp)
{
    if (bmp == nullptr)
    {
        return;
    }
    DIBSECTION section = {};
    if (GetObject(bmp, sizeof(section), &section) == 0 ||
        section.dsBm.bmBits == nullptr ||
        section.dsBm.bmBitsPixel != 32)
    {
        return;
    }
    const int width = section.dsBm.bmWidth;
    const int height = section.dsBm.bmHeight < 0 ? -section.dsBm.bmHeight : section.dsBm.bmHeight;
    const int stride = section.dsBm.bmWidthBytes;
    unsigned char* bits = static_cast<unsigned char*>(section.dsBm.bmBits);
    for (int y = 0; y < height; ++y)
    {
        unsigned char* row = bits + (y * stride);
        for (int x = 0; x < width; ++x)
        {
            unsigned char* px = row + (x * 4);
            if (px[0] == 255 && px[1] == 0 && px[2] == 255)
            {
                px[0] = 0;
                px[1] = 0;
                px[2] = 0;
                px[3] = 0;
            }
            else
            {
                px[3] = 255;
            }
        }
    }
}

static void BlitAlphaBitmap(
    HDC dest,
    int destX,
    int destY,
    int destW,
    int destH,
    HDC src,
    int srcX,
    int srcY,
    int srcW,
    int srcH
)
{
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    AlphaBlend(dest, destX, destY, destW, destH, src, srcX, srcY, srcW, srcH, blend);
}

static void FreeFrame(int index)
{
    if (index < 0 || index >= MAX_SLIDES)
    {
        return;
    }
    if (g_frames[index] != nullptr)
    {
        DeleteObject(g_frames[index]);
        g_frames[index] = nullptr;
    }
}

// Bumped every time a slide frame is drawn. Thumbnail caches compare this
// together with the frame handle so they only redraw a slide that changed.
static unsigned g_frameGeneration[MAX_SLIDES] = {};
static unsigned g_frameGenerationNext = 1;

unsigned Renderer_FrameGeneration(int index)
{
    if (index < 0 || index >= MAX_SLIDES)
    {
        return 0;
    }
    return g_frameGeneration[index];
}

static void PrerenderSlide(int index)
{
    if (index < 0 || index >= MAX_SLIDES)
    {
        return;
    }

    g_frameGeneration[index] = g_frameGenerationNext;
    g_frameGenerationNext += 1;

    int width = 0;
    int height = 0;
    GetNativeSize(&g_slides[index], &width, &height);

    HDC screen = GetDC(nullptr);
    if (g_frames[index] == nullptr)
    {
        g_frames[index] = CreateFrameBitmap(screen, width, height);
    }

    HDC memory = CreateCompatibleDC(screen);
    HBITMAP old = (HBITMAP)SelectObject(memory, g_frames[index]);
    ComposeSlide(memory, 0, 0, width, height, &g_slides[index], false);
    SelectObject(memory, old);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    SealFrameAlpha(g_frames[index]);
}

static void LoadDemoSlides()
{
    ReadDefaultDisplaySize(&g_canvasW, &g_canvasH);

    for (int i = 0; i < DEMO_SLIDE_COUNT; ++i)
    {
        g_slides[i].width = g_canvasW;
        g_slides[i].height = g_canvasH;
    }

    g_slides[SLIDE_FREEFORM].background = RGB(24, 24, 28);

    g_photo = LoadPhoto(L"C:\\Windows\\Web\\Wallpaper\\Windows\\img0.jpg");
    if (g_photo == nullptr)
    {
        g_photo = LoadPhoto(L"C:\\Windows\\Web\\Wallpaper\\Theme1\\img1.jpg");
    }

    Slide* slide1 = &g_slides[0];
    slide1->background = RGB(15, 32, 64);
    SlideElement* title = AddElement(slide1, ELEMENT_TEXT, LayoutX(40), LayoutY(160), LayoutX(720), LayoutY(80));
    title->fontSize = LayoutS(56);
    title->textColor = RGB(255, 255, 255);
    lstrcpyA(title->text, "Slide 1");
    SlideElement* subtitle = AddElement(slide1, ELEMENT_TEXT, LayoutX(40), LayoutY(250), LayoutX(720), LayoutY(40));
    subtitle->fontSize = LayoutS(22);
    subtitle->textColor = RGB(180, 200, 230);
    lstrcpyA(subtitle->text, "Text on the output screen");

    Slide* slide2 = &g_slides[1];
    slide2->background = RGB(245, 245, 245);
    SlideElement* heading = AddElement(slide2, ELEMENT_TEXT, LayoutX(40), LayoutY(20), LayoutX(720), LayoutY(50));
    heading->fontSize = LayoutS(32);
    heading->textColor = RGB(30, 30, 30);
    lstrcpyA(heading->text, "Shapes");
    SlideElement* rect = AddElement(slide2, ELEMENT_RECT, LayoutX(80), LayoutY(120), LayoutX(280), LayoutY(220));
    rect->fill = RGB(200, 60, 60);
    SlideElement* ellipse = AddElement(slide2, ELEMENT_ELLIPSE, LayoutX(420), LayoutY(120), LayoutX(280), LayoutY(220));
    ellipse->fill = RGB(40, 90, 180);

    Slide* slide3 = &g_slides[2];
    slide3->background = RGB(0, 0, 0);
    SlideElement* image = AddElement(slide3, ELEMENT_IMAGE, 0, 0, g_canvasW, g_canvasH);
    image->image = g_photo;
    image->placeholderIfMissing = true;
    SlideElement* caption = AddElement(slide3, ELEMENT_TEXT, LayoutX(40), LayoutY(400), LayoutX(720), LayoutY(50));
    caption->fontSize = LayoutS(24);
    caption->textColor = RGB(255, 255, 255);
    lstrcpyA(caption->text, g_photo != nullptr ? "Photo" : "Photo placeholder");

    Slide* slide4 = &g_slides[3];
    slide4->background = RGB(28, 28, 32);
    SlideElement* mixTitle = AddElement(slide4, ELEMENT_TEXT, LayoutX(20), LayoutY(16), LayoutX(760), LayoutY(40));
    mixTitle->fontSize = LayoutS(26);
    mixTitle->textColor = RGB(255, 255, 255);
    lstrcpyA(mixTitle->text, "Mixed slide");
    SlideElement* mixRect = AddElement(slide4, ELEMENT_RECT, LayoutX(40), LayoutY(80), LayoutX(200), LayoutY(120));
    mixRect->fill = RGB(80, 160, 90);
    SlideElement* mixEllipse = AddElement(slide4, ELEMENT_ELLIPSE, LayoutX(270), LayoutY(80), LayoutX(200), LayoutY(120));
    mixEllipse->fill = RGB(220, 160, 40);
    SlideElement* mixPhoto = AddElement(slide4, ELEMENT_IMAGE, LayoutX(500), LayoutY(80), LayoutX(260), LayoutY(160));
    mixPhoto->image = g_photo;
    mixPhoto->placeholderIfMissing = true;
    SlideElement* mixVideo = AddElement(slide4, ELEMENT_VIDEO, LayoutX(40), LayoutY(280), LayoutX(720), LayoutY(160));
    lstrcpyA(mixVideo->text, "Video (placeholder)");
    mixVideo->placeholderIfMissing = true;

    Slide* slide5 = &g_slides[SLIDE_TEXT];
    slide5->background = RGB(255, 255, 255);
    SlideElement* backdrop = AddElement(slide5, ELEMENT_IMAGE, 0, 0, g_canvasW, g_canvasH);
    backdrop->placeholderIfMissing = false;
    SlideElement* userText = AddElement(slide5, ELEMENT_TEXT, LayoutX(40), LayoutY(160), LayoutX(720), LayoutY(160));
    userText->fontSize = LayoutS(48);
    userText->textColor = RGB(0, 0, 0);
    lstrcpyA(userText->fontName, "Segoe UI");
    lstrcpyA(userText->text, "Type your text");

    Slide* slide6 = &g_slides[SLIDE_PHOTO];
    slide6->background = RGB(0, 0, 0);
    SlideElement* userPhoto = AddElement(slide6, ELEMENT_IMAGE, 0, 0, g_canvasW, g_canvasH);
    userPhoto->placeholderIfMissing = true;

    Slide* slide8 = &g_slides[SLIDE_VIDEO];
    slide8->background = RGB(0, 0, 0);
    SlideElement* slideVideo = AddElement(slide8, ELEMENT_VIDEO, 0, 0, g_canvasW, g_canvasH);
    slideVideo->displayMode = IMAGE_FIT;
    lstrcpyA(slideVideo->text, "Video");
    slideVideo->placeholderIfMissing = true;
    g_slideCount = DEMO_SLIDE_COUNT;
    g_nextSlideSerial = 1;
    for (int i = 0; i < g_slideCount; ++i)
    {
        EnsureSlideId(&g_slides[i]);
    }
}

bool Renderer_Startup()
{
    if (!App_Starting())
    {
        return false;
    }

    if (g_started)
    {
        return true;
    }

    LoadDemoSlides();

    for (int i = 0; i < g_slideCount; ++i)
    {
        PrerenderSlide(i);
        if (g_frames[i] == nullptr)
        {
            return false;
        }
    }

    g_currentSlide = 0;
    g_backgroundSlide = -1;
    Renderer_LoadTransitionDefaults();
    g_started = true;
    return true;
}

void Renderer_Shutdown()
{
    for (int i = 0; i < MAX_SLIDES; ++i)
    {
        FreeFrame(i);
    }

    delete g_photo;
    g_photo = nullptr;
    delete g_slide5Background;
    g_slide5Background = nullptr;
    delete g_slide6Photo;
    g_slide6Photo = nullptr;

    Slide* freeform = &g_slides[SLIDE_FREEFORM];
    for (int i = 0; i < freeform->elementCount; ++i)
    {
        Gdiplus::Image* image = freeform->elements[i].image;
        if (image != nullptr &&
            image != g_photo &&
            image != g_slide5Background &&
            image != g_slide6Photo)
        {
            delete image;
            freeform->elements[i].image = nullptr;
        }
    }

    if (g_editorCache != nullptr)
    {
        DeleteObject(g_editorCache);
        g_editorCache = nullptr;
    }
    if (g_liveOverlay != nullptr)
    {
        DeleteObject(g_liveOverlay);
        g_liveOverlay = nullptr;
    }
    g_liveOverlayW = 0;
    g_liveOverlayH = 0;
    if (g_hasElementClipboard)
    {
        Gdiplus::Image* clipImage = g_elementClipboard.image;
        if (clipImage != nullptr &&
            clipImage != g_photo &&
            clipImage != g_slide5Background &&
            clipImage != g_slide6Photo)
        {
            delete clipImage;
        }
        ZeroMemory(&g_elementClipboard, sizeof(g_elementClipboard));
        g_hasElementClipboard = false;
    }
    g_editorCacheW = 0;
    g_editorCacheH = 0;
    g_editorDragging = false;

    g_slideCount = 0;
    g_started = false;
    if (g_posterMf)
    {
        MFShutdown();
        g_posterMf = false;
    }
}

bool Renderer_SelectSlide(int index)
{
    if (!App_Ready() || index < 0 || index >= g_slideCount)
    {
        return false;
    }

    g_currentSlide = index;
    return true;
}

int Renderer_CurrentSlide()
{
    if (!App_Ready())
    {
        return 0;
    }

    return g_currentSlide;
}

int Renderer_SlideCount()
{
    return g_slideCount;
}

static int MapSlideIndex(int index, int from, int dest)
{
    if (index == from)
    {
        return dest;
    }
    if (from < dest)
    {
        if (index > from && index <= dest)
        {
            return index - 1;
        }
    }
    else if (index >= dest && index < from)
    {
        return index + 1;
    }
    return index;
}

bool Renderer_MoveSlide(int from, int dropSlot)
{
    if (!g_started || g_slideCount < 2)
    {
        return false;
    }
    if (from < 0 || from >= g_slideCount)
    {
        return false;
    }

    int insert = dropSlot;
    if (insert < 0)
    {
        insert = 0;
    }
    if (insert > g_slideCount)
    {
        insert = g_slideCount;
    }
    if (insert > from)
    {
        insert -= 1;
    }
    if (insert == from || insert < 0 || insert >= g_slideCount)
    {
        return false;
    }

    const Slide heldSlide = g_slides[from];
    const HBITMAP heldFrame = g_frames[from];
    if (from < insert)
    {
        for (int i = from; i < insert; ++i)
        {
            g_slides[i] = g_slides[i + 1];
            g_frames[i] = g_frames[i + 1];
        }
    }
    else
    {
        for (int i = from; i > insert; --i)
        {
            g_slides[i] = g_slides[i - 1];
            g_frames[i] = g_frames[i - 1];
        }
    }
    g_slides[insert] = heldSlide;
    g_frames[insert] = heldFrame;
    g_currentSlide = MapSlideIndex(g_currentSlide, from, insert);
    g_editSlide = MapSlideIndex(g_editSlide, from, insert);
    if (g_backgroundSlide >= 0)
    {
        g_backgroundSlide = MapSlideIndex(g_backgroundSlide, from, insert);
    }
    return true;
}

static Slide g_slideClipboard[MAX_SLIDES] = {};
static int g_slideClipboardCount = 0;

static void FreeSlideImages(Slide* slide)
{
    if (slide == nullptr)
    {
        return;
    }
    for (int i = 0; i < slide->elementCount; ++i)
    {
        Gdiplus::Image* image = slide->elements[i].image;
        if (image != nullptr &&
            image != g_photo &&
            image != g_slide5Background &&
            image != g_slide6Photo)
        {
            delete image;
        }
        slide->elements[i].image = nullptr;
    }
}

static void DeepCopySlide(Slide* dest, const Slide* src)
{
    *dest = *src;
    dest->id[0] = '\0';
    for (int i = 0; i < dest->elementCount; ++i)
    {
        Gdiplus::Image* image = dest->elements[i].image;
        if (image != nullptr)
        {
            Gdiplus::Image* clone = image->Clone();
            if (clone == nullptr || clone->GetLastStatus() != Gdiplus::Ok)
            {
                delete clone;
                dest->elements[i].image = nullptr;
            }
            else
            {
                dest->elements[i].image = clone;
            }
        }
    }
}

static void ClearSlideClipboard()
{
    for (int i = 0; i < g_slideClipboardCount; ++i)
    {
        FreeSlideImages(&g_slideClipboard[i]);
        ZeroMemory(&g_slideClipboard[i], sizeof(g_slideClipboard[i]));
    }
    g_slideClipboardCount = 0;
}

static int CompareIntsAsc(const void* a, const void* b)
{
    const int left = *static_cast<const int*>(a);
    const int right = *static_cast<const int*>(b);
    return left - right;
}

static int CompareIntsDesc(const void* a, const void* b)
{
    return CompareIntsAsc(b, a);
}

bool Renderer_DeleteSlides(const int* indices, int count)
{
    if (!g_started || indices == nullptr || count <= 0 || g_slideCount <= 1)
    {
        return false;
    }

    int sorted[MAX_SLIDES] = {};
    int unique = 0;
    for (int i = 0; i < count; ++i)
    {
        if (indices[i] < 0 || indices[i] >= g_slideCount)
        {
            continue;
        }
        bool exists = false;
        for (int j = 0; j < unique; ++j)
        {
            if (sorted[j] == indices[i])
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            sorted[unique++] = indices[i];
        }
    }
    if (unique <= 0)
    {
        return false;
    }
    qsort(sorted, unique, sizeof(int), CompareIntsAsc);
    if (unique >= g_slideCount)
    {
        // Always keep at least one slide.
        for (int i = 0; i < unique - 1; ++i)
        {
            sorted[i] = sorted[i + 1];
        }
        unique -= 1;
    }
    if (unique <= 0)
    {
        return false;
    }
    qsort(sorted, unique, sizeof(int), CompareIntsDesc);

    for (int i = 0; i < unique; ++i)
    {
        const int index = sorted[i];
        FreeSlideImages(&g_slides[index]);
        FreeFrame(index);
        for (int j = index; j < g_slideCount - 1; ++j)
        {
            g_slides[j] = g_slides[j + 1];
            g_frames[j] = g_frames[j + 1];
        }
        ZeroMemory(&g_slides[g_slideCount - 1], sizeof(g_slides[0]));
        g_frames[g_slideCount - 1] = nullptr;
        g_slideCount -= 1;
        if (g_currentSlide > index)
        {
            g_currentSlide -= 1;
        }
        else if (g_currentSlide == index)
        {
            if (g_currentSlide >= g_slideCount)
            {
                g_currentSlide = g_slideCount - 1;
            }
        }
        if (g_editSlide > index)
        {
            g_editSlide -= 1;
        }
        else if (g_editSlide == index)
        {
            if (g_editSlide >= g_slideCount)
            {
                g_editSlide = g_slideCount - 1;
            }
        }
        if (g_backgroundSlide > index)
        {
            g_backgroundSlide -= 1;
        }
        else if (g_backgroundSlide == index)
        {
            g_backgroundSlide = -1;
        }
    }
    if (g_currentSlide < 0)
    {
        g_currentSlide = 0;
    }
    if (g_editSlide < 0)
    {
        g_editSlide = 0;
    }
    return true;
}

bool Renderer_CopySlides(const int* indices, int count)
{
    if (!g_started || indices == nullptr || count <= 0)
    {
        return false;
    }

    int sorted[MAX_SLIDES] = {};
    int unique = 0;
    for (int i = 0; i < count; ++i)
    {
        if (indices[i] < 0 || indices[i] >= g_slideCount)
        {
            continue;
        }
        bool exists = false;
        for (int j = 0; j < unique; ++j)
        {
            if (sorted[j] == indices[i])
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            sorted[unique++] = indices[i];
        }
    }
    if (unique <= 0)
    {
        return false;
    }
    qsort(sorted, unique, sizeof(int), CompareIntsAsc);

    ClearSlideClipboard();
    for (int i = 0; i < unique && g_slideClipboardCount < MAX_SLIDES; ++i)
    {
        DeepCopySlide(&g_slideClipboard[g_slideClipboardCount], &g_slides[sorted[i]]);
        g_slideClipboardCount += 1;
    }
    return g_slideClipboardCount > 0;
}

bool Renderer_HasSlideClipboard()
{
    return g_slideClipboardCount > 0;
}

bool Renderer_PasteSlides(int insertBefore)
{
    if (!g_started || g_slideClipboardCount <= 0)
    {
        return false;
    }
    if (g_slideCount + g_slideClipboardCount > MAX_SLIDES)
    {
        return false;
    }

    int at = insertBefore;
    if (at < 0)
    {
        at = g_slideCount;
    }
    if (at > g_slideCount)
    {
        at = g_slideCount;
    }

    for (int i = 0; i < g_slideClipboardCount; ++i)
    {
        for (int j = g_slideCount; j > at; --j)
        {
            g_slides[j] = g_slides[j - 1];
            g_frames[j] = g_frames[j - 1];
        }
        DeepCopySlide(&g_slides[at], &g_slideClipboard[i]);
        EnsureSlideId(&g_slides[at]);
        g_frames[at] = nullptr;
        g_slideCount += 1;
        PrerenderSlide(at);
        at += 1;
    }
    if (at > 0)
    {
        g_currentSlide = at - 1;
    }
    return true;
}

static void NoteSlideSerial(const char* id)
{
    if (id == nullptr || strncmp(id, "slide-", 6) != 0)
    {
        return;
    }
    const int value = atoi(id + 6);
    if (value >= g_nextSlideSerial)
    {
        g_nextSlideSerial = value + 1;
    }
}

static void EnsureSlideId(Slide* slide)
{
    if (slide == nullptr)
    {
        return;
    }
    if (slide->id[0] != '\0')
    {
        NoteSlideSerial(slide->id);
        return;
    }
    wsprintfA(slide->id, "slide-%d", g_nextSlideSerial);
    g_nextSlideSerial += 1;
}

static void InitBlankSlide(int index)
{
    ZeroMemory(&g_slides[index], sizeof(g_slides[index]));
    g_slides[index].width = DOCUMENT_WIDTH;
    g_slides[index].height = DOCUMENT_HEIGHT;
    g_slides[index].background = RGB(0x18, 0x18, 0x1C);
    g_slides[index].role = SLIDE_ROLE_SLIDE;
    g_slides[index].transition = SLIDE_TRANSITION_CROSSFADE;
    g_slides[index].transitionDir = SLIDE_TRANSITION_DIR_LEFT;
    g_slides[index].transitionMs = 300;
    g_slides[index].transitionOverride = false;
    EnsureSlideId(&g_slides[index]);

    SlideElement* text = AddElement(&g_slides[index], ELEMENT_TEXT, 160, (DOCUMENT_HEIGHT - 160) / 2, 1600, 160);
    if (text != nullptr)
    {
        text->fontSize = 64;
        text->textColor = RGB(255, 255, 255);
        text->align = 1;
        lstrcpyA(text->fontName, "Segoe UI");
        lstrcpyA(text->slot, "body");
    }
}

bool Renderer_NewDocument()
{
    if (!g_started)
    {
        return false;
    }

    for (int i = 0; i < MAX_SLIDES; ++i)
    {
        FreeFrame(i);
        ZeroMemory(&g_slides[i], sizeof(g_slides[i]));
    }

    g_slideCount = 1;
    g_currentSlide = 0;
    g_editSlide = 0;
    g_backgroundSlide = -1;
    g_nextSlideSerial = 1;
    InitBlankSlide(0);
    PrerenderSlide(0);
    return g_frames[0] != nullptr;
}

bool Renderer_AddBlankSlide()
{
    if (!g_started || g_slideCount < 1 || g_slideCount >= MAX_SLIDES)
    {
        return false;
    }

    const int index = g_slideCount;
    InitBlankSlide(index);
    g_slideCount += 1;
    PrerenderSlide(index);
    if (g_frames[index] == nullptr)
    {
        g_slideCount -= 1;
        return false;
    }

    g_currentSlide = index;
    return true;
}

static void JsonWriteString(FILE* file, const char* text)
{
    fputc('"', file);
    if (text != nullptr)
    {
        for (const char* cursor = text; *cursor != '\0'; ++cursor)
        {
            const unsigned char ch = static_cast<unsigned char>(*cursor);
            if (ch == '"' || ch == '\\')
            {
                fputc('\\', file);
                fputc(ch, file);
            }
            else if (ch == '\n')
            {
                fputs("\\n", file);
            }
            else if (ch == '\r')
            {
                fputs("\\r", file);
            }
            else
            {
                fputc(ch, file);
            }
        }
    }
    fputc('"', file);
}

// Files are UTF-8 so decks and templates move cleanly to and from the Mac copy;
// in memory, text stays in the Windows ANSI code page that the GDI "A" calls draw.
static void JsonWriteText(FILE* file, const char* text)
{
    if (text == nullptr || text[0] == '\0')
    {
        JsonWriteString(file, text);
        return;
    }
    const int wideChars = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (wideChars <= 0)
    {
        JsonWriteString(file, text);
        return;
    }
    wchar_t* wide = static_cast<wchar_t*>(malloc(sizeof(wchar_t) * wideChars));
    const int utf8Bytes = wide != nullptr ? (MultiByteToWideChar(CP_ACP, 0, text, -1, wide, wideChars),
                                             WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr))
                                          : 0;
    char* utf8 = utf8Bytes > 0 ? static_cast<char*>(malloc(utf8Bytes)) : nullptr;
    if (utf8 != nullptr)
    {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, utf8Bytes, nullptr, nullptr);
        JsonWriteString(file, utf8);
    }
    else
    {
        JsonWriteString(file, text);
    }
    free(utf8);
    free(wide);
}

// Older Windows files hold raw ANSI bytes, which are rarely valid UTF-8, so text
// that doesn't decode as UTF-8 is kept as-is.
static void TextFromFile(char* text, int textChars)
{
    if (text == nullptr || textChars <= 0)
    {
        return;
    }
    bool ascii = true;
    for (const char* cursor = text; *cursor != '\0'; ++cursor)
    {
        if (static_cast<unsigned char>(*cursor) >= 0x80)
        {
            ascii = false;
            break;
        }
    }
    if (ascii)
    {
        return;
    }
    const int wideChars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (wideChars <= 0)
    {
        return;
    }
    wchar_t* wide = static_cast<wchar_t*>(malloc(sizeof(wchar_t) * wideChars));
    if (wide == nullptr)
    {
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, wideChars);
    if (WideCharToMultiByte(CP_ACP, 0, wide, -1, text, textChars, nullptr, nullptr) == 0)
    {
        text[textChars - 1] = '\0';
    }
    free(wide);
}

static void JsonWriteColor(FILE* file, COLORREF color)
{
    fprintf(file, "\"#%02X%02X%02X\"", GetRValue(color), GetGValue(color), GetBValue(color));
}

static const char* ElementKindName(ElementKind kind)
{
    switch (kind)
    {
        case ELEMENT_TEXT:
            return "text";
        case ELEMENT_RECT:
            return "rect";
        case ELEMENT_ELLIPSE:
            return "ellipse";
        case ELEMENT_IMAGE:
            return "image";
        case ELEMENT_VIDEO:
            return "video";
        default:
            return "text";
    }
}

static const char* DisplayModeName(ImageDisplayMode mode)
{
    if (mode == IMAGE_FIT)
    {
        return "fit";
    }
    if (mode == IMAGE_FILL)
    {
        return "fill";
    }
    return "stretch";
}

static void JsonWriteElement(FILE* file, const SlideElement* element)
{
    fprintf(
        file,
        "        {\r\n          \"kind\": \"%s\",\r\n          \"x\": %d,\r\n          \"y\": %d,\r\n          \"w\": %d,\r\n          \"h\": %d,\r\n          \"rotation\": %.2f",
        ElementKindName(element->kind),
        element->x,
        element->y,
        element->w,
        element->h,
        element->rotation
    );
    if (element->slot[0] != '\0')
    {
        fputs(",\r\n          \"slot\": ", file);
        JsonWriteString(file, element->slot);
    }
    if (element->locked)
    {
        fputs(",\r\n          \"locked\": true", file);
    }
    if (element->kind == ELEMENT_TEXT)
    {
        fputs(",\r\n          \"text\": ", file);
        JsonWriteText(file, element->text);
        fputs(",\r\n          \"fontName\": ", file);
        JsonWriteText(file, element->fontName[0] != '\0' ? element->fontName : "Segoe UI");
        fprintf(file, ",\r\n          \"fontSize\": %d,\r\n          \"textColor\": ", element->fontSize);
        JsonWriteColor(file, element->textColor);
        fprintf(
            file,
            ",\r\n          \"bold\": %s,\r\n          \"italic\": %s,\r\n          \"underline\": %s,\r\n          \"align\": \"%s\"",
            element->fontBold ? "true" : "false",
            element->fontItalic ? "true" : "false",
            element->fontUnderline ? "true" : "false",
            element->align == 2 ? "right" : (element->align == 1 ? "center" : "left")
        );
    }
    if (element->kind == ELEMENT_RECT || element->kind == ELEMENT_ELLIPSE)
    {
        fputs(",\r\n          \"fill\": ", file);
        JsonWriteColor(file, element->fill);
    }
    if (element->kind == ELEMENT_IMAGE || element->kind == ELEMENT_VIDEO)
    {
        fprintf(file, ",\r\n          \"displayMode\": \"%s\"", DisplayModeName(element->displayMode));
        if (element->videoPath[0] != L'\0')
        {
            const wchar_t* stored = element->videoPath;
            const wchar_t* slash = wcsrchr(stored, L'\\');
            const wchar_t* fwd = wcsrchr(stored, L'/');
            if (fwd != nullptr && (slash == nullptr || fwd > slash))
            {
                slash = fwd;
            }
            const wchar_t* fileName = slash != nullptr ? slash + 1 : stored;
            char utf8[MAX_PATH * 2] = {};
            WideCharToMultiByte(CP_UTF8, 0, fileName, -1, utf8, sizeof(utf8), nullptr, nullptr);
            fputs(",\r\n          \"path\": ", file);
            JsonWriteString(file, utf8);
        }
    }
    fputs("\r\n        }", file);
}

static void JsonWriteSlide(FILE* file, Slide* slide)
{
    const int slideW = slide->width > 0 ? slide->width : DOCUMENT_WIDTH;
    const int slideH = slide->height > 0 ? slide->height : DOCUMENT_HEIGHT;
    EnsureSlideId(slide);
    fputs("    {\r\n      \"id\": ", file);
    JsonWriteString(file, slide->id);
    fprintf(
        file,
        ",\r\n      \"role\": \"%s\",\r\n      \"transitionOverride\": %s,\r\n      \"transition\": \"%s\",\r\n      \"transitionDir\": \"%s\",\r\n      \"transitionMs\": %d,\r\n      \"width\": %d,\r\n      \"height\": %d,\r\n      \"background\": ",
        slide->role == SLIDE_ROLE_BACKGROUND ? "background" : "slide",
        slide->transitionOverride ? "true" : "false",
        Renderer_TransitionName(slide->transition),
        Renderer_TransitionDirName(slide->transitionDir),
        slide->transitionMs > 0 ? slide->transitionMs : 300,
        slideW,
        slideH
    );
    JsonWriteColor(file, slide->background);
    fputs(",\r\n      \"elements\": [", file);
    for (int e = 0; e < slide->elementCount; ++e)
    {
        if (e == 0)
        {
            fputs("\r\n", file);
        }
        else
        {
            fputs(",\r\n", file);
        }
        JsonWriteElement(file, &slide->elements[e]);
    }
    if (slide->elementCount > 0)
    {
        fputs("\r\n      ", file);
    }
    fputs("]\r\n    }", file);
}

bool Renderer_SaveDocument(const char* path, const char* name)
{
    if (!g_started || path == nullptr || path[0] == '\0')
    {
        return false;
    }

    FILE* file = nullptr;
    if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
    {
        return false;
    }

    fputs("{\r\n  \"format\": \"presentation-software-dev\",\r\n  \"version\": 1,\r\n  \"name\": ", file);
    JsonWriteText(file, name != nullptr ? name : "");
    fprintf(
        file,
        ",\r\n  \"canvas\": { \"width\": %d, \"height\": %d },\r\n  \"currentSlide\": %d,\r\n  \"editSlide\": %d,\r\n  \"slides\": [\r\n",
        DOCUMENT_WIDTH,
        DOCUMENT_HEIGHT,
        g_currentSlide,
        g_editSlide
    );

    for (int s = 0; s < g_slideCount; ++s)
    {
        JsonWriteSlide(file, &g_slides[s]);
        if (s + 1 < g_slideCount)
        {
            fputs(",", file);
        }
        fputs("\r\n", file);
    }

    fputs("  ]\r\n}\r\n", file);
    fclose(file);
    return true;
}

bool Renderer_SaveTemplate(const char* path, const char* id, const char* name)
{
    if (!g_started || path == nullptr || path[0] == '\0')
    {
        return false;
    }

    const int index = g_editSlide;
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }

    FILE* file = nullptr;
    if (fopen_s(&file, path, "wb") != 0 || file == nullptr)
    {
        return false;
    }

    fputs("{\r\n  \"format\": \"presentation-software-dev-template\",\r\n  \"version\": 1,\r\n  \"id\": ", file);
    JsonWriteString(file, id != nullptr ? id : "");
    fputs(",\r\n  \"name\": ", file);
    JsonWriteText(file, name != nullptr ? name : "");
    fprintf(
        file,
        ",\r\n  \"canvas\": { \"width\": %d, \"height\": %d },\r\n  \"slides\": [\r\n",
        DOCUMENT_WIDTH,
        DOCUMENT_HEIGHT
    );
    JsonWriteSlide(file, &g_slides[index]);
    fputs("\r\n  ]\r\n}\r\n", file);
    fclose(file);
    return true;
}

bool Renderer_LoadTemplate(const char* path, char* nameOut, int nameChars)
{
    return Renderer_LoadDocument(path, nameOut, nameChars);
}

static const char* JsonSkipWs(const char* p)
{
    while (p != nullptr && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
    {
        p += 1;
    }
    return p;
}

static const char* JsonSkipValue(const char* p);

static bool JsonParseString(const char*& p, char* out, int outChars)
{
    p = JsonSkipWs(p);
    if (p == nullptr || *p != '"')
    {
        return false;
    }
    p += 1;
    int written = 0;
    while (*p != '\0' && *p != '"')
    {
        char bytes[4] = { *p, 0, 0, 0 };
        int byteCount = 1;
        p += 1;
        if (bytes[0] == '\\' && *p != '\0')
        {
            char ch = *p;
            p += 1;
            if (ch == 'n')
            {
                ch = '\n';
            }
            else if (ch == 'r')
            {
                ch = '\r';
            }
            else if (ch == 't')
            {
                ch = '\t';
            }
            bytes[0] = ch;
            if (ch == 'u')
            {
                unsigned int code = 0;
                int digits = 0;
                while (digits < 4 && isxdigit(static_cast<unsigned char>(*p)))
                {
                    const char hex = *p;
                    code = code * 16 + (hex <= '9' ? hex - '0' : (hex | 0x20) - 'a' + 10);
                    p += 1;
                    digits += 1;
                }
                if (code < 0x80)
                {
                    bytes[0] = static_cast<char>(code);
                }
                else if (code < 0x800)
                {
                    bytes[0] = static_cast<char>(0xC0 | (code >> 6));
                    bytes[1] = static_cast<char>(0x80 | (code & 0x3F));
                    byteCount = 2;
                }
                else
                {
                    bytes[0] = static_cast<char>(0xE0 | (code >> 12));
                    bytes[1] = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    bytes[2] = static_cast<char>(0x80 | (code & 0x3F));
                    byteCount = 3;
                }
            }
        }
        if (out != nullptr && written + byteCount < outChars)
        {
            for (int b = 0; b < byteCount; ++b)
            {
                out[written + b] = bytes[b];
            }
            written += byteCount;
        }
    }
    if (*p != '"')
    {
        return false;
    }
    p += 1;
    if (out != nullptr && outChars > 0)
    {
        out[written] = '\0';
    }
    return true;
}

static bool JsonParseInt(const char*& p, int* value)
{
    p = JsonSkipWs(p);
    if (p == nullptr || value == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const long parsed = strtol(p, &end, 10);
    if (end == p)
    {
        return false;
    }
    *value = static_cast<int>(parsed);
    p = end;
    return true;
}

static bool JsonParseFloat(const char*& p, float* value)
{
    p = JsonSkipWs(p);
    if (p == nullptr || value == nullptr)
    {
        return false;
    }
    char* end = nullptr;
    const float parsed = strtof(p, &end);
    if (end == p)
    {
        return false;
    }
    *value = parsed;
    p = end;
    return true;
}

static bool JsonParseBool(const char*& p, bool* value)
{
    p = JsonSkipWs(p);
    if (p == nullptr || value == nullptr)
    {
        return false;
    }
    if (strncmp(p, "true", 4) == 0)
    {
        *value = true;
        p += 4;
        return true;
    }
    if (strncmp(p, "false", 5) == 0)
    {
        *value = false;
        p += 5;
        return true;
    }
    if (*p == '1')
    {
        *value = true;
        p += 1;
        return true;
    }
    if (*p == '0')
    {
        *value = false;
        p += 1;
        return true;
    }
    return false;
}

static bool JsonParseColor(const char*& p, COLORREF* color)
{
    char text[16] = {};
    if (!JsonParseString(p, text, sizeof(text)) || text[0] != '#' || lstrlenA(text) < 7)
    {
        return false;
    }
    unsigned int value = 0;
    sscanf_s(text + 1, "%x", &value);
    *color = RGB((value >> 16) & 255, (value >> 8) & 255, value & 255);
    return true;
}

static const char* JsonSkipValue(const char* p)
{
    p = JsonSkipWs(p);
    if (p == nullptr || *p == '\0')
    {
        return p;
    }
    if (*p == '"')
    {
        const char* cursor = p;
        JsonParseString(cursor, nullptr, 0);
        return cursor;
    }
    if (*p == '{')
    {
        int depth = 1;
        p += 1;
        while (*p != '\0' && depth > 0)
        {
            if (*p == '"')
            {
                JsonParseString(p, nullptr, 0);
                continue;
            }
            if (*p == '{')
            {
                depth += 1;
            }
            else if (*p == '}')
            {
                depth -= 1;
            }
            p += 1;
        }
        return p;
    }
    if (*p == '[')
    {
        int depth = 1;
        p += 1;
        while (*p != '\0' && depth > 0)
        {
            if (*p == '"')
            {
                JsonParseString(p, nullptr, 0);
                continue;
            }
            if (*p == '[')
            {
                depth += 1;
            }
            else if (*p == ']')
            {
                depth -= 1;
            }
            p += 1;
        }
        return p;
    }
    while (*p != '\0' && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\r' && *p != '\n')
    {
        p += 1;
    }
    return p;
}

static ElementKind ElementKindFromName(const char* name)
{
    if (lstrcmpiA(name, "rect") == 0)
    {
        return ELEMENT_RECT;
    }
    if (lstrcmpiA(name, "ellipse") == 0)
    {
        return ELEMENT_ELLIPSE;
    }
    if (lstrcmpiA(name, "image") == 0)
    {
        return ELEMENT_IMAGE;
    }
    if (lstrcmpiA(name, "video") == 0)
    {
        return ELEMENT_VIDEO;
    }
    return ELEMENT_TEXT;
}

static ImageDisplayMode DisplayModeFromName(const char* name)
{
    if (lstrcmpiA(name, "fit") == 0)
    {
        return IMAGE_FIT;
    }
    if (lstrcmpiA(name, "fill") == 0)
    {
        return IMAGE_FILL;
    }
    return IMAGE_STRETCH;
}

static bool JsonParseElement(const char*& p, Slide* slide)
{
    p = JsonSkipWs(p);
    if (*p != '{')
    {
        return false;
    }
    p += 1;

    char kindName[32] = "text";
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float rotation = 0.0f;
    char text[sizeof(SlideElement::text) * 3] = {};
    char fontName[64 * 3] = {};
    int fontSize = 32;
    COLORREF textColor = RGB(255, 255, 255);
    bool fontBold = false;
    bool fontItalic = false;
    bool fontUnderline = false;
    int align = 0;
    COLORREF fill = RGB(0, 0, 0);
    char displayName[16] = "stretch";
    char pathUtf8[MAX_PATH * 2] = {};
    bool locked = false;
    char slot[32] = {};

    while (*p != '\0')
    {
        p = JsonSkipWs(p);
        if (*p == '}')
        {
            p += 1;
            break;
        }
        if (*p == ',')
        {
            p += 1;
            continue;
        }
        char key[32] = {};
        if (!JsonParseString(p, key, sizeof(key)))
        {
            return false;
        }
        p = JsonSkipWs(p);
        if (*p != ':')
        {
            return false;
        }
        p += 1;
        if (lstrcmpA(key, "kind") == 0)
        {
            JsonParseString(p, kindName, sizeof(kindName));
        }
        else if (lstrcmpA(key, "x") == 0)
        {
            JsonParseInt(p, &x);
        }
        else if (lstrcmpA(key, "y") == 0)
        {
            JsonParseInt(p, &y);
        }
        else if (lstrcmpA(key, "w") == 0)
        {
            JsonParseInt(p, &w);
        }
        else if (lstrcmpA(key, "h") == 0)
        {
            JsonParseInt(p, &h);
        }
        else if (lstrcmpA(key, "rotation") == 0)
        {
            JsonParseFloat(p, &rotation);
        }
        else if (lstrcmpA(key, "text") == 0)
        {
            JsonParseString(p, text, sizeof(text));
        }
        else if (lstrcmpA(key, "fontName") == 0)
        {
            JsonParseString(p, fontName, sizeof(fontName));
        }
        else if (lstrcmpA(key, "fontSize") == 0)
        {
            JsonParseInt(p, &fontSize);
        }
        else if (lstrcmpA(key, "textColor") == 0)
        {
            JsonParseColor(p, &textColor);
        }
        else if (lstrcmpA(key, "bold") == 0)
        {
            JsonParseBool(p, &fontBold);
        }
        else if (lstrcmpA(key, "italic") == 0)
        {
            JsonParseBool(p, &fontItalic);
        }
        else if (lstrcmpA(key, "underline") == 0)
        {
            JsonParseBool(p, &fontUnderline);
        }
        else if (lstrcmpA(key, "align") == 0)
        {
            char alignName[16] = {};
            JsonParseString(p, alignName, sizeof(alignName));
            if (lstrcmpiA(alignName, "center") == 0)
            {
                align = 1;
            }
            else if (lstrcmpiA(alignName, "right") == 0)
            {
                align = 2;
            }
            else
            {
                align = 0;
            }
        }
        else if (lstrcmpA(key, "fill") == 0)
        {
            JsonParseColor(p, &fill);
        }
        else if (lstrcmpA(key, "displayMode") == 0)
        {
            JsonParseString(p, displayName, sizeof(displayName));
        }
        else if (lstrcmpA(key, "path") == 0)
        {
            JsonParseString(p, pathUtf8, sizeof(pathUtf8));
        }
        else if (lstrcmpA(key, "locked") == 0)
        {
            JsonParseBool(p, &locked);
        }
        else if (lstrcmpA(key, "slot") == 0)
        {
            JsonParseString(p, slot, sizeof(slot));
        }
        else
        {
            p = JsonSkipValue(p);
        }
    }

    SlideElement* element = AddElement(slide, ElementKindFromName(kindName), x, y, w, h);
    if (element == nullptr)
    {
        return true;
    }
    TextFromFile(text, sizeof(text));
    TextFromFile(fontName, sizeof(fontName));
    lstrcpynA(element->text, text, sizeof(element->text));
    lstrcpynA(element->fontName, fontName, sizeof(element->fontName));
    element->fontSize = fontSize;
    element->textColor = textColor;
    element->fontBold = fontBold;
    element->fontItalic = fontItalic;
    element->fontUnderline = fontUnderline;
    element->align = align;
    element->fill = fill;
    element->rotation = rotation;
    element->displayMode = DisplayModeFromName(displayName);
    element->locked = locked;
    lstrcpynA(element->slot, slot, sizeof(element->slot));
    if (pathUtf8[0] != '\0')
    {
        MultiByteToWideChar(CP_UTF8, 0, pathUtf8, -1, element->videoPath, MAX_PATH);
        wchar_t resolved[MAX_PATH] = {};
        const wchar_t* loadPath = element->videoPath;
        if (Document_ResolveAsset(element->videoPath, resolved, MAX_PATH))
        {
            loadPath = resolved;
        }
        if (element->kind == ELEMENT_IMAGE)
        {
            element->image = LoadPhoto(loadPath);
            element->placeholderIfMissing = true;
        }
        else if (element->kind == ELEMENT_VIDEO)
        {
            AttachVideoPoster(element, loadPath);
        }
    }
    return true;
}

static bool JsonParseSlide(const char*& p, Slide* slide, int canvasW, int canvasH)
{
    p = JsonSkipWs(p);
    if (*p != '{')
    {
        return false;
    }
    p += 1;
    ZeroMemory(slide, sizeof(*slide));
    slide->width = canvasW;
    slide->height = canvasH;
    slide->background = RGB(0x18, 0x18, 0x1C);
    slide->transition = SLIDE_TRANSITION_CROSSFADE;
    slide->transitionDir = SLIDE_TRANSITION_DIR_LEFT;
    slide->transitionMs = 300;
    slide->transitionOverride = false;
    bool sawTransitionField = false;
    bool sawOverrideField = false;

    while (*p != '\0')
    {
        p = JsonSkipWs(p);
        if (*p == '}')
        {
            p += 1;
            break;
        }
        if (*p == ',')
        {
            p += 1;
            continue;
        }
        char key[32] = {};
        if (!JsonParseString(p, key, sizeof(key)))
        {
            return false;
        }
        p = JsonSkipWs(p);
        if (*p != ':')
        {
            return false;
        }
        p += 1;
        if (lstrcmpA(key, "id") == 0)
        {
            JsonParseString(p, slide->id, sizeof(slide->id));
        }
        else if (lstrcmpA(key, "role") == 0)
        {
            char role[32] = {};
            JsonParseString(p, role, sizeof(role));
            slide->role = (lstrcmpA(role, "background") == 0) ? SLIDE_ROLE_BACKGROUND : SLIDE_ROLE_SLIDE;
        }
        else if (lstrcmpA(key, "transitionOverride") == 0)
        {
            JsonParseBool(p, &slide->transitionOverride);
            sawOverrideField = true;
        }
        else if (lstrcmpA(key, "transition") == 0)
        {
            char trans[32] = {};
            JsonParseString(p, trans, sizeof(trans));
            sawTransitionField = true;
            if (lstrcmpA(trans, "crossfade") == 0)
            {
                slide->transition = SLIDE_TRANSITION_CROSSFADE;
            }
            else if (lstrcmpA(trans, "fadeblack") == 0)
            {
                slide->transition = SLIDE_TRANSITION_FADE_BLACK;
            }
            else if (lstrcmpA(trans, "push") == 0)
            {
                slide->transition = SLIDE_TRANSITION_PUSH;
            }
            else if (lstrcmpA(trans, "wipe") == 0)
            {
                slide->transition = SLIDE_TRANSITION_WIPE;
            }
            else if (lstrcmpA(trans, "cover") == 0)
            {
                slide->transition = SLIDE_TRANSITION_COVER;
            }
            else
            {
                slide->transition = SLIDE_TRANSITION_NONE;
            }
        }
        else if (lstrcmpA(key, "transitionDir") == 0)
        {
            char dir[32] = {};
            JsonParseString(p, dir, sizeof(dir));
            sawTransitionField = true;
            if (lstrcmpA(dir, "right") == 0)
            {
                slide->transitionDir = SLIDE_TRANSITION_DIR_RIGHT;
            }
            else if (lstrcmpA(dir, "up") == 0)
            {
                slide->transitionDir = SLIDE_TRANSITION_DIR_UP;
            }
            else if (lstrcmpA(dir, "down") == 0)
            {
                slide->transitionDir = SLIDE_TRANSITION_DIR_DOWN;
            }
            else
            {
                slide->transitionDir = SLIDE_TRANSITION_DIR_LEFT;
            }
        }
        else if (lstrcmpA(key, "transitionMs") == 0)
        {
            JsonParseInt(p, &slide->transitionMs);
            sawTransitionField = true;
        }
        else if (lstrcmpA(key, "background") == 0)
        {
            JsonParseColor(p, &slide->background);
        }
        else if (lstrcmpA(key, "width") == 0)
        {
            JsonParseInt(p, &slide->width);
        }
        else if (lstrcmpA(key, "height") == 0)
        {
            JsonParseInt(p, &slide->height);
        }
        else if (lstrcmpA(key, "elements") == 0)
        {
            p = JsonSkipWs(p);
            if (*p != '[')
            {
                return false;
            }
            p += 1;
            while (*p != '\0')
            {
                p = JsonSkipWs(p);
                if (*p == ']')
                {
                    p += 1;
                    break;
                }
                if (*p == ',')
                {
                    p += 1;
                    continue;
                }
                if (!JsonParseElement(p, slide))
                {
                    return false;
                }
            }
        }
        else
        {
            p = JsonSkipValue(p);
        }
    }
    if (slide->width < 1)
    {
        slide->width = canvasW;
    }
    if (slide->height < 1)
    {
        slide->height = canvasH;
    }
    if (!sawOverrideField)
    {
        // Older docs baked the renderer default onto every slide. Only keep an
        // override when the stored values differ from the current role default.
        const bool asBg = slide->role == SLIDE_ROLE_BACKGROUND;
        const int defKind = asBg ? g_defaultBgTransition : g_defaultSlideTransition;
        const int defDir = asBg ? g_defaultBgTransitionDir : g_defaultSlideTransitionDir;
        const int defMs = asBg ? g_defaultBgTransitionMs : g_defaultSlideTransitionMs;
        const int ms = slide->transitionMs < 100 ? 300 : ClampTransitionMs(slide->transitionMs);
        slide->transitionOverride =
            sawTransitionField &&
            (ClampSlideTransition(slide->transition) != ClampSlideTransition(defKind) ||
             ClampSlideTransitionDir(slide->transitionDir) != ClampSlideTransitionDir(defDir) ||
             ms != ClampTransitionMs(defMs));
    }
    if (slide->role == SLIDE_ROLE_BACKGROUND)
    {
        bool anyLocked = false;
        for (int i = 0; i < slide->elementCount; ++i)
        {
            if (slide->elements[i].locked)
            {
                anyLocked = true;
                break;
            }
        }
        if (!anyLocked)
        {
            for (int i = 0; i < slide->elementCount; ++i)
            {
                if (slide->elements[i].kind == ELEMENT_IMAGE || slide->elements[i].kind == ELEMENT_VIDEO)
                {
                    slide->elements[i].locked = true;
                    break;
                }
            }
        }
    }
    return true;
}

bool Renderer_LoadDocument(const char* path, char* nameOut, int nameChars)
{
    if (!g_started || path == nullptr || path[0] == '\0')
    {
        return false;
    }

    FILE* file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || file == nullptr)
    {
        return false;
    }

    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0 || size > 8 * 1024 * 1024)
    {
        fclose(file);
        return false;
    }

    char* json = static_cast<char*>(malloc(static_cast<size_t>(size) + 1));
    if (json == nullptr)
    {
        fclose(file);
        return false;
    }
    const size_t read = fread(json, 1, static_cast<size_t>(size), file);
    fclose(file);
    json[read] = '\0';

    const char* p = JsonSkipWs(json);
    if (*p != '{')
    {
        free(json);
        return false;
    }
    p += 1;

    char name[128 * 3] = {};
    int canvasW = DOCUMENT_WIDTH;
    int canvasH = DOCUMENT_HEIGHT;
    int currentSlide = 0;
    int editSlide = 0;
    Slide* loaded = static_cast<Slide*>(malloc(sizeof(Slide) * MAX_SLIDES));
    if (loaded == nullptr)
    {
        free(json);
        return false;
    }
    ZeroMemory(loaded, sizeof(Slide) * MAX_SLIDES);
    int loadedCount = 0;
    bool ok = true;

    while (ok && *p != '\0')
    {
        p = JsonSkipWs(p);
        if (*p == '}')
        {
            break;
        }
        if (*p == ',')
        {
            p += 1;
            continue;
        }
        char key[32] = {};
        if (!JsonParseString(p, key, sizeof(key)))
        {
            ok = false;
            break;
        }
        p = JsonSkipWs(p);
        if (*p != ':')
        {
            ok = false;
            break;
        }
        p += 1;
        if (lstrcmpA(key, "name") == 0)
        {
            JsonParseString(p, name, sizeof(name));
        }
        else if (lstrcmpA(key, "currentSlide") == 0)
        {
            JsonParseInt(p, &currentSlide);
        }
        else if (lstrcmpA(key, "editSlide") == 0)
        {
            JsonParseInt(p, &editSlide);
        }
        else if (lstrcmpA(key, "canvas") == 0)
        {
            p = JsonSkipWs(p);
            if (*p != '{')
            {
                ok = false;
                break;
            }
            p += 1;
            while (*p != '\0')
            {
                p = JsonSkipWs(p);
                if (*p == '}')
                {
                    p += 1;
                    break;
                }
                if (*p == ',')
                {
                    p += 1;
                    continue;
                }
                char canvasKey[16] = {};
                if (!JsonParseString(p, canvasKey, sizeof(canvasKey)))
                {
                    ok = false;
                    break;
                }
                p = JsonSkipWs(p);
                if (*p != ':')
                {
                    ok = false;
                    break;
                }
                p += 1;
                if (lstrcmpA(canvasKey, "width") == 0)
                {
                    JsonParseInt(p, &canvasW);
                }
                else if (lstrcmpA(canvasKey, "height") == 0)
                {
                    JsonParseInt(p, &canvasH);
                }
                else
                {
                    p = JsonSkipValue(p);
                }
            }
        }
        else if (lstrcmpA(key, "slides") == 0)
        {
            p = JsonSkipWs(p);
            if (*p != '[')
            {
                ok = false;
                break;
            }
            p += 1;
            while (*p != '\0' && loadedCount < MAX_SLIDES)
            {
                p = JsonSkipWs(p);
                if (*p == ']')
                {
                    p += 1;
                    break;
                }
                if (*p == ',')
                {
                    p += 1;
                    continue;
                }
                if (!JsonParseSlide(p, &loaded[loadedCount], canvasW, canvasH))
                {
                    ok = false;
                    break;
                }
                loadedCount += 1;
            }
        }
        else
        {
            p = JsonSkipValue(p);
        }
    }

    free(json);
    if (!ok || loadedCount < 1)
    {
        free(loaded);
        return false;
    }

    for (int i = 0; i < MAX_SLIDES; ++i)
    {
        FreeFrame(i);
        ZeroMemory(&g_slides[i], sizeof(g_slides[i]));
    }
    for (int i = 0; i < loadedCount; ++i)
    {
        g_slides[i] = loaded[i];
        PrerenderSlide(i);
        if (g_frames[i] == nullptr)
        {
            free(loaded);
            return false;
        }
    }

    g_slideCount = loadedCount;
    Renderer_NormalizeInheritedTransitions();
    g_currentSlide = (currentSlide >= 0 && currentSlide < loadedCount) ? currentSlide : 0;
    g_editSlide = (editSlide >= 0 && editSlide < loadedCount) ? editSlide : 0;
    g_backgroundSlide = -1;
    for (int i = 0; i < g_slideCount; ++i)
    {
        if (g_slides[i].role == SLIDE_ROLE_BACKGROUND)
        {
            g_backgroundSlide = i;
            break;
        }
    }
    g_nextSlideSerial = 1;
    for (int i = 0; i < g_slideCount; ++i)
    {
        EnsureSlideId(&g_slides[i]);
    }
    free(loaded);
    if (nameOut != nullptr && nameChars > 0)
    {
        TextFromFile(name, sizeof(name));
        lstrcpynA(nameOut, name, nameChars);
    }
    return true;
}

static char* ReadJsonFile(const char* path)
{
    FILE* file = nullptr;
    if (path == nullptr || fopen_s(&file, path, "rb") != 0 || file == nullptr)
    {
        return nullptr;
    }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0 || size > 8 * 1024 * 1024)
    {
        fclose(file);
        return nullptr;
    }
    char* json = static_cast<char*>(malloc(static_cast<size_t>(size) + 1));
    if (json == nullptr)
    {
        fclose(file);
        return nullptr;
    }
    const size_t read = fread(json, 1, static_cast<size_t>(size), file);
    fclose(file);
    json[read] = '\0';
    return json;
}

// Reads the one slide of a template file into `out` without touching the open
// document. The caller frees the slide's images with FreeSlideImages.
static bool LoadTemplateSlide(const char* path, Slide* out)
{
    char* json = ReadJsonFile(path);
    if (json == nullptr)
    {
        return false;
    }

    const char* p = JsonSkipWs(json);
    bool found = false;
    if (*p == '{')
    {
        p += 1;
        int canvasW = DOCUMENT_WIDTH;
        int canvasH = DOCUMENT_HEIGHT;
        while (!found && *p != '\0')
        {
            p = JsonSkipWs(p);
            if (*p == '}')
            {
                break;
            }
            if (*p == ',')
            {
                p += 1;
                continue;
            }
            char key[32] = {};
            if (!JsonParseString(p, key, sizeof(key)))
            {
                break;
            }
            p = JsonSkipWs(p);
            if (*p != ':')
            {
                break;
            }
            p += 1;
            p = JsonSkipWs(p);
            if (lstrcmpA(key, "canvas") == 0 && *p == '{')
            {
                p += 1;
                while (*p != '\0')
                {
                    p = JsonSkipWs(p);
                    if (*p == '}')
                    {
                        p += 1;
                        break;
                    }
                    if (*p == ',')
                    {
                        p += 1;
                        continue;
                    }
                    char canvasKey[16] = {};
                    if (!JsonParseString(p, canvasKey, sizeof(canvasKey)))
                    {
                        break;
                    }
                    p = JsonSkipWs(p);
                    if (*p != ':')
                    {
                        break;
                    }
                    p += 1;
                    if (lstrcmpA(canvasKey, "width") == 0)
                    {
                        JsonParseInt(p, &canvasW);
                    }
                    else if (lstrcmpA(canvasKey, "height") == 0)
                    {
                        JsonParseInt(p, &canvasH);
                    }
                    else
                    {
                        p = JsonSkipValue(p);
                    }
                }
            }
            else if (lstrcmpA(key, "slides") == 0 && *p == '[')
            {
                p += 1;
                p = JsonSkipWs(p);
                found = *p == '{' && JsonParseSlide(p, out, canvasW, canvasH);
                break;
            }
            else
            {
                p = JsonSkipValue(p);
            }
        }
    }
    free(json);
    if (!found)
    {
        FreeSlideImages(out);
        ZeroMemory(out, sizeof(*out));
    }
    return found;
}

static int MainTextElement(const Slide* slide)
{
    int best = -1;
    long long bestArea = -1;
    for (int i = 0; i < slide->elementCount; ++i)
    {
        const SlideElement* element = &slide->elements[i];
        if (element->kind != ELEMENT_TEXT)
        {
            continue;
        }
        const long long area = static_cast<long long>(abs(element->w)) * abs(element->h);
        if (area > bestArea)
        {
            best = i;
            bestArea = area;
        }
    }
    return best;
}

// The slide keeps its id, role, transition and the words of its largest text
// box; everything else comes from the template. Template text is only a
// sample, so every template text box arrives empty before the slide's words
// go into the largest one. A background slide also keeps its bed so it still
// plays as a background.
static void ApplyTemplateSlide(Slide* slide, const Slide* templateSlide)
{
    Slide* fresh = static_cast<Slide*>(calloc(1, sizeof(Slide)));
    if (fresh == nullptr)
    {
        return;
    }
    DeepCopySlide(fresh, templateSlide);

    int dstW = 0;
    int dstH = 0;
    int srcW = 0;
    int srcH = 0;
    GetNativeSize(slide, &dstW, &dstH);
    GetNativeSize(templateSlide, &srcW, &srcH);
    if (dstW != srcW || dstH != srcH)
    {
        for (int i = 0; i < fresh->elementCount; ++i)
        {
            SlideElement* element = &fresh->elements[i];
            element->x = MulDiv(element->x, dstW, srcW);
            element->y = MulDiv(element->y, dstH, srcH);
            element->w = MulDiv(element->w, dstW, srcW);
            element->h = MulDiv(element->h, dstH, srcH);
            element->fontSize = max(1, MulDiv(element->fontSize, dstH, srcH));
        }
    }

    for (int i = 0; i < fresh->elementCount; ++i)
    {
        if (fresh->elements[i].kind == ELEMENT_TEXT)
        {
            fresh->elements[i].text[0] = '\0';
        }
    }

    const int from = MainTextElement(slide);
    const int to = MainTextElement(fresh);
    if (from >= 0)
    {
        if (to >= 0)
        {
            lstrcpynA(fresh->elements[to].text, slide->elements[from].text, sizeof(fresh->elements[to].text));
            FitTextElementHeight(&fresh->elements[to]);
        }
        else if (fresh->elementCount < MAX_ELEMENTS)
        {
            fresh->elements[fresh->elementCount] = slide->elements[from];
            fresh->elements[fresh->elementCount].image = nullptr;
            fresh->elementCount += 1;
        }
    }

    if (slide->role == SLIDE_ROLE_BACKGROUND)
    {
        for (int i = slide->elementCount - 1; i >= 0; --i)
        {
            if (!IsBedElement(slide, &slide->elements[i]) || fresh->elementCount >= MAX_ELEMENTS)
            {
                continue;
            }
            memmove(&fresh->elements[1], &fresh->elements[0], sizeof(SlideElement) * fresh->elementCount);
            fresh->elements[0] = slide->elements[i];
            fresh->elementCount += 1;
            slide->elements[i].image = nullptr;
        }
    }

    FreeSlideImages(slide);
    slide->background = fresh->background;
    memcpy(slide->elements, fresh->elements, sizeof(SlideElement) * fresh->elementCount);
    slide->elementCount = fresh->elementCount;
    free(fresh);
}

bool Renderer_ApplyTemplate(const char* path, const int* indices, int count)
{
    if (!App_Ready() || path == nullptr || indices == nullptr || count <= 0)
    {
        return false;
    }
    Slide* templateSlide = static_cast<Slide*>(calloc(1, sizeof(Slide)));
    if (templateSlide == nullptr)
    {
        return false;
    }
    if (!LoadTemplateSlide(path, templateSlide))
    {
        free(templateSlide);
        return false;
    }

    bool applied = false;
    for (int k = 0; k < count; ++k)
    {
        const int index = indices[k];
        if (index < 0 || index >= g_slideCount)
        {
            continue;
        }
        ApplyTemplateSlide(&g_slides[index], templateSlide);
        PrerenderSlide(index);
        applied = true;
    }
    FreeSlideImages(templateSlide);
    free(templateSlide);
    return applied;
}

HBITMAP Renderer_RenderTemplateThumb(const char* path, int width, int height)
{
    if (!g_started || width <= 0 || height <= 0)
    {
        return nullptr;
    }
    Slide* templateSlide = static_cast<Slide*>(calloc(1, sizeof(Slide)));
    if (templateSlide == nullptr)
    {
        return nullptr;
    }
    if (!LoadTemplateSlide(path, templateSlide))
    {
        free(templateSlide);
        return nullptr;
    }

    int nativeW = 0;
    int nativeH = 0;
    GetNativeSize(templateSlide, &nativeW, &nativeH);

    HDC screen = GetDC(nullptr);
    HBITMAP frame = CreateFrameBitmap(screen, nativeW, nativeH);
    HBITMAP thumb = CreateFrameBitmap(screen, width, height);
    HDC src = CreateCompatibleDC(screen);
    HDC dst = CreateCompatibleDC(screen);
    if (frame != nullptr && thumb != nullptr)
    {
        HBITMAP oldSrc = (HBITMAP)SelectObject(src, frame);
        ComposeSlide(src, 0, 0, nativeW, nativeH, templateSlide, false);
        SelectObject(src, oldSrc);
        SealFrameAlpha(frame);

        oldSrc = (HBITMAP)SelectObject(src, frame);
        HBITMAP oldDst = (HBITMAP)SelectObject(dst, thumb);
        FillCheckerboard(dst, 0, 0, width, height);
        BlitAlphaBitmap(dst, 0, 0, width, height, src, 0, 0, nativeW, nativeH);
        SelectObject(dst, oldDst);
        SelectObject(src, oldSrc);
    }
    DeleteDC(src);
    DeleteDC(dst);
    ReleaseDC(nullptr, screen);
    if (frame != nullptr)
    {
        DeleteObject(frame);
    }
    FreeSlideImages(templateSlide);
    free(templateSlide);
    return thumb;
}

void Renderer_BlitSlideContentAt(HDC hdc, int index, int x, int y, int width, int height)
{
    if (!App_Ready() || hdc == nullptr || width <= 0 || height <= 0)
    {
        return;
    }
    if (index < 0 || index >= g_slideCount)
    {
        return;
    }

    HBITMAP frame = g_frames[index];
    if (frame == nullptr)
    {
        return;
    }

    int sourceW = 0;
    int sourceH = 0;
    GetNativeSize(&g_slides[index], &sourceW, &sourceH);
    if (sourceW <= 0 || sourceH <= 0)
    {
        return;
    }

    HDC memory = CreateCompatibleDC(hdc);
    HBITMAP old = (HBITMAP)SelectObject(memory, frame);
    BlitAlphaBitmap(hdc, x, y, width, height, memory, 0, 0, sourceW, sourceH);
    SelectObject(memory, old);
    DeleteDC(memory);
}

void Renderer_BlitSlidePreviewAt(HDC hdc, int index, int x, int y, int width, int height)
{
    if (!App_Ready() || hdc == nullptr || width <= 0 || height <= 0)
    {
        return;
    }
    if (index < 0 || index >= g_slideCount)
    {
        return;
    }
    FillCheckerboard(hdc, x, y, width, height);
    ComposeSlideElements(hdc, x, y, width, height, &g_slides[index], -1, true);
}

void Renderer_BlitSlideAt(HDC hdc, int index, int x, int y, int width, int height)
{
    Renderer_BlitSlidePreviewAt(hdc, index, x, y, width, height);
}

HBITMAP Renderer_SlideFrame(int index, int* width, int* height)
{
    if (index < 0 || index >= g_slideCount || g_frames[index] == nullptr)
    {
        return nullptr;
    }
    if (width != nullptr && height != nullptr)
    {
        GetNativeSize(&g_slides[index], width, height);
    }
    return g_frames[index];
}

void Renderer_Blit(HDC hdc, int width, int height)
{
    RECT well = { 0, 0, width, height };
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &well, fill);
    DeleteObject(fill);
    Renderer_BlitSlideContentAt(hdc, g_currentSlide, 0, 0, width, height);
}

void Renderer_BlitAt(HDC hdc, int x, int y, int width, int height)
{
    RECT well = { x, y, x + width, y + height };
    HBRUSH fill = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &well, fill);
    DeleteObject(fill);
    Renderer_BlitSlideContentAt(hdc, g_currentSlide, x, y, width, height);
}

void Renderer_GetSlideSize(int index, int* width, int* height)
{
    if (index < 0 || index >= g_slideCount)
    {
        return;
    }

    GetNativeSize(&g_slides[index], width, height);
}

void Renderer_PresentTo(HWND hwnd)
{
    if (!App_Ready() || hwnd == nullptr || !IsWindow(hwnd))
    {
        return;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0)
    {
        return;
    }

    HDC hdc = GetDC(hwnd);
    Renderer_Blit(hdc, client.right, client.bottom);
    ReleaseDC(hwnd, hdc);
}

void Renderer_PresentHostThenDisplays(HWND host, HWND displays[3])
{
    if (!App_Ready())
    {
        return;
    }

    Renderer_PresentTo(host);

    if (displays == nullptr)
    {
        return;
    }

    Renderer_PresentTo(displays[0]);
    Renderer_PresentTo(displays[1]);
    Renderer_PresentTo(displays[2]);
}

static SlideElement* FindElement(Slide* slide, ElementKind kind)
{
    for (int i = 0; i < slide->elementCount; ++i)
    {
        if (slide->elements[i].kind == kind)
        {
            return &slide->elements[i];
        }
    }
    return nullptr;
}

static void RebuildSlide(int index)
{
    if (!App_Ready())
    {
        return;
    }

    PrerenderSlide(index);
    g_currentSlide = index;
}

static SlideElement* FindSlotElement(Slide* slide, const char* slot)
{
    if (slide == nullptr)
    {
        return nullptr;
    }
    if (slot != nullptr && slot[0] != '\0')
    {
        for (int i = 0; i < slide->elementCount; ++i)
        {
            if (slide->elements[i].kind == ELEMENT_TEXT &&
                lstrcmpiA(slide->elements[i].slot, slot) == 0)
            {
                return &slide->elements[i];
            }
        }
    }
    for (int i = 0; i < slide->elementCount; ++i)
    {
        if (slide->elements[i].kind == ELEMENT_TEXT)
        {
            return &slide->elements[i];
        }
    }
    return nullptr;
}

bool Renderer_SlideSlotEmpty(int slideIndex, const char* slot)
{
    if (!App_Ready() || slideIndex < 0 || slideIndex >= g_slideCount)
    {
        return false;
    }
    const SlideElement* element = FindSlotElement(&g_slides[slideIndex], slot);
    return element == nullptr || element->text[0] == '\0';
}

bool Renderer_SetSlideSlotText(int slideIndex, const char* slot, const char* text)
{
    if (!App_Ready() || slideIndex < 0 || slideIndex >= g_slideCount || text == nullptr)
    {
        return false;
    }

    SlideElement* element = FindSlotElement(&g_slides[slideIndex], slot);
    if (element == nullptr)
    {
        return false;
    }
    if (slot != nullptr && slot[0] != '\0' && element->slot[0] == '\0')
    {
        lstrcpynA(element->slot, slot, sizeof(element->slot));
    }
    lstrcpynA(element->text, text, sizeof(element->text));
    FitTextElementHeight(element);
    PrerenderSlide(slideIndex);
    return true;
}

void Renderer_GetSlide5Text(char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }

    out[0] = '\0';
    SlideElement* text = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_TEXT);
    if (text != nullptr)
    {
        lstrcpynA(out, text->text, outChars);
    }
}

void Renderer_SetSlide5Text(const char* text)
{
    if (!App_Ready())
    {
        return;
    }

    SlideElement* element = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_TEXT);
    if (element == nullptr || text == nullptr)
    {
        return;
    }

    lstrcpynA(element->text, text, sizeof(element->text));
    RebuildSlide(SLIDE_TEXT);
}

void Renderer_SetSlide5BackgroundColor(COLORREF color)
{
    if (!App_Ready())
    {
        return;
    }

    g_slides[SLIDE_TEXT].background = color;
    SlideElement* backdrop = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_IMAGE);
    if (backdrop != nullptr)
    {
        backdrop->image = nullptr;
    }

    delete g_slide5Background;
    g_slide5Background = nullptr;
    RebuildSlide(SLIDE_TEXT);
}

bool Renderer_SetSlide5BackgroundImage(const wchar_t* path)
{
    if (!App_Ready() || !App_HasFullGraphics())
    {
        return false;
    }

    Gdiplus::Image* image = LoadPhoto(path);
    if (image == nullptr)
    {
        return false;
    }

    delete g_slide5Background;
    g_slide5Background = image;

    SlideElement* backdrop = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_IMAGE);
    if (backdrop != nullptr)
    {
        backdrop->image = g_slide5Background;
    }

    RebuildSlide(SLIDE_TEXT);
    return true;
}

void Renderer_SetSlide5Font(const char* fontName, int fontSize)
{
    if (!App_Ready())
    {
        return;
    }

    SlideElement* element = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_TEXT);
    if (element == nullptr)
    {
        return;
    }

    if (fontName != nullptr && fontName[0] != '\0')
    {
        lstrcpynA(element->fontName, fontName, sizeof(element->fontName));
    }

    if (fontSize > 0)
    {
        element->fontSize = fontSize;
    }

    RebuildSlide(SLIDE_TEXT);
}

void Renderer_SetSlide5TextColor(COLORREF color)
{
    if (!App_Ready())
    {
        return;
    }

    SlideElement* element = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_TEXT);
    if (element == nullptr)
    {
        return;
    }

    element->textColor = color;
    RebuildSlide(SLIDE_TEXT);
}

void Renderer_GetSlide5Font(char* outName, int outChars, int* fontSize)
{
    SlideElement* element = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_TEXT);
    if (element == nullptr)
    {
        return;
    }

    if (outName != nullptr && outChars > 0)
    {
        lstrcpynA(outName, element->fontName[0] != '\0' ? element->fontName : "Segoe UI", outChars);
    }

    if (fontSize != nullptr)
    {
        *fontSize = element->fontSize;
    }
}

bool Renderer_SetSlide6Image(const wchar_t* path)
{
    if (!App_Ready() || !App_HasFullGraphics())
    {
        return false;
    }

    Gdiplus::Image* image = LoadPhoto(path);
    if (image == nullptr)
    {
        return false;
    }

    delete g_slide6Photo;
    g_slide6Photo = image;

    SlideElement* photo = FindElement(&g_slides[SLIDE_PHOTO], ELEMENT_IMAGE);
    if (photo != nullptr)
    {
        photo->image = g_slide6Photo;
    }

    RebuildSlide(SLIDE_PHOTO);
    return true;
}

void Renderer_SetSlide5ImageMode(ImageDisplayMode mode)
{
    if (!App_Ready())
    {
        return;
    }

    SlideElement* backdrop = FindElement(&g_slides[SLIDE_TEXT], ELEMENT_IMAGE);
    if (backdrop == nullptr)
    {
        return;
    }

    backdrop->displayMode = mode;
    RebuildSlide(SLIDE_TEXT);
}

void Renderer_SetSlide6ImageMode(ImageDisplayMode mode)
{
    if (!App_Ready())
    {
        return;
    }

    SlideElement* photo = FindElement(&g_slides[SLIDE_PHOTO], ELEMENT_IMAGE);
    if (photo == nullptr)
    {
        return;
    }

    photo->displayMode = mode;
    RebuildSlide(SLIDE_PHOTO);
}

static void NormalizeFrame(int* x, int* y, int* w, int* h)
{
    if (*w < 0)
    {
        *x += *w;
        *w = -*w;
    }
    if (*h < 0)
    {
        *y += *h;
        *h = -*h;
    }
    if (*w < 40)
    {
        *w = 40;
    }
    if (*h < 40)
    {
        *h = 40;
    }
}

static int EditSlideIndex()
{
    if (!g_started || g_slideCount <= 0)
    {
        return 0;
    }
    if (g_editSlide < 0 || g_editSlide >= g_slideCount)
    {
        return 0;
    }
    return g_editSlide;
}

static Slide* EditSlidePtr()
{
    return &g_slides[EditSlideIndex()];
}

bool Renderer_SetEditSlide(int index)
{
    if (!g_started || index < 0 || index >= g_slideCount)
    {
        return false;
    }
    g_editSlide = index;
    return true;
}

int Renderer_EditSlide()
{
    return EditSlideIndex();
}

int Renderer_FreeformCount()
{
    return EditSlidePtr()->elementCount;
}

bool Renderer_FreeformGet(int index, int* kind, int* x, int* y, int* w, int* h)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount)
    {
        return false;
    }

    const SlideElement* element = &slide->elements[index];
    if (kind != nullptr)
    {
        *kind = element->kind;
    }
    if (x != nullptr)
    {
        *x = element->x;
    }
    if (y != nullptr)
    {
        *y = element->y;
    }
    if (w != nullptr)
    {
        *w = element->w;
    }
    if (h != nullptr)
    {
        *h = element->h;
    }
    return true;
}

int Renderer_FreeformHitTest(int slideX, int slideY)
{
    return Renderer_FreeformHitTestPad(slideX, slideY, 0);
}

int Renderer_FreeformHitTestPad(int slideX, int slideY, int pad)
{
    Slide* slide = EditSlidePtr();
    for (int i = slide->elementCount - 1; i >= 0; --i)
    {
        if (slide->elements[i].locked)
        {
            continue;
        }
        if (HitElementLocal(&slide->elements[i], slideX, slideY, pad))
        {
            return i;
        }
    }
    return -1;
}

int Renderer_FreeformHitTestAny(int slideX, int slideY)
{
    Slide* slide = EditSlidePtr();
    for (int i = slide->elementCount - 1; i >= 0; --i)
    {
        if (HitElementLocal(&slide->elements[i], slideX, slideY, 0))
        {
            return i;
        }
    }
    return -1;
}

bool Renderer_FreeformIsLocked(int index)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount)
    {
        return false;
    }
    return slide->elements[index].locked;
}

float Renderer_FreeformGetRotation(int index)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount)
    {
        return 0.0f;
    }
    return slide->elements[index].rotation;
}

void Renderer_FreeformSetRotation(int index, float degrees)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].locked)
    {
        return;
    }
    while (degrees > 180.0f)
    {
        degrees -= 360.0f;
    }
    while (degrees < -180.0f)
    {
        degrees += 360.0f;
    }
    slide->elements[index].rotation = degrees;
}

int Renderer_FreeformAddText(int x, int y)
{
    if (!App_Ready())
    {
        return -1;
    }

    Slide* slide = EditSlidePtr();
    SlideElement* element = AddElement(slide, ELEMENT_TEXT, x, y, 600, 140);
    if (element == nullptr)
    {
        return -1;
    }

    element->fontSize = 64;
    element->textColor = RGB(255, 255, 255);
    element->fontBold = false;
    element->fontItalic = false;
    element->fontUnderline = false;
    element->align = 1;
    lstrcpyA(element->fontName, "Segoe UI");
    RebuildSlide(EditSlideIndex());
    return slide->elementCount - 1;
}

int Renderer_FreeformAddRect(int x, int y, int w, int h)
{
    if (!App_Ready())
    {
        return -1;
    }

    NormalizeFrame(&x, &y, &w, &h);
    Slide* slide = EditSlidePtr();
    SlideElement* element = AddElement(slide, ELEMENT_RECT, x, y, w, h);
    if (element == nullptr)
    {
        return -1;
    }

    element->fill = RGB(200, 60, 60);
    RebuildSlide(EditSlideIndex());
    return slide->elementCount - 1;
}

int Renderer_FreeformAddEllipse(int x, int y, int w, int h)
{
    if (!App_Ready())
    {
        return -1;
    }

    NormalizeFrame(&x, &y, &w, &h);
    Slide* slide = EditSlidePtr();
    SlideElement* element = AddElement(slide, ELEMENT_ELLIPSE, x, y, w, h);
    if (element == nullptr)
    {
        return -1;
    }

    element->fill = RGB(40, 90, 180);
    RebuildSlide(EditSlideIndex());
    return slide->elementCount - 1;
}

int Renderer_FreeformAddImage(int x, int y, int w, int h, const wchar_t* path)
{
    if (!App_Ready() || !App_HasFullGraphics())
    {
        return -1;
    }

    Gdiplus::Image* image = LoadPhoto(path);
    wchar_t resolved[MAX_PATH] = {};
    if (image == nullptr && Document_ResolveAsset(path, resolved, MAX_PATH))
    {
        image = LoadPhoto(resolved);
    }
    if (image == nullptr)
    {
        return -1;
    }

    NormalizeFrame(&x, &y, &w, &h);
    Slide* slide = EditSlidePtr();
    SlideElement* element = AddElement(slide, ELEMENT_IMAGE, x, y, w, h);
    if (element == nullptr)
    {
        delete image;
        return -1;
    }

    element->image = image;
    element->placeholderIfMissing = true;
    element->displayMode = IMAGE_FIT;
    if (path != nullptr)
    {
        lstrcpynW(element->videoPath, path, MAX_PATH);
    }
    RebuildSlide(EditSlideIndex());
    return slide->elementCount - 1;
}

void Renderer_FreeformSetFrame(int index, int x, int y, int w, int h)
{
    if (!App_Ready())
    {
        return;
    }

    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].locked)
    {
        return;
    }

    NormalizeFrame(&x, &y, &w, &h);
    slide->elements[index].x = x;
    slide->elements[index].y = y;
    slide->elements[index].w = w;
    slide->elements[index].h = h;
}

void Renderer_FreeformDelete(int index)
{
    if (!App_Ready())
    {
        return;
    }

    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount)
    {
        return;
    }

    Gdiplus::Image* image = slide->elements[index].image;
    if (image != nullptr &&
        image != g_photo &&
        image != g_slide5Background &&
        image != g_slide6Photo)
    {
        delete image;
    }

    for (int i = index; i < slide->elementCount - 1; ++i)
    {
        slide->elements[i] = slide->elements[i + 1];
    }
    slide->elementCount -= 1;
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformGetText(int index, char* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return;
    }

    out[0] = '\0';
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return;
    }

    lstrcpynA(out, slide->elements[index].text, outChars);
}

void Renderer_FreeformSetText(int index, const char* text)
{
    Renderer_FreeformSetTextLive(index, text);
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformSetTextLive(int index, const char* text)
{
    if (!App_Ready())
    {
        return;
    }

    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || text == nullptr)
    {
        return;
    }

    if (slide->elements[index].kind != ELEMENT_TEXT)
    {
        return;
    }

    lstrcpynA(slide->elements[index].text, text, sizeof(slide->elements[index].text));
}

bool Renderer_FreeformGetStyle(
    int index,
    char* fontName,
    int nameChars,
    int* fontSize,
    COLORREF* color,
    bool* bold,
    bool* italic,
    bool* underline)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return false;
    }

    const SlideElement* element = &slide->elements[index];
    if (fontName != nullptr && nameChars > 0)
    {
        lstrcpynA(fontName, element->fontName[0] != '\0' ? element->fontName : "Segoe UI", nameChars);
    }
    if (fontSize != nullptr)
    {
        *fontSize = element->fontSize;
    }
    if (color != nullptr)
    {
        *color = element->textColor;
    }
    if (bold != nullptr)
    {
        *bold = element->fontBold;
    }
    if (italic != nullptr)
    {
        *italic = element->fontItalic;
    }
    if (underline != nullptr)
    {
        *underline = element->fontUnderline;
    }
    return true;
}

int Renderer_FreeformGetAlign(int index)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return 1;
    }
    const int align = slide->elements[index].align;
    if (align < 0 || align > 2)
    {
        return 1;
    }
    return align;
}

void Renderer_FreeformSetAlign(int index, int align)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return;
    }
    if (align < 0)
    {
        align = 0;
    }
    if (align > 2)
    {
        align = 2;
    }
    slide->elements[index].align = align;
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformSetFontName(int index, const char* name)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT || name == nullptr)
    {
        return;
    }
    lstrcpynA(slide->elements[index].fontName, name, sizeof(slide->elements[index].fontName));
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformSetFontSize(int index, int size)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return;
    }
    if (size < 8)
    {
        size = 8;
    }
    if (size > 400)
    {
        size = 400;
    }
    slide->elements[index].fontSize = size;
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformSetTextColor(int index, COLORREF color)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return;
    }
    slide->elements[index].textColor = color;
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformSetFontStyle(int index, bool bold, bool italic, bool underline)
{
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount || slide->elements[index].kind != ELEMENT_TEXT)
    {
        return;
    }
    slide->elements[index].fontBold = bold;
    slide->elements[index].fontItalic = italic;
    slide->elements[index].fontUnderline = underline;
    RebuildSlide(EditSlideIndex());
}

void Renderer_FreeformBlitLive(HDC hdc, int x, int y, int width, int height)
{
    if (!App_Ready() || hdc == nullptr || width <= 0 || height <= 0)
    {
        return;
    }
    FillCheckerboard(hdc, x, y, width, height);
    ComposeSlideElements(hdc, x, y, width, height, EditSlidePtr(), -1, true);
}

void Renderer_FreeformRebuild()
{
    RebuildSlide(EditSlideIndex());
}

static void DropEditorCache()
{
    if (g_editorCache != nullptr)
    {
        DeleteObject(g_editorCache);
        g_editorCache = nullptr;
    }
    g_editorCacheW = 0;
    g_editorCacheH = 0;
    g_editorDragging = false;
}

void Renderer_FreeformBeginEditorDrag(int hideIndex, int previewW, int previewH)
{
    if (!App_Ready() || previewW <= 0 || previewH <= 0)
    {
        return;
    }

    DropEditorCache();

    HDC screen = GetDC(nullptr);
    g_editorCache = CreateFrameBitmap(screen, previewW, previewH);
    if (g_editorCache == nullptr)
    {
        ReleaseDC(nullptr, screen);
        return;
    }

    HDC memory = CreateCompatibleDC(screen);
    HBITMAP old = (HBITMAP)SelectObject(memory, g_editorCache);
    FillCheckerboard(memory, 0, 0, previewW, previewH);
    ComposeSlideElements(memory, 0, 0, previewW, previewH, EditSlidePtr(), hideIndex, true);
    SelectObject(memory, old);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    g_editorCacheW = previewW;
    g_editorCacheH = previewH;
    g_editorDragging = true;
}

void Renderer_FreeformEndEditorDrag()
{
    DropEditorCache();
}

bool Renderer_CurrentSlideHasVideo()
{
    if (!App_Ready())
    {
        return false;
    }

    const Slide* slide = &g_slides[g_currentSlide];
    for (int i = 0; i < slide->elementCount; ++i)
    {
        if (slide->elements[i].kind == ELEMENT_VIDEO &&
            slide->elements[i].videoPath[0] != L'\0' &&
            !IsBedElement(slide, &slide->elements[i]))
        {
            return true;
        }
    }
    return false;
}

int Renderer_GetVideoLayers(VideoLayer* out, int max)
{
    if (!App_Ready() || out == nullptr || max <= 0)
    {
        return 0;
    }

    const Slide* slide = &g_slides[g_currentSlide];
    int srcW = 0;
    int srcH = 0;
    GetNativeSize(slide, &srcW, &srcH);

    int count = 0;
    for (int i = 0; i < slide->elementCount && count < max; ++i)
    {
        const SlideElement* element = &slide->elements[i];
        if (element->kind != ELEMENT_VIDEO || element->videoPath[0] == L'\0' || IsBedElement(slide, element))
        {
            continue;
        }

        lstrcpynW(out[count].path, element->videoPath, MAX_PATH);
        wchar_t resolved[MAX_PATH] = {};
        if (Document_ResolveAsset(element->videoPath, resolved, MAX_PATH))
        {
            lstrcpynW(out[count].path, resolved, MAX_PATH);
        }
        out[count].x = element->x;
        out[count].y = element->y;
        out[count].w = element->w;
        out[count].h = element->h;
        out[count].slideW = srcW;
        out[count].slideH = srcH;
        out[count].mode = element->displayMode;
        count += 1;
    }
    return count;
}

int Renderer_CollectVideoPaths(wchar_t (*out)[MAX_PATH], int max)
{
    if (!App_Ready() || out == nullptr || max <= 0)
    {
        return 0;
    }

    int count = 0;
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int s = 0; s < g_slideCount && count < max; ++s)
        {
            const Slide* slide = &g_slides[s];
            for (int i = 0; i < slide->elementCount && count < max; ++i)
            {
                const SlideElement* element = &slide->elements[i];
                if (element->kind != ELEMENT_VIDEO || element->videoPath[0] == L'\0' ||
                    IsBedElement(slide, element) != (pass == 0))
                {
                    continue;
                }
                wchar_t path[MAX_PATH] = {};
                if (!Document_ResolveAsset(element->videoPath, path, MAX_PATH) || path[0] == L'\0')
                {
                    lstrcpynW(path, element->videoPath, MAX_PATH);
                }
                bool seen = false;
                for (int k = 0; k < count && !seen; ++k)
                {
                    seen = lstrcmpiW(out[k], path) == 0;
                }
                if (!seen)
                {
                    lstrcpynW(out[count], path, MAX_PATH);
                    count += 1;
                }
            }
        }
    }
    return count;
}

bool Renderer_SetSlide8Video(const wchar_t* path)
{
    if (!App_Ready() || path == nullptr || path[0] == L'\0')
    {
        return false;
    }

    SlideElement* element = FindElement(&g_slides[SLIDE_VIDEO], ELEMENT_VIDEO);
    if (element == nullptr)
    {
        return false;
    }

    lstrcpynW(element->videoPath, path, MAX_PATH);
    lstrcpyA(element->text, "Video");
    AttachVideoPoster(element, path);
    RebuildSlide(SLIDE_VIDEO);
    return true;
}

void Renderer_SetSlide8ImageMode(ImageDisplayMode mode)
{
    if (!App_Ready())
    {
        return;
    }

    SlideElement* element = FindElement(&g_slides[SLIDE_VIDEO], ELEMENT_VIDEO);
    if (element == nullptr)
    {
        return;
    }

    element->displayMode = mode;
    RebuildSlide(SLIDE_VIDEO);
}

int Renderer_FreeformAddVideo(int x, int y, int w, int h, const wchar_t* path)
{
    if (!App_Ready() || path == nullptr || path[0] == L'\0')
    {
        return -1;
    }

    NormalizeFrame(&x, &y, &w, &h);
    Slide* slide = EditSlidePtr();
    SlideElement* element = AddElement(slide, ELEMENT_VIDEO, x, y, w, h);
    if (element == nullptr)
    {
        return -1;
    }

    lstrcpynW(element->videoPath, path, MAX_PATH);
    element->displayMode = IMAGE_FIT;
    lstrcpyA(element->text, "Video");
    AttachVideoPoster(element, path);
    RebuildSlide(EditSlideIndex());
    return slide->elementCount - 1;
}

bool Renderer_AddFullscreenMediaSlide(const wchar_t* storedPath, bool video, bool asBackground)
{
    if (!App_Ready() || storedPath == nullptr || storedPath[0] == L'\0')
    {
        return false;
    }

    const int previous = g_currentSlide;
    if (!Renderer_AddBlankSlide())
    {
        return false;
    }

    const int index = g_currentSlide;
    g_editSlide = index;
    g_slides[index].role = asBackground ? SLIDE_ROLE_BACKGROUND : SLIDE_ROLE_SLIDE;
    g_slides[index].transitionOverride = false;

    int slideW = DOCUMENT_WIDTH;
    int slideH = DOCUMENT_HEIGHT;
    GetNativeSize(&g_slides[index], &slideW, &slideH);

    int element = -1;
    if (video)
    {
        element = Renderer_FreeformAddVideo(0, 0, slideW, slideH, storedPath);
    }
    else
    {
        element = Renderer_FreeformAddImage(0, 0, slideW, slideH, storedPath);
    }
    if (element < 0)
    {
        return false;
    }

    ImageDisplayMode mode = IMAGE_STRETCH;
    if (video)
    {
        const int fit = Store_GetInt("playback.video_fit", IMAGE_STRETCH);
        if (fit == IMAGE_FIT || fit == IMAGE_FILL)
        {
            mode = (ImageDisplayMode)fit;
        }
    }
    g_slides[index].elements[element].displayMode = mode;
    g_slides[index].elements[element].x = 0;
    g_slides[index].elements[element].y = 0;
    g_slides[index].elements[element].w = slideW;
    g_slides[index].elements[element].h = slideH;
    g_slides[index].elements[element].locked = asBackground;
    RebuildSlide(index);
    if (asBackground)
    {
        // Keep prior selection / armed BG. Dropping a BG card onto the deck
        // must not auto-arm or select it — the user picks it when they want it live.
        if (previous >= 0 && previous < g_slideCount && previous != index)
        {
            g_currentSlide = previous;
        }
    }
    return true;
}

bool Renderer_IsBackgroundSlide(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }
    return g_slides[index].role == SLIDE_ROLE_BACKGROUND;
}

static int ClampSlideTransition(int transition)
{
    switch (transition)
    {
        case SLIDE_TRANSITION_CROSSFADE:
        case SLIDE_TRANSITION_FADE_BLACK:
        case SLIDE_TRANSITION_PUSH:
        case SLIDE_TRANSITION_WIPE:
        case SLIDE_TRANSITION_COVER:
            return transition;
        default:
            return SLIDE_TRANSITION_NONE;
    }
}

static int ClampSlideTransitionDir(int direction)
{
    if (direction < SLIDE_TRANSITION_DIR_LEFT || direction > SLIDE_TRANSITION_DIR_DOWN)
    {
        return SLIDE_TRANSITION_DIR_LEFT;
    }
    return direction;
}

bool Renderer_TransitionNeedsDir(int transition)
{
    const int kind = ClampSlideTransition(transition);
    return kind == SLIDE_TRANSITION_PUSH ||
        kind == SLIDE_TRANSITION_WIPE ||
        kind == SLIDE_TRANSITION_COVER;
}

const char* Renderer_TransitionName(int transition)
{
    switch (ClampSlideTransition(transition))
    {
        case SLIDE_TRANSITION_CROSSFADE: return "crossfade";
        case SLIDE_TRANSITION_FADE_BLACK: return "fadeblack";
        case SLIDE_TRANSITION_PUSH: return "push";
        case SLIDE_TRANSITION_WIPE: return "wipe";
        case SLIDE_TRANSITION_COVER: return "cover";
        default: return "none";
    }
}

const char* Renderer_TransitionDirName(int direction)
{
    switch (ClampSlideTransitionDir(direction))
    {
        case SLIDE_TRANSITION_DIR_RIGHT: return "right";
        case SLIDE_TRANSITION_DIR_UP: return "up";
        case SLIDE_TRANSITION_DIR_DOWN: return "down";
        default: return "left";
    }
}

static int ClampTransitionMs(int milliseconds)
{
    if (milliseconds < 100)
    {
        return 100;
    }
    if (milliseconds > 5000)
    {
        return 5000;
    }
    return ((milliseconds + 50) / 100) * 100;
}

void Renderer_LoadTransitionDefaults()
{
    g_defaultSlideTransition = ClampSlideTransition(Store_GetInt("transition.slide_kind", SLIDE_TRANSITION_CROSSFADE));
    g_defaultSlideTransitionDir = ClampSlideTransitionDir(Store_GetInt("transition.slide_dir", SLIDE_TRANSITION_DIR_LEFT));
    g_defaultSlideTransitionMs = ClampTransitionMs(Store_GetInt("transition.slide_ms", 300));
    g_defaultBgTransition = ClampSlideTransition(Store_GetInt("transition.bg_kind", SLIDE_TRANSITION_CROSSFADE));
    g_defaultBgTransitionDir = ClampSlideTransitionDir(Store_GetInt("transition.bg_dir", SLIDE_TRANSITION_DIR_LEFT));
    g_defaultBgTransitionMs = ClampTransitionMs(Store_GetInt("transition.bg_ms", 300));
}

void Renderer_NormalizeInheritedTransitions()
{
    for (int i = 0; i < g_slideCount; ++i)
    {
        if (!g_slides[i].transitionOverride)
        {
            continue;
        }
        const bool asBg = g_slides[i].role == SLIDE_ROLE_BACKGROUND;
        const int defKind = ClampSlideTransition(asBg ? g_defaultBgTransition : g_defaultSlideTransition);
        const int defDir = ClampSlideTransitionDir(asBg ? g_defaultBgTransitionDir : g_defaultSlideTransitionDir);
        const int defMs = ClampTransitionMs(asBg ? g_defaultBgTransitionMs : g_defaultSlideTransitionMs);
        const int kind = ClampSlideTransition(g_slides[i].transition);
        const int dir = ClampSlideTransitionDir(g_slides[i].transitionDir);
        const int ms = g_slides[i].transitionMs < 100 ? 300 : ClampTransitionMs(g_slides[i].transitionMs);
        // Keep only real per-slide choices. Cut/None and values that just echo the
        // role default were baked by older builds and should follow the renderer global.
        if (kind == SLIDE_TRANSITION_NONE ||
            (kind == defKind && dir == defDir && ms == defMs))
        {
            g_slides[i].transitionOverride = false;
        }
    }
}

int Renderer_DefaultSlideTransition()
{
    return ClampSlideTransition(g_defaultSlideTransition);
}

int Renderer_DefaultSlideTransitionDir()
{
    return ClampSlideTransitionDir(g_defaultSlideTransitionDir);
}

int Renderer_DefaultSlideTransitionMs()
{
    return ClampTransitionMs(g_defaultSlideTransitionMs);
}

int Renderer_DefaultBgTransition()
{
    return ClampSlideTransition(g_defaultBgTransition);
}

int Renderer_DefaultBgTransitionDir()
{
    return ClampSlideTransitionDir(g_defaultBgTransitionDir);
}

int Renderer_DefaultBgTransitionMs()
{
    return ClampTransitionMs(g_defaultBgTransitionMs);
}

void Renderer_SetDefaultSlideTransition(int transition, int direction, int milliseconds)
{
    g_defaultSlideTransition = ClampSlideTransition(transition);
    g_defaultSlideTransitionDir = ClampSlideTransitionDir(direction);
    g_defaultSlideTransitionMs = ClampTransitionMs(milliseconds);
    Store_SetInt("transition.slide_kind", g_defaultSlideTransition);
    Store_SetInt("transition.slide_dir", g_defaultSlideTransitionDir);
    Store_SetInt("transition.slide_ms", g_defaultSlideTransitionMs);
}

void Renderer_SetDefaultBgTransition(int transition, int direction, int milliseconds)
{
    g_defaultBgTransition = ClampSlideTransition(transition);
    g_defaultBgTransitionDir = ClampSlideTransitionDir(direction);
    g_defaultBgTransitionMs = ClampTransitionMs(milliseconds);
    Store_SetInt("transition.bg_kind", g_defaultBgTransition);
    Store_SetInt("transition.bg_dir", g_defaultBgTransitionDir);
    Store_SetInt("transition.bg_ms", g_defaultBgTransitionMs);
}

int Renderer_GetSlideTransition(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return SLIDE_TRANSITION_NONE;
    }
    return ClampSlideTransition(g_slides[index].transition);
}

static void BakeEffectiveTransitionIfNeeded(int index)
{
    if (index < 0 || index >= g_slideCount || g_slides[index].transitionOverride)
    {
        return;
    }
    g_slides[index].transition = Renderer_EffectiveSlideTransition(index);
    g_slides[index].transitionDir = Renderer_EffectiveSlideTransitionDir(index);
    g_slides[index].transitionMs = Renderer_EffectiveSlideTransitionMs(index);
    g_slides[index].transitionOverride = true;
}

bool Renderer_SetSlideTransition(int index, int transition)
{
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }
    BakeEffectiveTransitionIfNeeded(index);
    g_slides[index].transition = ClampSlideTransition(transition);
    g_slides[index].transitionOverride = true;
    return true;
}

int Renderer_GetSlideTransitionDir(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return SLIDE_TRANSITION_DIR_LEFT;
    }
    return ClampSlideTransitionDir(g_slides[index].transitionDir);
}

bool Renderer_SetSlideTransitionDir(int index, int direction)
{
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }
    BakeEffectiveTransitionIfNeeded(index);
    g_slides[index].transitionDir = ClampSlideTransitionDir(direction);
    g_slides[index].transitionOverride = true;
    return true;
}

int Renderer_GetSlideTransitionMs(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return 300;
    }
    if (g_slides[index].transitionMs < 100)
    {
        return 300;
    }
    return ClampTransitionMs(g_slides[index].transitionMs);
}

bool Renderer_SetSlideTransitionMs(int index, int milliseconds)
{
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }
    BakeEffectiveTransitionIfNeeded(index);
    g_slides[index].transitionMs = ClampTransitionMs(milliseconds);
    g_slides[index].transitionOverride = true;
    return true;
}

bool Renderer_SlideUsesTransitionOverride(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }
    return g_slides[index].transitionOverride;
}

bool Renderer_SetSlideTransitionOverride(int index, bool on)
{
    if (index < 0 || index >= g_slideCount)
    {
        return false;
    }
    g_slides[index].transitionOverride = on;
    return true;
}

void Renderer_ClearSlideTransitionOverride(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return;
    }
    g_slides[index].transitionOverride = false;
}

int Renderer_EffectiveSlideTransition(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return SLIDE_TRANSITION_NONE;
    }
    if (g_slides[index].transitionOverride)
    {
        return ClampSlideTransition(g_slides[index].transition);
    }
    if (g_slides[index].role == SLIDE_ROLE_BACKGROUND)
    {
        return ClampSlideTransition(g_defaultBgTransition);
    }
    return ClampSlideTransition(g_defaultSlideTransition);
}

int Renderer_EffectiveSlideTransitionDir(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return SLIDE_TRANSITION_DIR_LEFT;
    }
    if (g_slides[index].transitionOverride)
    {
        return ClampSlideTransitionDir(g_slides[index].transitionDir);
    }
    if (g_slides[index].role == SLIDE_ROLE_BACKGROUND)
    {
        return ClampSlideTransitionDir(g_defaultBgTransitionDir);
    }
    return ClampSlideTransitionDir(g_defaultSlideTransitionDir);
}

int Renderer_EffectiveSlideTransitionMs(int index)
{
    if (index < 0 || index >= g_slideCount)
    {
        return 300;
    }
    if (g_slides[index].transitionOverride)
    {
        if (g_slides[index].transitionMs < 100)
        {
            return 300;
        }
        return ClampTransitionMs(g_slides[index].transitionMs);
    }
    if (g_slides[index].role == SLIDE_ROLE_BACKGROUND)
    {
        return ClampTransitionMs(g_defaultBgTransitionMs);
    }
    return ClampTransitionMs(g_defaultSlideTransitionMs);
}

void Renderer_ArmBackground(int index)
{
    if (index >= 0 && index < g_slideCount && g_slides[index].role == SLIDE_ROLE_BACKGROUND)
    {
        g_backgroundSlide = index;
        return;
    }
    g_backgroundSlide = -1;
}

int Renderer_ArmedBackground()
{
    if (g_backgroundSlide < 0 || g_backgroundSlide >= g_slideCount)
    {
        return -1;
    }
    return g_backgroundSlide;
}

bool Renderer_BackgroundVideoPath(wchar_t* out, int outChars)
{
    if (out == nullptr || outChars <= 0)
    {
        return false;
    }
    out[0] = L'\0';
    const int index = Renderer_ArmedBackground();
    if (index < 0)
    {
        return false;
    }

    const Slide* slide = &g_slides[index];
    for (int i = 0; i < slide->elementCount; ++i)
    {
        if (slide->elements[i].kind != ELEMENT_VIDEO ||
            slide->elements[i].videoPath[0] == L'\0' ||
            !IsBedElement(slide, &slide->elements[i]))
        {
            continue;
        }
        if (Document_ResolveAsset(slide->elements[i].videoPath, out, outChars) && out[0] != L'\0')
        {
            return true;
        }
        lstrcpynW(out, slide->elements[i].videoPath, outChars);
        return out[0] != L'\0';
    }
    return false;
}

static HBITMAP RenderBed(int index, int* width, int* height, bool videoPoster)
{
    if (!App_Ready() || index < 0 || index >= g_slideCount || g_slides[index].role != SLIDE_ROLE_BACKGROUND)
    {
        return nullptr;
    }

    int frameW = 0;
    int frameH = 0;
    GetNativeSize(&g_slides[index], &frameW, &frameH);
    if (frameW < 1 || frameH < 1)
    {
        return nullptr;
    }

    HDC screen = GetDC(nullptr);
    HBITMAP bed = CreateFrameBitmap(screen, frameW, frameH);
    if (bed != nullptr)
    {
        HDC memory = CreateCompatibleDC(screen);
        HBITMAP old = (HBITMAP)SelectObject(memory, bed);
        if (videoPoster)
        {
            const Slide* slide = &g_slides[index];
            RECT background = { 0, 0, frameW, frameH };
            HBRUSH backgroundBrush = CreateSolidBrush(SLIDE_KEY);
            FillRect(memory, &background, backgroundBrush);
            DeleteObject(backgroundBrush);
            SetBkMode(memory, TRANSPARENT);
            for (int i = 0; i < slide->elementCount; ++i)
            {
                if (IsBedElement(slide, &slide->elements[i]))
                {
                    ComposeOne(memory, 0, 0, frameW, frameH, slide, &slide->elements[i], false, true);
                }
            }
        }
        else
        {
            ComposeSlide(memory, 0, 0, frameW, frameH, &g_slides[index], true);
        }
        SelectObject(memory, old);
        DeleteDC(memory);
    }
    ReleaseDC(nullptr, screen);
    if (bed == nullptr)
    {
        return nullptr;
    }
    SealFrameAlpha(bed);

    if (width != nullptr)
    {
        *width = frameW;
    }
    if (height != nullptr)
    {
        *height = frameH;
    }
    return bed;
}

HBITMAP Renderer_RenderBackgroundBed(int index, int* width, int* height)
{
    return RenderBed(index, width, height, false);
}

HBITMAP Renderer_RenderBackgroundStill(int index, int* width, int* height)
{
    return RenderBed(index, width, height, true);
}

bool Renderer_UseAsSlide(int index)
{
    if (!App_Ready() || index < 0 || index >= g_slideCount)
    {
        return false;
    }
    if (g_slides[index].role != SLIDE_ROLE_BACKGROUND)
    {
        return false;
    }

    g_slides[index].role = SLIDE_ROLE_SLIDE;
    for (int i = 0; i < g_slides[index].elementCount; ++i)
    {
        g_slides[index].elements[i].locked = false;
    }
    if (g_backgroundSlide == index)
    {
        g_backgroundSlide = -1;
    }
    RebuildSlide(index);
    return true;
}

bool Renderer_UseAsBackground(int index)
{
    if (!App_Ready() || index < 0 || index >= g_slideCount)
    {
        return false;
    }
    if (g_slides[index].role == SLIDE_ROLE_BACKGROUND)
    {
        return true;
    }

    g_slides[index].role = SLIDE_ROLE_BACKGROUND;
    bool locked = false;
    for (int i = 0; i < g_slides[index].elementCount; ++i)
    {
        if (!locked &&
            (g_slides[index].elements[i].kind == ELEMENT_IMAGE ||
             g_slides[index].elements[i].kind == ELEMENT_VIDEO))
        {
            g_slides[index].elements[i].locked = true;
            locked = true;
        }
    }
    RebuildSlide(index);
    return true;
}

static void ClearElementClipboard()
{
    if (!g_hasElementClipboard)
    {
        return;
    }
    Gdiplus::Image* image = g_elementClipboard.image;
    if (image != nullptr &&
        image != g_photo &&
        image != g_slide5Background &&
        image != g_slide6Photo)
    {
        delete image;
    }
    ZeroMemory(&g_elementClipboard, sizeof(g_elementClipboard));
    g_hasElementClipboard = false;
}

static bool CloneElementImage(SlideElement* dest, const SlideElement* src)
{
    dest->image = nullptr;
    if (src->image == nullptr)
    {
        return true;
    }
    Gdiplus::Image* clone = src->image->Clone();
    if (clone == nullptr || clone->GetLastStatus() != Gdiplus::Ok)
    {
        delete clone;
        return false;
    }
    dest->image = clone;
    return true;
}

bool Renderer_FreeformCopy(int index)
{
    if (!App_Ready())
    {
        return false;
    }
    Slide* slide = EditSlidePtr();
    if (index < 0 || index >= slide->elementCount)
    {
        return false;
    }

    ClearElementClipboard();
    g_elementClipboard = slide->elements[index];
    g_elementClipboard.locked = false;
    g_elementClipboard.image = nullptr;
    if (!CloneElementImage(&g_elementClipboard, &slide->elements[index]))
    {
        ZeroMemory(&g_elementClipboard, sizeof(g_elementClipboard));
        return false;
    }
    g_hasElementClipboard = true;
    return true;
}

bool Renderer_FreeformHasClipboard()
{
    return g_hasElementClipboard;
}

int Renderer_FreeformPaste()
{
    if (!App_Ready() || !g_hasElementClipboard)
    {
        return -1;
    }

    Slide* slide = EditSlidePtr();
    SlideElement* element = AddElement(
        slide,
        g_elementClipboard.kind,
        g_elementClipboard.x,
        g_elementClipboard.y,
        g_elementClipboard.w,
        g_elementClipboard.h
    );
    if (element == nullptr)
    {
        return -1;
    }

    const int newIndex = slide->elementCount - 1;
    *element = g_elementClipboard;
    element->locked = false;
    element->image = nullptr;
    CloneElementImage(element, &g_elementClipboard);
    RebuildSlide(EditSlideIndex());
    return newIndex;
}

HBITMAP Renderer_ComposeLiveOverlay(int bottomIndex, int topIndex, bool topOn, int* width, int* height)
{
    int overlayW = DOCUMENT_WIDTH;
    int overlayH = DOCUMENT_HEIGHT;
    if (bottomIndex >= 0 && bottomIndex < g_slideCount)
    {
        GetNativeSize(&g_slides[bottomIndex], &overlayW, &overlayH);
    }
    else if (topOn && topIndex >= 0 && topIndex < g_slideCount)
    {
        GetNativeSize(&g_slides[topIndex], &overlayW, &overlayH);
    }
    if (overlayW < 1 || overlayH < 1)
    {
        return nullptr;
    }

    if (g_liveOverlay == nullptr || g_liveOverlayW != overlayW || g_liveOverlayH != overlayH)
    {
        if (g_liveOverlay != nullptr)
        {
            DeleteObject(g_liveOverlay);
            g_liveOverlay = nullptr;
        }
        HDC screen = GetDC(nullptr);
        g_liveOverlay = CreateFrameBitmap(screen, overlayW, overlayH);
        ReleaseDC(nullptr, screen);
        g_liveOverlayW = overlayW;
        g_liveOverlayH = overlayH;
    }
    if (g_liveOverlay == nullptr)
    {
        return nullptr;
    }

    HDC screen = GetDC(nullptr);
    HDC dest = CreateCompatibleDC(screen);
    HBITMAP oldDest = (HBITMAP)SelectObject(dest, g_liveOverlay);
    DIBSECTION section = {};
    if (GetObject(g_liveOverlay, sizeof(section), &section) != 0 && section.dsBm.bmBits != nullptr)
    {
        const int bytes = section.dsBm.bmWidthBytes * (section.dsBm.bmHeight < 0 ? -section.dsBm.bmHeight : section.dsBm.bmHeight);
        ZeroMemory(section.dsBm.bmBits, bytes);
    }

    HDC source = CreateCompatibleDC(screen);
    if (bottomIndex >= 0 && bottomIndex < g_slideCount && g_frames[bottomIndex] != nullptr)
    {
        int srcW = overlayW;
        int srcH = overlayH;
        GetNativeSize(&g_slides[bottomIndex], &srcW, &srcH);
        HBITMAP oldSrc = (HBITMAP)SelectObject(source, g_frames[bottomIndex]);
        BlitAlphaBitmap(dest, 0, 0, overlayW, overlayH, source, 0, 0, srcW, srcH);
        SelectObject(source, oldSrc);
    }
    if (topOn && topIndex >= 0 && topIndex < g_slideCount && topIndex != bottomIndex && g_frames[topIndex] != nullptr)
    {
        int srcW = overlayW;
        int srcH = overlayH;
        GetNativeSize(&g_slides[topIndex], &srcW, &srcH);
        HBITMAP oldSrc = (HBITMAP)SelectObject(source, g_frames[topIndex]);
        BlitAlphaBitmap(dest, 0, 0, overlayW, overlayH, source, 0, 0, srcW, srcH);
        SelectObject(source, oldSrc);
    }
    SelectObject(dest, oldDest);
    DeleteDC(source);
    DeleteDC(dest);
    ReleaseDC(nullptr, screen);

    if (width != nullptr)
    {
        *width = overlayW;
    }
    if (height != nullptr)
    {
        *height = overlayH;
    }
    return g_liveOverlay;
}

void Renderer_FreeformBlitEditor(HDC hdc, int x, int y, int width, int height, int liveIndex)
{
    if (!App_Ready() || hdc == nullptr || width <= 0 || height <= 0)
    {
        return;
    }

    if (!g_editorDragging || g_editorCache == nullptr)
    {
        Renderer_BlitSlideAt(hdc, EditSlideIndex(), x, y, width, height);
        return;
    }

    HDC memory = CreateCompatibleDC(hdc);
    HBITMAP old = (HBITMAP)SelectObject(memory, g_editorCache);
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(
        hdc, x, y, width, height,
        memory, 0, 0, g_editorCacheW, g_editorCacheH,
        SRCCOPY
    );
    SelectObject(memory, old);
    DeleteDC(memory);

    Slide* slide = EditSlidePtr();
    if (liveIndex >= 0 && liveIndex < slide->elementCount)
    {
        SetBkMode(hdc, TRANSPARENT);
        ComposeOne(hdc, x, y, width, height, slide, &slide->elements[liveIndex], true, true);
    }
}
