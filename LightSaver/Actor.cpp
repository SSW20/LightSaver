#include "Actor.h"

void Actor::Update(float DeltaTime)
{
	for (int i=0; i<Components.size(); ++i)
	{
		if (Components[i] == nullptr) continue;
		Components[i]->Update(DeltaTime);
	}
}
void Actor::CollectRenderObjects(std::vector<RenderObject>& RenderObjects) const
{
	for (auto& Comp : Components)
	{
		Comp->CollectRenderObjects(RenderObjects);
	}
}

Actor::~Actor() = default;