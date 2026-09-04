#include "BoxColliderComponent.h"
#include "Component.h"
#include <cfloat>
#include <algorithm>

AABB BoxColliderComponent::GetWorldCollisionBox()
{
	DirectX::XMMATRIX OwnerTransform = GetOwner()->GetActorTransform().GetWorldMatrix();
	AABB WorldCollisionBox = {};
	WorldCollisionBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
	WorldCollisionBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	for (int X = 0; X < 2; ++X)
	{
		for (int Y = 0; Y < 2; ++Y)
		{
			for (int Z = 0; Z < 2; ++Z)
			{
				DirectX::XMFLOAT3 LocalCorner =
				{
					X == 0 ? LocalCollisionBox.Min.x : LocalCollisionBox.Max.x,
					Y == 0 ? LocalCollisionBox.Min.y : LocalCollisionBox.Max.y,
					Z == 0 ? LocalCollisionBox.Min.z : LocalCollisionBox.Max.z
				};

				DirectX::XMVECTOR Corner = DirectX::XMLoadFloat3(&LocalCorner);
				Corner = DirectX::XMVector3TransformCoord(Corner, OwnerTransform);

				DirectX::XMFLOAT3 WorldCorner = {};
				DirectX::XMStoreFloat3(&WorldCorner, Corner);

				WorldCollisionBox.Min.x = (std::min)(WorldCollisionBox.Min.x, WorldCorner.x);
				WorldCollisionBox.Min.y = (std::min)(WorldCollisionBox.Min.y, WorldCorner.y);
				WorldCollisionBox.Min.z = (std::min)(WorldCollisionBox.Min.z, WorldCorner.z);
				WorldCollisionBox.Max.x = (std::max)(WorldCollisionBox.Max.x, WorldCorner.x);
				WorldCollisionBox.Max.y = (std::max)(WorldCollisionBox.Max.y, WorldCorner.y);
				WorldCollisionBox.Max.z = (std::max)(WorldCollisionBox.Max.z, WorldCorner.z);
			}
		}
	}

	return WorldCollisionBox;
}
