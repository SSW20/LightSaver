#pragma once
#include "Windows.h"
class InputManager
{
public:
	bool Initialize(HWND hWnd);
	void BeginFrame();
	bool IsKeyDown(int Key);
	bool IsKeyPressed(int Key);
	bool IsKeyReleased(int Key);
	long GetDeltaX() { return MouseDeltaX; }
	long GetDeltaY() { return MouseDeltaY; }

private:
	HWND WindowHandle;
	long MouseDeltaX = 0;
	long MouseDeltaY = 0;
	bool CurrentKeys[256] = {};
	bool PreviousKeys[256] = {};
	bool FirstWindowActive = true;
};

