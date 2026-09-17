#include "SkeletalModel.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include <filesystem>

bool SkeletalModel::Initialize(
	ID3D11Device* Device,
	const std::string& FilePath,
	const std::unordered_map<std::string, std::string>& TextureOverrides)
{
	BoneInfos.clear();
	BoneInfoMap.clear();
	SkeletalModelDatas.clear();
	SkeletalMaterialDatas.clear();

	Assimp::Importer Importer;

	// FBX의 복잡한 Pivot 보조 Node는 보존하지 않도록 설정
	Importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

	//aiProcess_PreTransformVertices 미리 Model Local로 굽기 X
	// 삼각형, 중복 정점 결합, 왼손 좌표계, 삼각형 정점 순서 변환, UV 상하 반전, Normal이 없으면 자동 생성, 정점당 BoneWeight 4개까지만 제한
	const unsigned int ImportFlag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights;
	const aiScene* Scene = Importer.ReadFile(FilePath, ImportFlag);
	if (Scene == nullptr || Scene->mRootNode == nullptr || (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) return false;

	CopyNodeTree(Scene->mRootNode, RootNode);


	for (UINT i = 0; i < Scene->mNumMeshes; ++i)
	{
		aiMesh* SourceMesh = Scene->mMeshes[i];
		auto NewMesh = ProcessMesh(Device, SourceMesh);
		if (NewMesh == nullptr) return false;

		SkeletalModelData NewModelData = {};
		NewModelData.MeshData = std::move(NewMesh);
		NewModelData.MaterialIndex = SourceMesh->mMaterialIndex;
		SkeletalModelDatas.push_back(std::move(NewModelData));
	}

	SkeletalMaterialDatas.resize(Scene->mNumMaterials);
	std::filesystem::path ModelPath = FilePath;
	for (UINT i = 0; i < Scene->mNumMaterials; ++i)
	{
		bool bLoaded = false;
		auto NewTexture = std::make_unique<Texture>();
		aiMaterial* SourceMaterial = Scene->mMaterials[i];
		aiString MaterialName;
		SourceMaterial->Get(AI_MATKEY_NAME, MaterialName);
		const auto Override = TextureOverrides.find(MaterialName.C_Str());
		if (Override != TextureOverrides.end())
		{
			// FBX에 남은 예전 경로 대신 이 모델에 지정된 텍스처를 사용한다.
			std::filesystem::path FullTexturePath = ModelPath.parent_path() / Override->second;
			if (!std::filesystem::exists(FullTexturePath) ||
				!NewTexture->Initialize(Device, FullTexturePath.c_str()))
			{
				return false;
			}
			bLoaded = true;
		}
		else
		{
			aiString TexturePath;
			if (SourceMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &TexturePath) == AI_SUCCESS)
			{
				std::filesystem::path FullTexturePath = ModelPath.parent_path() / TexturePath.C_Str();
				if (std::filesystem::exists(FullTexturePath))
				{
					if (!NewTexture->Initialize(Device, FullTexturePath.c_str())) return false;
					bLoaded = true;
				}
			}
		}
		if (!bLoaded)
		{
			NewTexture->InitializeByColor(Device, 255, 255, 255, 255);
		}

		SkeletalMaterialDatas[i].DiffuseTexture = std::move(NewTexture);
	}
		//aiColor3D SpecularColor(0.2f, 0.2f, 0.2f);
		//if (SourceMaterial->Get(AI_MATKEY_COLOR_SPECULAR, SpecularColor) == AI_SUCCESS)
		//{
		//	SkeletalModelDatas[i].SpecularStrength = std::max(SpecularColor.r, std::max(SpecularColor.g, SpecularColor.b));
		//}
		//float SpecularPower = 32.0f;
		//if (SourceMaterial->Get(AI_MATKEY_SHININESS, SpecularPower) == AI_SUCCESS)
		//{
		//	SkeletalModelDatas[i].SpecularPower = std::max(SpecularPower, 1.0f);
		//}


	return true;
}
UINT SkeletalModel::FindOrCreateBoneIndex(const std::string& BoneName)
{
	if (BoneInfoMap.find(BoneName) == BoneInfoMap.end())
	{
		BoneInfoMap[BoneName] = BoneInfos.size();
		BoneInfo NewBoneInfo;
		NewBoneInfo.BoneName = BoneName;
		BoneInfos.push_back(NewBoneInfo);
	}
	return BoneInfoMap[BoneName];
}

