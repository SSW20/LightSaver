#include "Mesh.h"

bool Mesh::Initialize(ID3D11Device* Device, const Vertex* vertices, UINT vertexCount, const UINT* indicies, UINT indexCount)
{
	D3D11_BUFFER_DESC VertexBufferDesc = {};
	D3D11_SUBRESOURCE_DATA VertexData = {};

	VertexBufferDesc.ByteWidth = sizeof(Vertex) * vertexCount;
	VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	VertexData.pSysMem = vertices;

	HRESULT result;
	result = Device->CreateBuffer(&VertexBufferDesc, &VertexData, &VertexBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC IndexBufferDesc = {};
	D3D11_SUBRESOURCE_DATA IndexData = {};

	IndexBufferDesc.ByteWidth = sizeof(UINT) * indexCount;
	IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	IndexCount = indexCount;

	IndexData.pSysMem = indicies;
	result = Device->CreateBuffer(&IndexBufferDesc, &IndexData, &IndexBuffer);
	if (FAILED(result)) return false;
	return true;
}

void Mesh::Bind(ID3D11DeviceContext* DeviceContext)
{
	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, Offset);
}

Mesh::~Mesh()
{
	if (VertexBuffer != nullptr)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}
	if (IndexBuffer != nullptr)
	{
		IndexBuffer->Release();
		IndexBuffer = nullptr;
	}
}
