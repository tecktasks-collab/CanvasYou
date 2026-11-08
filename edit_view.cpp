#include "edit_view.h"
#include "renderer.h"
#include "document.h"
#include "app.h"
#include "templates.h"
#include "history.h"

#include <windowsx.h>
#include <commdlg.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define EDIT_RAIL_W 200
#define EDIT_TOOL_W 56
#define EDIT_SIDE_W 320
#define EDIT_STATUS_H 30
#define EDIT_GAP 8
#define EDIT_MARGIN 8
#define EDIT_PANEL_RADIUS 10.0f
#define EDIT_CHIN 18
#define EDIT_RAIL_PAD 10
#define EDIT_CANVAS_PAD 24
#define EDIT_TYPE_BAR_H 48
#define EDIT_SIDE_TAB_H 34
#define EDIT_SIDE_SHAPE 0
#define EDIT_SIDE_TEXT 1
#define EDIT_SIDE_MOVE 2
#define EDIT_GRIP 10
#define EDIT_GRIP_HIT 12
#define EDIT_BORDER 8
#define EDIT_ROTATE_GAP 12
#define EDIT_HOVER_PAD 8
#define EDIT_COLOR_BASE RGB(0x26, 0x26, 0x26)
#define EDIT_COLOR_SURFACE RGB(0x2E, 0x2E, 0x2E)
#define EDIT_COLOR_RAISED RGB(0x38, 0x38, 0x38)
#define EDIT_COLOR_RAISED_HI RGB(0x42, 0x42, 0x42)
#define EDIT_COLOR_SUNKEN RGB(0x1E, 0x1E, 0x1E)
#define EDIT_COLOR_LINE RGB(0x3A, 0x3A, 0x3A)
#define EDIT_COLOR_EDGE RGB(0x45, 0x45, 0x45)
#define EDIT_COLOR_FIELD_EDGE RGB(0x33, 0x33, 0x33)
#define EDIT_COLOR_INK RGB(0xD0, 0xD0, 0xD0)
#define EDIT_COLOR_INK_DIM RGB(0x8C, 0x8C, 0x8C)
#define EDIT_COLOR_INK_FAINT RGB(0x5E, 0x5E, 0x5E)
#define EDIT_COLOR_INK_BRIGHT RGB(0xF2, 0xF2, 0xF2)
#define EDIT_COLOR_ACCENT RGB(0xE8, 0xE8, 0xE8)
// Older names still used by the drop lists and the colour picker.
#define EDIT_COLOR_FRAME EDIT_COLOR_RAISED
#define EDIT_COLOR_WELL EDIT_COLOR_EDGE
#define EDIT_COLOR_DARK EDIT_COLOR_SUNKEN
#define EDIT_COLOR_SELECT EDIT_COLOR_ACCENT
#define EDIT_TOOL_COUNT 6
#define EDIT_TOOL_CELL 42
#define EDIT_TOOL_GAP 6
#define EDIT_TOOL_RULE 5
#define EDIT_MAX_FONTS 400
#define EDIT_CARET_TIMER 2
#define EDIT_TEXT_MAX 1024
#define EDIT_WRAP_MAX 128
#define IDM_OBJ_CUT 860
#define IDM_OBJ_COPY 861
#define IDM_OBJ_PASTE 862
#define IDM_OBJ_DELETE 863
#define IDM_TPL_RENAME 870
#define TYPE_PAD 10
#define TYPE_GAP 8
#define TYPE_FONT_W 180
#define TYPE_SIZE_W 70
#define TYPE_CTRL_H 30
#define TYPE_STYLE 30
#define TYPE_RULE 5
#define TYPE_COLOR_W 96
#define TYPE_WELL_COUNT 7
#define DROP_ITEM_H 22
#define DROP_FONT_ROWS 12
#define PICK_PAD 8
#define PICK_CELL 22
#define PICK_GAP 4
#define PICK_COLS 8
#define PICK_INNER_W (PICK_COLS * PICK_CELL + (PICK_COLS - 1) * PICK_GAP)
#define PICK_HUE_W 20
#define PICK_SV_W (PICK_INNER_W - PICK_GAP * 2 - PICK_HUE_W)
#define PICK_SV_H 120
#define PICK_SV_Y (PICK_PAD + 2 * PICK_CELL + PICK_GAP + PICK_PAD)
#define PICK_ROW_Y (PICK_SV_Y + PICK_SV_H + PICK_PAD)
#define PICK_PREVIEW_W 44
#define PICK_RECENT_Y (PICK_ROW_Y + PICK_CELL + PICK_PAD + 1)
#define PICK_W (PICK_PAD * 2 + PICK_INNER_W)
#define PICK_H (PICK_RECENT_Y + PICK_CELL + PICK_PAD)
#define PICK_RECENT_MAX PICK_COLS

#define ID_EDIT_TEXT 2
#define ID_EDIT_PHOTO 3
#define ID_EDIT_VIDEO 4
#define ID_EDIT_RECT 5
#define ID_EDIT_ELLIPSE 6

enum EditTool
{
    EDIT_TOOL_SELECT = 0,
    EDIT_TOOL_TEXT,
    EDIT_TOOL_PHOTO,
    EDIT_TOOL_VIDEO,
    EDIT_TOOL_RECT,
    EDIT_TOOL_ELLIPSE
};

enum EditDrag
{
    EDIT_DRAG_NONE = 0,
    EDIT_DRAG_MOVE,
    EDIT_DRAG_RESIZE,
    EDIT_DRAG_CREATE,
    EDIT_DRAG_TEXTSEL,
    EDIT_DRAG_ROTATE
};

enum ResizeHandle
{
    HANDLE_NONE = 0,
    HANDLE_L,
    HANDLE_R,
    HANDLE_T,
    HANDLE_B,
    HANDLE_TL,
    HANDLE_TR,
    HANDLE_BL,
    HANDLE_BR,
    HANDLE_ROTATE
};

enum DropKind
{
    DROP_NONE = 0,
    DROP_FONT,
    DROP_SIZE,
    DROP_COLOR,
    DROP_ALIGN
};

static const char* kAlignNames[3] = { "Left", "Center", "Right" };

static const EditTool kToolOrder[EDIT_TOOL_COUNT] = {
    EDIT_TOOL_SELECT,
    EDIT_TOOL_TEXT,
    EDIT_TOOL_PHOTO,
    EDIT_TOOL_VIDEO,
    EDIT_TOOL_RECT,
    EDIT_TOOL_ELLIPSE
};
// A divider line is drawn after these positions in the tools column.
static const bool kToolRuleAfter[EDIT_TOOL_COUNT] = { true, false, false, true, false, false };

static const int kFontSizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 64, 72, 96, 128 };
static const int kFontSizeCount = sizeof(kFontSizes) / sizeof(kFontSizes[0]);
static const COLORREF kSwatches[] = {
    RGB(255, 255, 255), RGB(0, 0, 0), RGB(80, 80, 80), RGB(160, 160, 160),
    RGB(200, 60, 60), RGB(220, 120, 40), RGB(220, 180, 40), RGB(80, 160, 90),
    RGB(40, 90, 180), RGB(90, 70, 160), RGB(180, 70, 140), RGB(0xE8, 0xD4, 0xEE),
    RGB(0x84, 0x65, 0x8A), RGB(30, 30, 30), RGB(180, 200, 230), RGB(255, 200, 40)
};
static const int kSwatchCount = sizeof(kSwatches) / sizeof(kSwatches[0]);

static HWND g_root = nullptr;
static HWND g_list = nullptr;
static HWND g_tools = nullptr;
static HWND g_stage = nullptr;
static HWND g_canvas = nullptr;
static HWND g_status = nullptr;
static HWND g_typeBar = nullptr;
static HWND g_side = nullptr;
static HWND g_drop = nullptr;
static int g_selected = -1;
static int g_sideTab = EDIT_SIDE_SHAPE;
static bool g_snapObjects = true;
static bool g_snapGrid = false;
static int g_snapGridSize = 20;
static RECT g_sideTabRc[3] = {};
static RECT g_sideActionRc[8] = {};
static int g_sideActionCount = 0;
static EditTool g_tool = EDIT_TOOL_SELECT;
static EditDrag g_drag = EDIT_DRAG_NONE;
static ResizeHandle g_resizeHandle = HANDLE_NONE;
static int g_dragStartX = 0;
static int g_dragStartY = 0;
static int g_origX = 0;
static int g_origY = 0;
static int g_origW = 0;
static int g_origH = 0;
static float g_origRotation = 0.0f;
static float g_rotateStart = 0.0f;
static int g_hover = -1;
static bool g_snapX = false;
static bool g_snapY = false;
#define EDIT_SNAP 12
static int g_nowX = 0;
static int g_nowY = 0;
static int g_listScroll = 0;
static int g_slideHeld = -1;
static int g_slideDrop = -1;
static int g_slideDownY = 0;
static bool g_slideDragging = false;
static bool g_mouseOnSlide = false;
static int g_mouseSlideX = 0;
static int g_mouseSlideY = 0;
static bool g_textOpen = false;
// A press on an already-selected text box opens the text editor on release,
// unless the mouse travels far enough to make it a move.
static bool g_textClickPending = false;
static int g_textClickX = 0;
static int g_textClickY = 0;
#define EDIT_CLICK_SLOP 4
static bool g_swallowEnterChar = false;
static int g_caret = 0;
static int g_selAnchor = 0;
static bool g_caretOn = true;
static DropKind g_dropKind = DROP_NONE;
static int g_dropScrollY = 0;
static int g_dropWheelAcc = 0;
enum PickDrag
{
    PICK_DRAG_NONE = 0,
    PICK_DRAG_SV,
    PICK_DRAG_HUE
};
static COLORREF g_pickColor = RGB(255, 255, 255);
static float g_pickH = 0.0f;
static float g_pickS = 0.0f;
static float g_pickV = 1.0f;
static PickDrag g_pickDrag = PICK_DRAG_NONE;
// Set once the square or hue strip has recolored the text, so a whole visit to
// the picker is one undo step and one recent color.
static bool g_pickChanged = false;
static bool g_hexEditing = false;
static bool g_hexFresh = false;
static char g_hexBuf[8] = {};
static COLORREF g_pickRecent[PICK_RECENT_MAX] = {};
static int g_pickRecentCount = 0;
static char g_fonts[EDIT_MAX_FONTS][64] = {};
static int g_fontCount = 0;

static void LayoutEdit();
static void LayoutFloatingCanvas();
static void CloseTextEditor(bool save);
static void OpenTextEditor();
static void RefreshEditorChrome();
static void CommitStyleChange();
static void HideDrop();
static void PickRemember(COLORREF color);
static int PrefixWidth(HDC hdc, const char* text, int start, int chars);
static int AlignPad(HDC hdc, const char* text, int start, int vis, int boxW);
static void PlaceTypeBar();
static void DeleteTextRange(int from, int count);
static bool HasTextSel();
static LRESULT CALLBACK EditTypeBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK EditDropProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static int g_wrapStart[EDIT_WRAP_MAX] = {};
static int g_wrapLen[EDIT_WRAP_MAX] = {};
static bool g_wrapNl[EDIT_WRAP_MAX] = {};
static int g_wrapCount = 0;

static void CommitDocument()
{
    if (Template_IsActive())
    {
        Template_SaveCurrent();
        return;
    }
    Document_Save();
}

static void BeginMutatingDrag(EditDrag drag)
{
    History_Remember();
    g_drag = drag;
    g_snapX = false;
    g_snapY = false;
    g_textClickPending = false;
}

static int EditSlideIndex()
{
    return Renderer_EditSlide();
}

static bool PickPicture(HWND owner, wchar_t* path, int pathChars)
{
    path[0] = L'\0';
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = pathChars;
    dialog.lpstrFilter = L"Pictures (*.jpg;*.png;*.bmp;*.gif)\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0All files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) == TRUE;
}

static bool PickVideo(HWND owner, wchar_t* path, int pathChars)
{
    path[0] = L'\0';
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = path;
    dialog.nMaxFile = pathChars;
    dialog.lpstrFilter = L"Video (*.mp4;*.wmv;*.mov;*.avi;*.mkv)\0*.mp4;*.wmv;*.mov;*.avi;*.mkv\0All files\0*.*\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) == TRUE;
}

static int ThumbHeight(int railW)
{
    int inner = railW - 20;
    if (inner < 40)
    {
        inner = 40;
    }
    int slideW = 16;
    int slideH = 9;
    Renderer_GetSlideSize(EditSlideIndex(), &slideW, &slideH);
    if (slideW < 1)
    {
        slideW = 16;
    }
    if (slideH < 1)
    {
        slideH = 9;
    }
    int thumbH = MulDiv(inner, slideH, slideW);
    if (thumbH < 24)
    {
        thumbH = 24;
    }
    return thumbH + EDIT_CHIN + 12;
}

static void GetCanvasView(HWND canvas, RECT* view)
{
    GetClientRect(canvas, view);
}

static void GetSlideSize(int* slideW, int* slideH)
{
    *slideW = 1920;
    *slideH = 1080;
    Renderer_GetSlideSize(EditSlideIndex(), slideW, slideH);
    if (*slideW < 1)
    {
        *slideW = 1920;
    }
    if (*slideH < 1)
    {
        *slideH = 1080;
    }
}

static bool ClientToSlide(HWND canvas, int clientX, int clientY, int* slideX, int* slideY)
{
    RECT view = {};
    GetCanvasView(canvas, &view);
    if (clientX < view.left || clientY < view.top ||
        clientX >= view.right || clientY >= view.bottom)
    {
        return false;
    }

    const int viewW = view.right - view.left;
    const int viewH = view.bottom - view.top;
    if (viewW < 1 || viewH < 1)
    {
        return false;
    }
    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    *slideX = MulDiv(clientX - view.left, slideW, viewW);
    *slideY = MulDiv(clientY - view.top, slideH, viewH);
    return true;
}

static void ClientToSlideClamped(HWND canvas, int clientX, int clientY, int* slideX, int* slideY)
{
    RECT view = {};
    GetCanvasView(canvas, &view);
    if (clientX < view.left)
    {
        clientX = view.left;
    }
    if (clientY < view.top)
    {
        clientY = view.top;
    }
    if (clientX >= view.right)
    {
        clientX = view.right - 1;
    }
    if (clientY >= view.bottom)
    {
        clientY = view.bottom - 1;
    }
    ClientToSlide(canvas, clientX, clientY, slideX, slideY);
}

static void SlideToClient(HWND canvas, int slideX, int slideY, int* clientX, int* clientY)
{
    RECT view = {};
    GetCanvasView(canvas, &view);
    const int viewW = view.right - view.left;
    const int viewH = view.bottom - view.top;
    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    *clientX = view.left + MulDiv(slideX, viewW, slideW);
    *clientY = view.top + MulDiv(slideY, viewH, slideH);
}

static void SyncEditorCache(HWND canvas, int hideIndex)
{
    RECT view = {};
    GetCanvasView(canvas, &view);
    Renderer_FreeformBeginEditorDrag(hideIndex, view.right - view.left, view.bottom - view.top);
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

static int ClientPadToSlide(HWND canvas, int pad)
{
    RECT view = {};
    GetCanvasView(canvas, &view);
    const int viewW = view.right - view.left;
    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    if (viewW < 1)
    {
        return pad;
    }
    int mapped = MulDiv(pad, slideW, viewW);
    if (mapped < 1)
    {
        mapped = 1;
    }
    return mapped;
}

static void SlideToClientF(HWND canvas, float slideX, float slideY, int* clientX, int* clientY)
{
    int x = 0;
    int y = 0;
    SlideToClient(canvas, static_cast<int>(slideX + 0.5f), static_cast<int>(slideY + 0.5f), &x, &y);
    *clientX = x;
    *clientY = y;
}

static bool ObjectFrame(int index, int* x, int* y, int* w, int* h, float* rotation)
{
    if (!Renderer_FreeformGet(index, nullptr, x, y, w, h))
    {
        return false;
    }
    if (rotation != nullptr)
    {
        *rotation = Renderer_FreeformGetRotation(index);
    }
    return true;
}

static void ObjectCenter(int index, float* cx, float* cy)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    Renderer_FreeformGet(index, nullptr, &x, &y, &w, &h);
    *cx = x + (w * 0.5f);
    *cy = y + (h * 0.5f);
}

static void LocalHandleSlide(HWND canvas, int index, ResizeHandle which, float* sx, float* sy)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float rotation = 0.0f;
    ObjectFrame(index, &x, &y, &w, &h, &rotation);
    float px = static_cast<float>(x);
    float py = static_cast<float>(y);
    switch (which)
    {
        case HANDLE_T:
            px = x + (w * 0.5f);
            py = static_cast<float>(y);
            break;
        case HANDLE_B:
            px = x + (w * 0.5f);
            py = static_cast<float>(y + h);
            break;
        case HANDLE_L:
            px = static_cast<float>(x);
            py = y + (h * 0.5f);
            break;
        case HANDLE_R:
            px = static_cast<float>(x + w);
            py = y + (h * 0.5f);
            break;
        case HANDLE_TL:
            px = static_cast<float>(x);
            py = static_cast<float>(y);
            break;
        case HANDLE_TR:
            px = static_cast<float>(x + w);
            py = static_cast<float>(y);
            break;
        case HANDLE_BL:
            px = static_cast<float>(x);
            py = static_cast<float>(y + h);
            break;
        case HANDLE_BR:
            px = static_cast<float>(x + w);
            py = static_cast<float>(y + h);
            break;
        case HANDLE_ROTATE:
            px = static_cast<float>(x + w + ClientPadToSlide(canvas, EDIT_ROTATE_GAP));
            py = static_cast<float>(y);
            break;
        default:
            break;
    }
    const float cx = x + (w * 0.5f);
    const float cy = y + (h * 0.5f);
    RotateAround(cx, cy, rotation, &px, &py);
    *sx = px;
    *sy = py;
}

static void HandleClient(HWND canvas, int index, ResizeHandle which, int* cx, int* cy)
{
    float sx = 0.0f;
    float sy = 0.0f;
    LocalHandleSlide(canvas, index, which, &sx, &sy);
    SlideToClientF(canvas, sx, sy, cx, cy);
}

static void GripRect(int cx, int cy, RECT* out)
{
    const int half = EDIT_GRIP / 2;
    out->left = cx - half;
    out->top = cy - half;
    out->right = out->left + EDIT_GRIP;
    out->bottom = out->top + EDIT_GRIP;
}

static void HitGripRect(int cx, int cy, RECT* out)
{
    const int half = EDIT_GRIP_HIT / 2;
    out->left = cx - half;
    out->top = cy - half;
    out->right = cx + half;
    out->bottom = cy + half;
}

static void UnrotateSlide(int index, int slideX, int slideY, int* localX, int* localY)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float rotation = 0.0f;
    ObjectFrame(index, &x, &y, &w, &h, &rotation);
    float px = static_cast<float>(slideX);
    float py = static_cast<float>(slideY);
    RotateAround(x + (w * 0.5f), y + (h * 0.5f), -rotation, &px, &py);
    *localX = static_cast<int>(px + 0.5f);
    *localY = static_cast<int>(py + 0.5f);
}

static bool HitObjectInterior(HWND canvas, int index, int clientX, int clientY)
{
    int slideX = 0;
    int slideY = 0;
    if (!ClientToSlide(canvas, clientX, clientY, &slideX, &slideY))
    {
        return false;
    }
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (!Renderer_FreeformGet(index, nullptr, &x, &y, &w, &h))
    {
        return false;
    }
    int localX = 0;
    int localY = 0;
    UnrotateSlide(index, slideX, slideY, &localX, &localY);
    return localX >= x && localY >= y && localX < x + w && localY < y + h;
}

