#pragma once

#include <windows.h>

void EditView_Register(HINSTANCE instance);
void EditView_Create(HWND parent, HINSTANCE instance);
void EditView_Destroy();
void EditView_Show(bool show);
void EditView_Layout(int x, int y, int width, int height);
void EditView_Refresh();
// Make `index` the slide being edited and scroll the thumbnail rail to it.
// Does not touch the live/current slide.
void EditView_OpenSlide(int index);
bool EditView_HasSelection();
bool EditView_IsEditingText();
bool EditView_DeleteSelected();
bool EditView_CutSelected();
bool EditView_CopySelected();
bool EditView_Paste();
bool EditView_DuplicateSelected();
bool EditView_NudgeSelected(int dx, int dy);
void EditView_ClearSelection();
bool EditView_SelectAllObjects();
bool EditView_HandleKey(WPARAM key, bool shift, bool control);
