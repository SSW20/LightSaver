#include "Animator.h"
#include "SkeletalModel.h"
#include "AnimationClip.h"
#include "SkeletalMeshComponent.h"
bool Animator::Initialize(SkeletalModel* InModel)
{
	if (InModel == nullptr)
	{
		return false;
	}

	SKModel = InModel;

	FinalBoneMatrices.resize(SKModel->GetBoneCount());

	for (XMFLOAT4X4& Matrix : FinalBoneMatrices)
	{
		XMStoreFloat4x4(&Matrix, XMMatrixIdentity());
	}

	return true;
}
void Animator::Update(float DeltaTime)
{
	if (AnimClip == nullptr || bPaused)
	{
		return;
	}

	float TicksPerSecond = AnimClip->GetTicksPerSecond();
	CurrentTick += DeltaTime * TicksPerSecond;

	float Duration = AnimClip->GetDurationTicks();
	if (Duration <= 0.0f)
	{
		return;
	}

	if (bLoop)
	{
		CurrentTick = std::fmod(CurrentTick, Duration);
	}
	else if (CurrentTick > Duration)
	{
		CurrentTick = Duration;
	}
	CalculateBoneTransforms(SKModel->GetRootNode(),XMMatrixIdentity());

}
float Animator::GetCurrentTick() const
{
	return CurrentTick;
}

float Animator::GetNormalizedTime() const
{
	if (AnimClip == nullptr || AnimClip->GetDurationTicks() <= 0.0f)
	{
		return 0.0f;
	}

	return CurrentTick / AnimClip->GetDurationTicks();
}
bool Animator::IsFinished()
{
	return AnimClip != nullptr && !bLoop && CurrentTick >= AnimClip->GetDurationTicks();
}


void Animator::Play(AnimationClip* InClip, bool bInLoop)
{
	AnimClip = InClip;
	CurrentTick = 0.0f;
	bLoop = bInLoop;
	bPaused = false;
}

XMMATRIX Animator::CalculateNodeLocalTransform(const SkeletalNode* InNode)
{
	const AnimationChannel* Channel = AnimClip->FindChannel(InNode->Name);
	if (Channel == nullptr)
	{
		return XMLoadFloat4x4(&InNode->LocalTransform);
	}

	XMVECTOR Position = InterpolatePosition(*Channel);
	XMVECTOR Rotation = InterpolateRotation(*Channel);
	XMVECTOR Scale = InterpolateScale(*Channel);

	XMMATRIX ScaleMatrix = XMMatrixScalingFromVector(Scale);
	XMMATRIX RotationMatrix = XMMatrixRotationQuaternion(Rotation);
	XMMATRIX TranslationMatrix = XMMatrixTranslationFromVector(Position);

	return ScaleMatrix * RotationMatrix * TranslationMatrix;
}

void Animator::CalculateBoneTransforms(const SkeletalNode& Node, XMMATRIX ParentGlobalTransform)
{
	XMMATRIX LocalTransform = CalculateNodeLocalTransform(&Node);
	XMMATRIX GlobalTransform = LocalTransform * ParentGlobalTransform;

	UINT BoneIndex = 0;
	if (SKModel->FindBoneIndex(Node.Name, BoneIndex))
	{
		const BoneInfo* CurrentBoneInfo = SKModel->GetBoneInfo(BoneIndex);
		XMStoreFloat4x4(&FinalBoneMatrices[BoneIndex],XMLoadFloat4x4(&CurrentBoneInfo->OffsetMatrix) * GlobalTransform);
	}

	for (auto& Child : Node.Children)
	{
		CalculateBoneTransforms(Child, GlobalTransform);
	}
}

size_t Animator::FindNearestPosKey(const AnimationChannel& AnimChannel)
{
	for (size_t i = 0; i < AnimChannel.PositionKeys.size() - 1; ++i)
	{
		if (CurrentTick < AnimChannel.PositionKeys[i + 1].Tick)
		{
			return i;
		}
	}
	return AnimChannel.PositionKeys.size() - 2;
}

