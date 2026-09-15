#pragma once
#include "Model.h"
#include "Transform.h"

class SkeletalModel;
class Animator;

struct RenderObject
{
	Model* ModelSet = nullptr;
	// 애니메이션 모델
	SkeletalModel* SkeletalModelSet = nullptr;
	// 이 모델의 현재 자세
	const Animator* AnimatorSet = nullptr;
	Transform ModelWorldTransform;
};

