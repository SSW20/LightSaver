#pragma once
#include <d3d11.h>
#include <string>
#include <vector>
#include <memory>
#include "Mesh.h"
#include "Texture.h"
#include <DirectXMath.h>
struct aiMesh;

struct ModelData
{
	std::unique_ptr<Mesh> MeshData;
	UINT MaterialIndex = 0;
};

struct MaterialData
{
	std::unique_ptr<Texture> DiffuseTexture;
	float SpecularStrength = 0.2f;
	float SpecularPower = 32.0f;
};

struct alignas(16) MaterialBufferData
{
	float SpecularStrength;
	float SpecularPower;
	DirectX::XMFLOAT2 Padding;
};

static_assert(sizeof(MaterialBufferData) % 16 == 0);
class Model
{
public:
	bool Initialize(ID3D11Device* Device, const std::string& FilePath);
	void Draw(ID3D11DeviceContext* DeviceContext, ID3D11Buffer* MaterialBuffer);
	

private:
	std::unique_ptr<Mesh> ProcessMesh(ID3D11Device* Device, aiMesh* SourceMesh);
	std::vector<ModelData> ModelDatas;
	std::vector<MaterialData> MaterialDatas;

};