static bool HitObjectBorder(HWND canvas, int index, int clientX, int clientY)
{
    int slideX = 0;
    int slideY = 0;
    if (!ClientToSlide(canvas, clientX, clientY, &slideX, &slideY))
    {
        return false;
    }
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (!Renderer_FreeformGet(index, nullptr, &x, &y, &w, &h))
    {
        return false;
    }
    const int pad = ClientPadToSlide(canvas, EDIT_BORDER);
    int localX = 0;
    int localY = 0;
    UnrotateSlide(index, slideX, slideY, &localX, &localY);
    const bool outer =
        localX >= x - pad && localY >= y - pad && localX < x + w + pad && localY < y + h + pad;
    const bool inner =
        localX >= x + pad && localY >= y + pad && localX < x + w - pad && localY < y + h - pad;
    return outer && !inner;
}

static ResizeHandle HitHandle(HWND canvas, int clientX, int clientY)
{
    if (g_selected < 0)
    {
        return HANDLE_NONE;
    }

    POINT pt = { clientX, clientY };
    const ResizeHandle order[9] = {
        HANDLE_ROTATE,
        HANDLE_TL, HANDLE_TR, HANDLE_BL, HANDLE_BR,
        HANDLE_T, HANDLE_B, HANDLE_L, HANDLE_R
    };
    for (int i = 0; i < 9; ++i)
    {
        int hx = 0;
        int hy = 0;
        HandleClient(canvas, g_selected, order[i], &hx, &hy);
        RECT hit = {};
        HitGripRect(hx, hy, &hit);
        if (PtInRect(&hit, pt))
        {
            return order[i];
        }
    }
    return HANDLE_NONE;
}

static bool HitSelectionFrame(HWND canvas, int clientX, int clientY)
{
    return g_selected >= 0 && HitObjectBorder(canvas, g_selected, clientX, clientY);
}

static bool SelectedIsText()
{
    int kind = -1;
    return g_selected >= 0 && Renderer_FreeformGet(g_selected, &kind, nullptr, nullptr, nullptr, nullptr) &&
        kind == 0;
}

static void ApplyCenterSnap(int* x, int* y, int w, int h)
{
    g_snapX = false;
    g_snapY = false;
    if (x == nullptr || y == nullptr || !g_snapObjects)
    {
        return;
    }
    if ((GetKeyState(VK_MENU) & 0x8000) != 0)
    {
        return;
    }
    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    const int midX = slideW / 2;
    const int midY = slideH / 2;
    const int cx = *x + w / 2;
    const int cy = *y + h / 2;
    if (cx >= midX - EDIT_SNAP && cx <= midX + EDIT_SNAP)
    {
        *x = midX - w / 2;
        g_snapX = true;
    }
    if (cy >= midY - EDIT_SNAP && cy <= midY + EDIT_SNAP)
    {
        *y = midY - h / 2;
        g_snapY = true;
    }
}

static void DrawSnapGuides(HDC hdc, HWND canvas)
{
    if (!g_snapX && !g_snapY)
    {
        return;
    }
    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 220, 0));
    HPEN old = (HPEN)SelectObject(hdc, pen);
    if (g_snapX)
    {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        SlideToClient(canvas, slideW / 2, 0, &x0, &y0);
        SlideToClient(canvas, slideW / 2, slideH, &x1, &y1);
        MoveToEx(hdc, x0, y0, nullptr);
        LineTo(hdc, x1, y1);
    }
    if (g_snapY)
    {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        SlideToClient(canvas, 0, slideH / 2, &x0, &y0);
        SlideToClient(canvas, slideW, slideH / 2, &x1, &y1);
        MoveToEx(hdc, x0, y0, nullptr);
        LineTo(hdc, x1, y1);
    }
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void ApplyResize(int slideX, int slideY)
{
    int localNowX = 0;
    int localNowY = 0;
    int localStartX = 0;
    int localStartY = 0;
    float cx = g_origX + (g_origW * 0.5f);
    float cy = g_origY + (g_origH * 0.5f);
    float nx = static_cast<float>(slideX);
    float ny = static_cast<float>(slideY);
    float sx = static_cast<float>(g_dragStartX);
    float sy = static_cast<float>(g_dragStartY);
    RotateAround(cx, cy, -g_origRotation, &nx, &ny);
    RotateAround(cx, cy, -g_origRotation, &sx, &sy);
    localNowX = static_cast<int>(nx + 0.5f);
    localNowY = static_cast<int>(ny + 0.5f);
    localStartX = static_cast<int>(sx + 0.5f);
    localStartY = static_cast<int>(sy + 0.5f);
    const int dx = localNowX - localStartX;
    const int dy = localNowY - localStartY;
    int x = g_origX;
    int y = g_origY;
    int w = g_origW;
    int h = g_origH;
    switch (g_resizeHandle)
    {
        case HANDLE_R:
            w += dx;
            break;
        case HANDLE_L:
            x += dx;
            w -= dx;
            break;
        case HANDLE_B:
            h += dy;
            break;
        case HANDLE_T:
            y += dy;
            h -= dy;
            break;
        case HANDLE_BR:
            w += dx;
            h += dy;
            break;
        case HANDLE_BL:
            x += dx;
            w -= dx;
            h += dy;
            break;
        case HANDLE_TR:
            w += dx;
            y += dy;
            h -= dy;
            break;
        case HANDLE_TL:
            x += dx;
            w -= dx;
            y += dy;
            h -= dy;
            break;
        default:
            break;
    }
    Renderer_FreeformSetFrame(g_selected, x, y, w, h);
}

static void SetSizeCursor(ResizeHandle handle)
{
    LPCSTR id = IDC_ARROW;
    if (handle == HANDLE_L || handle == HANDLE_R)
    {
        id = IDC_SIZEWE;
    }
    else if (handle == HANDLE_T || handle == HANDLE_B)
    {
        id = IDC_SIZENS;
    }
    else if (handle == HANDLE_TL || handle == HANDLE_BR)
    {
        id = IDC_SIZENWSE;
    }
    else if (handle == HANDLE_TR || handle == HANDLE_BL)
    {
        id = IDC_SIZENESW;
    }
    else if (handle == HANDLE_ROTATE)
    {
        id = IDC_HAND;
    }
    SetCursor(LoadCursor(nullptr, id));
}

static void RefreshStatus()
{
    if (g_status != nullptr)
    {
        InvalidateRect(g_status, nullptr, FALSE);
    }
}

static void RefreshEditorChrome()
{
    if (g_canvas != nullptr)
    {
        InvalidateRect(g_canvas, nullptr, FALSE);
    }
    if (g_list != nullptr)
    {
        InvalidateRect(g_list, nullptr, FALSE);
    }
    if (g_tools != nullptr)
    {
        InvalidateRect(g_tools, nullptr, FALSE);
    }
    if (g_typeBar != nullptr)
    {
        InvalidateRect(g_typeBar, nullptr, FALSE);
    }
    if (g_side != nullptr)
    {
        InvalidateRect(g_side, nullptr, FALSE);
    }
    RefreshStatus();
    UpdateDocumentMenus();
}

static void CloseTextEditor(bool save)
{
    HideDrop();
    if (!g_textOpen)
    {
        return;
    }
    if (save && g_selected >= 0)
    {
        Renderer_FreeformRebuild();
        CommitDocument();
        RefreshSlideLists();
    }
    g_textOpen = false;
    g_caretOn = true;
    if (g_canvas != nullptr)
    {
        KillTimer(g_canvas, EDIT_CARET_TIMER);
        InvalidateRect(g_canvas, nullptr, FALSE);
    }
    if (g_typeBar != nullptr)
    {
        InvalidateRect(g_typeBar, nullptr, FALSE);
    }
}

static void OpenTextEditor()
{
    if (g_canvas == nullptr || !SelectedIsText())
    {
        return;
    }
    if (!g_textOpen)
    {
        History_Remember();
    }

    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    g_caret = lstrlenA(text);
    g_selAnchor = g_caret;
    g_textOpen = true;
    g_caretOn = true;
    SetFocus(g_canvas);
    SetTimer(g_canvas, EDIT_CARET_TIMER, 530, nullptr);
    InvalidateRect(g_canvas, nullptr, FALSE);
    if (g_typeBar != nullptr)
    {
        InvalidateRect(g_typeBar, nullptr, FALSE);
    }
}

// Selecting never starts text editing, so a selected text box can be moved,
// copied or deleted like any other object.
static void SelectObjectAt(int index)
{
    if (index >= 0 && Renderer_FreeformIsLocked(index))
    {
        index = -1;
    }
    CloseTextEditor(true);
    g_selected = index;
    RefreshEditorChrome();
}

static void RemapSelectionAfterDelete(int deleted)
{
    if (g_selected == deleted)
    {
        g_selected = -1;
    }
    else if (g_selected > deleted)
    {
        g_selected -= 1;
    }
    if (g_hover == deleted)
    {
        g_hover = -1;
    }
    else if (g_hover > deleted)
    {
        g_hover -= 1;
    }
}

static void ShowCanvasObjectMenu(HWND hwnd, int screenX, int screenY, int hit)
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    const bool hasObject = hit >= 0;
    const bool canPaste = Renderer_FreeformHasClipboard();
    AppendMenuA(menu, MF_STRING | (hasObject ? 0 : MF_GRAYED), IDM_OBJ_CUT, "Cu&t");
    AppendMenuA(menu, MF_STRING | (hasObject ? 0 : MF_GRAYED), IDM_OBJ_COPY, "&Copy");
    AppendMenuA(menu, MF_STRING | (canPaste ? 0 : MF_GRAYED), IDM_OBJ_PASTE, "&Paste");
    AppendMenuA(menu, MF_STRING | (hasObject ? 0 : MF_GRAYED), IDM_OBJ_DELETE, "&Delete");

    const int command = TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD,
        screenX,
        screenY,
        0,
        hwnd,
        nullptr
    );
    DestroyMenu(menu);
    if (command == 0)
    {
        return;
    }

    if (command == IDM_OBJ_COPY && hit >= 0)
    {
        Renderer_FreeformCopy(hit);
        return;
    }
    if (command == IDM_OBJ_CUT && hit >= 0)
    {
        History_Remember();
        if (Renderer_FreeformCopy(hit))
        {
            CloseTextEditor(false);
            RemapSelectionAfterDelete(hit);
            Renderer_FreeformDelete(hit);
            CommitDocument();
            RefreshEditorChrome();
        }
        return;
    }
    if (command == IDM_OBJ_DELETE && hit >= 0)
    {
        History_Remember();
        CloseTextEditor(false);
        RemapSelectionAfterDelete(hit);
        Renderer_FreeformDelete(hit);
        CommitDocument();
        RefreshEditorChrome();
        return;
    }
    if (command == IDM_OBJ_PASTE)
    {
        History_Remember();
        const int pasted = Renderer_FreeformPaste();
        if (pasted >= 0)
        {
            SelectObjectAt(pasted);
            CommitDocument();
        }
    }
}

#define TYPE_ALIGN_PART_W 54
#define TYPE_ALIGN_W (6 + (TYPE_ALIGN_PART_W * 3))

// Well indices: 0 font, 1 size, 2 colour, 3 alignment, 4 bold, 5 italic,
// 6 underline. On the bar they run font, size | B I U | alignment | colour.
static const int kTypeOrder[TYPE_WELL_COUNT] = { 0, 1, 4, 5, 6, 3, 2 };

static int TypeWellWidth(int which)
{
    switch (which)
    {
    case 0: return TYPE_FONT_W;
    case 1: return TYPE_SIZE_W;
    case 2: return TYPE_COLOR_W;
    case 3: return TYPE_ALIGN_W;
    default: return TYPE_STYLE;
    }
}

static bool TypeRuleBefore(int which)
{
    return which == 4 || which == 3 || which == 2;
}

static void TypeWell(int which, const RECT* client, RECT* out)
{
    int x = TYPE_PAD;
    for (int i = 0; i < TYPE_WELL_COUNT; ++i)
    {
        const int id = kTypeOrder[i];
        if (i > 0)
        {
            x += TYPE_GAP;
            if (TypeRuleBefore(id))
            {
                x += TYPE_RULE + TYPE_GAP;
            }
        }
        if (id == which)
        {
            break;
        }
        x += TypeWellWidth(id);
    }
    const int h = which == 3 ? TYPE_CTRL_H + 2 : TYPE_CTRL_H;
    const int barH = client->bottom > 0 ? client->bottom : EDIT_TYPE_BAR_H;
    out->left = x;
    out->top = (barH - 1 - h) / 2;
    out->right = x + TypeWellWidth(which);
    out->bottom = out->top + h;
}

static void TypeAlignPart(const RECT* well, int part, RECT* out)
{
    out->left = well->left + 3 + (part * TYPE_ALIGN_PART_W);
    out->right = out->left + TYPE_ALIGN_PART_W;
    out->top = well->top + 3;
    out->bottom = well->bottom - 3;
}

static int HitTypeWell(HWND hwnd, int x, int y)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    POINT pt = { x, y };
    for (int i = 0; i < TYPE_WELL_COUNT; ++i)
    {
        RECT well = {};
        TypeWell(i, &client, &well);
        if (PtInRect(&well, pt))
        {
            return i;
        }
    }
    return -1;
}

static void HideDrop()
{
    if (g_drop == nullptr)
    {
        return;
    }
    HWND drop = g_drop;
    const bool pickDragging = g_pickDrag != PICK_DRAG_NONE;
    const bool pickChanged = g_pickChanged;
    g_drop = nullptr;
    g_dropKind = DROP_NONE;
    g_dropScrollY = 0;
    g_dropWheelAcc = 0;
    g_pickDrag = PICK_DRAG_NONE;
    g_pickChanged = false;
    g_hexEditing = false;
    DestroyWindow(drop);
    if (pickChanged)
    {
        PickRemember(g_pickColor);
    }
    if (pickDragging)
    {
        CommitStyleChange();
    }
}

// The bar is docked along the top of the stage panel by LayoutEdit; it is
// dimmed unless a text box is selected.
static void PlaceTypeBar()
{
    if (g_typeBar != nullptr)
    {
        InvalidateRect(g_typeBar, nullptr, FALSE);
    }
}

static Gdiplus::Color EditGdiColor(COLORREF color, BYTE alpha = 255)
{
    return Gdiplus::Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

static void EditRoundPath(Gdiplus::GraphicsPath* path, float x, float y, float w, float h, float r)
{
    if (r * 2.0f > w)
    {
        r = w / 2.0f;
    }
    if (r * 2.0f > h)
    {
        r = h / 2.0f;
    }
    if (r <= 0.0f)
    {
        path->AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    const float d = r * 2.0f;
    path->AddArc(x, y, d, d, 180.0f, 90.0f);
    path->AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path->AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path->AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path->CloseFigure();
}

// Fills a rounded rectangle; an optional border is drawn inset so it stays
// inside the rectangle.
static void EditRound(HDC hdc, const RECT* rc, float radius, COLORREF fill,
                      COLORREF border = CLR_INVALID, float borderW = 1.0f)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    const float w = static_cast<float>(rc->right - rc->left);
    const float h = static_cast<float>(rc->bottom - rc->top);
    if (fill != CLR_INVALID)
    {
        Gdiplus::GraphicsPath path;
        EditRoundPath(&path, static_cast<float>(rc->left), static_cast<float>(rc->top), w, h, radius);
        Gdiplus::SolidBrush brush(EditGdiColor(fill));
        g.FillPath(&brush, &path);
    }
    if (border != CLR_INVALID && borderW > 0.0f)
    {
        const float half = borderW / 2.0f;
        Gdiplus::GraphicsPath path;
        EditRoundPath(&path, rc->left + half, rc->top + half, w - borderW, h - borderW, radius - half);
        Gdiplus::Pen pen(EditGdiColor(border), borderW);
        g.DrawPath(&pen, &path);
    }
}

// Paints the background colour outside a rounded corner so a square blit
// (slide thumbnail) reads as rounded.
static void EditMaskCorners(HDC hdc, const RECT* rc, float radius, COLORREF back)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    const float x = static_cast<float>(rc->left);
    const float y = static_cast<float>(rc->top);
    const float w = static_cast<float>(rc->right - rc->left);
    const float h = static_cast<float>(rc->bottom - rc->top);
    Gdiplus::GraphicsPath round;
    EditRoundPath(&round, x, y, w, h, radius);
    Gdiplus::Region region(Gdiplus::RectF(x, y, w, h));
    region.Exclude(&round);
    Gdiplus::SolidBrush brush(EditGdiColor(back));
    g.FillRegion(&brush, &region);
}

// Panel backgrounds: the window is filled with the base colour and a rounded
// surface is drawn over it. extendTop/extendBottom push the rounded shape past
// the window edge so only the opposite corners show as rounded.
static void EditPanel(HDC hdc, const RECT* client, int extendTop, int extendBottom)
{
    HBRUSH base = CreateSolidBrush(EDIT_COLOR_BASE);
    FillRect(hdc, client, base);
    DeleteObject(base);
    RECT shape = *client;
    shape.top -= extendTop;
    shape.bottom += extendBottom;
    EditRound(hdc, &shape, EDIT_PANEL_RADIUS, EDIT_COLOR_SURFACE);
}

static HFONT EditUiFont(int px, int weight = FW_NORMAL, bool italic = false, bool underline = false)
{
    return CreateFontA(-px, 0, 0, 0, weight, italic ? TRUE : FALSE, underline ? TRUE : FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

static void EditLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y1, nullptr);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static void EditChevron(HDC hdc, int cx, int cy, COLORREF color)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(EditGdiColor(color), 1.5f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::PointF pts[3] = {
        Gdiplus::PointF(cx - 4.0f, cy - 2.0f),
        Gdiplus::PointF(static_cast<float>(cx), cy + 2.0f),
        Gdiplus::PointF(cx + 4.0f, cy - 2.0f)
    };
    g.DrawLines(&pen, pts, 3);
}

static int CALLBACK CollectFontProc(const LOGFONTA* lf, const TEXTMETRICA*, DWORD, LPARAM)
{
    if (lf == nullptr || lf->lfFaceName[0] == '\0' || lf->lfFaceName[0] == '@')
    {
        return 1;
    }
    for (int i = 0; i < g_fontCount; ++i)
    {
        if (lstrcmpiA(g_fonts[i], lf->lfFaceName) == 0)
        {
            return 1;
        }
    }
    if (g_fontCount >= EDIT_MAX_FONTS)
    {
        return 0;
    }
    lstrcpynA(g_fonts[g_fontCount], lf->lfFaceName, 64);
    g_fontCount++;
    return 1;
}

static int FontNameCmp(const void* a, const void* b)
{
    return lstrcmpiA(static_cast<const char*>(a), static_cast<const char*>(b));
}

static void EnsureFonts()
{
    if (g_fontCount > 0)
    {
        return;
    }
    HDC hdc = GetDC(nullptr);
    LOGFONTA request = {};
    request.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExA(hdc, &request, CollectFontProc, 0, 0);
    ReleaseDC(nullptr, hdc);
    if (g_fontCount > 1)
    {
        qsort(g_fonts, static_cast<size_t>(g_fontCount), 64, FontNameCmp);
    }
}

static bool SetupTextFont(HDC hdc, HWND canvas, RECT* box, int* lineH, HFONT* font, HFONT* oldFont)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (!Renderer_FreeformGet(g_selected, nullptr, &x, &y, &w, &h))
    {
        return false;
    }

    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    SlideToClient(canvas, x, y, &left, &top);
    SlideToClient(canvas, x + w, y + h, &right, &bottom);
    box->left = left;
    box->top = top;
    box->right = right;
    box->bottom = bottom;

    char name[64] = "Segoe UI";
    int size = 64;
    COLORREF color = RGB(255, 255, 255);
    bool bold = false;
    bool italic = false;
    bool underline = false;
    Renderer_FreeformGetStyle(g_selected, name, sizeof(name), &size, &color, &bold, &italic, &underline);

    RECT view = {};
    GetCanvasView(canvas, &view);
    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    const int destH = view.bottom - view.top;
    int fontHeight = destH > 0 ? -MulDiv(size, destH, slideH) : -16;
    if (fontHeight == 0)
    {
        fontHeight = -12;
    }

    *font = CreateFontA(
        fontHeight,
        0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        italic ? TRUE : FALSE,
        underline ? TRUE : FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        name
    );
    *oldFont = static_cast<HFONT>(SelectObject(hdc, *font));
    TEXTMETRICA metrics = {};
    GetTextMetricsA(hdc, &metrics);
    *lineH = metrics.tmHeight + metrics.tmExternalLeading;
    if (*lineH < 1)
    {
        *lineH = 16;
    }
    return true;
}

