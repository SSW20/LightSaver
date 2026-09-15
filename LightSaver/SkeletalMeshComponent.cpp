#include "SkeletalMeshComponent.h"
#include "Actor.h"

SkeletalMeshComponent::SkeletalMeshComponent(Actor* Owner, SkeletalModel* InModel)
	: Component(Owner), ModelSet(InModel)
{
	if (ModelSet != nullptr)
	{
		AnimationPlayer.Initialize(ModelSet);
	}
}

void SkeletalMeshComponent::Update(float DeltaTime)
{
	AnimationPlayer.Update(DeltaTime);
}

void SkeletalMeshComponent::Play(AnimationClip* InClip, bool bLoop)
{
	AnimationPlayer.Play(InClip, bLoop);
}

void SkeletalMeshComponent::CollectRenderObjects(std::vector<RenderObject>& RenderObjects) const
{
    if (ModelSet == nullptr || GetOwner() == nullptr)
    {
        return;
    }

    RenderObject RenderObj;

    RenderObj.SkeletalModelSet = ModelSet;
    RenderObj.AnimatorSet = &AnimationPlayer;
    RenderObj.ModelWorldTransform = GetOwner()->GetActorTransform();
    RenderObjects.push_back(RenderObj);
}