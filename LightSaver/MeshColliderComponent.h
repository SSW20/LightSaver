#pragma once
#include "Component.h"
#include "CollisionTypes.h"

class Model;

class MeshColliderComponent : public Component
{
public:
	MeshColliderComponent(Actor* Owner, const Model* InModel);

	void SetCollisionModel(const Model* InModel);
	const Model* GetCollisionModel() const { return CollisionModel; }

	bool Raycast(const Ray& TestRay, float MaxDistance, RaycastHitResult& OutHit) const;

private:
	const Model* CollisionModel = nullptr;
};
