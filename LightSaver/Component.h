#pragma once
#include <vector>
#include "RenderObject.h"

class Actor;
class Component
{
public:
	Component(Actor* Owner)
		:pOwner(Owner) {};

	virtual ~Component() = default;

	virtual void Update(float DeltaTime);
	virtual void CollectRenderObjects(std::vector<RenderObject>& RenderObjects) const;
	Actor* GetOwner() const { return pOwner; }

private:
	Actor* pOwner = nullptr;
};