static void BuildWrapLines(HDC hdc, const char* text, int boxW)
{
    g_wrapCount = 0;
    if (boxW < 1)
    {
        boxW = 1;
    }

    const int len = lstrlenA(text);
    int i = 0;
    while (g_wrapCount < EDIT_WRAP_MAX)
    {
        g_wrapStart[g_wrapCount] = i;
        if (i >= len)
        {
            if (g_wrapCount == 0 || (len > 0 && text[len - 1] == '\n'))
            {
                g_wrapLen[g_wrapCount] = 0;
                g_wrapNl[g_wrapCount] = false;
                g_wrapCount++;
            }
            break;
        }

        int run = i;
        while (run < len && text[run] != '\n')
        {
            run++;
        }
        const int runLen = run - i;
        int fit = runLen;
        if (runLen > 0)
        {
            SIZE size = {};
            GetTextExtentExPointA(hdc, text + i, runLen, boxW, &fit, nullptr, &size);
            if (fit < 1)
            {
                fit = 1;
            }
            if (fit < runLen)
            {
                int brk = fit;
                while (brk > 0 && text[i + brk] != ' ' && text[i + brk] != '\t')
                {
                    brk--;
                }
                if (brk > 0)
                {
                    fit = brk + 1;
                }
            }
        }
        else
        {
            fit = 0;
        }

        g_wrapLen[g_wrapCount] = fit;
        g_wrapNl[g_wrapCount] = (i + fit >= run && run < len && text[run] == '\n');
        g_wrapCount++;
        i += fit;
        if (i < len && text[i] == '\n' && g_wrapNl[g_wrapCount - 1])
        {
            i++;
        }
        if (fit == 0 && !g_wrapNl[g_wrapCount - 1])
        {
            break;
        }
    }
}

static int CaretLineIndex(int caret, int textLen)
{
    (void)textLen;
    if (g_wrapCount < 1)
    {
        return 0;
    }
    for (int i = 0; i < g_wrapCount; ++i)
    {
        const int start = g_wrapStart[i];
        const int visEnd = start + g_wrapLen[i];
        if (g_wrapNl[i])
        {
            if (caret >= start && caret <= visEnd)
            {
                return i;
            }
        }
        else if (caret >= start && caret <= visEnd)
        {
            return i;
        }
    }
    return g_wrapCount - 1;
}

