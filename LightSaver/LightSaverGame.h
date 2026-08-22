#pragma once
#include "GameLoop.h"
#include <dxgi.h>
#include <DirectXMath.h>
#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "Texture.h"
#include "Model.h"
#include "Transform.h"
#include "RenderObject.h"
#include "World.h"

class Actor;
class MeshComponent;
struct alignas(16) CameraBufferData
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Projection;
};

struct alignas(16) ObjectBufferData
{
	DirectX::XMFLOAT4X4 World;
};

struct alignas(16) LightBufferData
{
	DirectX::XMFLOAT3 SpotDirection;
	float AmbientStrength;

	DirectX::XMFLOAT3 LightColor;
	float DiffuseStrength;

	DirectX::XMFLOAT3 LightPosition;
	float LightRange;

	float SpotOuterCos;
	float SpotInnerCos;
	DirectX::XMFLOAT2 Padding;
};

static_assert(sizeof(CameraBufferData) % 16 == 0);
static_assert(sizeof(ObjectBufferData) % 16 == 0);
static_assert(sizeof(LightBufferData) % 16 == 0);

class LightSaverGame : public GameLoop
{
public:
	virtual bool OnInitialize() override;
	virtual void Update(float deltaTime) override;
	virtual bool Render() override;

	bool DrawModel(Model& ModelSet,const DirectX::XMMATRIX& World);
	~LightSaverGame() override;

private:

	D3D11_VIEWPORT ViewPort = {};
	ID3D11Buffer* CameraBuffer = nullptr;
	ID3D11Buffer* ObjectBuffer = nullptr;
	ID3D11Buffer* LightBuffer = nullptr;
	ID3D11Buffer* MaterialBuffer = nullptr;
	float Rotation = 0.0f;
	float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
	float CameraSpeed = 3.0f;
	// 픽셀당 회전할 라디안 값
	float MouseSpeed = DirectX::XMConvertToRadians(0.1f);
	Camera MainCamera;
	Shader ShaderSet;
	Model SpiderModel; 
	Model FloorModel; 
	Model WallModel;
	World GameWorld;
	RenderObject FloorRenderObj, WallRenderObj;
	std::vector<RenderObject*> RenderObjects;
	Actor* SpiderActor = nullptr;
	MeshComponent* SpiderMeshComponent = nullptr;
};
