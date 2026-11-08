#pragma once

#include <windows.h>

// Draws Assets\<name>.svg (beside the exe) centered on cx,cy in a size x size
// square. Every stroke and fill is drawn in color, the way the icons use
// currentColor. Returns false when the icon is missing or can't be read.
bool SvgIcon_Draw(HDC hdc, const char* name, int cx, int cy, int size, COLORREF color);
