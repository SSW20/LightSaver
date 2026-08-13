#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>

using namespace DirectX;
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct Vertex
{
	float x;
	float y;
	float z;
};

struct alignas(16) CameraBufferData
{
	XMFLOAT4X4 View;
	XMFLOAT4X4 Projection;
};

struct alignas(16) ObjectBufferData
{
	XMFLOAT4X4 World;
};

static_assert(sizeof(CameraBufferData) % 16 == 0);
static_assert(sizeof(ObjectBufferData) % 16 == 0);

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


	// 흐름 : 디바이스에서 입력 받고 물리적인 신호 전달 -> 드라이버에서 운영체제 메세지로 번역 및 전달 -> 운영체제에서 포커스, 커서 위치, 마우스 캡처 등을 이용해 대상 HWND를 결정 -> 해당 창을 가지고 있는 스레드의 메세지 큐에 메세지 넣기
	//			-> 스레드는 메세지 큐에서 하나씩 꺼내 hWnd를 판단 및 해당 창의 윈도우 프로시저 호출 (만약 hWnd가 null 이라면 스레드 메세지로 이는 따로 윈도우 프로시저를 호출 하지 않음)

	// 질문 : 그러면 프로그램과 Windows Api는 무엇인가? 답: 소스 코드를 빌드하면 EXE 프로그램 파일이 생성 -> EXE를 실행하면 Windows가 프로세스와 메인 스레드를 생성
	//									Windows API는 프로그램이 운영체제의 창과 메시지 기능을 사용하기 위한 함수 인터페이스
	//									프로그래머는 WinodwAPI를 통해 커스텀하여 코드를 작성

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

	ID3D11DepthStencilView* DSV = nullptr;
	ID3D11Texture2D* DepthBuffer = nullptr;

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = 1280;
	DepthDesc.Height = 720;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.SampleDesc.Quality = 0;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	DepthDesc.CPUAccessFlags = 0;
	DepthDesc.MiscFlags = 0;

	result = Device->CreateTexture2D(&DepthDesc, nullptr, &DepthBuffer);
	if (FAILED(result)) return 0;

	result = Device->CreateDepthStencilView(DepthBuffer,nullptr,&DSV);

	if (FAILED(result)) return 0;

	float clearColor[4] ={0.1f, 0.2f, 0.3f, 1.0f  };

	Vertex vertices[] =
	{
		{ -0.5f, -0.5f, -0.5f }, // 0
		{  0.5f, -0.5f, -0.5f }, // 1
		{  0.5f, -0.5f,  0.5f }, // 2
		{ -0.5f, -0.5f,  0.5f }, // 3

		{ -0.5f,  0.5f, -0.5f }, // 4
		{  0.5f,  0.5f, -0.5f }, // 5
		{  0.5f,  0.5f,  0.5f }, // 6
		{ -0.5f,  0.5f,  0.5f }  // 7
	};

	ID3D11Buffer* VertexBuffer = nullptr;
	D3D11_BUFFER_DESC VertexDesc = {};
	D3D11_SUBRESOURCE_DATA VertexData = {};

	VertexDesc.ByteWidth = sizeof(vertices);
	VertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	VertexData.pSysMem = vertices;

	result = Device->CreateBuffer(&VertexDesc, &VertexData, &VertexBuffer);
	if (FAILED(result)) return 0;

	UINT indices[] =
	{
		// 앞면
		0, 4, 5,
		0, 5, 1,

		// 뒷면
		3, 2, 6,
		3, 6, 7,

		// 왼쪽
		0, 3, 7,
		0, 7, 4,

		// 오른쪽
		1, 5, 6,
		1, 6, 2,

		// 아래
		0, 1, 2,
		0, 2, 3,

		// 위
		4, 7, 6,
		4, 6, 5
	};

	ID3D11Buffer* IndexBuffer = nullptr;
	D3D11_BUFFER_DESC IndexDesc = {};
	IndexDesc.ByteWidth = sizeof(indices);
	IndexDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexData = {};
	IndexData.pSysMem = indices;

	result = Device->CreateBuffer(&IndexDesc, &IndexData, &IndexBuffer);
	if (FAILED(result)) return 0;


	ID3DBlob* VSBlob = nullptr;
	ID3DBlob* PSBlob = nullptr;
	ID3DBlob* ErrBlob = nullptr;

	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;

	result = D3DCompileFromFile(L"shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS_Main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VSBlob, &ErrBlob);
	if (FAILED(result)) return 0;
	result = D3DCompileFromFile(L"shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_Main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PSBlob, &ErrBlob);
	if (FAILED(result)) return 0;

	result = Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VS);
	if (FAILED(result)) return 0;
	result = Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PS);
	if (FAILED(result)) return 0;

	ID3D11InputLayout* InputLayout = nullptr;
	D3D11_INPUT_ELEMENT_DESC layout[] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	Device->CreateInputLayout(layout, 1, VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);


	VSBlob->Release();
	PSBlob->Release();

	D3D11_VIEWPORT ViewPort = {};

	ViewPort.TopLeftX = 0.f;
	ViewPort.TopLeftY = 0.f;
	ViewPort.Height = 720.f;
	ViewPort.Width = 1280.f;
	ViewPort.MaxDepth = 1.0f;
	ViewPort.MinDepth = 0.0f;
	UINT Stride = sizeof(Vertex);
	UINT Offset = 0;


	ID3D11Buffer* CameraBuffer = nullptr;
	ID3D11Buffer* ObjectBuffer = nullptr;

	D3D11_BUFFER_DESC CameraDesc = {};
	CameraDesc.ByteWidth = sizeof(CameraBufferData);
	CameraDesc.Usage = D3D11_USAGE_DYNAMIC;
	CameraDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CameraDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Device->CreateBuffer(&CameraDesc, nullptr, &CameraBuffer);
	if (FAILED(result)) return 0;

	D3D11_BUFFER_DESC ObjectDesc = {};
	ObjectDesc.ByteWidth = sizeof(ObjectBufferData);
	ObjectDesc.Usage = D3D11_USAGE_DYNAMIC;
	ObjectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ObjectDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Device->CreateBuffer(&ObjectDesc, nullptr, &ObjectBuffer);
	if (FAILED(result)) return 0;




	LARGE_INTEGER ticks, currentTime, prevTime;

	// 1초에 틱 수
	QueryPerformanceFrequency(&ticks);


	QueryPerformanceCounter(&prevTime);

	bool bRunning = true;
	MSG msg = { };

	XMVECTOR cameraPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f);
	XMVECTOR cameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX View = XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
	XMMATRIX Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1280.f / 720.f, 0.1f, 100.f);

	float Rotation = 0.0f;

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
			float deltaTime = static_cast<float>(
				static_cast<double>(currentTime.QuadPart - prevTime.QuadPart) /
				static_cast<double>(ticks.QuadPart));
			prevTime = currentTime;
			// UPDATE
			Rotation += deltaTime;
			XMMATRIX World = DirectX::XMMatrixRotationY(Rotation);

			D3D11_MAPPED_SUBRESOURCE MappedResource = {};
			result = DeviceContext->Map(CameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

			if (FAILED(result))
			{
				bRunning = false;
				break;
			}

			CameraBufferData* CameraData = static_cast<CameraBufferData*>(MappedResource.pData);
			DirectX::XMStoreFloat4x4(&CameraData->View, DirectX::XMMatrixTranspose(View));
			DirectX::XMStoreFloat4x4(&CameraData->Projection, DirectX::XMMatrixTranspose(Projection));
			DeviceContext->Unmap(CameraBuffer, 0);

			result = DeviceContext->Map(ObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

			if (FAILED(result))
			{
				bRunning = false;
				break;
			}

			ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
			DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
			DeviceContext->Unmap(ObjectBuffer, 0);

			// RENDER

			DeviceContext->OMSetRenderTargets(1, &RTV, DSV);
			DeviceContext->ClearRenderTargetView(RTV, clearColor);
			DeviceContext->ClearDepthStencilView(DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

			DeviceContext->RSSetViewports(1, &ViewPort);


			DeviceContext->IASetInputLayout(InputLayout);
			DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
			ID3D11Buffer* ConstantBuffers[] = { CameraBuffer,ObjectBuffer };
			DeviceContext->VSSetConstantBuffers(0, 2, ConstantBuffers);
			DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
			DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			DeviceContext->VSSetShader(VS, nullptr, 0);
			DeviceContext->PSSetShader(PS, nullptr, 0);

			DeviceContext->DrawIndexed(36, 0, 0);

			SwapChain->Present(1, 0);
		}


	}
	ObjectBuffer->Release();
	CameraBuffer->Release();
	IndexBuffer->Release();
	VertexBuffer->Release();
	InputLayout->Release();
	PS->Release();
	VS->Release();
	DSV->Release();
	DepthBuffer->Release();
	RTV->Release();
	DeviceContext->Release();
	SwapChain->Release();
	Device->Release();


	return 0;
}
