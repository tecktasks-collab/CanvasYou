#include "svg_icon.h"

#include <gdiplus.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SVG_ICON_MAX 64
#define SVG_SHAPE_MAX 16

// Covers the subset the Assets icons use: <svg>, <path>, <circle>, <ellipse>,
// <rect>, <line>, <polyline> and <polygon>, with fill/stroke presentation
// attributes on the shape or the root. Colors other than "none" are ignored
// because the icons are single-color.
struct SvgStyle
{
    bool fill;
    bool stroke;
    float strokeWidth;
    Gdiplus::LineCap cap;
    Gdiplus::LineJoin join;
};

struct SvgShape
{
    Gdiplus::GraphicsPath* path;
    SvgStyle style;
};

struct SvgIcon
{
    char name[48];
    bool loaded;
    float viewX;
    float viewY;
    float viewW;
    float viewH;
    SvgShape shapes[SVG_SHAPE_MAX];
    int shapeCount;
};

static SvgIcon g_icons[SVG_ICON_MAX] = {};
static int g_iconCount = 0;

static char* ReadIconFile(const char* name)
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash == nullptr)
    {
        return nullptr;
    }
    slash[1] = L'\0';
    wchar_t wideName[64] = {};
    MultiByteToWideChar(CP_UTF8, 0, name, -1, wideName, 64);
    if (lstrlenW(path) + lstrlenW(wideName) + 12 >= MAX_PATH)
    {
        return nullptr;
    }
    lstrcatW(path, L"Assets\\");
    lstrcatW(path, wideName);
    lstrcatW(path, L".svg");

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }
    const DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE || size == 0 || size > 256 * 1024)
    {
        CloseHandle(file);
        return nullptr;
    }
    char* text = static_cast<char*>(malloc(size + 1));
    DWORD read = 0;
    if (text == nullptr || !ReadFile(file, text, size, &read, nullptr))
    {
        free(text);
        CloseHandle(file);
        return nullptr;
    }
    CloseHandle(file);
    text[read] = '\0';
    return text;
}

// Finds the value of attr="..." in the tag starting at tag (up to its '>').
static const char* FindTagAttr(const char* tag, const char* attr, int* length)
{
    const char* end = strchr(tag, '>');
    const int attrLen = lstrlenA(attr);
    for (const char* cursor = tag + 1; *cursor != '\0' && (end == nullptr || cursor < end); ++cursor)
    {
        if ((cursor[-1] != ' ' && cursor[-1] != '\t' && cursor[-1] != '\r' && cursor[-1] != '\n') ||
            strncmp(cursor, attr, attrLen) != 0)
        {
            continue;
        }
        const char* p = cursor + attrLen;
        while (*p == ' ')
        {
            p += 1;
        }
        if (*p != '=')
        {
            continue;
        }
        p += 1;
        while (*p == ' ')
        {
            p += 1;
        }
        const char quote = *p;
        if (quote != '"' && quote != '\'')
        {
            continue;
        }
        p += 1;
        const char* close = strchr(p, quote);
        if (close == nullptr)
        {
            return nullptr;
        }
        *length = static_cast<int>(close - p);
        return p;
    }
    return nullptr;
}

static bool TagAttr(const char* tag, const char* attr, char* out, int outChars)
{
    int length = 0;
    const char* value = FindTagAttr(tag, attr, &length);
    if (value == nullptr)
    {
        return false;
    }
    if (length >= outChars)
    {
        length = outChars - 1;
    }
    memcpy(out, value, length);
    out[length] = '\0';
    return true;
}

static float TagFloat(const char* tag, const char* attr, float fallback)
{
    char value[32] = {};
    return TagAttr(tag, attr, value, sizeof(value)) ? static_cast<float>(atof(value)) : fallback;
}

