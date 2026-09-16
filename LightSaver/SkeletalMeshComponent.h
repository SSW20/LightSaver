#pragma once

#include "Component.h"
#include "Animator.h"

class SkeletalModel;
class AnimationClip;

class SkeletalMeshComponent : public Component
{
public:
	SkeletalMeshComponent(Actor* Owner, SkeletalModel* InModel);
	virtual void Update(float DeltaTime) override;
	virtual void CollectRenderObjects(std::vector<RenderObject>& RenderObjects) const override;
	void Play(AnimationClip* InClip,bool bLoop);
	void SetAnimationPaused(bool bPaused);

private:
	SkeletalModel* ModelSet = nullptr;
	Animator AnimationPlayer;
};