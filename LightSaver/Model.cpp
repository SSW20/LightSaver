#include "Model.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <algorithm>


bool Model::Initialize(ID3D11Device* Device, const std::string& FilePath, const wchar_t* DefaultTexturePath)
{
	/*
		Triangulate
		→ 사각형·다각형을 삼각형으로 변환

		JoinIdenticalVertices
		→ 같은 정점을 합쳐 Index Buffer로 재사용

		MakeLeftHanded
		→ DirectX의 왼손 좌표계로 변환

		FlipWindingOrder
		→ 삼각형 앞면 방향을 DirectX 방식에 맞춤

		FlipUVs
		→ 텍스처의 위아래 방향을 DirectX 방식에 맞춤

		PreTransformVertices
		→  Mesh Local 정점 × Node 변환 × 부모 Node 변환 → Model Local 정점으로 다시 저장

		GenSmoothNormals
		→ Noraml이 있으면 쓰되 없으면 알아서

		aiScene
		├─ RootNode
		├─ Mesh 목록
		├─ Material 목록
		└─ Animation 목록

			├─ mRootNode       모델의 계층 구조
			├─ mMeshes[]       실제 정점·인덱스 데이터
			├─ mMaterials[]    재질 정보
			├─ mTextures[]     포함된 텍스처
			└─ mAnimations[]   애니메이션 정보

			Scene											aiScene*
			│
			└─ mMeshes								 aiMesh* 배열
				├─ mMeshes[0] ──────────→ 첫 번째 aiMesh
				├─ mMeshes[1] ──────────→ 두 번째 aiMesh
				└─ mMeshes[2] ──────────→ 세 번째 aiMesh

			첫 번째 aiMesh
				├─ mVertices                  위치 배열
				│  ├─ mVertices[0]
				│  ├─ mVertices[1]
				│  └─ mVertices[2]
				│
				└─ mTextureCoords             UV 채널 배열
					└─ mTextureCoords[0]       첫 번째 UV 채널
						├─ mTextureCoords[0][0]		채널 0의 정점 0 의 UV 좌표
						├─ mTextureCoords[0][1]
						└─ mTextureCoords[0][2]

				aiMesh
				├─ mVertices[]   실제 정점 데이터
				└─ mFaces[]      어떤 정점을 연결할지 나타내는 면 목록
					 ├─ mFaces[0].mIndices[] = { 0, 1, 2 }
					 └─ mFaces[1].mIndices[] = { 2, 1, 3 }
	*/
	Assimp::Importer Importer;
	const unsigned int ImportFlag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals;
	const aiScene* Scene = Importer.ReadFile(FilePath, ImportFlag);
	if (Scene == nullptr || Scene->mRootNode == nullptr || (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) return false;

	ModelDatas.clear();
	MaterialDatas.clear();
	MaterialDatas.resize(Scene->mNumMaterials);

	std::filesystem::path ModelPath = FilePath;
	for (UINT i = 0; i < Scene->mNumMaterials; ++i)
	{
		auto NewTexture = std::make_unique<Texture>();
		aiMaterial* SourceMaterial = Scene->mMaterials[i];
		aiString TexturePath;
		if (SourceMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &TexturePath) == AI_SUCCESS) 
		{
			std::filesystem::path FullTexturePath = ModelPath.parent_path() / TexturePath.C_Str();

			if (!NewTexture->Initialize(Device, FullTexturePath.c_str())) return false;
			MaterialDatas[i].DiffuseTexture = std::move(NewTexture);
		}
		else if (DefaultTexturePath != nullptr)
		{
			if (!NewTexture->Initialize(Device, DefaultTexturePath)) return false;
			MaterialDatas[i].DiffuseTexture = std::move(NewTexture);
		}
		else
		{
			NewTexture->InitializeByColor(Device, 255, 255, 255, 255);
			MaterialDatas[i].DiffuseTexture = std::move(NewTexture);
		}

		aiColor3D SpecularColor(0.2f, 0.2f, 0.2f);
		if (SourceMaterial->Get(AI_MATKEY_COLOR_SPECULAR, SpecularColor) == AI_SUCCESS)
		{
			MaterialDatas[i].SpecularStrength = std::max(SpecularColor.r, std::max(SpecularColor.g, SpecularColor.b));
		}
		float SpecularPower = 32.0f;
		if (SourceMaterial->Get(AI_MATKEY_SHININESS, SpecularPower) == AI_SUCCESS)
		{
			MaterialDatas[i].SpecularPower = std::max(SpecularPower, 1.0f);
		}

	}


	for (UINT i = 0; i < Scene->mNumMeshes; ++i)
	{
		aiMesh* SourceMesh = Scene->mMeshes[i];
		auto NewMesh = ProcessMesh(Device, SourceMesh);
		if (NewMesh == nullptr) return false;

		ModelData NewModelData = {};
		NewModelData.MeshData = std::move(NewMesh);
		NewModelData.MaterialIndex = SourceMesh->mMaterialIndex;
		ModelDatas.push_back(std::move(NewModelData)); 
	}


	return true;
}

