#ifndef DR2C_INTERNAL_IMGUI_UI_H
#define DR2C_INTERNAL_IMGUI_UI_H

#include <windows.h>

#include "entity_data.h"

bool InitializeInternalUi(HWND window, HMODULE module);
void ShutdownInternalUi();
void RenderInternalUi();
LRESULT HandleInternalWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void QueueEntityWrite(int slot, const Dr2cEntityView &entity, unsigned int mask);
void ApplyPendingEntityWrite();
void ClearPendingEntityWrite();

#endif