#include "InputManager.h"
#include <cmath>
#include "UIRenderer.h"

void InputManager::BeginFrame()
{
	for (int Key = 0; Key < 256; ++Key)
	{
		PreviousKeys[Key] = CurrentKeys[Key];
	}

	if (GetForegroundWindow() != WindowHandle)
	{
		MouseDeltaX = 0;
		MouseDeltaY = 0;
		for (int Key = 0; Key < 256; ++Key)
		{
			PreviousKeys[Key] = CurrentKeys[Key] = false;
		}
		FirstWindowActive = true;
		return;
	}
	for (int Key = 0; Key < 256; ++Key)
	{
		CurrentKeys[Key] = (GetAsyncKeyState(Key) & 0x8000) != 0;
	}

	POINT MousePos;
	GetCursorPos(&MousePos);
	MouseClientPosition = MousePos;
	ScreenToClient(WindowHandle, &MouseClientPosition);

	if (!bMouseLocked)
	{
		MouseDeltaX = 0;
		MouseDeltaY = 0;
		FirstWindowActive = true;
		return;
	}

	RECT ClientSize;
	GetClientRect(WindowHandle, &ClientSize);

	POINT Center;
	Center.x = (ClientSize.left + ClientSize.right) / 2;
	Center.y = (ClientSize.top + ClientSize.bottom) / 2;

	ClientToScreen(WindowHandle, &Center);

	if (FirstWindowActive)
	{
		FirstWindowActive = false;
		SetCursorPos(Center.x, Center.y);
		return;
	}

	MouseDeltaX = MousePos.x - Center.x;
	MouseDeltaY = MousePos.y - Center.y;

	SetCursorPos(Center.x, Center.y);
}



bool InputManager::IsKeyDown(int Key) const
{
	if (GetForegroundWindow() != WindowHandle) return false;
	if (CurrentKeys[Key])
	{
		return true;
	}
	return false;
}

bool InputManager::IsKeyPressed(int Key) const
{
	if (GetForegroundWindow() != WindowHandle) return false;
	if (CurrentKeys[Key] && !PreviousKeys[Key])
	{
		return true;
	}
	return false;
}

bool InputManager::IsKeyReleased(int Key) const
{
	if (GetForegroundWindow() != WindowHandle) return false;
	if (!CurrentKeys[Key] && PreviousKeys[Key])
	{
		return true;
	}
	return false;
}

bool InputManager::Initialize(HWND hWnd)
{
	if (hWnd == NULL) return false;
	WindowHandle = hWnd;
	return true;
}

void InputManager::SetMouseLocked(bool bLocked)
{
	if (bMouseLocked == bLocked) return;

	bMouseLocked = bLocked;
	MouseDeltaX = 0;
	MouseDeltaY = 0;
	FirstWindowActive = true;

	if (bMouseLocked)
	{
		while (ShowCursor(FALSE) >= 0)
		{
		}
	}
	else
	{
		while (ShowCursor(TRUE) < 0)
		{
		}
	}
}
