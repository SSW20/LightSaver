#pragma once
#include <DirectXMath.h>
#include "Graphics.h"
#include <vector>
#include "Shader.h"
#include "Texture.h"
using namespace DirectX;

struct UIVertex
{
	XMFLOAT3 Position;
	XMFLOAT4 Color;
	XMFLOAT2 UV;
};

struct UIBatch
{
	Texture* TextureSet = nullptr;
	UINT StartVertex = 0;
	UINT VertexCount = 0;
};

class UIRenderer
{
public:
	bool Initialize(Graphics& InGraphics);
	void BeginFrame();
	void AddRectangle(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color);
	void AddRectanglePixels(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color);
	void AddRectanglePixelsImage(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color, Texture* TextureSet);

	bool EndFrame();
	UIRenderer() = default;
	~UIRenderer();
	UIRenderer(const UIRenderer&) = delete;
	UIRenderer& operator=(const UIRenderer&) = delete;
	const Graphics* GetGraphics() const { return Graphic; }
private:
	void AddQuad(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color, Texture* TextureSet);

	Graphics* Graphic = nullptr;
	ID3D11Buffer* UIVertexBuffer = nullptr;
	Shader UIShader;
	ID3D11DepthStencilState* DepthState = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;

	std::vector<UIVertex> Vertices;
	UINT MaxVertexCount = 256;
	std::vector<UIBatch> Batches;

	Texture WhiteTexture;

};


