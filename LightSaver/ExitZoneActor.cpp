#include "ExitZoneActor.h"

#include <cmath>

void ExitZoneActor::SetTriggerHalfSize(const DirectX::XMFLOAT3& HalfSize)
{
	TriggerHalfSize = HalfSize;
}

bool ExitZoneActor::Contains(const DirectX::XMFLOAT3& WorldPosition) const
{
	const DirectX::XMFLOAT3& ZonePosition = GetActorTransform().Position;

	return std::abs(WorldPosition.x - ZonePosition.x) <= TriggerHalfSize.x &&
		std::abs(WorldPosition.y - ZonePosition.y) <= TriggerHalfSize.y &&
		std::abs(WorldPosition.z - ZonePosition.z) <= TriggerHalfSize.z;
}
