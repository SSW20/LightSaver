#pragma once
#include "PlayerActor.h"
#include "World.h"

class NavigationGrid;

enum class MonsterState
{
	Chase,
	Frozen,
	Attack
};

class MonsterActor : public Actor
{
public:
	void RegisterTarget(Actor* Player);
	void Initialize(World* InWorld, NavigationGrid* InFirstFloorNav, NavigationGrid* InSecondFloorNav);
	void Reset(const DirectX::XMFLOAT3& SpawnPosition);

protected:
	virtual void OnUpdate(float DeltaTime) override;
	bool IsInLight();
private:
	void UpdateState(bool bInLight, float DistanceToTarget);
	void ChangeState(MonsterState NewState);
	void UpdateChase(float DeltaTime);
	void UpdateFrozen();
	void UpdateAttack();
	bool HasLineOfSightToTarget();

	PlayerActor* Target = nullptr;
	MonsterState CurrentState = MonsterState::Chase;
	float MovementSpeed = 5.0f;
	float AttackRange = 1.2f;
	float RayStart = 2.0f;
	float RayEnd = 3.0f;
	float GroundOffset = 0.4223f;
	float RotationSpeed = 5.0f;
	float ModelYawOffset = DirectX::XM_PIDIV2;
	DirectX::XMFLOAT3 LightCheckOffset = { 0.0f, 0.5f, 0.0f };
	World* GameWorld = nullptr;
	NavigationGrid* FirstFloorNavGrid = nullptr;
	NavigationGrid* SecondFloorNavGrid = nullptr;
	enum class StairTravelDirection
	{
		None,
		Up,
		Down
	};
	StairTravelDirection StairDirection = StairTravelDirection::None;
	std::vector<DirectX::XMFLOAT3> CurrentPath;
	size_t CurrentPathIndex = 0;
	float PathUpdateTimer = 0.0f;
	float PathUpdateInterval = 0.5f;
	float WaypointAcceptanceRadius = 0.2f;
};