void Model::Draw(ID3D11DeviceContext* DeviceContext, ID3D11Buffer* MaterialBuffer)
{
	for (const auto& ModelData : ModelDatas)
	{
		ModelData.MeshData->Bind(DeviceContext);
		MaterialDatas[ModelData.MaterialIndex].DiffuseTexture->Bind(DeviceContext);

		DeviceContext->PSSetConstantBuffers(3, 1, &MaterialBuffer);
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		DeviceContext->Map(MaterialBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
		MaterialBufferData* MaterialData = static_cast<MaterialBufferData*>(MappedResource.pData);
		MaterialData->SpecularPower = MaterialDatas[ModelData.MaterialIndex].SpecularPower;
		MaterialData->SpecularStrength = MaterialDatas[ModelData.MaterialIndex].SpecularStrength;
		MaterialData->Padding = { 0.0f,0.0f};
		DeviceContext->Unmap(MaterialBuffer, 0);


		DeviceContext->DrawIndexed(ModelData.MeshData->GetIndexCount(), 0, 0);

	}
}

const Mesh* Model::GetMesh(size_t Index) const
{
	if (Index >= ModelDatas.size())
	{
		return nullptr;
	}

	return ModelDatas[Index].MeshData.get();
}

std::unique_ptr<Mesh> Model::ProcessMesh(ID3D11Device* Device, aiMesh* SourceMesh)
{
	std::vector<Vertex> SourceVertices;
	SourceVertices.reserve(SourceMesh->mNumVertices);

	for (UINT i = 0; i < SourceMesh->mNumVertices; ++i)
	{
		Vertex NewVertex = {};
		NewVertex.x = SourceMesh->mVertices[i].x;
		NewVertex.y = SourceMesh->mVertices[i].y;
		NewVertex.z = SourceMesh->mVertices[i].z;

		if (SourceMesh->HasTextureCoords(0))
		{
			NewVertex.u = SourceMesh->mTextureCoords[0][i].x;
			NewVertex.v = SourceMesh->mTextureCoords[0][i].y;
		}

		if (SourceMesh->HasNormals())
		{
			NewVertex.nx = SourceMesh->mNormals[i].x;
			NewVertex.ny = SourceMesh->mNormals[i].y;
			NewVertex.nz = SourceMesh->mNormals[i].z;
		}

		SourceVertices.push_back(NewVertex);
	}

	std::vector<UINT> SourceIndices;
	SourceIndices.reserve(SourceMesh->mNumFaces * 3);

	for (UINT i = 0; i < SourceMesh->mNumFaces; ++i)
	{
		for (UINT j = 0; j < SourceMesh->mFaces[i].mNumIndices; ++j)
		{
			SourceIndices.push_back(SourceMesh->mFaces[i].mIndices[j]);
		}
	}

	if (SourceVertices.empty() || SourceIndices.empty())
	{
		return nullptr;
	}

	auto NewMesh = std::make_unique<Mesh>();
	if (!NewMesh->Initialize(Device, SourceVertices.data(), SourceVertices.size(), SourceIndices.data(), SourceIndices.size())) return nullptr;

	return NewMesh;
}

