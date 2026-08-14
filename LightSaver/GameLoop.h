#pragma once
#include "Windows.h"
#include "Graphics.h"
#include "Timer.h"

class GameLoop
{
public:
	GameLoop() = default;
	virtual ~GameLoop() = default;

	GameLoop(const GameLoop&) = delete;
	GameLoop& operator=(const GameLoop&) = delete;

	bool Initialize(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
	int Run();

protected:
	virtual bool OnInitialize() = 0;
	virtual void Update(float deltaTime) = 0;
	virtual bool Render() = 0;

	Graphics& GetGraphics();
	Windows& GetWindow();
	void RequestExit();

private:
	Windows windows;
	Graphics graphics;
	Timer timer;
	bool initialized = false;
	bool isRunning = false;
};
