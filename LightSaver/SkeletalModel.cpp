#include "SkeletalModel.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>

bool SkeletalModel::Initialize(ID3D11Device* Device, const std::string& FilePath)
{
	Assimp::Importer Importer;

	// FBX의 복잡한 Pivot 보조 Node는 보존하지 않도록 설정
	Importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,false);

	//aiProcess_PreTransformVertices 미리 Model Local로 굽기 X
	// 삼각형, 중복 정점 결합, 왼손 좌표계, 삼각형 정점 순서 변환, UV 상하 반전, Normal이 없으면 자동 생성, 정점당 BoneWeight 4개까지만 제한
	const unsigned int ImportFlag = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights;
	const aiScene* Scene = Importer.ReadFile(FilePath, ImportFlag);
	if (Scene == nullptr || Scene->mRootNode == nullptr || (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) return false;

	return true;
}