static void ReadStyle(const char* tag, SvgStyle* style)
{
    char value[32] = {};
    if (TagAttr(tag, "fill", value, sizeof(value)))
    {
        style->fill = lstrcmpiA(value, "none") != 0;
    }
    if (TagAttr(tag, "stroke", value, sizeof(value)))
    {
        style->stroke = lstrcmpiA(value, "none") != 0;
    }
    if (TagAttr(tag, "stroke-width", value, sizeof(value)))
    {
        style->strokeWidth = static_cast<float>(atof(value));
    }
    if (TagAttr(tag, "stroke-linecap", value, sizeof(value)))
    {
        style->cap = lstrcmpiA(value, "round") == 0 ? Gdiplus::LineCapRound
            : (lstrcmpiA(value, "square") == 0 ? Gdiplus::LineCapSquare : Gdiplus::LineCapFlat);
    }
    if (TagAttr(tag, "stroke-linejoin", value, sizeof(value)))
    {
        style->join = lstrcmpiA(value, "round") == 0 ? Gdiplus::LineJoinRound
            : (lstrcmpiA(value, "bevel") == 0 ? Gdiplus::LineJoinBevel : Gdiplus::LineJoinMiter);
    }
}

static const char* SkipSeparators(const char* p)
{
    while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r' || *p == '\n')
    {
        p += 1;
    }
    return p;
}

