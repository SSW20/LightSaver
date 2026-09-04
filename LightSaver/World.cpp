#include "World.h"
#include "Actor.h"
#include "BoxColliderComponent.h"
#include "MeshColliderComponent.h"
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
		if (BoxCollider != nullptr)
		{
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

		MeshColliderComponent* MeshCollider = Actor->FindComponent<MeshColliderComponent>();
		if (MeshCollider != nullptr)
		{
			if(MeshCollider->Raycast(TestRay, MaxDistance, OutHit))
			{
				MaxDistance = OutHit.Distance;
				bHit = true;
			}
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

bool World::FindFloor(const DirectX::XMFLOAT3& Position, float RayStart, float RayEnd, RaycastHitResult& OutHit)
{
	Ray GroundRay = {};
	GroundRay.Origin = Position;
	GroundRay.Origin.y += RayStart;
	GroundRay.Direction = { 0,-1,0 };
	float ClosestDistance = RayStart + RayEnd;
	bool bHitFloor = false;

	// 벽과 문틀의 BoxCollider 윗면을 바닥으로 오인하지 않도록
	// 실제 지형으로 사용하는 MeshCollider만 검사한다.
	for (const auto& Actor : Actors)
	{
		if (Actor == nullptr) continue;

		MeshColliderComponent* MeshCollider = Actor->FindComponent<MeshColliderComponent>();
		if (MeshCollider == nullptr) continue;

		RaycastHitResult CandidateHit = {};
		if (!MeshCollider->Raycast(GroundRay, ClosestDistance, CandidateHit)) continue;
		if (CandidateHit.Normal.y < 0.85f) continue;

		ClosestDistance = CandidateHit.Distance;
		OutHit = CandidateHit;
		bHitFloor = true;
	}

	return bHitFloor;
}
