#include "LightSaverGame.h"
#include <cmath>
bool LightSaverGame::OnInitialize()
{
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
	D3D11_BUFFER_DESC VertexDesc = {};
	D3D11_SUBRESOURCE_DATA VertexData = {};

	VertexDesc.ByteWidth = sizeof(vertices);
	VertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	VertexData.pSysMem = vertices;

	HRESULT result;

	result = GetGraphics().Device->CreateBuffer(&VertexDesc, &VertexData, &VertexBuffer);
	if (FAILED(result)) return false;

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


	D3D11_BUFFER_DESC IndexDesc = {};
	IndexDesc.ByteWidth = sizeof(indices);
	IndexDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexData = {};
	IndexData.pSysMem = indices;

	result = GetGraphics().Device->CreateBuffer(&IndexDesc, &IndexData, &IndexBuffer);
	if (FAILED(result)) return false;

	ID3DBlob* VSBlob = nullptr;
	ID3DBlob* PSBlob = nullptr;
	ID3DBlob* ErrBlob = nullptr;

	result = D3DCompileFromFile(L"shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS_Main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VSBlob, &ErrBlob);
	if (FAILED(result)) return false;
	result = D3DCompileFromFile(L"shader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_Main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PSBlob, &ErrBlob);
	if (FAILED(result)) return false;

	result = GetGraphics().Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VS);
	if (FAILED(result)) return false;
	result = GetGraphics().Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PS);
	if (FAILED(result)) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	result = GetGraphics().Device->CreateInputLayout(layout, 1, VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);

	if (FAILED(result)) return false;

	VSBlob->Release();
	PSBlob->Release();

	ViewPort.TopLeftX = 0.f;
	ViewPort.TopLeftY = 0.f;
	ViewPort.Height = 720.f;
	ViewPort.Width = 1280.f;
	ViewPort.MaxDepth = 1.0f;
	ViewPort.MinDepth = 0.0f;

	D3D11_BUFFER_DESC CameraDesc = {};
	CameraDesc.ByteWidth = sizeof(CameraBufferData);
	CameraDesc.Usage = D3D11_USAGE_DYNAMIC;
	CameraDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CameraDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = GetGraphics().Device->CreateBuffer(&CameraDesc, nullptr, &CameraBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC ObjectDesc = {};
	ObjectDesc.ByteWidth = sizeof(ObjectBufferData);
	ObjectDesc.Usage = D3D11_USAGE_DYNAMIC;
	ObjectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ObjectDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = GetGraphics().Device->CreateBuffer(&ObjectDesc, nullptr, &ObjectBuffer);
	if (FAILED(result)) return false;

	ShowCursor(FALSE);
    return true;
}

void LightSaverGame::Update(float deltaTime)
{
	Rotation += deltaTime;
	float ForwardInput = 0.f;
	float RightInput = 0.f;
	if (GetAsyncKeyState('W') & 0x8000)
	{
		ForwardInput += 1.f;
	}
	if (GetAsyncKeyState('A') & 0x8000)
	{
		RightInput -= 1.f;
	}
	if (GetAsyncKeyState('S') & 0x8000)
	{
		ForwardInput -= 1.f;
	}
	if (GetAsyncKeyState('D') & 0x8000)
	{
		RightInput += 1.f;
	}

	float InputDistance = std::sqrt(ForwardInput * ForwardInput + RightInput * RightInput);
	if (InputDistance > 1.0f)
	{
		ForwardInput /= InputDistance;
		RightInput /= InputDistance;
	}
	float MoveDistance = CameraSpeed * deltaTime;
	MainCamera.AddForward(ForwardInput * MoveDistance);
	MainCamera.AddRight(RightInput * MoveDistance);

	RECT ClientSize;
	GetClientRect(GetWindow().GetHWND(), &ClientSize);

	POINT Center;
	Center.x = (ClientSize.left + ClientSize.right) / 2;
	Center.y = (ClientSize.top + ClientSize.bottom) / 2;

	ClientToScreen(GetWindow().GetHWND(), &Center);

	POINT MousePos;
	GetCursorPos(&MousePos);

	long DeltaX = MousePos.x - Center.x;
	long DeltaY = MousePos.y - Center.y;

	MainCamera.AddRotation(DeltaX * MouseSpeed, -DeltaY * MouseSpeed);
	SetCursorPos(Center.x, Center.y);
}

bool LightSaverGame::Render()
{
	HRESULT result;

	DirectX::XMMATRIX World = DirectX::XMMatrixRotationY(Rotation);

	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	result = GetGraphics().DeviceContext->Map(CameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

	if (FAILED(result)) return false;

	CameraBufferData* CameraData = static_cast<CameraBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&CameraData->View, DirectX::XMMatrixTranspose(MainCamera.GetViewMatrix()));
	DirectX::XMStoreFloat4x4(&CameraData->Projection, DirectX::XMMatrixTranspose(MainCamera.GetProjectionMatrix()));
	GetGraphics().DeviceContext->Unmap(CameraBuffer, 0);

	result = GetGraphics().DeviceContext->Map(ObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

	if (FAILED(result)) return false;
	ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
	GetGraphics().DeviceContext->Unmap(ObjectBuffer, 0);

	GetGraphics().DeviceContext->OMSetRenderTargets(1, &GetGraphics().RTV, GetGraphics().DSV);
	GetGraphics().DeviceContext->ClearRenderTargetView(GetGraphics().RTV, clearColor);
	GetGraphics().DeviceContext->ClearDepthStencilView(GetGraphics().DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	GetGraphics().DeviceContext->RSSetViewports(1, &ViewPort);


	GetGraphics().DeviceContext->IASetInputLayout(InputLayout);
	GetGraphics().DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	ID3D11Buffer* ConstantBuffers[] = { CameraBuffer,ObjectBuffer };
	GetGraphics().DeviceContext->VSSetConstantBuffers(0, 2, ConstantBuffers);
	GetGraphics().DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	GetGraphics().DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	GetGraphics().DeviceContext->VSSetShader(VS, nullptr, 0);
	GetGraphics().DeviceContext->PSSetShader(PS, nullptr, 0);

	GetGraphics().DeviceContext->DrawIndexed(36, 0, 0);
    return true;
}

LightSaverGame::~LightSaverGame()
{
	if (ObjectBuffer != nullptr) ObjectBuffer->Release();
	if (CameraBuffer != nullptr) CameraBuffer->Release();
	if (IndexBuffer != nullptr) IndexBuffer->Release();
	if (VertexBuffer != nullptr) VertexBuffer->Release();
	if (InputLayout != nullptr) InputLayout->Release();
	if (PS != nullptr) PS->Release();
	if (VS != nullptr) VS->Release();
}