XMVECTOR Animator::InterpolatePosition(const AnimationChannel& AnimChannel)
{
	if (AnimChannel.PositionKeys.empty())
	{
		return XMVectorZero();
	}
	else if (AnimChannel.PositionKeys.size() == 1)
	{
		return XMLoadFloat3(&AnimChannel.PositionKeys[0].Value);
	}
	else if (CurrentTick >= AnimChannel.PositionKeys.back().Tick)
	{
		return XMLoadFloat3(&AnimChannel.PositionKeys.back().Value);
	}

	size_t CurrentIndex = FindNearestPosKey(AnimChannel);
	size_t NextIndex = CurrentIndex + 1;

	AnimationVectorKey CurrentKey = AnimChannel.PositionKeys[CurrentIndex];
	AnimationVectorKey NextKey = AnimChannel.PositionKeys[NextIndex];

	XMVECTOR CurrentVal = XMLoadFloat3(&CurrentKey.Value);
	XMVECTOR NextVal = XMLoadFloat3(&NextKey.Value);

	float TickDiff = NextKey.Tick - CurrentKey.Tick;
	if (TickDiff <= 0.0f)
	{
		return CurrentVal;
	}

	float LerpFactor = (CurrentTick - CurrentKey.Tick) / TickDiff;

	return XMVectorLerp(CurrentVal, NextVal, LerpFactor);
}

size_t Animator::FindNearestScaleKey(const AnimationChannel& AnimChannel)
{
	for (size_t i = 0; i < AnimChannel.ScalingKeys.size() - 1; ++i)
	{
		if (CurrentTick < AnimChannel.ScalingKeys[i + 1].Tick)
		{
			return i;
		}
	}
	return AnimChannel.ScalingKeys.size() - 2;
}

XMVECTOR Animator::InterpolateScale(const AnimationChannel& AnimChannel)
{
	if (AnimChannel.ScalingKeys.empty())
	{
		return XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
	}
	else if (AnimChannel.ScalingKeys.size() == 1)
	{
		return XMLoadFloat3(&AnimChannel.ScalingKeys[0].Value);
	}
	else if (CurrentTick >= AnimChannel.ScalingKeys.back().Tick)
	{
		return XMLoadFloat3(&AnimChannel.ScalingKeys.back().Value);
	}

	size_t CurrentIndex = FindNearestScaleKey(AnimChannel);
	size_t NextIndex = CurrentIndex + 1;

	AnimationVectorKey CurrentKey = AnimChannel.ScalingKeys[CurrentIndex];
	AnimationVectorKey NextKey = AnimChannel.ScalingKeys[NextIndex];

	XMVECTOR CurrentVal = XMLoadFloat3(&CurrentKey.Value);
	XMVECTOR NextVal = XMLoadFloat3(&NextKey.Value);

	float TickDiff = NextKey.Tick - CurrentKey.Tick;
	if (TickDiff <= 0.0f)
	{
		return CurrentVal;
	}

	float LerpFactor = (CurrentTick - CurrentKey.Tick) / TickDiff;

	return XMVectorLerp(CurrentVal, NextVal, LerpFactor);
}

size_t Animator::FindNearestRotKey(const AnimationChannel& AnimChannel)
{
	for (size_t i = 0; i < AnimChannel.RotationKeys.size() - 1; ++i)
	{
		if (CurrentTick < AnimChannel.RotationKeys[i + 1].Tick)
		{
			return i;
		}
	}
	return AnimChannel.RotationKeys.size() - 2;
}

XMVECTOR Animator::InterpolateRotation(const AnimationChannel& AnimChannel)
{
	if (AnimChannel.RotationKeys.empty())
	{
		return XMQuaternionIdentity();
	}
	else if (AnimChannel.RotationKeys.size() == 1)
	{
		return XMLoadFloat4(&AnimChannel.RotationKeys[0].Value);
	}
	else if (CurrentTick >= AnimChannel.RotationKeys.back().Tick)
	{
		return XMLoadFloat4(&AnimChannel.RotationKeys.back().Value);
	}

	size_t CurrentIndex = FindNearestRotKey(AnimChannel);
	size_t NextIndex = CurrentIndex + 1;

	AnimationQuaternionKey CurrentKey = AnimChannel.RotationKeys[CurrentIndex];
	AnimationQuaternionKey NextKey = AnimChannel.RotationKeys[NextIndex];

	XMVECTOR CurrentVal = XMLoadFloat4(&CurrentKey.Value);
	XMVECTOR NextVal = XMLoadFloat4(&NextKey.Value);

	float TickDiff = NextKey.Tick - CurrentKey.Tick;
	if (TickDiff <= 0.0f)
	{
		return CurrentVal;
	}

	float LerpFactor = (CurrentTick - CurrentKey.Tick) / TickDiff;

	return XMQuaternionNormalize(XMQuaternionSlerp(CurrentVal, NextVal, LerpFactor));
}
