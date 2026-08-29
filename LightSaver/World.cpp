#include "World.h"
#include "Actor.h"
#include "BoxColliderComponent.h"
void World::AddActor(std::unique_ptr<Actor> Actor)
{
	if (Actor == nullptr) return;
	Actors.push_back(std::move(Actor));
}

void World::Update(float DeltaTime)
{
	for (int i = 0; i < Actors.size(); ++i)
	{
		if (Actors[i] == nullptr) continue;
		Actors[i]->Update(DeltaTime);
	}
}
World::World() = default;
World::~World() = default;

bool World::Raycast(const Ray& TestRay, float MaxDistance, RaycastHitResult& OutHit)
{
	bool bHit = false;
	for (const auto& Actor : Actors)
	{
		if (Actor == nullptr) continue;
		BoxColliderComponent* BoxCollider = Actor->FindComponent<BoxColliderComponent>();
		if (BoxCollider == nullptr) continue;
		if (RaycastAABB(TestRay, BoxCollider->GetWorldCollisionBox(), MaxDistance, OutHit))
		{
			if (OutHit.Distance < MaxDistance)
			{
				MaxDistance = OutHit.Distance;
			}
			bHit = true;
			OutHit.HitActor = Actor.get();
		}
	}
	return bHit;
}

bool World::OverlapAABB(const AABB& TestBox)
{
	for (const auto& Actor : Actors)
	{
		if (Actor == nullptr) continue;
		BoxColliderComponent* BoxCollider = Actor->FindComponent<BoxColliderComponent>();
		if (BoxCollider == nullptr) continue;
		if (!BoxCollider->DoesBlocksMovement()) continue;

		if (Intersects(BoxCollider->GetWorldCollisionBox(), TestBox))
		{
			return true;
		}
	}
	return false;
}

void World::CollectRenderObjects(std::vector<RenderObject>& OutRenderObjects) const
{
	for (auto& Actor : Actors)
	{
		Actor->CollectRenderObjects(OutRenderObjects);
	}
}

