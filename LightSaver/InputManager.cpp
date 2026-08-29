#include "InputManager.h"
#include <cmath>

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

	RECT ClientSize;
	GetClientRect(WindowHandle, &ClientSize);

	POINT Center;
	Center.x = (ClientSize.left + ClientSize.right) / 2;
	Center.y = (ClientSize.top + ClientSize.bottom) / 2;

	ClientToScreen(WindowHandle, &Center);

	POINT MousePos;
	GetCursorPos(&MousePos);

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

bool InputManager::IsKeyDown(int Key)
{
	if (GetForegroundWindow() != WindowHandle) return false;
	if (CurrentKeys[Key])
	{
		return true;
	}
	return false;
}

bool InputManager::IsKeyPressed(int Key)
{
	if (GetForegroundWindow() != WindowHandle) return false;
	if (CurrentKeys[Key] && !PreviousKeys[Key])
	{
		return true;
	}
	return false;
}

bool InputManager::IsKeyReleased(int Key)
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
