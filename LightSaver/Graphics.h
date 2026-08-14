#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
class Graphics
{
public:
	Graphics& operator=(const Graphics&) = delete;
	Graphics(const Graphics&) = delete;
	bool Initialize(HWND hWnd)
	{
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = 1280;
		sd.BufferDesc.Height = 720;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.SampleDesc.Count = 1;
		sd.Windowed = TRUE;
		sd.OutputWindow = hWnd;

		HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, NULL, &DeviceContext);
		if (FAILED(result)) return 0;

		ID3D11Texture2D* BackBuffer = nullptr;
		result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
		if (FAILED(result)) return 0;

		result = Device->CreateRenderTargetView(BackBuffer, NULL, &RTV);
		if (FAILED(result)) return 0;

		BackBuffer->Release();

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

		result = Device->CreateDepthStencilView(DepthBuffer, nullptr, &DSV);

		if (FAILED(result)) return 0;

		return 1;

	}
	Graphics() {};
	~Graphics()
	{
		if (DSV != nullptr) DSV->Release();
		if (DepthBuffer != nullptr) DepthBuffer->Release();
		if (RTV != nullptr) RTV->Release();
		if (DeviceContext != nullptr) DeviceContext->Release();
		if (SwapChain != nullptr) SwapChain->Release();
		if (Device != nullptr) Device->Release();
	}

	ID3D11Device* Device = nullptr;
	IDXGISwapChain* SwapChain = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11RenderTargetView* RTV = nullptr;
	ID3D11DepthStencilView* DSV = nullptr;
	ID3D11Texture2D* DepthBuffer = nullptr;
};
