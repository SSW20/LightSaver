#pragma once
#include "GameLoop.h"
#include <dxgi.h>
#include <DirectXMath.h>
#include "Camera.h"

struct Vertex
{
	float x;
	float y;
	float z;
};

struct alignas(16) CameraBufferData
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Projection;
};

struct alignas(16) ObjectBufferData
{
	DirectX::XMFLOAT4X4 World;
};

static_assert(sizeof(CameraBufferData) % 16 == 0);
static_assert(sizeof(ObjectBufferData) % 16 == 0);

class LightSaverGame : public GameLoop
{
public:
	virtual bool OnInitialize() override;
	virtual void Update(float deltaTime) override;
	virtual bool Render() override;

	~LightSaverGame() override;

private:
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
	D3D11_VIEWPORT ViewPort = {};
	UINT Stride = sizeof(Vertex);
	UINT Offset = 0;
	ID3D11Buffer* CameraBuffer = nullptr;
	ID3D11Buffer* ObjectBuffer = nullptr;
	float Rotation = 0.0f;
	float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
	Camera MainCamera;
	float CameraSpeed = 3.0f;
	// 픽셀당 회전할 라디안 값
	float MouseSpeed = DirectX::XMConvertToRadians(0.1f);
};
