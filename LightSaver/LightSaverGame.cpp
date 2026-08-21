#include "LightSaverGame.h"
#include "Shader.h"
#include <cmath>
#include <vector>
bool LightSaverGame::OnInitialize()
{

	//std::vector<Vertex> vertices;
	//std::vector<UINT> indices;

	//auto AddFace = [&](const DirectX::XMFLOAT3& v0, const DirectX::XMFLOAT3& v1, const DirectX::XMFLOAT3& v2, const DirectX::XMFLOAT3& v3)
	//	{
	//		UINT baseIndex = vertices.size();

	//		vertices.push_back({ v0.x, v0.y, v0.z, 0.0f, 1.0f });
	//		vertices.push_back({ v1.x, v1.y, v1.z, 0.0f, 0.0f });
	//		vertices.push_back({ v2.x, v2.y, v2.z, 1.0f, 0.0f });
	//		vertices.push_back({ v3.x, v3.y, v3.z, 1.0f, 1.0f });

	//		indices.push_back(baseIndex + 0);
	//		indices.push_back(baseIndex + 1);
	//		indices.push_back(baseIndex + 2);
	//		indices.push_back(baseIndex + 0);
	//		indices.push_back(baseIndex + 2);
	//		indices.push_back(baseIndex + 3);
	//	};

	//// 앞면
	//AddFace({ -0.5f, -0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f });
	//// 뒷면
	//AddFace({ 0.5f, -0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f }, { -0.5f, -0.5f, 0.5f });
	//// 왼쪽
	//AddFace({ -0.5f, -0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f });
	//// 오른쪽
	//AddFace({ 0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { 0.5f, 0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f });
	//// 아래
	//AddFace({ -0.5f, -0.5f, 0.5f }, { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, 0.5f });
	//// 위
	//AddFace({ -0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, -0.5f });

	HRESULT result;

	/*if (!MeshSet.Initialize(GetGraphics().Device, vertices.data(), 24, indices.data(), 36))
	{
		return false;
	}*/

	if (!ShaderSet.Initialize(GetGraphics().Device, L"shader.hlsl"))
	{
		return false;
	}
	ViewPort.TopLeftX = 0.f;
	ViewPort.TopLeftY = 0.f;
	ViewPort.Height = 720.f;
	ViewPort.Width = 1280.f;
	ViewPort.MaxDepth = 1.0f;
	ViewPort.MinDepth = 0.0f;


	if (!SpiderModel.Initialize(GetGraphics().Device, "Assets/Models/Spider/spider.obj")) return false;
	if (!WallModel.Initialize(GetGraphics().Device, "Assets/Models/Room/Wall.obj")) return false;
	if (!FloorModel.Initialize(GetGraphics().Device, "Assets/Models/Room/Floor.obj")) return false;

	Transform SpiderTransform, FloorTransform, WallTransform;
	SpiderTransform.Scale = { 0.01f, 0.01f, 0.01f };
	WallTransform.Position = { 0.0f, -0.45f, 4.0f };
	WallTransform.Scale = { 12.0f, 1.0f, 12.0f };
	FloorTransform.Position = { 0.0f, 1.8f, 10.0f };
	FloorTransform.Scale = { 12.0f, 4.5f, 1.0f };

	SpiderRenderObj.ModelSet = &SpiderModel;
	SpiderRenderObj.ModelWorldTransform = SpiderTransform;
	WallRenderObj.ModelSet = &WallModel;
	WallRenderObj.ModelWorldTransform = FloorTransform;
	FloorRenderObj.ModelSet = &FloorModel;
	FloorRenderObj.ModelWorldTransform = WallTransform;

	RenderObjects.push_back(&SpiderRenderObj);
	RenderObjects.push_back(&FloorRenderObj);
	RenderObjects.push_back(&WallRenderObj);

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

	D3D11_BUFFER_DESC LightDesc = {};
	LightDesc.ByteWidth = sizeof(LightBufferData);
	LightDesc.Usage = D3D11_USAGE_DYNAMIC;
	LightDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	LightDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = GetGraphics().Device->CreateBuffer(&LightDesc, nullptr, &LightBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC MaterialDesc = {};
	MaterialDesc.ByteWidth = sizeof(MaterialBufferData);
	MaterialDesc.Usage = D3D11_USAGE_DYNAMIC;
	MaterialDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	MaterialDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = GetGraphics().Device->CreateBuffer(&MaterialDesc, nullptr, &MaterialBuffer);
	if (FAILED(result)) return false;

	ShowCursor(FALSE);
	return true;
}

void LightSaverGame::Update(float deltaTime)
{
	Rotation += deltaTime;
	SpiderRenderObj.ModelWorldTransform.Rotation.y = Rotation;

	if (GetForegroundWindow() != GetWindow().GetHWND()) return;

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


	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	result = GetGraphics().DeviceContext->Map(CameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

	if (FAILED(result)) return false;

	CameraBufferData* CameraData = static_cast<CameraBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&CameraData->View, DirectX::XMMatrixTranspose(MainCamera.GetViewMatrix()));
	DirectX::XMStoreFloat4x4(&CameraData->Projection, DirectX::XMMatrixTranspose(MainCamera.GetProjectionMatrix()));
	GetGraphics().DeviceContext->Unmap(CameraBuffer, 0);

	result = GetGraphics().DeviceContext->Map(LightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(result)) return false;
	LightBufferData* LightData = static_cast<LightBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat3(&LightData->SpotDirection, MainCamera.GetForwardVector());
	LightData->AmbientStrength = 0.1f;
	DirectX::XMStoreFloat3(&LightData->LightColor, { 1.0f,1.0f,1.0f });
	LightData->DiffuseStrength = 0.95f;
	DirectX::XMStoreFloat3(&LightData->LightPosition, MainCamera.GetCameraPosition());
	LightData->LightRange = 30.0f;
	LightData->SpotOuterCos = std::cos(DirectX::XMConvertToRadians(30.0f));
	LightData->SpotInnerCos = std::cos(DirectX::XMConvertToRadians(7.0f));
	LightData->Padding = { 0.0f, 0.0f };
	GetGraphics().DeviceContext->Unmap(LightBuffer, 0);

	GetGraphics().DeviceContext->OMSetRenderTargets(1, &GetGraphics().RTV, GetGraphics().DSV);
	GetGraphics().DeviceContext->ClearRenderTargetView(GetGraphics().RTV, clearColor);
	GetGraphics().DeviceContext->ClearDepthStencilView(GetGraphics().DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	GetGraphics().DeviceContext->RSSetViewports(1, &ViewPort);

	ID3D11Buffer* ConstantBuffers[] = { CameraBuffer,ObjectBuffer, LightBuffer };
	GetGraphics().DeviceContext->VSSetConstantBuffers(0, 3, ConstantBuffers);
	GetGraphics().DeviceContext->PSSetConstantBuffers(2, 1, &LightBuffer);
	GetGraphics().DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ShaderSet.Bind(GetGraphics().DeviceContext);

	for (RenderObject* RenderObj : RenderObjects)
	{
		if (RenderObj == nullptr) continue;
		if (!DrawModel(*RenderObj->ModelSet, RenderObj->ModelWorldTransform.GetWorldMatrix())) return false;
	}

	return true;
}

bool LightSaverGame::DrawModel(Model& ModelSet, const DirectX::XMMATRIX& World)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	HRESULT result;
	result = GetGraphics().DeviceContext->Map(ObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(result)) return false;
	ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
	GetGraphics().DeviceContext->Unmap(ObjectBuffer, 0);
	ModelSet.Draw(GetGraphics().DeviceContext, MaterialBuffer);
	return true;
}

LightSaverGame::~LightSaverGame()
{
	if (ObjectBuffer != nullptr) ObjectBuffer->Release();
	if (CameraBuffer != nullptr) CameraBuffer->Release();
	if (LightBuffer != nullptr) LightBuffer->Release();
	if (MaterialBuffer != nullptr) MaterialBuffer->Release();
}
