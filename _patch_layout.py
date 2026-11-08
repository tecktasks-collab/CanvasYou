from pathlib import Path

p = Path(r"c:\Users\theel\Documents\Presentation Software Dev\manage_screens.cpp")
text = p.read_text(encoding="utf-8")
start = text.find("static void LayoutOutputTiles()")
end = text.find("static LRESULT CALLBACK OutputMapProc")
if start < 0 or end < 0:
    raise SystemExit(f"markers {start} {end}")

new_layout = """static void EnsureUiFont()
{
    if (g_uiFont != nullptr)
    {
        return;
    }
    g_uiFont = CreateFontA(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
    );
}

static void LayoutOutputTiles()
{
    if (g_outputMap == nullptr)
    {
        return;
    }

    RECT map = {};
    GetClientRect(g_outputMap, &map);

    for (int i = 0; i < MAX_MANAGED_SCREENS; ++i)
    {
        SetRectEmpty(&g_outputs[i].tile);
        SetRectEmpty(&g_outputs[i].preview);
        SetRectEmpty(&g_outputs[i].chooserRc);
    }

    if (g_outputCount <= 0)
    {
        LayoutSlotCombos();
        return;
    }

    const int margin = 16;
    const int gap = TILE_GAP;
    int availW = map.right - (margin * 2) - (gap * (g_outputCount - 1));
    if (availW < 40)
    {
        availW = 40;
    }

    int width = availW / g_outputCount;
    if (width > 300)
    {
        width = 300;
    }
    if (width < 140)
    {
        width = 140;
    }

    int previewH = MulDiv(width, 9, 16);
    if (previewH < 80)
    {
        previewH = 80;
    }
    int height = previewH + 10 + CHOOSER_H;
    const int maxH = map.bottom - 40;
    if (height > maxH && maxH > 90)
    {
        height = maxH;
        previewH = height - 10 - CHOOSER_H;
        if (previewH < 56)
        {
            previewH = 56;
            height = previewH + 10 + CHOOSER_H;
        }
        width = MulDiv(previewH, 16, 9);
    }

    const int totalWidth = (width * g_outputCount) + (gap * (g_outputCount - 1));
    int x = (map.right - totalWidth) / 2;
    if (x < margin)
    {
        x = margin;
    }
    const int y = 24;

    for (int i = 0; i < g_outputCount; ++i)
    {
        g_outputs[i].tile.left = x;
        g_outputs[i].tile.top = y;
        g_outputs[i].tile.right = x + width;
        g_outputs[i].tile.bottom = y + height;

        g_outputs[i].preview = g_outputs[i].tile;
        g_outputs[i].preview.bottom = g_outputs[i].preview.top + previewH;

        g_outputs[i].chooserRc.left = x;
        g_outputs[i].chooserRc.right = x + width;
        g_outputs[i].chooserRc.top = g_outputs[i].preview.bottom + 10;
        g_outputs[i].chooserRc.bottom = g_outputs[i].chooserRc.top + CHOOSER_H;

        x += width + gap;
    }

    LayoutSlotCombos();
}

"""

p.write_text(text[:start] + new_layout + text[end:], encoding="utf-8")
print("ok")