static bool NextNumber(const char*& p, float* value)
{
    p = SkipSeparators(p);
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

static bool NextFlag(const char*& p, bool* flag)
{
    p = SkipSeparators(p);
    if (*p != '0' && *p != '1')
    {
        return false;
    }
    *flag = *p == '1';
    p += 1;
    return true;
}

static void AddCubic(Gdiplus::GraphicsPath* path, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
    path->AddBezier(x0, y0, x1, y1, x2, y2, x3, y3);
}

static void AddArc(
    Gdiplus::GraphicsPath* path,
    float x0, float y0, float rx, float ry, float angleDeg, bool largeArc, bool sweep, float x1, float y1)
{
    if (rx == 0.0f || ry == 0.0f || (x0 == x1 && y0 == y1))
    {
        path->AddLine(x0, y0, x1, y1);
        return;
    }
    const float pi = 3.14159265f;
    rx = fabsf(rx);
    ry = fabsf(ry);
    const float phi = angleDeg * pi / 180.0f;
    const float cosPhi = cosf(phi);
    const float sinPhi = sinf(phi);
    const float dx = (x0 - x1) / 2.0f;
    const float dy = (y0 - y1) / 2.0f;
    const float x0p = cosPhi * dx + sinPhi * dy;
    const float y0p = -sinPhi * dx + cosPhi * dy;
    const float lambda = (x0p * x0p) / (rx * rx) + (y0p * y0p) / (ry * ry);
    if (lambda > 1.0f)
    {
        rx *= sqrtf(lambda);
        ry *= sqrtf(lambda);
    }
    float num = rx * rx * ry * ry - rx * rx * y0p * y0p - ry * ry * x0p * x0p;
    const float den = rx * rx * y0p * y0p + ry * ry * x0p * x0p;
    float coef = den > 0.0f && num > 0.0f ? sqrtf(num / den) : 0.0f;
    if (largeArc == sweep)
    {
        coef = -coef;
    }
    const float cxp = coef * (rx * y0p / ry);
    const float cyp = coef * -(ry * x0p / rx);
    const float cx = cosPhi * cxp - sinPhi * cyp + (x0 + x1) / 2.0f;
    const float cy = sinPhi * cxp + cosPhi * cyp + (y0 + y1) / 2.0f;
    const float theta1 = atan2f((y0p - cyp) / ry, (x0p - cxp) / rx);
    float delta = atan2f((-y0p - cyp) / ry, (-x0p - cxp) / rx) - theta1;
    if (sweep && delta < 0.0f)
    {
        delta += 2.0f * pi;
    }
    else if (!sweep && delta > 0.0f)
    {
        delta -= 2.0f * pi;
    }

    const int segments = static_cast<int>(ceilf(fabsf(delta) / (pi / 2.0f)));
    const float step = delta / (segments > 0 ? segments : 1);
    const float k = 4.0f / 3.0f * tanf(step / 4.0f);
    float angle = theta1;
    float px = x0;
    float py = y0;
    for (int s = 0; s < segments; ++s)
    {
        const float a1 = angle + step;
        const float c1 = cosf(angle);
        const float s1 = sinf(angle);
        const float c2 = cosf(a1);
        const float s2 = sinf(a1);
        const float e1x = rx * (c1 - k * s1);
        const float e1y = ry * (s1 + k * c1);
        const float e2x = rx * (c2 + k * s2);
        const float e2y = ry * (s2 - k * c2);
        const float ex = rx * c2;
        const float ey = ry * s2;
        const float qx1 = cx + cosPhi * e1x - sinPhi * e1y;
        const float qy1 = cy + sinPhi * e1x + cosPhi * e1y;
        const float qx2 = cx + cosPhi * e2x - sinPhi * e2y;
        const float qy2 = cy + sinPhi * e2x + cosPhi * e2y;
        const float qx = s + 1 == segments ? x1 : cx + cosPhi * ex - sinPhi * ey;
        const float qy = s + 1 == segments ? y1 : cy + sinPhi * ex + cosPhi * ey;
        AddCubic(path, px, py, qx1, qy1, qx2, qy2, qx, qy);
        px = qx;
        py = qy;
        angle = a1;
    }
}

static void BuildPath(Gdiplus::GraphicsPath* path, const char* d)
{
    const char* p = d;
    char command = 0;
    float x = 0.0f;
    float y = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    float lastCtrlX = 0.0f;
    float lastCtrlY = 0.0f;
    char lastCommand = 0;
    while (true)
    {
        p = SkipSeparators(p);
        if (*p == '\0')
        {
            break;
        }
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))
        {
            command = *p;
            p += 1;
        }
        else if (command == 0)
        {
            break;
        }
        const bool relative = command >= 'a' && command <= 'z';
        const char upper = relative ? static_cast<char>(command - 32) : command;
        const float baseX = relative ? x : 0.0f;
        const float baseY = relative ? y : 0.0f;
        float v[7] = {};
        bool ok = true;

        if (upper == 'Z')
        {
            path->CloseFigure();
            x = startX;
            y = startY;
            lastCommand = 'Z';
            command = 0;
            continue;
        }
        if (upper == 'M')
        {
            ok = NextNumber(p, &v[0]) && NextNumber(p, &v[1]);
            if (!ok)
            {
                break;
            }
            path->StartFigure();
            x = baseX + v[0];
            y = baseY + v[1];
            startX = x;
            startY = y;
            command = relative ? 'l' : 'L';
        }
        else if (upper == 'L')
        {
            ok = NextNumber(p, &v[0]) && NextNumber(p, &v[1]);
            if (!ok)
            {
                break;
            }
            path->AddLine(x, y, baseX + v[0], baseY + v[1]);
            x = baseX + v[0];
            y = baseY + v[1];
        }
        else if (upper == 'H' || upper == 'V')
        {
            ok = NextNumber(p, &v[0]);
            if (!ok)
            {
                break;
            }
            const float nx = upper == 'H' ? baseX + v[0] : x;
            const float ny = upper == 'V' ? baseY + v[0] : y;
            path->AddLine(x, y, nx, ny);
            x = nx;
            y = ny;
        }
        else if (upper == 'C' || upper == 'S')
        {
            float c1x = 0.0f;
            float c1y = 0.0f;
            if (upper == 'C')
            {
                ok = NextNumber(p, &v[0]) && NextNumber(p, &v[1]);
                c1x = baseX + v[0];
                c1y = baseY + v[1];
            }
            else
            {
                const bool chained = lastCommand == 'C' || lastCommand == 'S';
                c1x = chained ? 2.0f * x - lastCtrlX : x;
                c1y = chained ? 2.0f * y - lastCtrlY : y;
            }
            ok = ok && NextNumber(p, &v[2]) && NextNumber(p, &v[3]) && NextNumber(p, &v[4]) && NextNumber(p, &v[5]);
            if (!ok)
            {
                break;
            }
            const float c2x = baseX + v[2];
            const float c2y = baseY + v[3];
            const float ex = baseX + v[4];
            const float ey = baseY + v[5];
            AddCubic(path, x, y, c1x, c1y, c2x, c2y, ex, ey);
            lastCtrlX = c2x;
            lastCtrlY = c2y;
            x = ex;
            y = ey;
        }
        else if (upper == 'Q' || upper == 'T')
        {
            float qx = 0.0f;
            float qy = 0.0f;
            if (upper == 'Q')
            {
                ok = NextNumber(p, &v[0]) && NextNumber(p, &v[1]);
                qx = baseX + v[0];
                qy = baseY + v[1];
            }
            else
            {
                const bool chained = lastCommand == 'Q' || lastCommand == 'T';
                qx = chained ? 2.0f * x - lastCtrlX : x;
                qy = chained ? 2.0f * y - lastCtrlY : y;
            }
            ok = ok && NextNumber(p, &v[2]) && NextNumber(p, &v[3]);
            if (!ok)
            {
                break;
            }
            const float ex = baseX + v[2];
            const float ey = baseY + v[3];
            AddCubic(
                path,
                x, y,
                x + 2.0f / 3.0f * (qx - x), y + 2.0f / 3.0f * (qy - y),
                ex + 2.0f / 3.0f * (qx - ex), ey + 2.0f / 3.0f * (qy - ey),
                ex, ey);
            lastCtrlX = qx;
            lastCtrlY = qy;
            x = ex;
            y = ey;
        }
        else if (upper == 'A')
        {
            bool largeArc = false;
            bool sweep = false;
            ok = NextNumber(p, &v[0]) && NextNumber(p, &v[1]) && NextNumber(p, &v[2]) &&
                 NextFlag(p, &largeArc) && NextFlag(p, &sweep) && NextNumber(p, &v[5]) && NextNumber(p, &v[6]);
            if (!ok)
            {
                break;
            }
            const float ex = baseX + v[5];
            const float ey = baseY + v[6];
            AddArc(path, x, y, v[0], v[1], v[2], largeArc, sweep, ex, ey);
            x = ex;
            y = ey;
        }
        else
        {
            break;
        }
        lastCommand = upper;
    }
}

