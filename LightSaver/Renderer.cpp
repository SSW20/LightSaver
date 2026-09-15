
#include "Renderer.h"
#include "Graphics.h"
#include "Animator.h"
#include "SkeletalModel.h"

Renderer::~Renderer()
{
	if (ObjectBuffer != nullptr) ObjectBuffer->Release();
	if (MaterialBuffer != nullptr) MaterialBuffer->Release();
	if (LightBuffer != nullptr) LightBuffer->Release();
	if (FogBuffer != nullptr) FogBuffer->Release();
	if (CameraBuffer != nullptr) CameraBuffer->Release();
	if (BoneBuffer != nullptr) BoneBuffer->Release();
}

bool Renderer::Initialize(Graphics& InGraphics)
{
	ViewPort.TopLeftX = 0.f;
	ViewPort.TopLeftY = 0.f;
	ViewPort.Height = InGraphics.Height;
	ViewPort.Width = InGraphics.Width;
	ViewPort.MaxDepth = 1.0f;
	ViewPort.MinDepth = 0.0f;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 },
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA,0 }
	};

	Graphic = &InGraphics;

	if (!ShaderSet.Initialize(Graphic->Device, L"shader.hlsl", layout, 3))
	{
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC SkeletalLayout[] = {
		{"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	if (!SkeletalShader.Initialize(Graphic->Device, L"AnimShader.hlsl", SkeletalLayout, 5))
	{
		return false;
	}
	SetBuffers();
	return true;
}

bool Renderer::Render(const World& WorldSet, Camera& MainCamera, bool bFlashlightOn)
{
	Graphic->DeviceContext->OMSetRenderTargets(1, &Graphic->RTV, Graphic->DSV);
	Graphic->DeviceContext->ClearRenderTargetView(Graphic->RTV, clearColor);
	Graphic->DeviceContext->ClearDepthStencilView(Graphic->DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
	Graphic->DeviceContext->RSSetViewports(1, &ViewPort);
	Graphic->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ShaderSet.Bind(Graphic->DeviceContext);

	UpdateBuffers(MainCamera, bFlashlightOn);
	DrawWorld(WorldSet);
	return true;

}

bool Renderer::UpdateBoneBuffer(const Animator& AnimatorSet)
{
	const auto& BoneMatrices = AnimatorSet.GetFinalBoneMatrices();

	if (BoneMatrices.size() > 128)
	{
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE
		MappedResource = {};

	HRESULT Result = Graphic->DeviceContext->Map(
			BoneBuffer,
			0,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&MappedResource);

	if (FAILED(Result)) return false;

	BoneBufferData* BufferData = static_cast<BoneBufferData*>(MappedResource.pData);
	for (size_t i = 0; i < BoneMatrices.size(); ++i)
	{
		XMMATRIX BoneMatrix = XMLoadFloat4x4(&BoneMatrices[i]);
		XMStoreFloat4x4(&BufferData->BoneMatrices[i], XMMatrixTranspose(BoneMatrix));
	}

	Graphic->DeviceContext->Unmap(BoneBuffer,0);
	Graphic->DeviceContext->VSSetConstantBuffers(5,1,&BoneBuffer);

	return true;
}

bool Renderer::DrawWorld(const World& WorldSet)
{
	std::vector<RenderObject> RenderObjects;
	WorldSet.CollectRenderObjects(RenderObjects);
	for (const auto& RenderObj : RenderObjects)
	{
		const DirectX::XMMATRIX World = RenderObj.ModelWorldTransform.GetWorldMatrix();

		// 기존 정적 모델은 일반 Shader로
		if (RenderObj.ModelSet != nullptr)
		{
			if (!DrawModel(*RenderObj.ModelSet, World)) return false;
			continue;
		}

		// 애니메이션 모델은 AnimShader로
		if (RenderObj.SkeletalModelSet != nullptr && RenderObj.AnimatorSet != nullptr)
		{
			if (!DrawSkeletalModel(*RenderObj.SkeletalModelSet, *RenderObj.AnimatorSet, World)) return false;
		}
	}
	return true;
}

bool Renderer::DrawModel(Model& ModelSet, const DirectX::XMMATRIX& World)
{
	// 이전 물체가 SkeletalShader를 사용했을 수 있으므로 일반 Shader로 되돌린다.
	ShaderSet.Bind(Graphic->DeviceContext);

	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	HRESULT result;
	result = Graphic->DeviceContext->Map(ObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(result)) return false;

	ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
	Graphic->DeviceContext->Unmap(ObjectBuffer, 0);
	ModelSet.Draw(Graphic->DeviceContext, MaterialBuffer);
	return true;
}

bool Renderer::DrawSkeletalModel(SkeletalModel& ModelSet, const Animator& AnimatorSet, const DirectX::XMMATRIX& World)
{
	// Bone 정보가 포함된 정점 형식을 처리하는 애니메이션 Shader를 선택한다.
	SkeletalShader.Bind(Graphic->DeviceContext);

	// 몬스터 전체의 위치, 회전, 크기를 나타내는 World 행렬을 GPU에 전달한다.
	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	HRESULT Result = Graphic->DeviceContext->Map(
		ObjectBuffer,
		0,
		D3D11_MAP_WRITE_DISCARD,
		0,
		&MappedResource);

	if (FAILED(Result)) return false;

	ObjectBufferData* ObjectData = static_cast<ObjectBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat4x4(&ObjectData->World, DirectX::XMMatrixTranspose(World));
	Graphic->DeviceContext->Unmap(ObjectBuffer, 0);

	// Animator가 계산한 현재 프레임의 Bone 행렬들을 Vertex Shader의 b5에 연결한다.
	if (!UpdateBoneBuffer(AnimatorSet)) return false;

	// SkeletalModel이 가진 모든 SkeletalMesh의 버퍼를 연결하고 DrawIndexed를 호출한다.
	ModelSet.Draw(Graphic->DeviceContext);
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

	D3D11_BUFFER_DESC FogDesc = {};
	FogDesc.ByteWidth = sizeof(FogBufferData);
	FogDesc.Usage = D3D11_USAGE_DYNAMIC;
	FogDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	FogDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	result = Graphic->Device->CreateBuffer(&FogDesc, nullptr, &FogBuffer);
	if (FAILED(result)) return false;

	ID3D11Buffer* VsBuffers[] = { CameraBuffer, ObjectBuffer };
	ID3D11Buffer* PsBuffers[] = { LightBuffer, MaterialBuffer, FogBuffer };

	Graphic->DeviceContext->VSSetConstantBuffers(0, 2, VsBuffers);
	Graphic->DeviceContext->PSSetConstantBuffers(2, 3, PsBuffers);

	D3D11_BUFFER_DESC BoneDesc = {};

	BoneDesc.ByteWidth = sizeof(BoneBufferData);
	BoneDesc.Usage = D3D11_USAGE_DYNAMIC;
	BoneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	BoneDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Graphic->Device->CreateBuffer(&BoneDesc, nullptr, &BoneBuffer);

	if (FAILED(result)) return false;

	return true;
}

bool Renderer::UpdateBuffers(Camera& MainCamera, bool bFlashlightOn)
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
	LightData->LightEnabled = bFlashlightOn ? 1.0f : 0.0f;
	LightData->Padding = 0.0f;
	Graphic->DeviceContext->Unmap(LightBuffer, 0);

	result = Graphic->DeviceContext->Map(FogBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(result)) return false;

	FogBufferData* FogData = static_cast<FogBufferData*>(MappedResource.pData);
	DirectX::XMStoreFloat3(&FogData->CameraPosition, MainCamera.GetCameraPosition());
	FogData->FogDensity = 0.12f;
	FogData->FogColor = { clearColor[0], clearColor[1], clearColor[2] };
	FogData->Padding = 0.0f;
	Graphic->DeviceContext->Unmap(FogBuffer, 0);

	return true;
}