static void DrawTextCaret(HDC hdc, HWND canvas)
{
    if (!g_textOpen || HasTextSel() || !g_caretOn || g_selected < 0)
    {
        return;
    }

    RECT box = {};
    int lineH = 16;
    HFONT font = nullptr;
    HFONT oldFont = nullptr;
    if (!SetupTextFont(hdc, canvas, &box, &lineH, &font, &oldFont))
    {
        return;
    }

    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    int caret = g_caret;
    if (caret < 0)
    {
        caret = 0;
    }
    if (caret > len)
    {
        caret = len;
    }
    BuildWrapLines(hdc, text, box.right - box.left);
    const int line = CaretLineIndex(caret, len);
    const int start = g_wrapStart[line];
    int prefix = caret - start;
    if (prefix < 0)
    {
        prefix = 0;
    }
    if (prefix > g_wrapLen[line])
    {
        prefix = g_wrapLen[line];
    }

    SIZE size = {};
    if (prefix > 0)
    {
        GetTextExtentPoint32A(hdc, text + start, prefix, &size);
    }

    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    float rotation = 0.0f;
    ObjectFrame(g_selected, &x, &y, &w, &h, &rotation);
    const int boxW = box.right - box.left;
    const int boxH = box.bottom - box.top;
    const int pad = AlignPad(hdc, text, start, g_wrapLen[line], boxW);
    float sx = static_cast<float>(x);
    float sy = static_cast<float>(y);
    if (boxW > 0 && boxH > 0)
    {
        sx += (static_cast<float>(pad + size.cx) * w) / boxW;
        sy += (static_cast<float>(line * lineH) * h) / boxH;
    }
    float ex = sx;
    float ey = sy;
    if (boxH > 0)
    {
        ey += (static_cast<float>(lineH) * h) / boxH;
    }
    const float cx = x + (w * 0.5f);
    const float cy = y + (h * 0.5f);
    RotateAround(cx, cy, rotation, &sx, &sy);
    RotateAround(cx, cy, rotation, &ex, &ey);

    COLORREF color = RGB(255, 255, 255);
    Renderer_FreeformGetStyle(g_selected, nullptr, 0, nullptr, &color, nullptr, nullptr, nullptr);
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    SlideToClientF(canvas, sx, sy, &x0, &y0);
    SlideToClientF(canvas, ex, ey, &x1, &y1);
    HPEN pen = CreatePen(PS_SOLID, 2, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, x0, y0, nullptr);
    LineTo(hdc, x1, y1);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

static bool HasTextSel()
{
    return g_selAnchor != g_caret;
}

static void SelRange(int* from, int* count)
{
    int a = g_selAnchor;
    int b = g_caret;
    if (a > b)
    {
        const int swap = a;
        a = b;
        b = swap;
    }
    *from = a;
    *count = b - a;
}

static int PrefixWidth(HDC hdc, const char* text, int start, int chars)
{
    SIZE size = {};
    if (chars > 0)
    {
        GetTextExtentPoint32A(hdc, text + start, chars, &size);
    }
    return size.cx;
}

static int AlignPad(HDC hdc, const char* text, int start, int vis, int boxW)
{
    const int align = Renderer_FreeformGetAlign(g_selected);
    if (align < 1 || boxW < 1)
    {
        return 0;
    }
    const int lineW = PrefixWidth(hdc, text, start, vis);
    if (align == 2)
    {
        return boxW - lineW;
    }
    return (boxW - lineW) / 2;
}

static void DrawTextSelection(HDC hdc, HWND canvas)
{
    if (!g_textOpen || !HasTextSel() || g_selected < 0)
    {
        return;
    }

    RECT box = {};
    int lineH = 16;
    HFONT font = nullptr;
    HFONT oldFont = nullptr;
    if (!SetupTextFont(hdc, canvas, &box, &lineH, &font, &oldFont))
    {
        return;
    }

    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    int from = 0;
    int count = 0;
    SelRange(&from, &count);
    const int to = from + count;
    BuildWrapLines(hdc, text, box.right - box.left);

    HBRUSH fill = CreateSolidBrush(RGB(40, 90, 180));
    for (int i = 0; i < g_wrapCount; ++i)
    {
        const int start = g_wrapStart[i];
        const int vis = g_wrapLen[i];
        int lineEnd = start + vis;
        if (g_wrapNl[i])
        {
            lineEnd += 1;
        }
        int selA = from > start ? from : start;
        int selB = to < lineEnd ? to : lineEnd;
        if (selA >= selB)
        {
            continue;
        }
        int visA = selA - start;
        int visB = selB - start;
        if (visA < 0)
        {
            visA = 0;
        }
        if (visB > vis)
        {
            visB = vis;
        }
        if (visB < visA)
        {
            continue;
        }

        const int pad = AlignPad(hdc, text, start, vis, box.right - box.left);
        const int x0 = box.left + pad + PrefixWidth(hdc, text, start, visA);
        int x1 = box.left + pad + PrefixWidth(hdc, text, start, visB);
        if (x1 <= x0)
        {
            x1 = x0 + 4;
        }
        RECT hi = { x0, box.top + (i * lineH), x1, box.top + ((i + 1) * lineH) };
        FillRect(hdc, &hi, fill);
        if (visB > visA)
        {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            RECT glyph = hi;
            DrawTextA(hdc, text + start + visA, visB - visA, &glyph, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
    DeleteObject(fill);

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

static void PlaceCaretFromClick(HWND canvas, int clientX, int clientY, bool extend)
{
    HDC hdc = GetDC(canvas);
    RECT box = {};
    int lineH = 16;
    HFONT font = nullptr;
    HFONT oldFont = nullptr;
    if (!SetupTextFont(hdc, canvas, &box, &lineH, &font, &oldFont))
    {
        ReleaseDC(canvas, hdc);
        return;
    }

    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    BuildWrapLines(hdc, text, box.right - box.left);

    int slideX = 0;
    int slideY = 0;
    ClientToSlideClamped(canvas, clientX, clientY, &slideX, &slideY);
    int localX = 0;
    int localY = 0;
    UnrotateSlide(g_selected, slideX, slideY, &localX, &localY);
    int ox = 0;
    int oy = 0;
    int ow = 0;
    int oh = 0;
    ObjectFrame(g_selected, &ox, &oy, &ow, &oh, nullptr);
    const int boxW = box.right - box.left;
    const int boxH = box.bottom - box.top;
    int localClientX = 0;
    int localClientY = 0;
    if (ow > 0 && oh > 0 && boxW > 0 && boxH > 0)
    {
        localClientX = MulDiv(localX - ox, boxW, ow);
        localClientY = MulDiv(localY - oy, boxH, oh);
    }

    int line = 0;
    if (lineH > 0)
    {
        line = localClientY / lineH;
    }
    if (line < 0)
    {
        line = 0;
    }
    if (g_wrapCount > 0 && line >= g_wrapCount)
    {
        line = g_wrapCount - 1;
    }

    if (g_wrapCount < 1)
    {
        g_caret = lstrlenA(text);
    }
    else
    {
        const int start = g_wrapStart[line];
        const int vis = g_wrapLen[line];
        int fit = 0;
        const int pad = AlignPad(hdc, text, start, vis, boxW);
        const int x = localClientX - pad;
        if (x > 0 && vis > 0)
        {
            SIZE size = {};
            GetTextExtentExPointA(hdc, text + start, vis, x, &fit, nullptr, &size);
        }
        if (fit < 0)
        {
            fit = 0;
        }
        if (fit > vis)
        {
            fit = vis;
        }
        g_caret = start + fit;
        if (g_wrapNl[line] && fit >= vis)
        {
            g_caret = start + vis;
        }
    }

    if (!extend)
    {
        g_selAnchor = g_caret;
    }
    SelectObject(hdc, oldFont);
    DeleteObject(font);
    ReleaseDC(canvas, hdc);
    g_caretOn = true;
    InvalidateRect(canvas, nullptr, FALSE);
}

static void ClampCaret()
{
    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    if (g_caret < 0)
    {
        g_caret = 0;
    }
    if (g_caret > len)
    {
        g_caret = len;
    }
    if (g_selAnchor < 0)
    {
        g_selAnchor = 0;
    }
    if (g_selAnchor > len)
    {
        g_selAnchor = len;
    }
}

static void CollapseSelToCaret(bool toStart)
{
    if (!HasTextSel())
    {
        return;
    }
    int from = 0;
    int count = 0;
    SelRange(&from, &count);
    g_caret = toStart ? from : (from + count);
    g_selAnchor = g_caret;
}

static bool DeleteSelection()
{
    if (!HasTextSel())
    {
        return false;
    }
    int from = 0;
    int count = 0;
    SelRange(&from, &count);
    DeleteTextRange(from, count);
    g_selAnchor = g_caret;
    return true;
}

static void LiveReplaceText(const char* text)
{
    Renderer_FreeformSetTextLive(g_selected, text);
    ClampCaret();
    g_caretOn = true;
    if (g_canvas != nullptr)
    {
        InvalidateRect(g_canvas, nullptr, FALSE);
    }
}

static void InsertTextChars(const char* insert)
{
    if (!g_textOpen || insert == nullptr || insert[0] == '\0')
    {
        return;
    }
    DeleteSelection();
    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    const int add = lstrlenA(insert);
    ClampCaret();
    if (len + add >= EDIT_TEXT_MAX)
    {
        return;
    }
    memmove(text + g_caret + add, text + g_caret, static_cast<size_t>(len - g_caret + 1));
    memcpy(text + g_caret, insert, static_cast<size_t>(add));
    g_caret += add;
    g_selAnchor = g_caret;
    LiveReplaceText(text);
}

static void DeleteTextRange(int from, int count)
{
    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    if (from < 0 || count < 1 || from >= len)
    {
        return;
    }
    if (from + count > len)
    {
        count = len - from;
    }
    memmove(text + from, text + from + count, static_cast<size_t>(len - from - count + 1));
    g_caret = from;
    g_selAnchor = from;
    LiveReplaceText(text);
}

static void MoveCaretLine(HWND canvas, int delta, bool extend)
{
    HDC hdc = GetDC(canvas);
    RECT box = {};
    int lineH = 16;
    HFONT font = nullptr;
    HFONT oldFont = nullptr;
    if (!SetupTextFont(hdc, canvas, &box, &lineH, &font, &oldFont))
    {
        ReleaseDC(canvas, hdc);
        return;
    }

    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    ClampCaret();
    BuildWrapLines(hdc, text, box.right - box.left);
    const int line = CaretLineIndex(g_caret, len);
    int prefix = g_caret - g_wrapStart[line];
    if (prefix > g_wrapLen[line])
    {
        prefix = g_wrapLen[line];
    }
    SIZE size = {};
    if (prefix > 0)
    {
        GetTextExtentPoint32A(hdc, text + g_wrapStart[line], prefix, &size);
    }
    const int boxW = box.right - box.left;
    const int pad = AlignPad(hdc, text, g_wrapStart[line], g_wrapLen[line], boxW);
    const int visualX = pad + size.cx;

    int next = line + delta;
    if (next < 0)
    {
        next = 0;
    }
    if (next >= g_wrapCount)
    {
        next = g_wrapCount - 1;
    }

    int fit = 0;
    const int vis = g_wrapLen[next];
    if (vis > 0)
    {
        SIZE ignore = {};
        const int nextPad = AlignPad(hdc, text, g_wrapStart[next], vis, boxW);
        int target = visualX - nextPad;
        if (target < 0)
        {
            target = 0;
        }
        GetTextExtentExPointA(hdc, text + g_wrapStart[next], vis, target, &fit, nullptr, &ignore);
    }
    g_caret = g_wrapStart[next] + fit;
    if (g_wrapNl[next] && fit >= vis)
    {
        g_caret = g_wrapStart[next] + vis;
    }
    if (!extend)
    {
        g_selAnchor = g_caret;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    ReleaseDC(canvas, hdc);
    g_caretOn = true;
    InvalidateRect(canvas, nullptr, FALSE);
}

static void MoveCaretHomeEnd(HWND canvas, bool home, bool extend)
{
    HDC hdc = GetDC(canvas);
    RECT box = {};
    int lineH = 16;
    HFONT font = nullptr;
    HFONT oldFont = nullptr;
    if (!SetupTextFont(hdc, canvas, &box, &lineH, &font, &oldFont))
    {
        ReleaseDC(canvas, hdc);
        return;
    }

    char text[EDIT_TEXT_MAX] = {};
    Renderer_FreeformGetText(g_selected, text, sizeof(text));
    const int len = lstrlenA(text);
    BuildWrapLines(hdc, text, box.right - box.left);
    const int line = CaretLineIndex(g_caret, len);
    g_caret = home ? g_wrapStart[line] : (g_wrapStart[line] + g_wrapLen[line]);
    if (!extend)
    {
        g_selAnchor = g_caret;
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);
    ReleaseDC(canvas, hdc);
    g_caretOn = true;
    InvalidateRect(canvas, nullptr, FALSE);
}

static bool HitSelectedInterior(HWND canvas, int clientX, int clientY)
{
    return g_selected >= 0 && HitObjectInterior(canvas, g_selected, clientX, clientY);
}

// The text editor records an undo step when it opens; style changes made
// without it need their own.
static void RememberStyleChange()
{
    if (!g_textOpen)
    {
        History_Remember();
    }
}

static void CommitStyleChange()
{
    CommitDocument();
    RefreshSlideLists();
    RefreshEditorChrome();
}

static void ToggleFontFlag(int which)
{
    char name[64] = {};
    int size = 64;
    COLORREF color = RGB(255, 255, 255);
    bool bold = false;
    bool italic = false;
    bool underline = false;
    if (!Renderer_FreeformGetStyle(g_selected, name, sizeof(name), &size, &color, &bold, &italic, &underline))
    {
        return;
    }
    if (which == 0)
    {
        bold = !bold;
    }
    else if (which == 1)
    {
        italic = !italic;
    }
    else
    {
        underline = !underline;
    }
    RememberStyleChange();
    Renderer_FreeformSetFontStyle(g_selected, bold, italic, underline);
    CommitStyleChange();
}

static void DrawWell(HDC hdc, const RECT* cell, bool armed)
{
    HBRUSH well = CreateSolidBrush(armed ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_DARK);
    FillRect(hdc, cell, well);
    DeleteObject(well);
    HPEN pen = CreatePen(PS_SOLID, armed ? 2 : 1, armed ? EDIT_COLOR_SELECT : EDIT_COLOR_WELL);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    RoundRect(hdc, cell->left, cell->top, cell->right, cell->bottom, 6, 6);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void DrawChevron(HDC hdc, int right, int cy, COLORREF color)
{
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN old = static_cast<HPEN>(SelectObject(hdc, pen));
    MoveToEx(hdc, right - 12, cy - 3, nullptr);
    LineTo(hdc, right - 8, cy + 2);
    LineTo(hdc, right - 4, cy - 3);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

static int DropContentHeight()
{
    if (g_dropKind == DROP_FONT)
    {
        return g_fontCount * DROP_ITEM_H;
    }
    if (g_dropKind == DROP_SIZE)
    {
        return kFontSizeCount * DROP_ITEM_H;
    }
    if (g_dropKind == DROP_ALIGN)
    {
        return 3 * DROP_ITEM_H;
    }
    return 0;
}

static void SyncDropScroll(HWND hwnd)
{
    if (hwnd == nullptr || g_dropKind == DROP_COLOR)
    {
        return;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    const int content = DropContentHeight();
    const int page = client.bottom > 0 ? client.bottom : 1;
    int maxPos = content > page ? content - page : 0;
    if (g_dropScrollY < 0)
    {
        g_dropScrollY = 0;
    }
    if (g_dropScrollY > maxPos)
    {
        g_dropScrollY = maxPos;
    }

    SCROLLINFO info = {};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = content > 0 ? content - 1 : 0;
    info.nPage = page;
    info.nPos = g_dropScrollY;
    SetScrollInfo(hwnd, SB_VERT, &info, TRUE);
}

static COLORREF HsvToRgb(float h, float s, float v)
{
    while (h >= 360.0f)
    {
        h -= 360.0f;
    }
    while (h < 0.0f)
    {
        h += 360.0f;
    }
    const float c = v * s;
    const float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (h < 60.0f)
    {
        r = c; g = x;
    }
    else if (h < 120.0f)
    {
        r = x; g = c;
    }
    else if (h < 180.0f)
    {
        g = c; b = x;
    }
    else if (h < 240.0f)
    {
        g = x; b = c;
    }
    else if (h < 300.0f)
    {
        r = x; b = c;
    }
    else
    {
        r = c; b = x;
    }
    return RGB(
        static_cast<int>((r + m) * 255.0f + 0.5f),
        static_cast<int>((g + m) * 255.0f + 0.5f),
        static_cast<int>((b + m) * 255.0f + 0.5f)
    );
}

// Grays have no hue, so the hue is left alone for them and the strip keeps its
// place.
static void RgbToHsv(COLORREF color, float* h, float* s, float* v)
{
    const float r = GetRValue(color) / 255.0f;
    const float g = GetGValue(color) / 255.0f;
    const float b = GetBValue(color) / 255.0f;
    const float maxC = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const float minC = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const float delta = maxC - minC;
    *v = maxC;
    *s = maxC > 0.0f ? delta / maxC : 0.0f;
    if (delta <= 0.0f)
    {
        return;
    }
    float hue = 0.0f;
    if (maxC == r)
    {
        hue = 60.0f * fmodf((g - b) / delta, 6.0f);
    }
    else if (maxC == g)
    {
        hue = 60.0f * (((b - r) / delta) + 2.0f);
    }
    else
    {
        hue = 60.0f * (((r - g) / delta) + 4.0f);
    }
    *h = hue < 0.0f ? hue + 360.0f : hue;
}

static void PickSwatchRect(int index, int rowY, RECT* rect)
{
    const int col = index % PICK_COLS;
    const int row = index / PICK_COLS;
    rect->left = PICK_PAD + col * (PICK_CELL + PICK_GAP);
    rect->top = rowY + row * (PICK_CELL + PICK_GAP);
    rect->right = rect->left + PICK_CELL;
    rect->bottom = rect->top + PICK_CELL;
}

static void PickSvRect(RECT* rect)
{
    SetRect(rect, PICK_PAD, PICK_SV_Y, PICK_PAD + PICK_SV_W, PICK_SV_Y + PICK_SV_H);
}

static void PickHueRect(RECT* rect)
{
    const int left = PICK_PAD + PICK_SV_W + PICK_GAP * 2;
    SetRect(rect, left, PICK_SV_Y, left + PICK_HUE_W, PICK_SV_Y + PICK_SV_H);
}

static void PickPreviewRect(RECT* rect)
{
    SetRect(rect, PICK_PAD, PICK_ROW_Y, PICK_PAD + PICK_PREVIEW_W, PICK_ROW_Y + PICK_CELL);
}

static void PickHexRect(RECT* rect)
{
    SetRect(rect, PICK_PAD + PICK_PREVIEW_W + PICK_GAP * 2, PICK_ROW_Y, PICK_PAD + PICK_INNER_W, PICK_ROW_Y + PICK_CELL);
}

static int PickHitCell(int x, int y, int rowY, int count)
{
    for (int i = 0; i < count; ++i)
    {
        RECT cell = {};
        PickSwatchRect(i, rowY, &cell);
        if (x >= cell.left && x < cell.right && y >= cell.top && y < cell.bottom)
        {
            return i;
        }
    }
    return -1;
}

static void PickRemember(COLORREF color)
{
    int at = g_pickRecentCount < PICK_RECENT_MAX ? g_pickRecentCount : PICK_RECENT_MAX - 1;
    for (int i = 0; i < g_pickRecentCount; ++i)
    {
        if (g_pickRecent[i] == color)
        {
            at = i;
            break;
        }
    }
    for (int i = at; i > 0; --i)
    {
        g_pickRecent[i] = g_pickRecent[i - 1];
    }
    g_pickRecent[0] = color;
    if (at == g_pickRecentCount && g_pickRecentCount < PICK_RECENT_MAX)
    {
        g_pickRecentCount += 1;
    }
}

static void PickSetLive(COLORREF color)
{
    g_pickColor = color;
    Renderer_FreeformSetTextColor(g_selected, color);
    if (g_canvas != nullptr)
    {
        InvalidateRect(g_canvas, nullptr, FALSE);
    }
    if (g_typeBar != nullptr)
    {
        InvalidateRect(g_typeBar, nullptr, FALSE);
    }
    if (g_drop != nullptr)
    {
        InvalidateRect(g_drop, nullptr, FALSE);
    }
}

static void PickApplyAndClose(COLORREF color)
{
    if (!g_pickChanged)
    {
        RememberStyleChange();
    }
    Renderer_FreeformSetTextColor(g_selected, color);
    g_pickColor = color;
    g_pickChanged = true;
    HideDrop();
    if (g_canvas != nullptr)
    {
        SetFocus(g_canvas);
    }
    CommitStyleChange();
}

static void PickTrack(int x, int y)
{
    if (g_pickDrag == PICK_DRAG_SV)
    {
        RECT sv = {};
        PickSvRect(&sv);
        float s = static_cast<float>(x - sv.left) / (PICK_SV_W - 1);
        float v = 1.0f - static_cast<float>(y - sv.top) / (PICK_SV_H - 1);
        g_pickS = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
        g_pickV = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
    else if (g_pickDrag == PICK_DRAG_HUE)
    {
        RECT hue = {};
        PickHueRect(&hue);
        float h = 360.0f * static_cast<float>(y - hue.top) / (PICK_SV_H - 1);
        g_pickH = h < 0.0f ? 0.0f : (h > 359.9f ? 359.9f : h);
    }
    else
    {
        return;
    }
    PickSetLive(HsvToRgb(g_pickH, g_pickS, g_pickV));
}

static void PickFinishDrag()
{
    if (g_pickDrag == PICK_DRAG_NONE)
    {
        return;
    }
    g_pickDrag = PICK_DRAG_NONE;
    CommitStyleChange();
}

static bool PickHexColor(const char* hex, COLORREF* color)
{
    const int length = lstrlenA(hex);
    if (length != 3 && length != 6)
    {
        return false;
    }
    char full[7] = {};
    for (int i = 0; i < 6; ++i)
    {
        full[i] = length == 3 ? hex[i / 2] : hex[i];
    }
    const unsigned long value = strtoul(full, nullptr, 16);
    *color = RGB((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
    return true;
}

static void PickFrame(HDC hdc, const RECT* rect, COLORREF color, int width)
{
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    Rectangle(hdc, rect->left, rect->top, rect->right, rect->bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void PickFill(HDC hdc, const RECT* rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, rect, brush);
    DeleteObject(brush);
}

static void PickBlitGradient(HDC hdc, const RECT* rect, bool hueStrip)
{
    const int w = rect->right - rect->left;
    const int h = rect->bottom - rect->top;
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib == nullptr || bits == nullptr)
    {
        return;
    }
    DWORD* pixels = static_cast<DWORD*>(bits);
    const COLORREF pure = HsvToRgb(g_pickH, 1.0f, 1.0f);
    for (int y = 0; y < h; ++y)
    {
        COLORREF rowColor = 0;
        if (hueStrip)
        {
            rowColor = HsvToRgb(360.0f * y / (h > 1 ? h - 1 : 1), 1.0f, 1.0f);
        }
        const float v = 1.0f - static_cast<float>(y) / (h > 1 ? h - 1 : 1);
        for (int x = 0; x < w; ++x)
        {
            int r = GetRValue(rowColor);
            int g = GetGValue(rowColor);
            int b = GetBValue(rowColor);
            if (!hueStrip)
            {
                const float s = static_cast<float>(x) / (w > 1 ? w - 1 : 1);
                r = static_cast<int>(v * (255.0f + s * (GetRValue(pure) - 255.0f)) + 0.5f);
                g = static_cast<int>(v * (255.0f + s * (GetGValue(pure) - 255.0f)) + 0.5f);
                b = static_cast<int>(v * (255.0f + s * (GetBValue(pure) - 255.0f)) + 0.5f);
            }
            pixels[y * w + x] = (static_cast<DWORD>(r) << 16) | (static_cast<DWORD>(g) << 8) | static_cast<DWORD>(b);
        }
    }
    HDC src = CreateCompatibleDC(hdc);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(src, dib));
    BitBlt(hdc, rect->left, rect->top, w, h, src, 0, 0, SRCCOPY);
    SelectObject(src, oldBmp);
    DeleteDC(src);
    DeleteObject(dib);
}

static void PaintColorPicker(HDC hdc)
{
    for (int i = 0; i < kSwatchCount && i < PICK_COLS * 2; ++i)
    {
        RECT cell = {};
        PickSwatchRect(i, PICK_PAD, &cell);
        PickFill(hdc, &cell, kSwatches[i]);
        const bool current = kSwatches[i] == g_pickColor;
        PickFrame(hdc, &cell, current ? EDIT_COLOR_SELECT : EDIT_COLOR_WELL, current ? 2 : 1);
    }

    RECT sv = {};
    PickSvRect(&sv);
    PickBlitGradient(hdc, &sv, false);
    PickFrame(hdc, &sv, EDIT_COLOR_WELL, 1);
    const int markX = sv.left + static_cast<int>(g_pickS * (PICK_SV_W - 1) + 0.5f);
    const int markY = sv.top + static_cast<int>((1.0f - g_pickV) * (PICK_SV_H - 1) + 0.5f);
    HRGN clip = CreateRectRgn(sv.left, sv.top, sv.right, sv.bottom);
    SelectClipRgn(hdc, clip);
    RECT ring = { markX - 6, markY - 6, markX + 7, markY + 7 };
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    HPEN outer = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, outer));
    Ellipse(hdc, ring.left, ring.top, ring.right, ring.bottom);
    HPEN inner = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    SelectObject(hdc, inner);
    Ellipse(hdc, ring.left + 1, ring.top + 1, ring.right - 1, ring.bottom - 1);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(outer);
    DeleteObject(inner);
    SelectClipRgn(hdc, nullptr);
    DeleteObject(clip);

    RECT hue = {};
    PickHueRect(&hue);
    PickBlitGradient(hdc, &hue, true);
    PickFrame(hdc, &hue, EDIT_COLOR_WELL, 1);
    const int hueY = hue.top + static_cast<int>(g_pickH / 360.0f * (PICK_SV_H - 1) + 0.5f);
    RECT thumb = { hue.left - 3, hueY - 3, hue.right + 3, hueY + 4 };
    PickFrame(hdc, &thumb, RGB(0, 0, 0), 1);
    InflateRect(&thumb, -1, -1);
    PickFrame(hdc, &thumb, RGB(255, 255, 255), 1);

    RECT preview = {};
    PickPreviewRect(&preview);
    PickFill(hdc, &preview, g_pickColor);
    PickFrame(hdc, &preview, EDIT_COLOR_WELL, 1);

    RECT hexWell = {};
    PickHexRect(&hexWell);
    PickFill(hdc, &hexWell, g_hexEditing ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_DARK);
    PickFrame(hdc, &hexWell, g_hexEditing ? EDIT_COLOR_SELECT : EDIT_COLOR_WELL, g_hexEditing ? 2 : 1);
    char hexText[16] = {};
    if (g_hexEditing)
    {
        wsprintfA(hexText, "#%s|", g_hexBuf);
    }
    else
    {
        wsprintfA(hexText, "#%02X%02X%02X", GetRValue(g_pickColor), GetGValue(g_pickColor), GetBValue(g_pickColor));
    }
    RECT hexLabel = hexWell;
    hexLabel.left += 8;
    SetTextColor(hdc, g_hexEditing ? EDIT_COLOR_SELECT : EDIT_COLOR_INK);
    DrawTextA(hdc, hexText, -1, &hexLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HPEN divider = CreatePen(PS_SOLID, 1, EDIT_COLOR_WELL);
    oldPen = static_cast<HPEN>(SelectObject(hdc, divider));
    MoveToEx(hdc, PICK_PAD, PICK_RECENT_Y - PICK_PAD / 2 - 1, nullptr);
    LineTo(hdc, PICK_PAD + PICK_INNER_W, PICK_RECENT_Y - PICK_PAD / 2 - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(divider);

    for (int i = 0; i < PICK_RECENT_MAX; ++i)
    {
        RECT cell = {};
        PickSwatchRect(i, PICK_RECENT_Y, &cell);
        if (i < g_pickRecentCount)
        {
            PickFill(hdc, &cell, g_pickRecent[i]);
            PickFrame(hdc, &cell, EDIT_COLOR_WELL, 1);
        }
        else if (g_pickRecentCount > 0)
        {
            PickFill(hdc, &cell, EDIT_COLOR_DARK);
        }
    }
    if (g_pickRecentCount == 0)
    {
        RECT hint = { PICK_PAD, PICK_RECENT_Y, PICK_PAD + PICK_INNER_W, PICK_RECENT_Y + PICK_CELL };
        SetTextColor(hdc, RGB(0x8E, 0x7E, 0x92));
        DrawTextA(hdc, "Recent colors", -1, &hint, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void PickBeginHexEdit(bool fresh)
{
    g_hexEditing = true;
    g_hexFresh = fresh;
    wsprintfA(g_hexBuf, "%02X%02X%02X", GetRValue(g_pickColor), GetGValue(g_pickColor), GetBValue(g_pickColor));
}

static void PickHexChar(WPARAM ch)
{
    if (ch == VK_ESCAPE)
    {
        HideDrop();
        if (g_canvas != nullptr)
        {
            SetFocus(g_canvas);
        }
        return;
    }
    if (ch == VK_RETURN)
    {
        COLORREF color = 0;
        if (g_hexEditing && PickHexColor(g_hexBuf, &color))
        {
            PickApplyAndClose(color);
        }
        return;
    }
    const bool hexDigit = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    if (!g_hexEditing)
    {
        if (!hexDigit && ch != '#')
        {
            return;
        }
        PickBeginHexEdit(true);
    }
    if (ch == VK_BACK)
    {
        const int length = lstrlenA(g_hexBuf);
        g_hexBuf[g_hexFresh ? 0 : (length > 0 ? length - 1 : 0)] = '\0';
        g_hexFresh = false;
    }
    else if (hexDigit)
    {
        if (g_hexFresh)
        {
            g_hexBuf[0] = '\0';
            g_hexFresh = false;
        }
        const int length = lstrlenA(g_hexBuf);
        if (length < 6)
        {
            g_hexBuf[length] = static_cast<char>(ch >= 'a' ? ch - 32 : ch);
            g_hexBuf[length + 1] = '\0';
        }
    }
    else if (ch == '#')
    {
        g_hexBuf[0] = '\0';
        g_hexFresh = false;
    }
    if (g_drop != nullptr)
    {
        InvalidateRect(g_drop, nullptr, FALSE);
    }
}

static void ShowDrop(DropKind kind, HWND bar, const RECT* well)
{
    HideDrop();
    if (kind == DROP_NONE || bar == nullptr)
    {
        return;
    }
    EnsureFonts();
    g_dropKind = kind;
    g_dropScrollY = 0;
    g_dropWheelAcc = 0;

    RECT screen = *well;
    MapWindowPoints(bar, nullptr, reinterpret_cast<POINT*>(&screen), 2);

    int width = well->right - well->left;
    int height = 120;
    DWORD style = WS_POPUP | WS_BORDER;
    if (kind == DROP_FONT)
    {
        if (width < 200)
        {
            width = 200;
        }
        int rows = g_fontCount;
        if (rows > DROP_FONT_ROWS)
        {
            rows = DROP_FONT_ROWS;
        }
        if (rows < 1)
        {
            rows = 1;
        }
        height = rows * DROP_ITEM_H;
        char current[64] = {};
        Renderer_FreeformGetStyle(g_selected, current, sizeof(current), nullptr, nullptr, nullptr, nullptr, nullptr);
        int index = 0;
        for (int i = 0; i < g_fontCount; ++i)
        {
            if (lstrcmpiA(g_fonts[i], current) == 0)
            {
                index = i;
                break;
            }
        }
        g_dropScrollY = (index * DROP_ITEM_H) - ((rows / 2) * DROP_ITEM_H);
        if (g_fontCount > rows)
        {
            style |= WS_VSCROLL;
        }
    }
    else if (kind == DROP_SIZE)
    {
        height = kFontSizeCount * DROP_ITEM_H;
        if (width < 56)
        {
            width = 56;
        }
    }
    else if (kind == DROP_ALIGN)
    {
        height = 3 * DROP_ITEM_H;
        if (width < TYPE_ALIGN_W)
        {
            width = TYPE_ALIGN_W;
        }
    }
    else
    {
        width = PICK_W;
        height = PICK_H;
        COLORREF color = RGB(255, 255, 255);
        Renderer_FreeformGetStyle(g_selected, nullptr, 0, nullptr, &color, nullptr, nullptr, nullptr);
        g_pickColor = color;
        RgbToHsv(color, &g_pickH, &g_pickS, &g_pickV);
        g_pickDrag = PICK_DRAG_NONE;
        g_hexEditing = false;
    }

    RECT outer = { 0, 0, width, height };
    AdjustWindowRectEx(&outer, style, FALSE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);

    g_drop = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "EditDropWindow",
        "",
        style,
        screen.left,
        screen.bottom + 2,
        outer.right - outer.left,
        outer.bottom - outer.top,
        bar,
        nullptr,
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrA(bar, GWLP_HINSTANCE)),
        nullptr
    );
    if (g_drop != nullptr)
    {
        SyncDropScroll(g_drop);
        ShowWindow(g_drop, SW_SHOW);
        SetFocus(g_drop);
    }
}

static void ApplyDropIndex(int index)
{
    if (index < 0)
    {
        return;
    }
    RememberStyleChange();
    if (g_dropKind == DROP_FONT)
    {
        if (index >= g_fontCount)
        {
            return;
        }
        Renderer_FreeformSetFontName(g_selected, g_fonts[index]);
    }
    else if (g_dropKind == DROP_SIZE)
    {
        if (index >= kFontSizeCount)
        {
            return;
        }
        Renderer_FreeformSetFontSize(g_selected, kFontSizes[index]);
    }
    else if (g_dropKind == DROP_ALIGN)
    {
        if (index < 0 || index > 2)
        {
            return;
        }
        Renderer_FreeformSetAlign(g_selected, index);
    }
    HideDrop();
    if (g_canvas != nullptr)
    {
        SetFocus(g_canvas);
    }
    CommitStyleChange();
}

static void LayoutFloatingCanvas()
{
    if (g_stage == nullptr || g_canvas == nullptr)
    {
        return;
    }

    RECT client = {};
    GetClientRect(g_stage, &client);
    int availW = client.right - (EDIT_CANVAS_PAD * 2);
    int availH = client.bottom - (EDIT_CANVAS_PAD * 2);
    if (availW < 40)
    {
        availW = 40;
    }
    if (availH < 24)
    {
        availH = 24;
    }

    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    int width = availW;
    int height = MulDiv(width, slideH, slideW);
    if (height > availH)
    {
        height = availH;
        width = MulDiv(height, slideW, slideH);
    }

    const int slideX = (client.right - width) / 2;
    const int slideY = (client.bottom - height) / 2;
    MoveWindow(g_canvas, slideX, slideY, width, height, TRUE);
    InvalidateRect(g_stage, nullptr, FALSE);
}

// Three panels sit on the base colour: the slide rail, the tools column and
// the stage panel. The stage panel stacks the type bar, the canvas area and
// the status bar.
static void LayoutEdit()
{
    if (g_root == nullptr)
    {
        return;
    }

    RECT client = {};
    GetClientRect(g_root, &client);
    const int width = client.right;
    int panelH = client.bottom - EDIT_MARGIN;
    if (panelH < EDIT_TYPE_BAR_H + EDIT_STATUS_H + 40)
    {
        panelH = EDIT_TYPE_BAR_H + EDIT_STATUS_H + 40;
    }
    int rail = EDIT_RAIL_W;
    if (rail > width / 3)
    {
        rail = width / 3;
    }
    int x = EDIT_MARGIN;
    if (g_list != nullptr)
    {
        MoveWindow(g_list, x, 0, rail, panelH, TRUE);
    }
    x += rail + EDIT_GAP;
    if (g_tools != nullptr)
    {
        MoveWindow(g_tools, x, 0, EDIT_TOOL_W, panelH, TRUE);
    }
    x += EDIT_TOOL_W + EDIT_GAP;

    int side = EDIT_SIDE_W;
    if (side > (width - x) / 2)
    {
        side = (width - x) / 2;
    }
    if (side < 180)
    {
        side = 180;
    }
    int bodyW = width - EDIT_MARGIN - x - side - EDIT_GAP;
    if (bodyW < 80)
    {
        bodyW = 80;
    }
    const int stageH = panelH - EDIT_TYPE_BAR_H - EDIT_STATUS_H;
    if (g_typeBar != nullptr)
    {
        MoveWindow(g_typeBar, x, 0, bodyW, EDIT_TYPE_BAR_H, TRUE);
    }
    if (g_stage != nullptr)
    {
        MoveWindow(g_stage, x, EDIT_TYPE_BAR_H, bodyW, stageH, TRUE);
    }
    if (g_status != nullptr)
    {
        MoveWindow(g_status, x, EDIT_TYPE_BAR_H + stageH, bodyW, EDIT_STATUS_H, TRUE);
    }
    if (g_side != nullptr)
    {
        MoveWindow(g_side, x + bodyW + EDIT_GAP, 0, side, panelH, TRUE);
    }
    LayoutFloatingCanvas();
}

static void SelectEditSlide(int index)
{
    CloseTextEditor(true);
    if (!Renderer_SetEditSlide(index))
    {
        return;
    }
    g_selected = -1;
    g_hover = -1;
    g_drag = EDIT_DRAG_NONE;
    g_tool = EDIT_TOOL_SELECT;
    Renderer_FreeformEndEditorDrag();
    LayoutFloatingCanvas();
    RefreshEditorChrome();
    CommitDocument();
}

static int HitTestList(HWND hwnd, int y)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int row = ThumbHeight(client.right);
    if (row <= 0)
    {
        return -1;
    }
    const int offset = y + g_listScroll - EDIT_RAIL_PAD;
    if (offset < 0)
    {
        return -1;
    }
    const int index = offset / row;
    if (index < 0 || index >= Renderer_SlideCount())
    {
        return -1;
    }
    return index;
}

static int DropFromY(HWND hwnd, int y)
{
    const int count = Renderer_SlideCount();
    if (count <= 1)
    {
        return 0;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    const int rowH = ThumbHeight(client.right);
    if (rowH <= 0)
    {
        return 0;
    }

    int slot = (y + g_listScroll - EDIT_RAIL_PAD + (rowH / 2)) / rowH;
    if (slot < 0)
    {
        slot = 0;
    }
    if (slot > count)
    {
        slot = count;
    }
    return slot;
}

static void DrawEditInsertMark(HDC hdc, int left, int right, int y)
{
    HPEN pen = CreatePen(PS_SOLID, 2, EDIT_COLOR_ACCENT);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, left, y, nullptr);
    LineTo(hdc, right, y);
    MoveToEx(hdc, left, y - 5, nullptr);
    LineTo(hdc, left, y + 5);
    MoveToEx(hdc, right, y - 5, nullptr);
    LineTo(hdc, right, y + 5);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static void FinishSlidePointer(HWND hwnd, bool selectIfClick)
{
    if (g_slideHeld < 0)
    {
        return;
    }

    if (g_slideDragging)
    {
        if (Renderer_MoveSlide(g_slideHeld, g_slideDrop))
        {
            if (Template_IsActive())
            {
                Template_SaveCurrent();
            }
            else
            {
                Document_SaveOrWarn(hwnd);
            }
        }
        RefreshSlideLists();
    }
    else if (selectIfClick)
    {
        SelectEditSlide(g_slideHeld);
    }

    g_slideHeld = -1;
    g_slideDrop = -1;
    g_slideDragging = false;
    InvalidateRect(hwnd, nullptr, FALSE);
}

static LRESULT CALLBACK EditListProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP buffer = CreateCompatibleBitmap(hdc, client.right, client.bottom);
            HBITMAP old = (HBITMAP)SelectObject(mem, buffer);
            EditPanel(mem, &client, 0, 0);

            const int count = Renderer_SlideCount();
            const int rowH = ThumbHeight(client.right);
            const int current = EditSlideIndex();
            HFONT font = EditUiFont(11);
            HFONT oldFont = (HFONT)SelectObject(mem, font);
            SetBkMode(mem, TRANSPARENT);

            // Keep the cards inside the rounded panel while scrolled.
            HRGN clip = CreateRectRgn(0, 4, client.right, client.bottom - 4);
            SelectClipRgn(mem, clip);

            for (int i = 0; i < count; ++i)
            {
                if (g_slideDragging && i == g_slideHeld)
                {
                    continue;
                }

                RECT cell = {
                    EDIT_RAIL_PAD,
                    EDIT_RAIL_PAD + (i * rowH) - g_listScroll,
                    client.right - EDIT_RAIL_PAD,
                    EDIT_RAIL_PAD + ((i + 1) * rowH) - 12 - g_listScroll
                };
                if (cell.bottom < 0 || cell.top > client.bottom)
                {
                    continue;
                }

                RECT thumb = { cell.left, cell.top, cell.right, cell.bottom - EDIT_CHIN };
                RECT chin = { cell.left, thumb.bottom + 3, cell.right, cell.bottom };
                if (i == current)
                {
                    RECT ring = thumb;
                    InflateRect(&ring, 4, 4);
                    EditRound(mem, &ring, 9.0f, CLR_INVALID, EDIT_COLOR_ACCENT, 2.0f);
                }
                if (thumb.right > thumb.left && thumb.bottom > thumb.top)
                {
                    Renderer_BlitSlideAt(
                        mem,
                        i,
                        thumb.left,
                        thumb.top,
                        thumb.right - thumb.left,
                        thumb.bottom - thumb.top
                    );
                    EditMaskCorners(mem, &thumb, 6.0f, EDIT_COLOR_SURFACE);
                }

                char label[128] = {};
                const char* tag = "";
                if (Template_IsActive())
                {
                    Template_CurrentName(label, sizeof(label));
                }
                if (label[0] == '\0')
                {
                    wsprintfA(label, "%d", i + 1);
                    tag = "BG";
                    if (!Renderer_IsBackgroundSlide(i))
                    {
                        tag = "";
                        if (Renderer_SlideUsesTransitionOverride(i))
                        {
                            const int kind = Renderer_GetSlideTransition(i);
                            if (kind != SLIDE_TRANSITION_NONE)
                            {
                                tag = Renderer_TransitionName(kind);
                            }
                        }
                    }
                }
                SetTextColor(mem, EDIT_COLOR_INK_DIM);
                RECT left = chin;
                left.left += 2;
                RECT right = chin;
                right.right -= 2;
                if (tag[0] != '\0')
                {
                    DrawTextA(mem, tag, -1, &right, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    SIZE tagSize = {};
                    GetTextExtentPoint32A(mem, tag, lstrlenA(tag), &tagSize);
                    left.right = right.right - tagSize.cx - 6;
                }
                DrawTextA(mem, label, -1, &left, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }

            if (g_slideDragging)
            {
                int slot = g_slideDrop;
                if (slot < 0)
                {
                    slot = 0;
                }
                if (slot > count)
                {
                    slot = count;
                }
                const int lineY = EDIT_RAIL_PAD + (slot * rowH) - g_listScroll - 6;
                DrawEditInsertMark(mem, EDIT_RAIL_PAD, client.right - EDIT_RAIL_PAD, lineY);
            }

            SelectClipRgn(mem, nullptr);
            DeleteObject(clip);
            SelectObject(mem, oldFont);
            DeleteObject(font);
            BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(buffer);
            DeleteDC(mem);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            const int index = HitTestList(hwnd, GET_Y_LPARAM(lParam));
            if (index >= 0)
            {
                g_slideHeld = index;
                g_slideDrop = index;
                g_slideDownY = GET_Y_LPARAM(lParam);
                g_slideDragging = false;
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            if (g_slideHeld < 0 || (wParam & MK_LBUTTON) == 0)
            {
                break;
            }
            const int y = GET_Y_LPARAM(lParam);
            int travel = y - g_slideDownY;
            if (travel < 0)
            {
                travel = -travel;
            }
            if (!g_slideDragging && travel > 6 && Renderer_SlideCount() > 1)
            {
                g_slideDragging = true;
                SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            if (g_slideDragging)
            {
                const int target = DropFromY(hwnd, y);
                if (target != g_slideDrop)
                {
                    g_slideDrop = target;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP:
            if (g_slideHeld >= 0)
            {
                FinishSlidePointer(hwnd, true);
                ReleaseCapture();
            }
            return 0;

        case WM_RBUTTONUP:
        {
            if (!Template_IsActive())
            {
                break;
            }
            const int index = HitTestList(hwnd, GET_Y_LPARAM(lParam));
            if (index < 0)
            {
                return 0;
            }
            SelectEditSlide(index);
            POINT screen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &screen);
            HMENU menu = CreatePopupMenu();
            AppendMenuA(menu, MF_STRING, IDM_TPL_RENAME, "Rename");
            const int cmd = TrackPopupMenu(
                menu,
                TPM_RETURNCMD | TPM_RIGHTBUTTON,
                screen.x,
                screen.y,
                0,
                hwnd,
                nullptr
            );
            DestroyMenu(menu);
            if (cmd == IDM_TPL_RENAME)
            {
                HWND owner = GetAncestor(hwnd, GA_ROOT);
                char name[128] = {};
                Template_CurrentName(name, sizeof(name));
                if (Template_PromptName(owner, "Rename Template", name, name, sizeof(name)))
                {
                    if (!Template_Rename(name))
                    {
                        MessageBoxA(owner, "Could not rename that template.", "Template Maker", MB_OK | MB_ICONWARNING);
                    }
                    EditView_Refresh();
                }
            }
            return 0;
        }

        case WM_CAPTURECHANGED:
            FinishSlidePointer(hwnd, false);
            return 0;

        case WM_MOUSEWHEEL:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            const int rowH = ThumbHeight(client.right);
            const int content = EDIT_RAIL_PAD + (Renderer_SlideCount() * rowH);
            int next = g_listScroll - GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 48;
            if (next < 0)
            {
                next = 0;
            }
            const int maxPos = content > client.bottom ? content - client.bottom : 0;
            if (next > maxPos)
            {
                next = maxPos;
            }
            g_listScroll = next;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void AddRoundRectUnits(Gdiplus::GraphicsPath* path, float x, float y, float w, float h, float r)
{
    path->StartFigure();
    EditRoundPath(path, x, y, w, h, r);
}

// Tool glyphs use 24-unit coordinates (same as the icon artwork) and are drawn
// 20 px square centred on (cx, cy) with a 1.7-unit stroke.
static void DrawToolIcon(HDC hdc, EditTool tool, int cx, int cy, COLORREF color)
{
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    g.TranslateTransform(cx - 10.0f, cy - 10.0f);
    g.ScaleTransform(20.0f / 24.0f, 20.0f / 24.0f);
    Gdiplus::Pen pen(EditGdiColor(color), 1.7f);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::GraphicsPath path;
    switch (tool)
    {
        case EDIT_TOOL_SELECT:
        {
            Gdiplus::PointF pts[4] = {
                Gdiplus::PointF(4.0f, 3.0f), Gdiplus::PointF(11.0f, 20.0f),
                Gdiplus::PointF(13.5f, 12.5f), Gdiplus::PointF(21.0f, 10.0f)
            };
            path.AddPolygon(pts, 4);
            break;
        }
        case EDIT_TOOL_TEXT:
            path.StartFigure();
            path.AddLine(5.0f, 5.0f, 19.0f, 5.0f);
            path.StartFigure();
            path.AddLine(12.0f, 5.0f, 12.0f, 19.0f);
            path.StartFigure();
            path.AddLine(9.0f, 19.0f, 15.0f, 19.0f);
            break;
        case EDIT_TOOL_PHOTO:
        {
            AddRoundRectUnits(&path, 3.0f, 4.0f, 18.0f, 16.0f, 3.0f);
            path.StartFigure();
            path.AddEllipse(7.0f, 8.0f, 4.0f, 4.0f);
            Gdiplus::PointF pts[3] = {
                Gdiplus::PointF(21.0f, 16.0f), Gdiplus::PointF(16.0f, 11.0f), Gdiplus::PointF(7.0f, 20.0f)
            };
            path.StartFigure();
            path.AddLines(pts, 3);
            break;
        }
        case EDIT_TOOL_VIDEO:
        {
            AddRoundRectUnits(&path, 2.5f, 5.0f, 14.0f, 14.0f, 3.0f);
            Gdiplus::PointF pts[4] = {
                Gdiplus::PointF(16.5f, 10.0f), Gdiplus::PointF(21.5f, 7.0f),
                Gdiplus::PointF(21.5f, 17.0f), Gdiplus::PointF(16.5f, 14.0f)
            };
            path.StartFigure();
            path.AddLines(pts, 4);
            break;
        }
        case EDIT_TOOL_RECT:
            AddRoundRectUnits(&path, 4.0f, 6.0f, 16.0f, 12.0f, 2.0f);
            break;
        case EDIT_TOOL_ELLIPSE:
            path.AddEllipse(3.0f, 5.0f, 18.0f, 14.0f);
            break;
        default:
            break;
    }
    g.DrawPath(&pen, &path);
}

static int ToolCell(RECT* client, int index, RECT* out)
{
    int y = 8;
    for (int i = 0; i < index; ++i)
    {
        y += EDIT_TOOL_CELL + EDIT_TOOL_GAP;
        if (kToolRuleAfter[i])
        {
            y += EDIT_TOOL_RULE + EDIT_TOOL_GAP;
        }
    }
    out->left = (client->right - EDIT_TOOL_CELL) / 2;
    out->top = y;
    out->right = out->left + EDIT_TOOL_CELL;
    out->bottom = out->top + EDIT_TOOL_CELL;
    return EDIT_TOOL_CELL;
}

static int HitTestTool(HWND hwnd, int y)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    for (int i = 0; i < EDIT_TOOL_COUNT; ++i)
    {
        RECT cell = {};
        ToolCell(&client, i, &cell);
        if (y >= cell.top && y < cell.bottom)
        {
            return i;
        }
    }
    return -1;
}

static void InsertCenteredMedia(bool video)
{
    wchar_t path[MAX_PATH] = {};
    const bool picked = video ? PickVideo(g_root, path, MAX_PATH) : PickPicture(g_root, path, MAX_PATH);
    if (!picked)
    {
        g_tool = EDIT_TOOL_SELECT;
        RefreshEditorChrome();
        return;
    }

    wchar_t asset[MAX_PATH] = {};
    if (!Document_ImportAsset(path, asset, MAX_PATH))
    {
        g_tool = EDIT_TOOL_SELECT;
        RefreshEditorChrome();
        return;
    }

    int slideW = 1920;
    int slideH = 1080;
    GetSlideSize(&slideW, &slideH);
    const int w = video ? slideW / 2 : slideW / 3;
    const int h = video ? slideH / 2 : slideH / 3;
    const int x = (slideW - w) / 2;
    const int y = (slideH - h) / 2;
    CloseTextEditor(true);
    History_Remember();
    if (video)
    {
        g_selected = Renderer_FreeformAddVideo(x, y, w, h, asset);
    }
    else
    {
        g_selected = Renderer_FreeformAddImage(x, y, w, h, asset);
    }
    g_tool = EDIT_TOOL_SELECT;
    CommitDocument();
    RefreshEditorChrome();
}

static LRESULT CALLBACK EditToolsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP buffer = CreateCompatibleBitmap(hdc, client.right, client.bottom);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(mem, buffer);
            EditPanel(mem, &client, 0, 0);

            for (int i = 0; i < EDIT_TOOL_COUNT; ++i)
            {
                RECT cell = {};
                ToolCell(&client, i, &cell);
                const bool armed = g_tool == kToolOrder[i];
                if (armed)
                {
                    EditRound(mem, &cell, 8.0f, EDIT_COLOR_RAISED_HI, EDIT_COLOR_ACCENT, 1.5f);
                }
                const int cx = (cell.left + cell.right) / 2;
                const int cy = (cell.top + cell.bottom) / 2;
                DrawToolIcon(mem, kToolOrder[i], cx, cy, armed ? EDIT_COLOR_INK_BRIGHT : EDIT_COLOR_INK);
                if (kToolRuleAfter[i])
                {
                    const int ruleY = cell.bottom + EDIT_TOOL_GAP + (EDIT_TOOL_RULE / 2);
                    EditLine(mem, cx - 13, ruleY, cx + 13, ruleY, EDIT_COLOR_LINE);
                }
            }
            BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBitmap);
            DeleteObject(buffer);
            DeleteDC(mem);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONUP:
        {
            const int index = HitTestTool(hwnd, GET_Y_LPARAM(lParam));
            if (index < 0)
            {
                return 0;
            }
            const EditTool tool = kToolOrder[index];
            if (tool == EDIT_TOOL_PHOTO)
            {
                InsertCenteredMedia(false);
                return 0;
            }
            if (tool == EDIT_TOOL_VIDEO)
            {
                InsertCenteredMedia(true);
                return 0;
            }
            g_tool = (g_tool == tool) ? EDIT_TOOL_SELECT : tool;
            InvalidateRect(hwnd, nullptr, FALSE);
            if (g_status != nullptr)
            {
                InvalidateRect(g_status, nullptr, FALSE);
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EditTypeBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC hdc = CreateCompatibleDC(target);
            HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
            HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(hdc, buffer));
            EditPanel(hdc, &client, 0, 20);
            EditLine(hdc, 0, client.bottom - 1, client.right, client.bottom - 1, EDIT_COLOR_LINE);

            char fontName[64] = "Segoe UI";
            int fontSize = 64;
            COLORREF color = RGB(255, 255, 255);
            bool bold = false;
            bool italic = false;
            bool underline = false;
            const bool enabled = SelectedIsText() && Renderer_FreeformGetStyle(
                g_selected,
                fontName,
                sizeof(fontName),
                &fontSize,
                &color,
                &bold,
                &italic,
                &underline
            );
            if (!enabled)
            {
                bold = false;
                italic = false;
                underline = false;
            }

            HFONT font = EditUiFont(13);
            HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, font));
            SetBkMode(hdc, TRANSPARENT);
            const COLORREF ink = enabled ? EDIT_COLOR_INK : EDIT_COLOR_INK_FAINT;
            const COLORREF value = enabled ? EDIT_COLOR_INK_BRIGHT : EDIT_COLOR_INK_FAINT;

            for (int i = 1; i < TYPE_WELL_COUNT; ++i)
            {
                if (!TypeRuleBefore(kTypeOrder[i]))
                {
                    continue;
                }
                RECT well = {};
                TypeWell(kTypeOrder[i], &client, &well);
                const int ruleX = well.left - TYPE_GAP - (TYPE_RULE / 2) - 1;
                const int mid = (client.bottom - 1) / 2;
                EditLine(hdc, ruleX, mid - 11, ruleX, mid + 11, EDIT_COLOR_LINE);
            }

            RECT well = {};
            TypeWell(0, &client, &well);
            EditRound(hdc, &well, 6.0f, g_dropKind == DROP_FONT ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED,
                      EDIT_COLOR_EDGE);
            RECT label = well;
            label.left += 10;
            label.right -= 26;
            SetTextColor(hdc, value);
            DrawTextA(hdc, fontName, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            EditChevron(hdc, well.right - 14, (well.top + well.bottom) / 2, ink);

            TypeWell(1, &client, &well);
            EditRound(hdc, &well, 6.0f, g_dropKind == DROP_SIZE ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED,
                      EDIT_COLOR_EDGE);
            char sizeText[16] = {};
            wsprintfA(sizeText, "%d", fontSize);
            label = well;
            label.left += 10;
            label.right -= 26;
            DrawTextA(hdc, sizeText, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            EditChevron(hdc, well.right - 14, (well.top + well.bottom) / 2, ink);

            const bool armed[3] = { bold, italic, underline };
            for (int i = 0; i < 3; ++i)
            {
                TypeWell(4 + i, &client, &well);
                if (armed[i])
                {
                    EditRound(hdc, &well, 6.0f, EDIT_COLOR_RAISED_HI);
                }
                HFONT letterFont = EditUiFont(14, i == 0 ? FW_BOLD : FW_NORMAL, i == 1, i == 2);
                HFONT prior = static_cast<HFONT>(SelectObject(hdc, letterFont));
                SetTextColor(hdc, armed[i] ? EDIT_COLOR_INK_BRIGHT : ink);
                const char* letters[3] = { "B", "I", "U" };
                DrawTextA(hdc, letters[i], -1, &well, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, prior);
                DeleteObject(letterFont);
            }

            TypeWell(3, &client, &well);
            EditRound(hdc, &well, 6.0f, EDIT_COLOR_SUNKEN, EDIT_COLOR_FIELD_EDGE);
            const int align = enabled ? Renderer_FreeformGetAlign(g_selected) : -1;
            for (int part = 0; part < 3; ++part)
            {
                RECT seg = {};
                TypeAlignPart(&well, part, &seg);
                if (part == align)
                {
                    EditRound(hdc, &seg, 4.0f, EDIT_COLOR_RAISED_HI);
                }
                SetTextColor(hdc, part == align ? EDIT_COLOR_INK_BRIGHT : (enabled ? EDIT_COLOR_INK_DIM : ink));
                DrawTextA(hdc, kAlignNames[part], -1, &seg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            TypeWell(2, &client, &well);
            EditRound(hdc, &well, 6.0f, g_dropKind == DROP_COLOR ? EDIT_COLOR_RAISED : EDIT_COLOR_SUNKEN,
                      EDIT_COLOR_FIELD_EDGE);
            const int mid = (well.top + well.bottom) / 2;
            RECT swatch = { well.left + 6, mid - 9, well.left + 24, mid + 9 };
            if (enabled)
            {
                EditRound(hdc, &swatch, 4.0f, color);
            }
            else
            {
                EditRound(hdc, &swatch, 4.0f, CLR_INVALID, EDIT_COLOR_INK_FAINT);
            }
            char hex[16] = "#------";
            if (enabled)
            {
                wsprintfA(hex, "#%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));
            }
            label = well;
            label.left = swatch.right + 8;
            SetTextColor(hdc, enabled ? EDIT_COLOR_INK : ink);
            DrawTextA(hdc, hex, -1, &label, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            SelectObject(hdc, oldFont);
            DeleteObject(font);
            BitBlt(target, 0, 0, client.right, client.bottom, hdc, 0, 0, SRCCOPY);
            SelectObject(hdc, oldBitmap);
            DeleteObject(buffer);
            DeleteDC(hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            if (!SelectedIsText())
            {
                HideDrop();
                return 0;
            }
            const int well = HitTypeWell(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            RECT client = {};
            GetClientRect(hwnd, &client);
            RECT cell = {};
            if (well == 0)
            {
                TypeWell(0, &client, &cell);
                ShowDrop(DROP_FONT, hwnd, &cell);
            }
            else if (well == 1)
            {
                TypeWell(1, &client, &cell);
                ShowDrop(DROP_SIZE, hwnd, &cell);
            }
            else if (well == 2)
            {
                TypeWell(2, &client, &cell);
                ShowDrop(DROP_COLOR, hwnd, &cell);
            }
            else if (well == 3)
            {
                HideDrop();
                TypeWell(3, &client, &cell);
                const int part = (GET_X_LPARAM(lParam) - cell.left - 3) / TYPE_ALIGN_PART_W;
                if (part >= 0 && part <= 2 && part != Renderer_FreeformGetAlign(g_selected))
                {
                    RememberStyleChange();
                    Renderer_FreeformSetAlign(g_selected, part);
                    CommitStyleChange();
                }
                if (g_canvas != nullptr)
                {
                    SetFocus(g_canvas);
                }
            }
            else if (well >= 4 && well <= 6)
            {
                HideDrop();
                ToggleFontFlag(well - 4);
                if (g_canvas != nullptr)
                {
                    SetFocus(g_canvas);
                }
            }
            return 0;
        }

        case WM_CHAR:
        case WM_KEYDOWN:
        case WM_KEYUP:
            if (g_canvas != nullptr)
            {
                return SendMessageA(g_canvas, uMsg, wParam, lParam);
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EditDropProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (client.right <= 0 || client.bottom <= 0)
            {
                EndPaint(hwnd, &paint);
                return 0;
            }

            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP buffer = CreateCompatibleBitmap(hdc, client.right, client.bottom);
            HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(mem, buffer));
            HBRUSH fill = CreateSolidBrush(EDIT_COLOR_FRAME);
            FillRect(mem, &client, fill);
            DeleteObject(fill);

            HFONT font = EditUiFont(12);
            HFONT oldFont = static_cast<HFONT>(SelectObject(mem, font));
            SetBkMode(mem, TRANSPARENT);

            if (g_dropKind == DROP_COLOR)
            {
                PaintColorPicker(mem);
            }
            else
            {
                char currentFont[64] = {};
                int currentSize = 0;
                Renderer_FreeformGetStyle(
                    g_selected,
                    currentFont,
                    sizeof(currentFont),
                    &currentSize,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                );
                const int count = (g_dropKind == DROP_FONT)
                    ? g_fontCount
                    : (g_dropKind == DROP_ALIGN ? 3 : kFontSizeCount);
                const int first = g_dropScrollY / DROP_ITEM_H;
                int last = (g_dropScrollY + client.bottom) / DROP_ITEM_H;
                if (last >= count)
                {
                    last = count - 1;
                }
                for (int i = first; i <= last && i >= 0; ++i)
                {
                    RECT row = {
                        0,
                        (i * DROP_ITEM_H) - g_dropScrollY,
                        client.right,
                        ((i + 1) * DROP_ITEM_H) - g_dropScrollY
                    };
                    bool current = false;
                    char line[64] = {};
                    if (g_dropKind == DROP_FONT)
                    {
                        lstrcpynA(line, g_fonts[i], sizeof(line));
                        current = lstrcmpiA(line, currentFont) == 0;
                    }
                    else if (g_dropKind == DROP_ALIGN)
                    {
                        lstrcpynA(line, kAlignNames[i], sizeof(line));
                        current = i == Renderer_FreeformGetAlign(g_selected);
                    }
                    else
                    {
                        wsprintfA(line, "%d", kFontSizes[i]);
                        current = kFontSizes[i] == currentSize;
                    }
                    if (current)
                    {
                        HBRUSH well = CreateSolidBrush(EDIT_COLOR_RAISED_HI);
                        FillRect(mem, &row, well);
                        DeleteObject(well);
                    }
                    row.left += 8;
                    row.right -= 4;
                    SetTextColor(mem, current ? EDIT_COLOR_INK_BRIGHT : EDIT_COLOR_INK);
                    DrawTextA(mem, line, -1, &row, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
            }

            SelectObject(mem, oldFont);
            DeleteObject(font);
            BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(buffer);
            DeleteDC(mem);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (g_dropKind == DROP_COLOR)
            {
                const int swatch = PickHitCell(x, y, PICK_PAD, kSwatchCount < PICK_COLS * 2 ? kSwatchCount : PICK_COLS * 2);
                const int recent = PickHitCell(x, y, PICK_RECENT_Y, g_pickRecentCount);
                const POINT point = { x, y };
                RECT sv = {};
                RECT hue = {};
                RECT hexWell = {};
                PickSvRect(&sv);
                PickHueRect(&hue);
                PickHexRect(&hexWell);
                InflateRect(&hue, 3, 0);
                if (swatch >= 0)
                {
                    PickApplyAndClose(kSwatches[swatch]);
                    return 0;
                }
                if (recent >= 0)
                {
                    PickApplyAndClose(g_pickRecent[recent]);
                    return 0;
                }
                g_hexEditing = false;
                if (PtInRect(&sv, point) || PtInRect(&hue, point))
                {
                    if (!g_pickChanged)
                    {
                        RememberStyleChange();
                        g_pickChanged = true;
                    }
                    g_pickDrag = PtInRect(&sv, point) ? PICK_DRAG_SV : PICK_DRAG_HUE;
                    if (g_pickDrag == PICK_DRAG_HUE && (g_pickS < 0.05f || g_pickV < 0.05f))
                    {
                        g_pickS = 1.0f;
                        g_pickV = 1.0f;
                    }
                    SetCapture(hwnd);
                    PickTrack(x, y);
                }
                else if (PtInRect(&hexWell, point))
                {
                    PickBeginHexEdit(true);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else
            {
                const int index = (y + g_dropScrollY) / DROP_ITEM_H;
                ApplyDropIndex(index);
            }
            return 0;
        }

        case WM_MOUSEMOVE:
            if (g_pickDrag != PICK_DRAG_NONE)
            {
                PickTrack(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_pickDrag != PICK_DRAG_NONE)
            {
                PickTrack(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                ReleaseCapture();
                PickFinishDrag();
            }
            return 0;

        case WM_CAPTURECHANGED:
            PickFinishDrag();
            return 0;

        case WM_CHAR:
            if (g_dropKind == DROP_COLOR)
            {
                PickHexChar(wParam);
            }
            else if (wParam == VK_ESCAPE)
            {
                HideDrop();
                if (g_canvas != nullptr)
                {
                    SetFocus(g_canvas);
                }
            }
            return 0;

        case WM_VSCROLL:
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            int pos = g_dropScrollY;
            const int page = client.bottom > 0 ? client.bottom : DROP_ITEM_H;
            switch (LOWORD(wParam))
            {
                case SB_LINEUP:
                    pos -= DROP_ITEM_H;
                    break;
                case SB_LINEDOWN:
                    pos += DROP_ITEM_H;
                    break;
                case SB_PAGEUP:
                    pos -= page;
                    break;
                case SB_PAGEDOWN:
                    pos += page;
                    break;
                case SB_TOP:
                    pos = 0;
                    break;
                case SB_BOTTOM:
                    pos = DropContentHeight();
                    break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION:
                {
                    SCROLLINFO info = {};
                    info.cbSize = sizeof(info);
                    info.fMask = SIF_TRACKPOS;
                    GetScrollInfo(hwnd, SB_VERT, &info);
                    pos = info.nTrackPos;
                    break;
                }
                default:
                    break;
            }
            g_dropScrollY = pos;
            SyncDropScroll(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL:
            if (g_dropKind == DROP_FONT || g_dropKind == DROP_SIZE || g_dropKind == DROP_ALIGN)
            {
                UINT lines = 3;
                SystemParametersInfoA(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
                const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int step = DROP_ITEM_H * 3;
                if (lines == WHEEL_PAGESCROLL)
                {
                    RECT client = {};
                    GetClientRect(hwnd, &client);
                    step = client.bottom;
                }
                else if (lines > 0)
                {
                    step = DROP_ITEM_H * static_cast<int>(lines);
                }
                g_dropScrollY -= MulDiv(delta, step, WHEEL_DELTA);
                SyncDropScroll(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_KILLFOCUS:
            HideDrop();
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EditStatusProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC hdc = CreateCompatibleDC(target);
            HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
            HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(hdc, buffer));
            EditPanel(hdc, &client, 20, 0);
            EditLine(hdc, 0, 0, client.right, 0, EDIT_COLOR_LINE);

            int slideW = 1920;
            int slideH = 1080;
            GetSlideSize(&slideW, &slideH);

            static const char* kKindNames[5] = { "Text box", "Rectangle", "Ellipse", "Image", "Video" };
            char left[96] = {};
            char right[96] = {};
            int kind = -1;
            int x = 0;
            int y = 0;
            int w = 0;
            int h = 0;
            const bool hasSelection = g_selected >= 0 && Renderer_FreeformGet(g_selected, &kind, &x, &y, &w, &h);
            if (Template_IsActive())
            {
                lstrcpynA(left, "Template", sizeof(left));
            }
            else
            {
                wsprintfA(left, "Slide %d of %d", EditSlideIndex() + 1, Renderer_SlideCount());
            }
            if (hasSelection)
            {
                const char* name = kind >= 0 && kind < 5 ? kKindNames[kind] : "Object";
                const int used = lstrlenA(left);
                wsprintfA(left + used, " \xB7 %s selected", name);
                wsprintfA(right, "X %d   Y %d   W %d   H %d", x, y, w, h);
            }
            else if (g_mouseOnSlide)
            {
                wsprintfA(right, "%d x %d   \xB7   Mouse %d, %d", slideW, slideH, g_mouseSlideX, g_mouseSlideY);
            }
            else
            {
                wsprintfA(right, "%d x %d", slideW, slideH);
            }

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, EDIT_COLOR_INK_DIM);
            HFONT font = EditUiFont(12);
            HFONT old = (HFONT)SelectObject(hdc, font);
            RECT text = client;
            text.left += 12;
            text.right -= 12;
            DrawTextA(hdc, right, -1, &text, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SIZE rightSize = {};
            GetTextExtentPoint32A(hdc, right, lstrlenA(right), &rightSize);
            text.right -= rightSize.cx + 16;
            DrawTextA(hdc, left, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            SelectObject(hdc, old);
            DeleteObject(font);
            BitBlt(target, 0, 0, client.right, client.bottom, hdc, 0, 0, SRCCOPY);
            SelectObject(hdc, oldBitmap);
            DeleteObject(buffer);
            DeleteDC(hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static void DrawObjectOutline(HDC hdc, HWND canvas, int index, COLORREF color, float thickness)
{
    const ResizeHandle corners[4] = { HANDLE_TL, HANDLE_TR, HANDLE_BR, HANDLE_BL };
    Gdiplus::PointF pts[4] = {};
    for (int i = 0; i < 4; ++i)
    {
        int hx = 0;
        int hy = 0;
        HandleClient(canvas, index, corners[i], &hx, &hy);
        pts[i] = Gdiplus::PointF(static_cast<float>(hx), static_cast<float>(hy));
    }
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(EditGdiColor(color), thickness);
    g.DrawPolygon(&pen, pts, 4);
}

static void DrawGrip(HDC hdc, int cx, int cy)
{
    RECT cell = {};
    GripRect(cx, cy, &cell);
    EditRound(hdc, &cell, 3.0f, EDIT_COLOR_ACCENT, RGB(0x22, 0x22, 0x22), 1.5f);
}

static void DrawHandles(HDC hdc, HWND canvas)
{
    if (g_hover >= 0 && g_hover != g_selected && g_drag == EDIT_DRAG_NONE)
    {
        DrawObjectOutline(hdc, canvas, g_hover, RGB(220, 230, 255), 2.0f);
    }
    if (g_selected < 0 || Renderer_FreeformIsLocked(g_selected))
    {
        return;
    }

    DrawObjectOutline(hdc, canvas, g_selected, EDIT_COLOR_ACCENT, 1.5f);

    const ResizeHandle grips[8] = {
        HANDLE_T, HANDLE_B, HANDLE_L, HANDLE_R,
        HANDLE_TL, HANDLE_TR, HANDLE_BL, HANDLE_BR
    };
    for (int i = 0; i < 8; ++i)
    {
        int hx = 0;
        int hy = 0;
        HandleClient(canvas, g_selected, grips[i], &hx, &hy);
        DrawGrip(hdc, hx, hy);
    }

    int rx = 0;
    int ry = 0;
    HandleClient(canvas, g_selected, HANDLE_ROTATE, &rx, &ry);
    int trx = 0;
    int try_ = 0;
    HandleClient(canvas, g_selected, HANDLE_TR, &trx, &try_);
    HPEN line = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, line));
    MoveToEx(hdc, trx, try_, nullptr);
    LineTo(hdc, rx, ry);
    SelectObject(hdc, oldPen);
    DeleteObject(line);
    DrawGrip(hdc, rx, ry);
}

static LRESULT CALLBACK EditCanvasProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS | DLGC_WANTALLKEYS;

        case WM_SETCURSOR:
        {
            if (g_drag == EDIT_DRAG_RESIZE || g_drag == EDIT_DRAG_ROTATE)
            {
                SetSizeCursor(g_resizeHandle);
                return TRUE;
            }
            if (g_drag == EDIT_DRAG_MOVE)
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                return TRUE;
            }
            POINT pt = {};
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (g_selected >= 0)
            {
                const ResizeHandle handle = HitHandle(hwnd, pt.x, pt.y);
                if (handle != HANDLE_NONE)
                {
                    SetSizeCursor(handle);
                    return TRUE;
                }
                if (HitSelectionFrame(hwnd, pt.x, pt.y))
                {
                    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                    return TRUE;
                }
                if (g_textOpen && HitSelectedInterior(hwnd, pt.x, pt.y))
                {
                    SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                    return TRUE;
                }
            }
            if (g_hover >= 0 && HitObjectBorder(hwnd, g_hover, pt.x, pt.y))
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
                return TRUE;
            }
            break;
        }

        case WM_SIZE:
            if (g_drag == EDIT_DRAG_MOVE || g_drag == EDIT_DRAG_RESIZE)
            {
                SyncEditorCache(hwnd, g_selected);
            }
            else if (g_drag == EDIT_DRAG_CREATE)
            {
                SyncEditorCache(hwnd, -1);
            }
            return 0;

        case WM_TIMER:
            if (wParam == EDIT_CARET_TIMER && g_textOpen)
            {
                g_caretOn = !g_caretOn;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (client.right <= 0 || client.bottom <= 0)
            {
                EndPaint(hwnd, &paint);
                return 0;
            }

            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP buffer = CreateCompatibleBitmap(hdc, client.right, client.bottom);
            HBITMAP old = (HBITMAP)SelectObject(mem, buffer);
            HBRUSH backdrop = CreateSolidBrush(RGB(40, 40, 44));
            FillRect(mem, &client, backdrop);
            DeleteObject(backdrop);

            RECT view = {};
            GetCanvasView(hwnd, &view);
            if (g_drag == EDIT_DRAG_MOVE || g_drag == EDIT_DRAG_RESIZE)
            {
            Renderer_FreeformBlitEditor(
                mem,
                view.left,
                view.top,
                view.right - view.left,
                view.bottom - view.top,
                    g_selected
                );
            }
            else if (g_drag == EDIT_DRAG_CREATE)
            {
                Renderer_FreeformBlitEditor(
                    mem,
                    view.left,
                    view.top,
                    view.right - view.left,
                    view.bottom - view.top,
                    -1
                );
            }
            else
            {
                Renderer_FreeformBlitLive(
                    mem,
                    view.left,
                    view.top,
                    view.right - view.left,
                    view.bottom - view.top
                );
            }

            if (g_drag == EDIT_DRAG_CREATE)
            {
                int left = 0;
                int top = 0;
                int right = 0;
                int bottom = 0;
                SlideToClient(hwnd, g_dragStartX, g_dragStartY, &left, &top);
                SlideToClient(hwnd, g_nowX, g_nowY, &right, &bottom);
                if (right < left)
                {
                    const int swap = left;
                    left = right;
                    right = swap;
                }
                if (bottom < top)
                {
                    const int swap = top;
                    top = bottom;
                    bottom = swap;
                }
                HBRUSH oldBrush = (HBRUSH)SelectObject(mem, GetStockObject(NULL_BRUSH));
                HPEN pen = CreatePen(PS_DOT, 1, RGB(255, 255, 255));
                HPEN oldPen = (HPEN)SelectObject(mem, pen);
                Rectangle(mem, left, top, right, bottom);
                SelectObject(mem, oldPen);
                SelectObject(mem, oldBrush);
                DeleteObject(pen);
            }
            DrawHandles(mem, hwnd);
            DrawSnapGuides(mem, hwnd);
            if (g_selected >= 0 && (g_drag == EDIT_DRAG_NONE || g_drag == EDIT_DRAG_TEXTSEL))
            {
                DrawTextSelection(mem, hwnd);
                DrawTextCaret(mem, hwnd);
            }

            BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(buffer);
            DeleteDC(mem);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_LBUTTONDOWN:
        {
            HideDrop();
            SetFocus(hwnd);
            const int clientX = GET_X_LPARAM(lParam);
            const int clientY = GET_Y_LPARAM(lParam);
            int slideX = 0;
            int slideY = 0;
            if (!ClientToSlide(hwnd, clientX, clientY, &slideX, &slideY))
            {
                return 0;
            }

            if (g_tool == EDIT_TOOL_SELECT)
            {
                const ResizeHandle handle = HitHandle(hwnd, clientX, clientY);
                if (handle == HANDLE_ROTATE)
                {
                    CloseTextEditor(true);
                    Renderer_FreeformGet(g_selected, nullptr, &g_origX, &g_origY, &g_origW, &g_origH);
                    g_origRotation = Renderer_FreeformGetRotation(g_selected);
                    float cx = 0.0f;
                    float cy = 0.0f;
                    ObjectCenter(g_selected, &cx, &cy);
                    g_rotateStart = atan2f(static_cast<float>(slideY) - cy, static_cast<float>(slideX) - cx) * 180.0f / 3.14159265f;
                    BeginMutatingDrag(EDIT_DRAG_ROTATE);
                    g_resizeHandle = HANDLE_ROTATE;
                    g_dragStartX = slideX;
                    g_dragStartY = slideY;
                    SetCapture(hwnd);
                    SetSizeCursor(HANDLE_ROTATE);
                    return 0;
                }
                if (handle != HANDLE_NONE)
                {
                    CloseTextEditor(true);
                    Renderer_FreeformGet(g_selected, nullptr, &g_origX, &g_origY, &g_origW, &g_origH);
                    g_origRotation = Renderer_FreeformGetRotation(g_selected);
                    BeginMutatingDrag(EDIT_DRAG_RESIZE);
                    g_resizeHandle = handle;
                    g_dragStartX = slideX;
                    g_dragStartY = slideY;
                    SyncEditorCache(hwnd, g_selected);
                    SetCapture(hwnd);
                    SetSizeCursor(handle);
                    return 0;
                }

                if (HitSelectionFrame(hwnd, clientX, clientY))
                {
                    CloseTextEditor(true);
                    Renderer_FreeformGet(g_selected, nullptr, &g_origX, &g_origY, &g_origW, &g_origH);
                    g_origRotation = Renderer_FreeformGetRotation(g_selected);
                    BeginMutatingDrag(EDIT_DRAG_MOVE);
                    g_dragStartX = slideX;
                    g_dragStartY = slideY;
                    SyncEditorCache(hwnd, g_selected);
                    SetCapture(hwnd);
                return 0;
            }

                if (g_textOpen && HitSelectedInterior(hwnd, clientX, clientY))
                {
                    const bool extend = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    PlaceCaretFromClick(hwnd, clientX, clientY, extend);
                    g_drag = EDIT_DRAG_TEXTSEL;
                    SetCapture(hwnd);
                    return 0;
                }

                const int pad = ClientPadToSlide(hwnd, EDIT_HOVER_PAD);
                const int borderHit = Renderer_FreeformHitTestPad(slideX, slideY, pad);
                if (borderHit >= 0 && HitObjectBorder(hwnd, borderHit, clientX, clientY))
                {
                    CloseTextEditor(true);
                    g_selected = borderHit;
                    Renderer_FreeformGet(g_selected, nullptr, &g_origX, &g_origY, &g_origW, &g_origH);
                    g_origRotation = Renderer_FreeformGetRotation(g_selected);
                    BeginMutatingDrag(EDIT_DRAG_MOVE);
                    g_dragStartX = slideX;
                    g_dragStartY = slideY;
                    SyncEditorCache(hwnd, g_selected);
                    SetCapture(hwnd);
                    RefreshEditorChrome();
                return 0;
            }

                const int hit = Renderer_FreeformHitTest(slideX, slideY);
                const bool clickToEdit = hit >= 0 && hit == g_selected && SelectedIsText();
                SelectObjectAt(hit);
                if (g_selected >= 0)
                {
                    Renderer_FreeformGet(g_selected, nullptr, &g_origX, &g_origY, &g_origW, &g_origH);
                    g_origRotation = Renderer_FreeformGetRotation(g_selected);
                    BeginMutatingDrag(EDIT_DRAG_MOVE);
                    g_dragStartX = slideX;
                    g_dragStartY = slideY;
                    g_textClickPending = clickToEdit;
                    g_textClickX = clientX;
                    g_textClickY = clientY;
                    SyncEditorCache(hwnd, g_selected);
                    SetCapture(hwnd);
                }
                return 0;
            }

            if (g_tool == EDIT_TOOL_TEXT)
            {
                CloseTextEditor(true);
                g_selected = Renderer_FreeformAddText(slideX, slideY);
                g_tool = EDIT_TOOL_SELECT;
                CommitDocument();
                OpenTextEditor();
                RefreshEditorChrome();
                return 0;
            }

            BeginMutatingDrag(EDIT_DRAG_CREATE);
            g_dragStartX = slideX;
            g_dragStartY = slideY;
            g_nowX = slideX;
            g_nowY = slideY;
            SyncEditorCache(hwnd, -1);
            SetCapture(hwnd);
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            TRACKMOUSEEVENT track = {};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = hwnd;
            TrackMouseEvent(&track);

            int slideX = 0;
            int slideY = 0;
            g_mouseOnSlide = ClientToSlide(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &slideX, &slideY);
            if (g_mouseOnSlide)
            {
                g_mouseSlideX = slideX;
                g_mouseSlideY = slideY;
            }
            RefreshStatus();

            if (g_drag == EDIT_DRAG_NONE)
            {
                int nextHover = -1;
                if (g_mouseOnSlide)
                {
                    const int pad = ClientPadToSlide(hwnd, EDIT_HOVER_PAD);
                    nextHover = Renderer_FreeformHitTestPad(slideX, slideY, pad);
                }
                if (nextHover != g_hover)
                {
                    g_hover = nextHover;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }

            if (g_drag == EDIT_DRAG_TEXTSEL && (wParam & MK_LBUTTON) != 0)
            {
                PlaceCaretFromClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), true);
                return 0;
            }

            if (g_drag == EDIT_DRAG_NONE || (wParam & MK_LBUTTON) == 0)
            {
                return 0;
            }
            ClientToSlideClamped(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &slideX, &slideY);
            g_nowX = slideX;
            g_nowY = slideY;
            if (g_drag == EDIT_DRAG_MOVE && g_selected >= 0)
            {
                if (g_textClickPending)
                {
                    if (abs(GET_X_LPARAM(lParam) - g_textClickX) <= EDIT_CLICK_SLOP &&
                        abs(GET_Y_LPARAM(lParam) - g_textClickY) <= EDIT_CLICK_SLOP)
                    {
                        return 0;
                    }
                    g_textClickPending = false;
                }
                int x = g_origX + (slideX - g_dragStartX);
                int y = g_origY + (slideY - g_dragStartY);
                ApplyCenterSnap(&x, &y, g_origW, g_origH);
                Renderer_FreeformSetFrame(g_selected, x, y, g_origW, g_origH);
            }
            else if (g_drag == EDIT_DRAG_RESIZE && g_selected >= 0)
            {
                ApplyResize(slideX, slideY);
            }
            else if (g_drag == EDIT_DRAG_ROTATE && g_selected >= 0)
            {
                float cx = 0.0f;
                float cy = 0.0f;
                ObjectCenter(g_selected, &cx, &cy);
                const float angle = atan2f(static_cast<float>(slideY) - cy, static_cast<float>(slideX) - cx) * 180.0f / 3.14159265f;
                Renderer_FreeformSetRotation(g_selected, g_origRotation + (angle - g_rotateStart));
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            RefreshStatus();
            return 0;
        }

        case WM_MOUSELEAVE:
            g_mouseOnSlide = false;
            if (g_hover != -1)
            {
                g_hover = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            RefreshStatus();
            return 0;

        case WM_LBUTTONUP:
        {
            if (g_drag == EDIT_DRAG_TEXTSEL)
            {
                g_drag = EDIT_DRAG_NONE;
                ReleaseCapture();
                return 0;
            }
            if (g_drag == EDIT_DRAG_CREATE)
            {
                Renderer_FreeformEndEditorDrag();
                int slideX = 0;
                int slideY = 0;
                if (!ClientToSlide(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &slideX, &slideY))
                {
                    slideX = g_dragStartX + 240;
                    slideY = g_dragStartY + 160;
                }
                const int w = slideX - g_dragStartX;
                const int h = slideY - g_dragStartY;
                if (g_tool == EDIT_TOOL_ELLIPSE)
                {
                    g_selected = Renderer_FreeformAddEllipse(g_dragStartX, g_dragStartY, w, h);
                }
                else
                {
                    g_selected = Renderer_FreeformAddRect(g_dragStartX, g_dragStartY, w, h);
                }
                g_tool = EDIT_TOOL_SELECT;
            }
            const bool openText = g_drag == EDIT_DRAG_MOVE && g_textClickPending && SelectedIsText();
            g_textClickPending = false;
            if (g_drag == EDIT_DRAG_MOVE || g_drag == EDIT_DRAG_RESIZE || g_drag == EDIT_DRAG_ROTATE)
            {
                Renderer_FreeformEndEditorDrag();
                Renderer_FreeformRebuild();
            }
            if (g_drag != EDIT_DRAG_NONE)
            {
                CommitDocument();
            }
            g_drag = EDIT_DRAG_NONE;
            g_snapX = false;
            g_snapY = false;
            g_resizeHandle = HANDLE_NONE;
            ReleaseCapture();
            if (openText)
            {
                OpenTextEditor();
                PlaceCaretFromClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), false);
            }
            RefreshEditorChrome();
            return 0;
        }

        case WM_RBUTTONUP:
        {
            HideDrop();
            const int clientX = GET_X_LPARAM(lParam);
            const int clientY = GET_Y_LPARAM(lParam);
            int slideX = 0;
            int slideY = 0;
            int hit = -1;
            if (ClientToSlide(hwnd, clientX, clientY, &slideX, &slideY))
            {
                hit = Renderer_FreeformHitTestAny(slideX, slideY);
            }
            if (hit >= 0 && !Renderer_FreeformIsLocked(hit))
            {
                SelectObjectAt(hit);
            }
            POINT pt = { clientX, clientY };
            ClientToScreen(hwnd, &pt);
            ShowCanvasObjectMenu(hwnd, pt.x, pt.y, hit);
            return 0;
        }

        case WM_KEYDOWN:
            if (g_textOpen && wParam == VK_ESCAPE)
            {
                CloseTextEditor(true);
                RefreshEditorChrome();
                return 0;
            }
            if (!g_textOpen && (wParam == VK_RETURN || wParam == VK_F2) && SelectedIsText())
            {
                g_swallowEnterChar = wParam == VK_RETURN;
                OpenTextEditor();
                RefreshEditorChrome();
                return 0;
            }
            if (g_textOpen)
            {
                const bool extend = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (control && (wParam == 'A' || wParam == 'a'))
                {
                    char text[EDIT_TEXT_MAX] = {};
                    Renderer_FreeformGetText(g_selected, text, sizeof(text));
                    g_selAnchor = 0;
                    g_caret = lstrlenA(text);
                    g_caretOn = true;
            InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_LEFT)
                {
                    if (!extend && HasTextSel())
                    {
                        CollapseSelToCaret(true);
                    }
                    else if (g_caret > 0)
                    {
                        g_caret--;
                        if (!extend)
                        {
                            g_selAnchor = g_caret;
                        }
                    }
                    else if (!extend)
                    {
                        g_selAnchor = g_caret;
                    }
                    g_caretOn = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
                if (wParam == VK_RIGHT)
                {
                    char text[EDIT_TEXT_MAX] = {};
                    Renderer_FreeformGetText(g_selected, text, sizeof(text));
                    if (!extend && HasTextSel())
                    {
                        CollapseSelToCaret(false);
                    }
                    else if (g_caret < lstrlenA(text))
                    {
                        g_caret++;
                        if (!extend)
                        {
                            g_selAnchor = g_caret;
                        }
                    }
                    else if (!extend)
                    {
                        g_selAnchor = g_caret;
                    }
                    g_caretOn = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_UP)
                {
                    MoveCaretLine(hwnd, -1, extend);
                    return 0;
                }
                if (wParam == VK_DOWN)
                {
                    MoveCaretLine(hwnd, 1, extend);
                    return 0;
                }
                if (wParam == VK_HOME)
                {
                    MoveCaretHomeEnd(hwnd, true, extend);
                    return 0;
                }
                if (wParam == VK_END)
                {
                    MoveCaretHomeEnd(hwnd, false, extend);
                    return 0;
                }
                if (wParam == VK_BACK)
                {
                    if (!DeleteSelection() && g_caret > 0)
                    {
                        DeleteTextRange(g_caret - 1, 1);
                    }
                    return 0;
                }
                if (wParam == VK_DELETE)
                {
                    if (!DeleteSelection())
                    {
                        DeleteTextRange(g_caret, 1);
                    }
                    return 0;
                }
                return 0;
            }
            {
                const bool extend = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (EditView_HandleKey(wParam, extend, control))
                {
                    return 0;
                }
            }
            return 0;

        case WM_CHAR:
            if (g_swallowEnterChar && (wParam == 13 || wParam == 10))
            {
                g_swallowEnterChar = false;
                return 0;
            }
            g_swallowEnterChar = false;
            if (!g_textOpen)
            {
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
            {
                return 0;
            }
            if (wParam == 8 || wParam == 127)
            {
                return 0;
            }
            if (wParam == 13 || wParam == 10)
            {
                InsertTextChars("\n");
                return 0;
            }
            if (wParam >= 32 && wParam < 256)
            {
                char glyph[2] = { static_cast<char>(wParam), 0 };
                InsertTextChars(glyph);
            }
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EditStageProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            LayoutFloatingCanvas();
            return 0;

        case WM_LBUTTONDOWN:
            CloseTextEditor(true);
            g_selected = -1;
            g_tool = EDIT_TOOL_SELECT;
            RefreshEditorChrome();
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC target = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            if (client.right <= 0 || client.bottom <= 0)
            {
                EndPaint(hwnd, &paint);
                return 0;
            }
            HDC hdc = CreateCompatibleDC(target);
            HBITMAP buffer = CreateCompatibleBitmap(target, client.right, client.bottom);
            HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(hdc, buffer));
            {
                Gdiplus::Graphics g(hdc);
                const float w = static_cast<float>(client.right);
                const float h = static_cast<float>(client.bottom);
                Gdiplus::SolidBrush edge(Gdiplus::Color(255, 0x24, 0x24, 0x24));
                g.FillRectangle(&edge, 0.0f, 0.0f, w, h);
                const float r = sqrtf((w * w) + (h * h)) / 2.0f;
                Gdiplus::GraphicsPath glow;
                glow.AddEllipse((w / 2.0f) - r, (h / 2.0f) - r, r * 2.0f, r * 2.0f);
                Gdiplus::PathGradientBrush radial(&glow);
                radial.SetCenterColor(Gdiplus::Color(255, 0x2A, 0x2A, 0x2A));
                Gdiplus::Color surround(255, 0x24, 0x24, 0x24);
                int surroundCount = 1;
                radial.SetSurroundColors(&surround, &surroundCount);
                g.FillRectangle(&radial, 0.0f, 0.0f, w, h);

                if (g_canvas != nullptr && IsWindowVisible(g_canvas))
                {
                    RECT canvas = {};
                    GetWindowRect(g_canvas, &canvas);
                    MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&canvas), 2);
                    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    const int steps = 14;
                    for (int i = steps; i >= 1; --i)
                    {
                        const float grow = static_cast<float>(i) * 2.0f;
                        const float t = 1.0f - (static_cast<float>(i) / (steps + 1));
                        Gdiplus::GraphicsPath shade;
                        EditRoundPath(
                            &shade,
                            canvas.left - grow,
                            canvas.top - grow + 10.0f,
                            (canvas.right - canvas.left) + (grow * 2.0f),
                            (canvas.bottom - canvas.top) + (grow * 2.0f),
                            4.0f + grow
                        );
                        Gdiplus::SolidBrush brush(Gdiplus::Color(static_cast<BYTE>(t * t * 22.0f), 0, 0, 0));
                        g.FillPath(&brush, &shade);
                    }
                }
            }
            BitBlt(target, 0, 0, client.right, client.bottom, hdc, 0, 0, SRCCOPY);
            SelectObject(hdc, oldBitmap);
            DeleteObject(buffer);
            DeleteDC(hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static bool SelectedIsShape()
{
    int kind = -1;
    if (g_selected < 0 || !Renderer_FreeformGet(g_selected, &kind, nullptr, nullptr, nullptr, nullptr))
    {
        return false;
    }
    return kind == 1 || kind == 2 || kind == 3 || kind == 4;
}

static void SideText(HDC hdc, const char* text, RECT rc, COLORREF color, UINT flags)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, flags | DT_NOPREFIX);
}

static void PaintEditSide(HWND hwnd, HDC hdc)
{
    RECT client = {};
    GetClientRect(hwnd, &client);
    HBRUSH base = CreateSolidBrush(EDIT_COLOR_BASE);
    FillRect(hdc, &client, base);
    DeleteObject(base);
    EditRound(hdc, &client, EDIT_PANEL_RADIUS, EDIT_COLOR_SURFACE, CLR_INVALID, 0.0f);

    const char* tabs[3] = { "Shape", "Text", "Movement" };
    const int tabW = (client.right - 24 - 8) / 3;
    g_sideActionCount = 0;
    HFONT title = CreateFontA(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT body = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, title);
    for (int i = 0; i < 3; ++i)
    {
        RECT tab = { 12 + i * (tabW + 4), 12, 12 + i * (tabW + 4) + tabW, 12 + EDIT_SIDE_TAB_H };
        g_sideTabRc[i] = tab;
        const bool on = g_sideTab == i;
        EditRound(hdc, &tab, 6.0f, on ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED);
        SideText(hdc, tabs[i], tab, on ? EDIT_COLOR_INK_BRIGHT : EDIT_COLOR_INK, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, body);
    int y = 12 + EDIT_SIDE_TAB_H + 16;
    const int left = 16;
    const int right = client.right - 16;

    auto addAction = [&](RECT rc) {
        if (g_sideActionCount < 8)
        {
            g_sideActionRc[g_sideActionCount++] = rc;
        }
    };

    if (g_sideTab == EDIT_SIDE_SHAPE)
    {
        if (SelectedIsShape())
        {
            int kind = 0, x = 0, yy = 0, w = 0, h = 0;
            Renderer_FreeformGet(g_selected, &kind, &x, &yy, &w, &h);
            const float rot = Renderer_FreeformGetRotation(g_selected);
            SideText(hdc, "Shape", { left, y, right, y + 18 }, EDIT_COLOR_INK_DIM, DT_LEFT | DT_SINGLELINE);
            y += 26;
            char line[128] = {};
            wsprintfA(line, "X  %d    Y  %d", x, yy);
            SideText(hdc, line, { left, y, right, y + 18 }, EDIT_COLOR_INK_BRIGHT, DT_LEFT | DT_SINGLELINE);
            y += 22;
            wsprintfA(line, "W  %d    H  %d", w, h);
            SideText(hdc, line, { left, y, right, y + 18 }, EDIT_COLOR_INK_BRIGHT, DT_LEFT | DT_SINGLELINE);
            y += 22;
            wsprintfA(line, "Rotation  %.0f", rot);
            SideText(hdc, line, { left, y, right, y + 18 }, EDIT_COLOR_INK_BRIGHT, DT_LEFT | DT_SINGLELINE);
            y += 28;
            SideText(hdc, "Lock, fill, stroke, and shadow controls land here next.",
                     { left, y, right, y + 40 }, EDIT_COLOR_INK_FAINT, DT_LEFT | DT_WORDBREAK);
        }
        else
        {
            SideText(hdc, "Deck", { left, y, right, y + 18 }, EDIT_COLOR_INK_DIM, DT_LEFT | DT_SINGLELINE);
            y += 26;
            int sw = 1920, sh = 1080;
            Renderer_GetSlideSize(Renderer_EditSlide(), &sw, &sh);
            char line[64] = {};
            wsprintfA(line, "Resolution  %d x %d", sw, sh);
            SideText(hdc, line, { left, y, right, y + 18 }, EDIT_COLOR_INK_BRIGHT, DT_LEFT | DT_SINGLELINE);
            y += 28;

            RECT snapObj = { left, y, right, y + 28 };
            addAction(snapObj);
            EditRound(hdc, &snapObj, 6.0f, g_snapObjects ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED);
            SideText(hdc, g_snapObjects ? "Snap to objects  On" : "Snap to objects  Off",
                     snapObj, EDIT_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            y += 36;

            RECT snapGrid = { left, y, right, y + 28 };
            addAction(snapGrid);
            EditRound(hdc, &snapGrid, 6.0f, g_snapGrid ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED);
            SideText(hdc, g_snapGrid ? "Snap to grid  On" : "Snap to grid  Off",
                     snapGrid, EDIT_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            y += 36;
            wsprintfA(line, "Grid size  %d px", g_snapGridSize);
            SideText(hdc, line, { left, y, right, y + 18 }, EDIT_COLOR_INK, DT_LEFT | DT_SINGLELINE);
        }
    }
    else if (g_sideTab == EDIT_SIDE_TEXT)
    {
        if (SelectedIsText())
        {
            SideText(hdc, "Text", { left, y, right, y + 18 }, EDIT_COLOR_INK_DIM, DT_LEFT | DT_SINGLELINE);
            y += 26;
            SideText(hdc, "Use the type bar for font, size, style, align, and color.",
                     { left, y, right, y + 48 }, EDIT_COLOR_INK, DT_LEFT | DT_WORDBREAK);
            y += 56;
            SideText(hdc, "Advanced text options move into this tab next.",
                     { left, y, right, y + 40 }, EDIT_COLOR_INK_FAINT, DT_LEFT | DT_WORDBREAK);
        }
        else
        {
            SideText(hdc, "Slide", { left, y, right, y + 18 }, EDIT_COLOR_INK_DIM, DT_LEFT | DT_SINGLELINE);
            y += 26;
            RECT dup = { left, y, right, y + 30 };
            addAction(dup);
            EditRound(hdc, &dup, 6.0f, EDIT_COLOR_RAISED);
            SideText(hdc, "Duplicate slide", dup, EDIT_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            y += 40;
            SideText(hdc, "Background color and Enabled (skip in show) land here next.",
                     { left, y, right, y + 48 }, EDIT_COLOR_INK_FAINT, DT_LEFT | DT_WORDBREAK);
        }
    }
    else
    {
        if (g_selected >= 0)
        {
            SideText(hdc, "Element motion", { left, y, right, y + 18 }, EDIT_COLOR_INK_DIM, DT_LEFT | DT_SINGLELINE);
            y += 26;
            SideText(hdc, "Entrance, emphasis, and exit animations land here next.",
                     { left, y, right, y + 48 }, EDIT_COLOR_INK_FAINT, DT_LEFT | DT_WORDBREAK);
        }
        else
        {
            SideText(hdc, "Slide transition", { left, y, right, y + 18 }, EDIT_COLOR_INK_DIM, DT_LEFT | DT_SINGLELINE);
            y += 26;
            const int slide = Renderer_EditSlide();
            const bool usesOverride = Renderer_SlideUsesTransitionOverride(slide);
            const int current = Renderer_EffectiveSlideTransition(slide);
            const int currentDir = Renderer_EffectiveSlideTransitionDir(slide);
            const int currentMs = Renderer_EffectiveSlideTransitionMs(slide);
            {
                RECT row = { left, y, right, y + 26 };
                addAction(row);
                const bool on = !usesOverride;
                EditRound(hdc, &row, 6.0f, on ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED);
                SideText(hdc, "Default", row, EDIT_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                y += 32;
            }
            const char* labels[6] = { "None", "Crossfade", "Fade black", "Push", "Wipe", "Cover" };
            const int values[6] = {
                SLIDE_TRANSITION_NONE,
                SLIDE_TRANSITION_CROSSFADE,
                SLIDE_TRANSITION_FADE_BLACK,
                SLIDE_TRANSITION_PUSH,
                SLIDE_TRANSITION_WIPE,
                SLIDE_TRANSITION_COVER
            };
            for (int i = 0; i < 6; ++i)
            {
                RECT row = { left, y, right, y + 26 };
                addAction(row);
                const bool on = usesOverride && current == values[i];
                EditRound(hdc, &row, 6.0f, on ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED);
                SideText(hdc, labels[i], row, EDIT_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                y += 32;
            }
            if (Renderer_TransitionNeedsDir(current))
            {
                const char* dirs[4] = { "L", "R", "U", "D" };
                const int gap = 4;
                const int cellW = (right - left - gap * 3) / 4;
                for (int i = 0; i < 4; ++i)
                {
                    RECT cell = { left + i * (cellW + gap), y, left + i * (cellW + gap) + cellW, y + 24 };
                    addAction(cell);
                    const bool on = currentDir == i;
                    EditRound(hdc, &cell, 6.0f, on ? EDIT_COLOR_RAISED_HI : EDIT_COLOR_RAISED);
                    SideText(hdc, dirs[i], cell, EDIT_COLOR_INK_BRIGHT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
                y += 32;
            }
            {
                char msLabel[48] = {};
                wsprintfA(msLabel, "%d ms (live pinwheel)", currentMs);
                SideText(hdc, msLabel, { left, y, right, y + 18 }, EDIT_COLOR_INK_FAINT, DT_LEFT | DT_SINGLELINE);
            }
        }
    }

    SelectObject(hdc, old);
    DeleteObject(title);
    DeleteObject(body);
}

static void ClickEditSide(HWND hwnd, int x, int y)
{
    POINT pt = { x, y };
    for (int i = 0; i < 3; ++i)
    {
        if (PtInRect(&g_sideTabRc[i], pt))
        {
            g_sideTab = i;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }

    if (g_sideTab == EDIT_SIDE_SHAPE && !SelectedIsShape())
    {
        if (g_sideActionCount >= 1 && PtInRect(&g_sideActionRc[0], pt))
        {
            g_snapObjects = !g_snapObjects;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (g_sideActionCount >= 2 && PtInRect(&g_sideActionRc[1], pt))
        {
            g_snapGrid = !g_snapGrid;
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }
    else if (g_sideTab == EDIT_SIDE_TEXT && !SelectedIsText())
    {
        if (g_sideActionCount >= 1 && PtInRect(&g_sideActionRc[0], pt))
        {
            const int slide = Renderer_EditSlide();
            if (slide >= 0)
            {
                const int indices[1] = { slide };
                History_Remember();
                if (Renderer_CopySlides(indices, 1) && Renderer_PasteSlides(slide + 1))
                {
                    CommitDocument();
                    EditView_OpenSlide(slide + 1);
                }
            }
            return;
        }
    }
    else if (g_sideTab == EDIT_SIDE_MOVE && g_selected < 0)
    {
        const int values[6] = {
            SLIDE_TRANSITION_NONE,
            SLIDE_TRANSITION_CROSSFADE,
            SLIDE_TRANSITION_FADE_BLACK,
            SLIDE_TRANSITION_PUSH,
            SLIDE_TRANSITION_WIPE,
            SLIDE_TRANSITION_COVER
        };
        for (int i = 0; i < g_sideActionCount; ++i)
        {
            if (!PtInRect(&g_sideActionRc[i], pt))
            {
                continue;
            }
            History_Remember();
            const int slide = Renderer_EditSlide();
            if (i == 0)
            {
                Renderer_ClearSlideTransitionOverride(slide);
            }
            else if (i <= 6)
            {
                Renderer_SetSlideTransition(slide, values[i - 1]);
            }
            else if (Renderer_TransitionNeedsDir(Renderer_EffectiveSlideTransition(slide)))
            {
                Renderer_SetSlideTransitionDir(slide, i - 7);
            }
            CommitDocument();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
    }
}

static LRESULT CALLBACK EditSideProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            PaintEditSide(hwnd, hdc);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_LBUTTONUP:
            ClickEditSide(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

static LRESULT CALLBACK EditRootProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            LayoutEdit();
            return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC hdc = BeginPaint(hwnd, &paint);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HBRUSH fill = CreateSolidBrush(EDIT_COLOR_BASE);
            FillRect(hdc, &client, fill);
            DeleteObject(fill);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_MOUSEWHEEL:
            if (g_list != nullptr)
            {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT listRc = {};
                GetWindowRect(g_list, &listRc);
                if (PtInRect(&listRc, pt))
                {
                    return SendMessageA(g_list, WM_MOUSEWHEEL, wParam, lParam);
                }
            }
            return 0;

        case WM_DESTROY:
            CloseTextEditor(false);
            HideDrop();
            g_root = nullptr;
            g_list = nullptr;
            g_tools = nullptr;
            g_stage = nullptr;
            g_canvas = nullptr;
            g_status = nullptr;
            g_typeBar = nullptr;
            g_side = nullptr;
            g_drop = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void EditView_Register(HINSTANCE instance)
{
    WNDCLASSA root = {};
    root.lpfnWndProc = EditRootProc;
    root.hInstance = instance;
    root.lpszClassName = "EditViewWindow";
    root.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    root.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&root);

    WNDCLASSA list = {};
    list.lpfnWndProc = EditListProc;
    list.hInstance = instance;
    list.lpszClassName = "EditSlideListWindow";
    list.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    list.hCursor = LoadCursor(nullptr, IDC_ARROW);
    list.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&list);

    WNDCLASSA tools = {};
    tools.lpfnWndProc = EditToolsProc;
    tools.hInstance = instance;
    tools.lpszClassName = "EditToolsWindow";
    tools.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    tools.hCursor = LoadCursor(nullptr, IDC_HAND);
    tools.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&tools);

    WNDCLASSA stage = {};
    stage.lpfnWndProc = EditStageProc;
    stage.hInstance = instance;
    stage.lpszClassName = "EditStageWindow";
    stage.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    stage.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&stage);

    WNDCLASSA canvas = {};
    canvas.lpfnWndProc = EditCanvasProc;
    canvas.hInstance = instance;
    canvas.lpszClassName = "EditCanvasWindow";
    canvas.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    canvas.hCursor = LoadCursor(nullptr, IDC_ARROW);
    canvas.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&canvas);

    WNDCLASSA typeBar = {};
    typeBar.lpfnWndProc = EditTypeBarProc;
    typeBar.hInstance = instance;
    typeBar.lpszClassName = "EditTypeBarWindow";
    typeBar.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    typeBar.hCursor = LoadCursor(nullptr, IDC_ARROW);
    typeBar.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&typeBar);

    WNDCLASSA drop = {};
    drop.lpfnWndProc = EditDropProc;
    drop.hInstance = instance;
    drop.lpszClassName = "EditDropWindow";
    drop.hbrBackground = CreateSolidBrush(EDIT_COLOR_RAISED);
    drop.hCursor = LoadCursor(nullptr, IDC_ARROW);
    drop.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    RegisterClassA(&drop);

    WNDCLASSA status = {};
    status.lpfnWndProc = EditStatusProc;
    status.hInstance = instance;
    status.lpszClassName = "EditStatusWindow";
    status.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    status.hCursor = LoadCursor(nullptr, IDC_ARROW);
    status.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&status);

    WNDCLASSA side = {};
    side.lpfnWndProc = EditSideProc;
    side.hInstance = instance;
    side.lpszClassName = "EditSideWindow";
    side.hbrBackground = CreateSolidBrush(EDIT_COLOR_BASE);
    side.hCursor = LoadCursor(nullptr, IDC_HAND);
    side.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassA(&side);
}

void EditView_Create(HWND parent, HINSTANCE instance)
{
    if (g_root != nullptr)
    {
        return;
    }

    g_root = CreateWindowExA(
        0,
        "EditViewWindow",
        "",
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 100, 100,
        parent,
        nullptr,
        instance,
        nullptr
    );
    g_list = CreateWindowExA(
        0, "EditSlideListWindow", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, EDIT_RAIL_W, 100,
        g_root, nullptr, instance, nullptr
    );
    g_stage = CreateWindowExA(
        0, "EditStageWindow", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        EDIT_RAIL_W, 0, 100, 100,
        g_root, nullptr, instance, nullptr
    );
    g_tools = CreateWindowExA(
        0, "EditToolsWindow", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, EDIT_TOOL_W, 100,
        g_root, nullptr, instance, nullptr
    );
    g_canvas = CreateWindowExA(
        0, "EditCanvasWindow", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, 100,
        g_stage, nullptr, instance, nullptr
    );
    g_typeBar = CreateWindowExA(
        0, "EditTypeBarWindow", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 100, EDIT_TYPE_BAR_H,
        g_root, nullptr, instance, nullptr
    );
    g_status = CreateWindowExA(
        0, "EditStatusWindow", "",
        WS_CHILD | WS_VISIBLE,
        EDIT_RAIL_W, 80, 100, EDIT_STATUS_H,
        g_root, nullptr, instance, nullptr
    );
    g_side = CreateWindowExA(
        0, "EditSideWindow", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, EDIT_SIDE_W, 100,
        g_root, nullptr, instance, nullptr
    );
}

void EditView_Destroy()
{
    if (g_root != nullptr)
    {
        DestroyWindow(g_root);
    }
}

void EditView_Show(bool show)
{
    if (g_root == nullptr)
    {
        return;
    }
    ShowWindow(g_root, show ? SW_SHOW : SW_HIDE);
    if (show)
    {
        LayoutEdit();
        EditView_Refresh();
    }
}

void EditView_Layout(int x, int y, int width, int height)
{
    if (g_root == nullptr)
    {
        return;
    }
    MoveWindow(g_root, x, y, width, height, TRUE);
    LayoutEdit();
}

void EditView_OpenSlide(int index)
{
    if (index < 0 || index >= Renderer_SlideCount())
    {
        return;
    }
    SelectEditSlide(index);

    // Bring the thumbnail into view in the rail (same geometry as the wheel handler).
    if (g_list != nullptr)
    {
        RECT client = {};
        GetClientRect(g_list, &client);
        const int rowH = ThumbHeight(client.right);
        if (rowH > 0 && client.bottom > 0)
        {
            const int content = EDIT_RAIL_PAD + (Renderer_SlideCount() * rowH);
            const int maxPos = content > client.bottom ? content - client.bottom : 0;
            const int top = EDIT_RAIL_PAD + (index * rowH);
            const int bottom = top + rowH;
            int next = g_listScroll;
            if (top < g_listScroll)
            {
                next = top - 8;
            }
            else if (bottom > g_listScroll + client.bottom)
            {
                next = bottom - client.bottom + 8;
            }
            if (next < 0)
            {
                next = 0;
            }
            if (next > maxPos)
            {
                next = maxPos;
            }
            g_listScroll = next;
        }
    }
    EditView_Refresh();
}

void EditView_Refresh()
{
    if (g_list != nullptr)
    {
        InvalidateRect(g_list, nullptr, FALSE);
    }
    if (g_canvas != nullptr)
    {
        InvalidateRect(g_canvas, nullptr, FALSE);
    }
    if (g_tools != nullptr)
    {
        InvalidateRect(g_tools, nullptr, FALSE);
    }
    if (g_typeBar != nullptr)
    {
        InvalidateRect(g_typeBar, nullptr, FALSE);
    }
    if (g_side != nullptr)
    {
        InvalidateRect(g_side, nullptr, FALSE);
    }
    RefreshStatus();
}

bool EditView_HasSelection()
{
    return g_root != nullptr && g_selected >= 0;
}

bool EditView_IsEditingText()
{
    return g_textOpen;
}

bool EditView_DeleteSelected()
{
    if (g_selected < 0)
    {
        return false;
    }

    History_Remember();
    CloseTextEditor(false);
    Renderer_FreeformDelete(g_selected);
    g_selected = -1;
    CommitDocument();
    RefreshEditorChrome();
    return true;
}

bool EditView_CutSelected()
{
    if (g_selected < 0)
    {
        return false;
    }
    History_Remember();
    CloseTextEditor(false);
    if (!Renderer_FreeformCopy(g_selected))
    {
        return false;
    }
    Renderer_FreeformDelete(g_selected);
    g_selected = -1;
    CommitDocument();
    RefreshEditorChrome();
    return true;
}

bool EditView_CopySelected()
{
    if (g_selected < 0)
    {
        return false;
    }
    return Renderer_FreeformCopy(g_selected);
}

bool EditView_Paste()
{
    if (!Renderer_FreeformHasClipboard())
    {
        return false;
    }
    History_Remember();
    CloseTextEditor(false);
    const int pasted = Renderer_FreeformPaste();
    if (pasted < 0)
    {
        return false;
    }
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (Renderer_FreeformGet(pasted, nullptr, &x, &y, &w, &h))
    {
        Renderer_FreeformSetFrame(pasted, x + 16, y + 16, w, h);
    }
    g_selected = pasted;
    CommitDocument();
    RefreshEditorChrome();
    return true;
}

bool EditView_DuplicateSelected()
{
    if (g_selected < 0)
    {
        return false;
    }
    if (!Renderer_FreeformCopy(g_selected))
    {
        return false;
    }
    return EditView_Paste();
}

bool EditView_NudgeSelected(int dx, int dy)
{
    if (g_selected < 0 || Renderer_FreeformIsLocked(g_selected))
    {
        return false;
    }
    History_Remember();
    CloseTextEditor(true);
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (!Renderer_FreeformGet(g_selected, nullptr, &x, &y, &w, &h))
    {
        return false;
    }
    Renderer_FreeformSetFrame(g_selected, x + dx, y + dy, w, h);
    CommitDocument();
    RefreshEditorChrome();
    return true;
}

void EditView_ClearSelection()
{
    CloseTextEditor(true);
    g_selected = -1;
    RefreshEditorChrome();
}

bool EditView_SelectAllObjects()
{
    return false;
}

bool EditView_HandleKey(WPARAM key, bool shift, bool control)
{
    if (g_root == nullptr || !IsWindowVisible(g_root))
    {
        return false;
    }
    if (g_textOpen)
    {
        return false;
    }
    if (control)
    {
        if (key == 'X' || key == 'x')
        {
            return EditView_CutSelected();
        }
        if (key == 'C' || key == 'c')
        {
            return EditView_CopySelected();
        }
        if (key == 'V' || key == 'v')
        {
            return EditView_Paste();
        }
        if (key == 'D' || key == 'd')
        {
            return EditView_DuplicateSelected();
        }
        if (key == 'A' || key == 'a')
        {
            return EditView_SelectAllObjects();
        }
        return false;
    }
    if (key == VK_ESCAPE)
    {
        if (g_selected >= 0)
        {
            EditView_ClearSelection();
            return true;
        }
        return false;
    }
    if (key == VK_DELETE)
    {
        return EditView_DeleteSelected();
    }
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN)
    {
        if (g_selected < 0)
        {
            return false;
        }
        const int step = shift ? 10 : 1;
        int dx = 0;
        int dy = 0;
        if (key == VK_LEFT)
        {
            dx = -step;
        }
        else if (key == VK_RIGHT)
        {
            dx = step;
        }
        else if (key == VK_UP)
        {
            dy = -step;
        }
        else if (key == VK_DOWN)
        {
            dy = step;
        }
        return EditView_NudgeSelected(dx, dy);
    }
    return false;
}
