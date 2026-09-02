#include "UIRenderer.h"

bool UIRenderer::Initialize(Graphics& InGraphics)
{
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	Graphic = &InGraphics;

	if (!UIShader.Initialize(Graphic->Device, L"UIShader.hlsl", layout, 3))
	{
		return false;
	}
	WhiteTexture.InitializeByColor(Graphic->Device, 255, 255, 255, 255);


	HRESULT result;
	D3D11_BUFFER_DESC UIDesc = {};
	UIDesc.ByteWidth = sizeof(UIVertex) * MaxVertexCount;
	UIDesc.Usage = D3D11_USAGE_DYNAMIC;
	UIDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	UIDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	result = Graphic->Device->CreateBuffer(&UIDesc, nullptr, &UIVertexBuffer);
	if (FAILED(result)) return false;

	D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
	DepthDesc.DepthEnable = false;
	DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	DepthDesc.StencilEnable = false;
	result = Graphic->Device->CreateDepthStencilState(&DepthDesc, &DepthState);
	if (FAILED(result)) return false;

	D3D11_BLEND_DESC BlendDesc = {};
	BlendDesc.AlphaToCoverageEnable = false;
	BlendDesc.IndependentBlendEnable = false;

	D3D11_RENDER_TARGET_BLEND_DESC& RenderTargetBlend = BlendDesc.RenderTarget[0];
	RenderTargetBlend.BlendEnable = true;
	RenderTargetBlend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	RenderTargetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	RenderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
	RenderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
	RenderTargetBlend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	RenderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	RenderTargetBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	result = Graphic->Device->CreateBlendState(&BlendDesc, &AlphaBlendState);

	if (FAILED(result)) return false;

	Vertices.reserve(MaxVertexCount);

	return true;
}

void UIRenderer::AddRectangle(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color)
{
	AddQuad(Left, Top, Right, Bottom, Color, &WhiteTexture);
}

void UIRenderer::AddQuad(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color, Texture* TextureSet)
{
	if (Vertices.size() + 6 > MaxVertexCount) return;

	UINT StartVertex = Vertices.size();

	UIVertex v1 = { {Left,Top,0},Color, { 0.0f, 0.0f } };
	UIVertex v2 = { {Right,Bottom,0},Color, { 1.0f, 1.0f } };
	UIVertex v3 = { {Left,Bottom,0},Color,{ 0.0f, 1.0f } };
	UIVertex v4 = { {Right,Top,0},Color,{ 1.0f, 0.0f } };

	UIVertex TempVertices[] = { v1,v2,v3,v4 };
	int Indices[] = { 1,2,3,1,4,2 };

	for (int i = 0; i < 6; ++i)
	{
		Vertices.push_back(TempVertices[Indices[i] - 1]);
	}

	if (!Batches.empty() && Batches.back().TextureSet == TextureSet)
	{
		Batches.back().VertexCount += 6;
		return;
	}

	UIBatch NewBatch;
	NewBatch.TextureSet = TextureSet;
	NewBatch.StartVertex = StartVertex;
	NewBatch.VertexCount = 6;
	Batches.push_back(NewBatch);
}

void UIRenderer::AddRectanglePixels(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color)
{
	float ScreenWidth = Graphic->Width;
	float ScreenHeight = Graphic->Height;

	float ClipLeft = Left / ScreenWidth * 2.0f - 1.0f;
	float ClipRight = Right / ScreenWidth * 2.0f - 1.0f;
	float ClipTop = 1.0f - Top / ScreenHeight * 2.0f;
	float ClipBottom = 1.0f - Bottom / ScreenHeight * 2.0f;

	AddRectangle(ClipLeft, ClipTop, ClipRight, ClipBottom, Color);
}

void UIRenderer::AddRectanglePixelsImage(float Left, float Top, float Right, float Bottom, const XMFLOAT4& Color, Texture* TextureSet)
{
	float ScreenWidth = Graphic->Width;
	float ScreenHeight = Graphic->Height;

	float ClipLeft = Left / ScreenWidth * 2.0f - 1.0f;
	float ClipRight = Right / ScreenWidth * 2.0f - 1.0f;
	float ClipTop = 1.0f - Top / ScreenHeight * 2.0f;
	float ClipBottom = 1.0f - Bottom / ScreenHeight * 2.0f;

	AddQuad(ClipLeft, ClipTop, ClipRight, ClipBottom, Color, TextureSet);
}

bool UIRenderer::EndFrame()
{
	if (Vertices.empty()) return true;
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	result = Graphic->DeviceContext->Map(
		UIVertexBuffer,
		0,
		D3D11_MAP_WRITE_DISCARD,
		0,
		&MappedResource
	);
	if (FAILED(result)) return false;

	memcpy_s(MappedResource.pData, sizeof(UIVertex) * MaxVertexCount, Vertices.data(), sizeof(UIVertex) * Vertices.size());
	Graphic->DeviceContext->Unmap(UIVertexBuffer, 0);

	UINT Stride = sizeof(UIVertex);
	UINT Offset = 0;

	ID3D11DepthStencilState* PreviousDepthState = nullptr;
	UINT PreviousStencilReference = 0;

	ID3D11BlendState* PreviousBlendState = nullptr;
	float PreviousBlendFactor[4] = {};
	UINT PreviousSampleMask = 0;

	Graphic->DeviceContext->OMGetBlendState(&PreviousBlendState,PreviousBlendFactor,&PreviousSampleMask);
	Graphic->DeviceContext->OMGetDepthStencilState(&PreviousDepthState, &PreviousStencilReference);

	UIShader.Bind(Graphic->DeviceContext);
	Graphic->DeviceContext->IASetVertexBuffers(0, 1, &UIVertexBuffer, &Stride, &Offset);
	Graphic->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Graphic->DeviceContext->OMSetDepthStencilState(DepthState, 0);
	Graphic->DeviceContext->OMSetBlendState(AlphaBlendState, nullptr, 0xffffffff);

	for (auto& Batch : Batches)
	{
		Batch.TextureSet->Bind(Graphic->DeviceContext);
		Graphic->DeviceContext->Draw(Batch.VertexCount, Batch.StartVertex);
	}


	Graphic->DeviceContext->OMSetDepthStencilState(PreviousDepthState, PreviousStencilReference);
	Graphic->DeviceContext->OMSetBlendState(PreviousBlendState,PreviousBlendFactor,PreviousSampleMask);

	if (PreviousDepthState != nullptr)
	{
		PreviousDepthState->Release();
	}
	if (PreviousBlendState != nullptr)
	{
		PreviousBlendState->Release();
	}


	return true;
}
UIRenderer::~UIRenderer()
{
	if (UIVertexBuffer != nullptr)
	{
		UIVertexBuffer->Release();
	}
	if (DepthState != nullptr)
	{
		DepthState->Release();
	}
	if (AlphaBlendState != nullptr)
	{
		AlphaBlendState->Release();
	}
}
void UIRenderer::BeginFrame()
{
	Vertices.clear();
	Batches.clear();
}
