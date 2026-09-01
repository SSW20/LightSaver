#pragma once
#include "PlayerActor.h"
#include "World.h"
class MonsterActor : public Actor
{
public:
	void RegisterTarget(Actor* Player);
	void Initialize(World* InWorld);

protected:
	virtual void OnUpdate(float DeltaTime) override;
	bool IsInLight();
private:
	PlayerActor* Target = nullptr;
	float MovementSpeed = 1.0f;
	float AcceptanceRange = 3.0f;
	float RayStart = 2.0f;
	float RayEnd = 3.0f;
	float GroundOffset = 0.4223f;
	float RotationSpeed = 5.0f;
	float ModelYawOffset = DirectX::XM_PIDIV2;
	DirectX::XMFLOAT3 LightCheckOffset = { 0.0f, 0.5f, 0.0f };
	World* GameWorld = nullptr;
};

