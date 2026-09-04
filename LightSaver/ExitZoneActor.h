#pragma once

#include "Actor.h"

class ExitZoneActor : public Actor
{
public:
	void SetTriggerHalfSize(const DirectX::XMFLOAT3& HalfSize);
	bool Contains(const DirectX::XMFLOAT3& WorldPosition) const;

private:
	DirectX::XMFLOAT3 TriggerHalfSize = { 1.0f, 1.0f, 1.0f };
};