static void AddPoints(Gdiplus::GraphicsPath* path, const char* points, bool closed)
{
    const char* p = points;
    float px = 0.0f;
    float py = 0.0f;
    bool first = true;
    float x = 0.0f;
    float y = 0.0f;
    path->StartFigure();
    while (NextNumber(p, &x) && NextNumber(p, &y))
    {
        if (!first)
        {
            path->AddLine(px, py, x, y);
        }
        first = false;
        px = x;
        py = y;
    }
    if (closed)
    {
        path->CloseFigure();
    }
}

static Gdiplus::GraphicsPath* BuildShape(const char* tag)
{
    Gdiplus::GraphicsPath* path = new Gdiplus::GraphicsPath(Gdiplus::FillModeWinding);
    if (strncmp(tag, "<path", 5) == 0)
    {
        int length = 0;
        const char* d = FindTagAttr(tag, "d", &length);
        char* data = d != nullptr ? static_cast<char*>(malloc(length + 1)) : nullptr;
        if (data != nullptr)
        {
            memcpy(data, d, length);
            data[length] = '\0';
            BuildPath(path, data);
            free(data);
        }
    }
    else if (strncmp(tag, "<circle", 7) == 0 || strncmp(tag, "<ellipse", 8) == 0)
    {
        const float cx = TagFloat(tag, "cx", 0.0f);
        const float cy = TagFloat(tag, "cy", 0.0f);
        const float r = TagFloat(tag, "r", 0.0f);
        const float rx = TagFloat(tag, "rx", r);
        const float ry = TagFloat(tag, "ry", r);
        path->AddEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    }
    else if (strncmp(tag, "<rect", 5) == 0)
    {
        path->AddRectangle(Gdiplus::RectF(
            TagFloat(tag, "x", 0.0f), TagFloat(tag, "y", 0.0f), TagFloat(tag, "width", 0.0f), TagFloat(tag, "height", 0.0f)));
    }
    else if (strncmp(tag, "<line", 5) == 0)
    {
        path->AddLine(TagFloat(tag, "x1", 0.0f), TagFloat(tag, "y1", 0.0f), TagFloat(tag, "x2", 0.0f), TagFloat(tag, "y2", 0.0f));
    }
    else if (strncmp(tag, "<polyline", 9) == 0 || strncmp(tag, "<polygon", 8) == 0)
    {
        char points[1024] = {};
        if (TagAttr(tag, "points", points, sizeof(points)))
        {
            AddPoints(path, points, strncmp(tag, "<polygon", 8) == 0);
        }
    }
    else
    {
        delete path;
        return nullptr;
    }
    return path;
}

