#pragma once
#include "Windows.h"
class InputManager
{
public:
	bool Initialize(HWND hWnd);
	void BeginFrame();
	bool IsKeyDown(int Key) const;
	bool IsKeyPressed(int Key) const;
	bool IsKeyReleased(int Key) const;
	POINT GetMouseClientPosition() const { return MouseClientPosition; }
	void SetMouseLocked(bool bLocked);
	long GetDeltaX() { return MouseDeltaX; }
	long GetDeltaY() { return MouseDeltaY; }

private:
	HWND WindowHandle;
	long MouseDeltaX = 0;
	long MouseDeltaY = 0;
	bool CurrentKeys[256] = {};
	bool PreviousKeys[256] = {};
	bool FirstWindowActive = true;
	bool bMouseLocked = false;
	POINT MouseClientPosition = {};
};

