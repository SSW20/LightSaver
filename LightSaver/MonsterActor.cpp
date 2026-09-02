#include "MonsterActor.h"
#include "NavigationGrid.h"

void MonsterActor::OnUpdate(float DeltaTime)
{
	if (Target == nullptr || GameWorld == nullptr || NavGrid == nullptr) return;
	if (!Target->IsAlive()) return;

	DirectX::XMVECTOR ActorPos = DirectX::XMLoadFloat3(&GetActorTransform().Position);
	DirectX::XMVECTOR TargetPos = DirectX::XMLoadFloat3(&Target->GetActorTransform().Position);
	DirectX::XMVECTOR ActorPosXZ = DirectX::XMVectorSetY(ActorPos, 0.0f);
	DirectX::XMVECTOR TargetPosXZ = DirectX::XMVectorSetY(TargetPos, 0.0f);
	DirectX::XMVECTOR ToTarget = DirectX::XMVectorSubtract(TargetPosXZ, ActorPosXZ);
	float DistanceToTarget = DirectX::XMVectorGetX(DirectX::XMVector3Length(ToTarget));

	bool bInLight = IsInLight();
	UpdateState(bInLight, DistanceToTarget);

	switch (CurrentState)
	{
	case MonsterState::Chase:
		UpdateChase(DeltaTime);
		break;
	case MonsterState::Frozen:
		UpdateFrozen();
		break;
	case MonsterState::Attack:
		UpdateAttack();
		break;
	}
}

void MonsterActor::UpdateState(bool bInLight, float DistanceToTarget)
{
	if (bInLight)
	{
		ChangeState(MonsterState::Frozen);
	}
	else if (DistanceToTarget <= AttackRange && HasLineOfSightToTarget())
	{
		ChangeState(MonsterState::Attack);
	}
	else
	{
		ChangeState(MonsterState::Chase);
	}
}

void MonsterActor::ChangeState(MonsterState NewState)
{
	if (CurrentState == NewState) return;

	CurrentState = NewState;
	if (CurrentState == MonsterState::Chase)
	{
		PathUpdateTimer = 0.0f;
	}
}

void MonsterActor::UpdateChase(float DeltaTime)
{

	PathUpdateTimer -= DeltaTime;
	if (PathUpdateTimer <= 0.0f)
	{
		CurrentPath.clear();
		CurrentPathIndex = 0;
		bool bPathFound = NavGrid->FindPath(GetActorTransform().Position, Target->GetActorTransform().Position, CurrentPath);

		if (bPathFound && CurrentPath.size() > 1)
		{
			CurrentPathIndex = 1;
		}
		PathUpdateTimer = PathUpdateInterval;
	}

	if (CurrentPathIndex >= CurrentPath.size()) return;

	DirectX::XMVECTOR ActorPos = DirectX::XMLoadFloat3(&GetActorTransform().Position);
	DirectX::XMVECTOR ActorPosXZ = DirectX::XMVectorSetY(ActorPos, 0.0f);

	DirectX::XMVECTOR WaypointPos, WaypointPosXZ, ToWaypoint = { 0 };
	float DistanceToWaypoint = 0.0f;
	while (CurrentPathIndex < CurrentPath.size())
	{
		WaypointPos = DirectX::XMLoadFloat3(&CurrentPath[CurrentPathIndex]);
		WaypointPosXZ = DirectX::XMVectorSetY(WaypointPos, 0.0f);
		ToWaypoint = DirectX::XMVectorSubtract(WaypointPosXZ, ActorPosXZ);
		DistanceToWaypoint = DirectX::XMVectorGetX(DirectX::XMVector3Length(ToWaypoint));

		if (DistanceToWaypoint < WaypointAcceptanceRadius)
		{
			++CurrentPathIndex;
		}
		else break;
	}
	if (CurrentPathIndex >= CurrentPath.size()) return;

	DirectX::XMVECTOR CandidatePos = ActorPos;
	// XZ 설정
	DirectX::XMVECTOR ToWaypointDir = DirectX::XMVector3Normalize(ToWaypoint);
	float MoveDistance = MovementSpeed * DeltaTime;
	if (MoveDistance > DistanceToWaypoint)
	{
		MoveDistance = DistanceToWaypoint;
	}

	DirectX::XMVECTOR MoveAmount = DirectX::XMVectorScale(ToWaypointDir, MoveDistance);
	CandidatePos = DirectX::XMVectorAdd(ActorPosXZ, MoveAmount);
	CandidatePos = DirectX::XMVectorSetY(CandidatePos, GetActorTransform().Position.y);

	// Y 설정 
	DirectX::XMFLOAT3 CandidatePosition = {};
	DirectX::XMStoreFloat3(&CandidatePosition, CandidatePos);

	RaycastHitResult GroundHit = {};
	if (!GameWorld->FindFloor(CandidatePosition, RayStart, RayEnd, GroundHit)) return;

	CandidatePosition.y = GroundHit.Position.y + GroundOffset;
	GetActorTransform().Position = CandidatePosition;

	// 회전 설정
	DirectX::XMVECTOR GroundNormal = DirectX::XMLoadFloat3(&GroundHit.Normal);
	GroundNormal = DirectX::XMVector3Normalize(GroundNormal);

	DirectX::XMVECTOR DesiredForward = ToWaypoint;

	float NormalAmount = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DesiredForward, GroundNormal));
	DesiredForward = DirectX::XMVectorSubtract(DesiredForward, DirectX::XMVectorScale(GroundNormal, NormalAmount));

	float ForwardLengthSquared = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DesiredForward));
	if (ForwardLengthSquared <= 0.000001f)
	{
		return;
	}

	DesiredForward = DirectX::XMVector3Normalize(DesiredForward);
	DirectX::XMVECTOR Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(GroundNormal, DesiredForward));
	DirectX::XMVECTOR Up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DesiredForward, Right));

	DirectX::XMFLOAT3 ForwardFloat = {};
	DirectX::XMFLOAT3 RightFloat = {};
	DirectX::XMFLOAT3 UpFloat = {};
	DirectX::XMStoreFloat3(&ForwardFloat, DesiredForward);
	DirectX::XMStoreFloat3(&RightFloat, Right);
	DirectX::XMStoreFloat3(&UpFloat, Up);

	DirectX::XMMATRIX SurfaceRotation(
		RightFloat.x, RightFloat.y, RightFloat.z, 0.0f,
		UpFloat.x, UpFloat.y, UpFloat.z, 0.0f,
		ForwardFloat.x, ForwardFloat.y, ForwardFloat.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	DirectX::XMMATRIX ModelForwardCorrection = DirectX::XMMatrixRotationY(ModelYawOffset);
	DirectX::XMMATRIX TargetRotationMatrix = DirectX::XMMatrixMultiply(ModelForwardCorrection, SurfaceRotation);
	DirectX::XMVECTOR TargetRotation = DirectX::XMQuaternionRotationMatrix(TargetRotationMatrix);

	DirectX::XMVECTOR CurrentRotation = GetActorTransform().GetRotationQuaternion();
	float RotationAlpha = RotationSpeed * DeltaTime;
	if (RotationAlpha > 1.0f)
	{
		RotationAlpha = 1.0f;
	}

	DirectX::XMVECTOR SmoothedRotation = DirectX::XMQuaternionSlerp(CurrentRotation, TargetRotation, RotationAlpha);
	GetActorTransform().SetRotationQuaternion(SmoothedRotation);
}

