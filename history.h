#pragma once

#include <windows.h>

void History_Clear();
void History_Remember();
bool History_CanUndo();
bool History_CanRedo();
bool History_Undo();
bool History_Redo();
