#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WCHAR WindowClass[] = L"LightSaver";
	WCHAR WindowTitle[] = L"LightSaver";

	WNDCLASS wc = {};
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = WindowClass;

	if(RegisterClass(&wc) == 0)
		return 0;

	HWND hWnd = CreateWindowExW(
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

	if (hWnd == NULL)
	{
		return 0;
	}

	ShowWindow(hWnd, nCmdShow);


	// Win32 입력 흐름:
	// 입력 장치 -> 드라이버 -> Windows 입력 시스템 -> 대상 HWND의 소유 스레드 메시지 큐
	// -> PeekMessageW -> DispatchMessageW -> 해당 창의 WindowProc.
	// HWND가 없는 스레드 메시지(WM_QUIT 등)는 메시지 루프에서 직접 처리한다.
	// Windows API는 프로그램이 운영체제의 창, 입력, 메시지 기능을 사용하는 함수 인터페이스다.

	// Direct3D 11 렌더링 흐름:
	// Device는 GPU 리소스를 생성하고, DeviceContext는 파이프라인 상태와 렌더링 명령을 관리한다.
	// SwapChain은 표시용 버퍼를 관리하며, Back Buffer는 실제 픽셀을 저장하는 Texture2D다.
	// RTV는 Back Buffer를 Output Merger의 출력 대상으로 연결하는 View이고,
	// Viewport는 셰이더 출력 좌표를 RenderTarget의 어느 픽셀 영역에 배치할지 정한다.
	// Vertex Buffer -> Input Assembler/Input Layout -> Vertex Shader -> Rasterizer
	// -> Pixel Shader -> Output Merger/RTV -> Back Buffer -> Present 순서로 화면이 완성된다.

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = 1280;
	sd.BufferDesc.Height = 720;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.OutputWindow = hWnd;

	ID3D11Device* Device = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11RenderTargetView* RTV = nullptr;

	HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, NULL, &DeviceContext);
	if (FAILED(result)) return 0;

	ID3D11Texture2D* BackBuffer = nullptr;
	result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**) &BackBuffer);
	if (FAILED(result)) return 0;

	result = Device->CreateRenderTargetView(BackBuffer, NULL, &RTV);
	if (FAILED(result)) return 0;

	BackBuffer->Release();

	float clearColor[4] =
	{
		0.1f, // Red
		0.2f, // Green
		0.3f, // Blue
		1.0f  // Alpha
	};




	LARGE_INTEGER ticks, currentTime, prevTime;

	// 1초에 틱 수
	QueryPerformanceFrequency(&ticks);


	QueryPerformanceCounter(&prevTime);

	bool bRunning = true;
	MSG msg = { };
	while (bRunning && msg.message != WM_QUIT)
	{
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				bRunning = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		else
		{
			// deltaTime 구하기
			QueryPerformanceCounter(&currentTime);
			float deltaTime = (double)(currentTime.QuadPart - prevTime.QuadPart) / (double)ticks.QuadPart;
			prevTime = currentTime;
			// UPDATE
			// RENDER

			DeviceContext->OMSetRenderTargets(1, &RTV, NULL);
			DeviceContext->ClearRenderTargetView(RTV, clearColor);
			SwapChain->Present(1, 0);
		}


	}

	RTV->Release();
	DeviceContext->Release();
	SwapChain->Release();
	Device->Release();


	return 0;
}
