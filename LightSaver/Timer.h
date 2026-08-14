#pragma once
#include <windows.h>

class Timer
{
public:
	Timer();
	float GetDeltaTime();
private:
	LARGE_INTEGER ticks, prevTime;
};