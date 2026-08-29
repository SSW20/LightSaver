#include "BoxColliderComponent.h"
#include "Component.h"

AABB BoxColliderComponent::GetWorldCollisionBox()
{
	DirectX::XMVECTOR LocalMax = DirectX::XMLoadFloat3(&LocalCollisionBox.Max);
	DirectX::XMVECTOR LocalMin = DirectX::XMLoadFloat3(&LocalCollisionBox.Min);
	DirectX::XMMATRIX OwnerTransform = GetOwner()->GetActorTransform().GetWorldMatrix();


	DirectX::XMVECTOR WorldMax = DirectX::XMVector3Transform(LocalMax, OwnerTransform);
	DirectX::XMVECTOR WorldMin = DirectX::XMVector3Transform(LocalMin, OwnerTransform);

	AABB WorldCollisionBox = {};
	DirectX::XMStoreFloat3(&WorldCollisionBox.Max, WorldMax);
	DirectX::XMStoreFloat3(&WorldCollisionBox.Min, WorldMin);
	return WorldCollisionBox;
}
