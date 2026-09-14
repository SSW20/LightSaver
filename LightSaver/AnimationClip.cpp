#include "AnimationClip.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/config.h>
#include <filesystem>
#include <wtypes.h>

bool AnimationClip::Initialize(const std::string& FilePath)
{
	Channels.clear();
	
	Assimp::Importer Importer;
	const unsigned int ImportFlag = aiProcess_MakeLeftHanded;
	Importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS,false);	// Pivot 정보 보존으로 인한 중간 노드 생성 금지
	const aiScene* Scene = Importer.ReadFile(FilePath, ImportFlag);
	if (Scene == nullptr || Scene->mNumAnimations == 0 || (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) return false;

	aiAnimation* SourceAnimation = Scene->mAnimations[0];
	TicksPerSecond = SourceAnimation->mTicksPerSecond;
	DurationTicks = SourceAnimation->mDuration;
	Name = std::filesystem::path(FilePath).stem().string();


	for (UINT i = 0; i < SourceAnimation->mNumChannels; ++i)
	{
		aiNodeAnim* SourceChannel = SourceAnimation->mChannels[i];
		AnimationChannel AnimChannel = {};

		AnimChannel.NodeName = SourceChannel->mNodeName.C_Str();

		AnimChannel.PositionKeys.resize(SourceChannel->mNumPositionKeys);
		for (UINT j = 0; j < SourceChannel->mNumPositionKeys; ++j)
		{
			AnimChannel.PositionKeys[j].Tick = SourceChannel->mPositionKeys[j].mTime;
			AnimChannel.PositionKeys[j].Value = XMFLOAT3(SourceChannel->mPositionKeys[j].mValue.x, SourceChannel->mPositionKeys[j].mValue.y, SourceChannel->mPositionKeys[j].mValue.z);
		}

		AnimChannel.ScalingKeys.resize(SourceChannel->mNumScalingKeys);
		for (UINT j = 0; j < SourceChannel->mNumScalingKeys; ++j)
		{
			AnimChannel.ScalingKeys[j].Tick = SourceChannel->mScalingKeys[j].mTime;
			AnimChannel.ScalingKeys[j].Value = XMFLOAT3(SourceChannel->mScalingKeys[j].mValue.x, SourceChannel->mScalingKeys[j].mValue.y, SourceChannel->mScalingKeys[j].mValue.z);
		}

		AnimChannel.RotationKeys.resize(SourceChannel->mNumRotationKeys);
		for (UINT j = 0; j < SourceChannel->mNumRotationKeys; ++j)
		{
			AnimChannel.RotationKeys[j].Tick = SourceChannel->mRotationKeys[j].mTime;
			AnimChannel.RotationKeys[j].Value = XMFLOAT4(SourceChannel->mRotationKeys[j].mValue.x, SourceChannel->mRotationKeys[j].mValue.y, SourceChannel->mRotationKeys[j].mValue.z, SourceChannel->mRotationKeys[j].mValue.w);
		}
		Channels.emplace(SourceChannel->mNodeName.C_Str(), std::move(AnimChannel));
	}
	return true;
}
