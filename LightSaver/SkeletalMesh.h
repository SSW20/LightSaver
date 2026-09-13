#pragma once
#include <DirectXMath.h>
#include <wtypes.h>
#include <d3d11.h>

using namespace DirectX;
struct SkeletalVertex
{
	XMFLOAT3 Position;
	XMFLOAT2 UV;
	XMFLOAT3 Normal;

	UINT BoneIndices[4] = { 0,0,0,0 };
	float BoneWeights[4] = { 0.0f,0.0f,0.0f,0.0f };

	bool AddBoneInfluence(UINT BoneIndex, float BoneWeight)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (BoneWeights[i] == 0.0f)
			{
				BoneIndices[i] = BoneIndex;
				BoneWeights[i] = BoneWeight;
				return true;
			}
		}
		return false;
	}
};

class SkeletalMesh
{
public:
	bool Initialize(ID3D11Device* Device, const SkeletalVertex* vertices, UINT vertexCount, const UINT* indicies, UINT indexCount);
	void Bind(ID3D11DeviceContext* DeviceContext);
	UINT GetIndexCount() const { return IndexCount; }
	~SkeletalMesh();

private:
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	UINT Stride = sizeof(SkeletalVertex);
	UINT IndexCount = 0;

};

