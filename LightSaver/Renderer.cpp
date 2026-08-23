
#include "Renderer.h"
#include "Graphics.h"


Renderer::~Renderer()
{
	if (ObjectBuffer != nullptr) ObjectBuffer->Release();
	if (MaterialBuffer != nullptr) MaterialBuffer->Release();
	if (LightBuffer != nullptr) LightBuffer->Release();
	if (CameraBuffer != nullptr) CameraBuffer->Release();
}

bool Renderer::Initialize(Graphics& InGraphics)
{
	ViewPort.TopLeftX = 0.f;
	ViewPort.TopLeftY = 0.f;
	ViewPort.Height = 720.f;
	ViewPort.Width = 1280.f;
	ViewPort.MaxDepth = 1.0f;
	ViewPort.MinDepth = 0.0f;

	Graphic = &InGraphics;

	if (!ShaderSet.Initialize(Graphic->Device, L"shader.hlsl"))
	{
		return false;
	}

	SetBuffers();
	return true;
}

bool Renderer::Render(const World& WorldSet, Camera& MainCamera)
{

	Graphic->DeviceContext->OMSetRenderTargets(1, &Graphic->RTV, Graphic->DSV);
	Graphic->DeviceContext->ClearRenderTargetView(Graphic->RTV, clearColor);
	Graphic->DeviceContext->ClearDepthStencilView(Graphic->DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	Graphic->DeviceContext->RSSetViewports(1, &ViewPort);
	Graphic->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ShaderSet.Bind(Graphic->DeviceContext);

	UpdateBuffers(MainCamera);
	DrawWorld(WorldSet);
	return true;

}

bool Renderer::DrawWorld(const World& WorldSet)
{
	std::vector<RenderObject> RenderObjects;
	WorldSet.CollectRenderObjects(RenderObjects);
	for (const auto& RenderObj : RenderObjects)
	{
		if (RenderObj.ModelSet != nullptr)
		{
			DrawModel(*RenderObj.ModelSet, RenderObj.ModelWorldTransform.GetWorldMatrix());
		}
	}
	return true;
}

bool Renderer::DrawModel(Model& ModelSet, const DirectX::XMMATRIX& World)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	HRESULT result;
	result = Graphic->DeviceContext->Map(ObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(result)) return false;

	ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
	Graphic->DeviceContext->Unmap(ObjectBuffer, 0);
	ModelSet.Draw(Graphic->DeviceContext,  MaterialBuffer);
	return true;
}

bool Renderer::SetBuffers()
{
	HRESULT result;
	D3D11_BUFFER_DESC ObjectDesc = {};
	ObjectDesc.ByteWidth = sizeof(ObjectBufferData);
	ObjectDesc.Usage = D3D11_USAGE_DYNAMIC;
	ObjectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ObjectDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Graphic->Device->CreateBuffer(&ObjectDesc, nullptr, &ObjectBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC MaterialDesc = {};
	MaterialDesc.ByteWidth = sizeof(MaterialBufferData);
	MaterialDesc.Usage = D3D11_USAGE_DYNAMIC;
	MaterialDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	MaterialDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Graphic->Device->CreateBuffer(&MaterialDesc, nullptr, &MaterialBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC CameraDesc = {};
	CameraDesc.ByteWidth = sizeof(CameraBufferData);
	CameraDesc.Usage = D3D11_USAGE_DYNAMIC;
	CameraDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CameraDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Graphic->Device->CreateBuffer(&CameraDesc, nullptr, &CameraBuffer);
	if (FAILED(result)) return false;


	D3D11_BUFFER_DESC LightDesc = {};
	LightDesc.ByteWidth = sizeof(LightBufferData);
	LightDesc.Usage = D3D11_USAGE_DYNAMIC;
	LightDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	LightDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Graphic->Device->CreateBuffer(&LightDesc, nullptr, &LightBuffer);
	if (FAILED(result)) return false;

	ID3D11Buffer* VsBuffers[] = { CameraBuffer, ObjectBuffer };
	ID3D11Buffer* PsBuffers[] = { LightBuffer, MaterialBuffer };

	Graphic->DeviceContext->VSSetConstantBuffers(0, 2, VsBuffers);
	Graphic->DeviceContext->PSSetConstantBuffers(2, 2, PsBuffers);
	return true;
}

bool Renderer::UpdateBuffers(Camera& MainCamera)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	result = Graphic->DeviceContext->Map(CameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);


	if (FAILED(result)) return false;

	CameraBufferData* CameraData = static_cast<CameraBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&CameraData->View, DirectX::XMMatrixTranspose(MainCamera.GetViewMatrix()));
	DirectX::XMStoreFloat4x4(&CameraData->Projection, DirectX::XMMatrixTranspose(MainCamera.GetProjectionMatrix()));
	Graphic->DeviceContext->Unmap(CameraBuffer, 0);

	result = Graphic->DeviceContext->Map(LightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
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
	Graphic->DeviceContext->Unmap(LightBuffer, 0);
	return true;
}
