#pragma once
#include <d3d11.h>
#include <string>
#include <vector>
#include <memory>
#include "Mesh.h"
#include "Texture.h"
struct aiMesh;

struct ModelData
{
	std::unique_ptr<Mesh> MeshData;
	UINT MaterialIndex = 0;
};

struct MaterialData
{
	std::unique_ptr<Texture> DiffuseTexture;
};

class Model
{
public:
	bool Initialize(ID3D11Device* Device, const std::string& FilePath);
	void Draw(ID3D11DeviceContext* DeviceContext);
	

private:
	std::unique_ptr<Mesh> ProcessMesh(ID3D11Device* Device, aiMesh* SourceMesh);
	std::vector<ModelData> ModelDatas;
	std::vector<MaterialData> MaterialDatas;

};

