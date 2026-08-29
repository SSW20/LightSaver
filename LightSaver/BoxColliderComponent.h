#pragma once
#include "CollisionTypes.h"
#include "Actor.h"

class BoxColliderComponent : public Component
{
public:
	BoxColliderComponent(Actor* Owner)
		:Component(Owner) {};
	void SetCollisionBox(const AABB& CollisionBox)
	{
		LocalCollisionBox = CollisionBox;
	}
	const AABB& GetCollisionBox() const { return LocalCollisionBox; }
	bool DoesBlocksMovement() const { return bBlocksMovement; }
	void SetBlockMovement(bool bBlock) { bBlocksMovement = bBlock; }
	AABB GetWorldCollisionBox();
private:
	AABB LocalCollisionBox = {};
	bool bBlocksMovement = true;
};