bool SkeletalModel::FindBoneIndex(const std::string& BoneName, UINT& OutBoneIndex)
{
	if (BoneInfoMap.find(BoneName) == BoneInfoMap.end())
	{
		return false;
	}
	OutBoneIndex = BoneInfoMap[BoneName];
	return true;
}

void SkeletalModel::Draw(ID3D11DeviceContext* DeviceContext)
{
	if (DeviceContext == nullptr)
	{
		return;
	}

	for (const auto& ModelData : SkeletalModelDatas)
	{
		if (ModelData.MeshData == nullptr)
		{
			continue;
		}
		SkeletalMaterialDatas[ModelData.MaterialIndex].DiffuseTexture->Bind(DeviceContext);
		ModelData.MeshData->Bind(DeviceContext);
		DeviceContext->DrawIndexed(ModelData.MeshData->GetIndexCount(),0,0);
	}
}

void SkeletalModel::CopyNodeTree(const aiNode* SourceNode, SkeletalNode& DestinationNode)
{
	DestinationNode.Name = SourceNode->mName.C_Str();
	DirectX::XMFLOAT4X4 RowMajorMatrix;
	std::memcpy(&RowMajorMatrix, &SourceNode->mTransformation, sizeof(aiMatrix4x4));
	DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&RowMajorMatrix);
	mat = DirectX::XMMatrixTranspose(mat);
	DirectX::XMStoreFloat4x4(&DestinationNode.LocalTransform, mat);

	DestinationNode.Children.clear();
	DestinationNode.Children.reserve(SourceNode->mNumChildren);


	for (UINT i = 0; i < SourceNode->mNumChildren; ++i)
	{
		SkeletalNode ChildNode;
		DestinationNode.Children.push_back(ChildNode);
		CopyNodeTree(SourceNode->mChildren[i], DestinationNode.Children.back());
	}

}

std::unique_ptr<SkeletalMesh> SkeletalModel::ProcessMesh(ID3D11Device* Device, aiMesh* SourceMesh)
{
	// 정점 
	std::vector<SkeletalVertex> SourceVertices;
	SourceVertices.reserve(SourceMesh->mNumVertices);
	for (UINT i = 0; i < SourceMesh->mNumVertices; ++i)
	{
		SkeletalVertex NewVertex = {};
		NewVertex.Position = XMFLOAT3(SourceMesh->mVertices[i].x, SourceMesh->mVertices[i].y, SourceMesh->mVertices[i].z);

		if (SourceMesh->HasTextureCoords(0))
		{
			NewVertex.UV = XMFLOAT2(SourceMesh->mTextureCoords[0][i].x, SourceMesh->mTextureCoords[0][i].y);
		}

		if (SourceMesh->HasNormals())
		{
			NewVertex.Normal = XMFLOAT3(SourceMesh->mNormals[i].x, SourceMesh->mNormals[i].y, SourceMesh->mNormals[i].z);
		}

		SourceVertices.push_back(NewVertex);
	}

	if (SourceMesh->HasBones())
	{
		// 본
		for (UINT i = 0; i < SourceMesh->mNumBones; ++i)
		{
			aiBone* SourceBone = SourceMesh->mBones[i];
			UINT BoneIndex = FindOrCreateBoneIndex(SourceBone->mName.C_Str());

			for (UINT j = 0; j < SourceBone->mNumWeights; ++j)
			{
				const aiVertexWeight& SourceWeight = SourceBone->mWeights[j];
				UINT VertexIndex = SourceWeight.mVertexId;
				float Weight = SourceWeight.mWeight;

				if (!SourceVertices[VertexIndex].AddBoneInfluence(BoneIndex, Weight)) continue;
			}

			DirectX::XMFLOAT4X4 RowMajorMatrix;
			std::memcpy(&RowMajorMatrix, &SourceBone->mOffsetMatrix, sizeof(aiMatrix4x4));
			DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&RowMajorMatrix);
			mat = DirectX::XMMatrixTranspose(mat);

			DirectX::XMStoreFloat4x4(&BoneInfos[BoneIndex].OffsetMatrix, mat);

		}
	}

	// 인덱스 
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

	auto NewMesh = std::make_unique<SkeletalMesh>();
	if (!NewMesh->Initialize(Device, SourceVertices.data(), SourceVertices.size(), SourceIndices.data(), SourceIndices.size())) return nullptr;

	return NewMesh;
}
