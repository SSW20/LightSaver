#pragma once
#include <DirectXMath.h>
#include <vector>
using namespace DirectX;
class AnimationClip;
class SkeletalModel;
struct AnimationChannel;
struct SkeletalNode;

class Animator
{
public:
	bool Initialize(SkeletalModel* InModel);
	void Update(float DeltaTime);
	float GetCurrentTick();
	void Play(AnimationClip* InClip, bool bInLoop);
	XMMATRIX CalculateNodeLocalTransform(const SkeletalNode* InNode);
	const std::vector<XMFLOAT4X4>& GetFinalBoneMatrices() const { return FinalBoneMatrices; }
	void SetPaused(bool InPaused) { bPaused = InPaused; }
private:
	void CalculateBoneTransforms(const SkeletalNode& Node, XMMATRIX ParentGlobalTransform);
	size_t FindNearestPosKey(const AnimationChannel& AnimChannel);
	DirectX::XMVECTOR InterpolatePosition(const AnimationChannel& AnimChannel);
	size_t FindNearestScaleKey(const AnimationChannel& AnimChannel);
	DirectX::XMVECTOR InterpolateScale(const AnimationChannel& AnimChannel);
	size_t FindNearestRotKey(const AnimationChannel& AnimChannel);
	DirectX::XMVECTOR InterpolateRotation(const AnimationChannel& AnimChannel);

	SkeletalModel* SKModel = nullptr;
	AnimationClip* AnimClip = nullptr;

	float CurrentTick = 0.0f;
	bool bLoop = false;
	bool bPaused = false;
	std::vector<XMFLOAT4X4> FinalBoneMatrices;
};
