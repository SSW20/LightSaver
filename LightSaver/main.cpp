#include <windows.h>

#include "Timer.h"
#include "Windows.h"
#include "Graphics.h"
#include "LightSaverGame.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// 흐름 : 디바이스에서 입력 받고 물리적인 신호 전달 -> 드라이버에서 운영체제 메세지로 번역 및 전달 -> 운영체제에서 포커스, 커서 위치, 마우스 캡처 등을 이용해 대상 HWND를 결정 -> 해당 창을 가지고 있는 스레드의 메세지 큐에 메세지 넣기
	//			-> 스레드는 메세지 큐에서 하나씩 꺼내 hWnd를 판단 및 해당 창의 윈도우 프로시저 호출 (만약 hWnd가 null 이라면 스레드 메세지로 이는 따로 윈도우 프로시저를 호출 하지 않음)

	// 질문 : 그러면 프로그램과 Windows Api는 무엇인가? 답: 소스 코드를 빌드하면 EXE 프로그램 파일이 생성 -> EXE를 실행하면 Windows가 프로세스와 메인 스레드를 생성
	//									Windows API는 프로그램이 운영체제의 창과 메시지 기능을 사용하기 위한 함수 인터페이스
	//									프로그래머는 WinodwAPI를 통해 커스텀하여 코드를 작성



	LightSaverGame LightSaver;
	if (!LightSaver.Initialize(hInstance, hPrevInstance, lpCmdLine, nCmdShow)) return 0;

	return LightSaver.Run();
#if 0
	Windows window;
	if (!window.Initialize(hInstance, hPrevInstance, lpCmdLine, nCmdShow)) return 0;
	Graphics graphic;
	if (!graphic.Initialize(window.GetHWND())) return 0;

	float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };

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

	HRESULT result;

	result = graphic.Device->CreateBuffer(&VertexDesc, &VertexData, &VertexBuffer);
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

	result = graphic.Device->CreateBuffer(&IndexDesc, &IndexData, &IndexBuffer);
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

	result = graphic.Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VS);
	if (FAILED(result)) return 0;
	result = graphic.Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PS);
	if (FAILED(result)) return 0;

	ID3D11InputLayout* InputLayout = nullptr;
	D3D11_INPUT_ELEMENT_DESC layout[] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	graphic.Device->CreateInputLayout(layout, 1, VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);


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

	result = graphic.Device->CreateBuffer(&CameraDesc, nullptr, &CameraBuffer);
	if (FAILED(result)) return 0;

	D3D11_BUFFER_DESC ObjectDesc = {};
	ObjectDesc.ByteWidth = sizeof(ObjectBufferData);
	ObjectDesc.Usage = D3D11_USAGE_DYNAMIC;
	ObjectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ObjectDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = graphic.Device->CreateBuffer(&ObjectDesc, nullptr, &ObjectBuffer);
	if (FAILED(result)) return 0;

	bool bRunning = true;
	XMVECTOR cameraPosition = XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f);
	XMVECTOR cameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR cameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX View = XMMatrixLookAtLH(cameraPosition, cameraTarget, cameraUp);
	XMMATRIX Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 1280.f / 720.f, 0.1f, 100.f);

	float Rotation = 0.0f;

	Timer timer;

	while (bRunning && window.GetMSG().message != WM_QUIT)
	{
		if (!window.PeekMSG()) break;
		// deltaTime 구하기

		// UPDATE
		Rotation += timer.GetDeltaTime();
		XMMATRIX World = DirectX::XMMatrixRotationY(Rotation);

		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		result = graphic.DeviceContext->Map(CameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

		if (FAILED(result))
		{
			bRunning = false;
			break;
		}

		CameraBufferData* CameraData = static_cast<CameraBufferData*>(MappedResource.pData);
		DirectX::XMStoreFloat4x4(&CameraData->View, DirectX::XMMatrixTranspose(View));
		DirectX::XMStoreFloat4x4(&CameraData->Projection, DirectX::XMMatrixTranspose(Projection));
		graphic.DeviceContext->Unmap(CameraBuffer, 0);

		result = graphic.DeviceContext->Map(ObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

		if (FAILED(result))
		{
			bRunning = false;
			break;
		}

		ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
		DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
		graphic.DeviceContext->Unmap(ObjectBuffer, 0);

		// RENDER

		graphic.DeviceContext->OMSetRenderTargets(1, &graphic.RTV, graphic.DSV);
		graphic.DeviceContext->ClearRenderTargetView(graphic.RTV, clearColor);
		graphic.DeviceContext->ClearDepthStencilView(graphic.DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

		graphic.DeviceContext->RSSetViewports(1, &ViewPort);


		graphic.DeviceContext->IASetInputLayout(InputLayout);
		graphic.DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
		ID3D11Buffer* ConstantBuffers[] = { CameraBuffer,ObjectBuffer };
		graphic.DeviceContext->VSSetConstantBuffers(0, 2, ConstantBuffers);
		graphic.DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		graphic.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		graphic.DeviceContext->VSSetShader(VS, nullptr, 0);
		graphic.DeviceContext->PSSetShader(PS, nullptr, 0);

		graphic.DeviceContext->DrawIndexed(36, 0, 0);

		graphic.SwapChain->Present(1, 0);


	}
	ObjectBuffer->Release();
	CameraBuffer->Release();
	IndexBuffer->Release();
	VertexBuffer->Release();
	InputLayout->Release();
	PS->Release();
	VS->Release();

#endif

	return 0;
}
