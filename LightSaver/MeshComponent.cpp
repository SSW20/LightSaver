#include "MeshComponent.h"

RenderObject MeshComponent::CreateRenderObj() const
{
	RenderObject RenderObj;
	RenderObj.ModelSet = ModelSet;
	RenderObj.ModelWorldTransform = GetOwner()->GetActorTransform();
	return RenderObj;
}

void MeshComponent::CollectRenderObjects(std::vector<RenderObject>& RenderObjects) const
{
	if (ModelSet != nullptr && GetOwner() != nullptr)
	{
		RenderObject RenderObj = CreateRenderObj();
		RenderObjects.push_back(RenderObj);
	}
}