static void LoadSvgIcon(SvgIcon* icon)
{
    char* text = ReadIconFile(icon->name);
    if (text == nullptr)
    {
        return;
    }
    const char* root = strstr(text, "<svg");
    if (root == nullptr)
    {
        free(text);
        return;
    }

    SvgStyle base = {};
    base.fill = true;
    base.stroke = false;
    base.strokeWidth = 1.0f;
    base.cap = Gdiplus::LineCapFlat;
    base.join = Gdiplus::LineJoinMiter;
    ReadStyle(root, &base);

    icon->viewX = 0.0f;
    icon->viewY = 0.0f;
    icon->viewW = TagFloat(root, "width", 24.0f);
    icon->viewH = TagFloat(root, "height", 24.0f);
    char viewBox[64] = {};
    if (TagAttr(root, "viewBox", viewBox, sizeof(viewBox)))
    {
        const char* p = viewBox;
        float box[4] = {};
        if (NextNumber(p, &box[0]) && NextNumber(p, &box[1]) && NextNumber(p, &box[2]) && NextNumber(p, &box[3]) &&
            box[2] > 0.0f && box[3] > 0.0f)
        {
            icon->viewX = box[0];
            icon->viewY = box[1];
            icon->viewW = box[2];
            icon->viewH = box[3];
        }
    }

    const char* cursor = strchr(root, '>');
    while (cursor != nullptr && icon->shapeCount < SVG_SHAPE_MAX)
    {
        cursor = strchr(cursor, '<');
        if (cursor == nullptr)
        {
            break;
        }
        Gdiplus::GraphicsPath* path = BuildShape(cursor);
        if (path != nullptr)
        {
            SvgShape* shape = &icon->shapes[icon->shapeCount];
            shape->path = path;
            shape->style = base;
            ReadStyle(cursor, &shape->style);
            icon->shapeCount += 1;
        }
        cursor += 1;
    }
    free(text);
    icon->loaded = icon->shapeCount > 0;
}

static SvgIcon* FindIcon(const char* name)
{
    for (int i = 0; i < g_iconCount; ++i)
    {
        if (lstrcmpA(g_icons[i].name, name) == 0)
        {
            return &g_icons[i];
        }
    }
    if (g_iconCount >= SVG_ICON_MAX)
    {
        return nullptr;
    }
    SvgIcon* icon = &g_icons[g_iconCount];
    g_iconCount += 1;
    lstrcpynA(icon->name, name, sizeof(icon->name));
    LoadSvgIcon(icon);
    return icon;
}

bool SvgIcon_Draw(HDC hdc, const char* name, int cx, int cy, int size, COLORREF color)
{
    if (hdc == nullptr || name == nullptr || size <= 0)
    {
        return false;
    }
    SvgIcon* icon = FindIcon(name);
    if (icon == nullptr || !icon->loaded)
    {
        return false;
    }

    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.TranslateTransform(cx - size / 2.0f, cy - size / 2.0f);
    graphics.ScaleTransform(size / icon->viewW, size / icon->viewH);
    graphics.TranslateTransform(-icon->viewX, -icon->viewY);

    const Gdiplus::Color ink(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::SolidBrush brush(ink);
    for (int i = 0; i < icon->shapeCount; ++i)
    {
        const SvgShape* shape = &icon->shapes[i];
        if (shape->style.fill)
        {
            graphics.FillPath(&brush, shape->path);
        }
        if (shape->style.stroke && shape->style.strokeWidth > 0.0f)
        {
            Gdiplus::Pen pen(ink, shape->style.strokeWidth);
            pen.SetLineCap(shape->style.cap, shape->style.cap, Gdiplus::DashCapRound);
            pen.SetLineJoin(shape->style.join);
            graphics.DrawPath(&pen, shape->path);
        }
    }
    return true;
}