void MonsterActor::UpdateFrozen()
{
}

void MonsterActor::UpdateAttack()
{
	if (Target == nullptr || !Target->IsAlive()) return;
	Target->TakeDamage(1);
}

bool MonsterActor::HasLineOfSightToTarget()
{
	DirectX::XMVECTOR ActorPos = DirectX::XMLoadFloat3(&GetActorTransform().Position);
	DirectX::XMVECTOR LightOffset = DirectX::XMLoadFloat3(&LightCheckOffset);
	DirectX::XMVECTOR RayOrigin = DirectX::XMVectorAdd(ActorPos, LightOffset);
	DirectX::XMVECTOR TargetPos = Target->GetCamera().GetCameraPosition();
	DirectX::XMVECTOR ToTarget = DirectX::XMVectorSubtract(TargetPos, RayOrigin);
	float Distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(ToTarget));
	if (Distance <= 0.00001f) return true;

	Ray AttackRay = {};
	DirectX::XMVECTOR RayDirection = DirectX::XMVector3Normalize(ToTarget);
	DirectX::XMStoreFloat3(&AttackRay.Origin, RayOrigin);
	DirectX::XMStoreFloat3(&AttackRay.Direction, RayDirection);

	RaycastHitResult OutHit = {};
	return !GameWorld->Raycast(AttackRay, Distance, OutHit);
}

bool MonsterActor::IsInLight()
{
	// 거리 30 , out 30 , in 7 
	DirectX::XMVECTOR CameraForward = Target->GetCamera().GetForwardVector();
	DirectX::XMVECTOR ActorPos = DirectX::XMLoadFloat3(&GetActorTransform().Position);
	DirectX::XMVECTOR LightOffset = DirectX::XMLoadFloat3(&LightCheckOffset);
	DirectX::XMVECTOR ActorLightPoint = DirectX::XMVectorAdd(ActorPos, LightOffset);

	DirectX::XMVECTOR CameraPos = Target->GetCamera().GetCameraPosition();
	DirectX::XMVECTOR CameraToActor = DirectX::XMVectorSubtract(ActorLightPoint, CameraPos);
	DirectX::XMVECTOR ByCameraDir = DirectX::XMVector3Normalize(CameraToActor);

	float dot = XMVectorGetX(DirectX::XMVector3Dot(ByCameraDir, CameraForward));
	//float rad = acosf(dot);
	//float angle = DirectX::XMConvertToDegrees(rad);
	if (dot < cos(DirectX::XMConvertToRadians(30.0f))) return false;

	float Distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(CameraToActor));
	if (Distance > 30.0f) return false;

	Ray CameraRay = {};
	DirectX::XMStoreFloat3(&CameraRay.Direction, ByCameraDir);
	DirectX::XMStoreFloat3(&CameraRay.Origin, CameraPos);

	RaycastHitResult OutHit;
	if (GameWorld->Raycast(CameraRay, Distance, OutHit)) return false;

	return true;

}

void MonsterActor::Initialize(World* InWorld, NavigationGrid* InNav)
{
	if (InWorld == nullptr || InNav == nullptr) return;
	GameWorld = InWorld;
	NavGrid = InNav;
}

void MonsterActor::RegisterTarget(Actor* TargetActor)
{
	if (TargetActor == nullptr) return;
	Target = dynamic_cast<PlayerActor*>(TargetActor);
}
