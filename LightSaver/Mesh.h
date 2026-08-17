#pragma once
#include <d3d11.h>

struct Vertex
{
	float x;
	float y;
	float z;
};

class Mesh
{
public:
	bool Initialize(ID3D11Device* Device, const Vertex* vertices, UINT vertexCount, const UINT* indicies, UINT indexCount);
	void Bind(ID3D11DeviceContext* DeviceContext);
	UINT GetIndexCount() const { return IndexCount; }
	~Mesh();

private:
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	UINT Stride = sizeof(Vertex);
	UINT IndexCount = 0;
};
