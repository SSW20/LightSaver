#include "GameLoop.h"

bool GameLoop::Initialize(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	if (!windows.Initialize(hInstance, hPrevInstance, lpCmdLine, nCmdShow)) return false;
	if (!graphics.Initialize(windows.GetHWND())) return false;
	if (!OnInitialize()) return false;

	initialized = true;
	return true;
}

int GameLoop::Run()
{
	if (!initialized) return -1;

	isRunning = true;

	while (isRunning)
	{
		if (!windows.PeekMSG()) break;

		const float deltaTime = timer.GetDeltaTime();
		Update(deltaTime);

		if (!Render()) return -1;

		const HRESULT result = graphics.SwapChain->Present(1, 0);
		if (FAILED(result)) return -1;
	}

	return 0;
}

Graphics& GameLoop::GetGraphics()
{
	return graphics;
}

Windows& GameLoop::GetWindow()
{
	return windows;
}

void GameLoop::RequestExit()
{
	isRunning = false;
}
