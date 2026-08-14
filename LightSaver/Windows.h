#pragma once
#include <windows.h>


class Windows
{
public:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
		return 0;
	}

	Windows() {};

	bool Initialize(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
	{
		WCHAR WindowClass[] = L"LightSaver";
		WCHAR WindowTitle[] = L"LightSaver";

		WNDCLASS wc = {};
		wc.hInstance = hInstance;
		wc.lpfnWndProc = WindowProc;
		wc.lpszClassName = WindowClass;
		RegisterClassW(&wc);

		hWnd = CreateWindowExW(
			0,
			WindowClass,
			WindowTitle,
			WS_OVERLAPPEDWINDOW,
			0, 0, 1280, 720,
			NULL,
			NULL,
			hInstance,
			NULL
		);


		ShowWindow(hWnd, nCmdShow);
		return 1;
	}


	bool PeekMSG()
	{
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				return 0;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return 1;
	}

	HWND GetHWND() const
	{
		return hWnd;
	}

	MSG GetMSG() const
	{
		return msg;
	}
private:
	HWND hWnd = nullptr;
	MSG msg = {};
};
