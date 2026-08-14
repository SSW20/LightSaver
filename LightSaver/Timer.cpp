#include "Timer.h"

Timer::Timer()
{
	QueryPerformanceFrequency(&ticks);
	QueryPerformanceCounter(&prevTime);
}


float Timer::GetDeltaTime()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	float deltaTime = (double)(currentTime.QuadPart - prevTime.QuadPart) / (double)ticks.QuadPart;

	prevTime = currentTime;

	return deltaTime;
}
