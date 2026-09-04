#pragma once
#include "World.h"
#include <DirectXMath.h>
#include "Model.h"
#include "Camera.h"
#include <cmath>
#include "Shader.h"
class Graphics;

struct alignas(16) ObjectBufferData
{
	DirectX::XMFLOAT4X4 World;
};
struct alignas(16) CameraBufferData
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Projection;
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
	float LightEnabled;
	float Padding;
};

struct alignas(16) FogBufferData
{
	DirectX::XMFLOAT3 CameraPosition;
	float FogDensity;

	DirectX::XMFLOAT3 FogColor;
	float Padding;
};

static_assert(sizeof(CameraBufferData) % 16 == 0);
static_assert(sizeof(ObjectBufferData) % 16 == 0);
static_assert(sizeof(LightBufferData) % 16 == 0);
static_assert(sizeof(FogBufferData) % 16 == 0);

class Renderer
{
public:
	Renderer() = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(const Renderer&) = delete;
	~Renderer();
		
	bool Initialize(Graphics& InGraphics);
	bool Render(const World& WorldSet, Camera& MainCamera, bool bFlashlightOn);
private:
	Graphics* Graphic = nullptr;
	ID3D11Buffer* ObjectBuffer = nullptr;
	ID3D11Buffer* MaterialBuffer = nullptr;
	ID3D11Buffer* LightBuffer = nullptr;
	ID3D11Buffer* FogBuffer = nullptr;
	ID3D11Buffer* CameraBuffer = nullptr;
	D3D11_VIEWPORT ViewPort = {};
	float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
	Shader ShaderSet;

	bool SetBuffers();
	bool UpdateBuffers(Camera& MainCamera, bool bFlashlightOn);
	bool DrawWorld(const World& WorldSet);
	bool DrawModel(Model& ModelSet, const DirectX::XMMATRIX& World);

};

