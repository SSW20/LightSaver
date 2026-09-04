#include "HospitalLevel.h"

#include "Actor.h"
#include "BoxColliderComponent.h"
#include "MeshColliderComponent.h"
#include "MeshComponent.h"
#include "World.h"

namespace
{
	constexpr float ModuleScale = 0.02f;
	constexpr float TileSize = 4.0f;
	constexpr int TileCountX = 7;
	constexpr int TileCountZ = 7;
	constexpr float FirstTileX = -12.0f;
	constexpr float FirstTileZ = -8.0f;
	constexpr float WestWallX = -14.0f;
	constexpr float EastWallX = 14.0f;
	constexpr float SouthWallZ = -10.0f;
	constexpr float NorthWallZ = 18.0f;
	constexpr float FloorHeight = 5.0f;
	constexpr float StairX = -12.0f;
	constexpr float StairStartZ = -8.0f;
	constexpr int StairCount = 20;
	constexpr float StairHeight = FloorHeight / StairCount;
	constexpr float StairDepth = 0.6f;
	constexpr float StairRun = StairCount * StairDepth;
	constexpr float StairSlopeLength = 13.0f;
	constexpr float StairAngle = 0.39479112f;

	constexpr float RightAngle = DirectX::XM_PIDIV2;
}

bool HospitalLevel::Initialize(ID3D11Device* Device, World& GameWorld)
{
	if (Device == nullptr) return false;

	if (!FloorModel.Initialize(
		Device,
		"Assets/source/floor_tile_1.fbx",
		L"Assets/textures/Floors_1/Floors_1_BaseColor.png"))
	{
		return false;
	}

	if (!WallModel.Initialize(
		Device,
		"Assets/source/tile_wall.fbx",
		L"Assets/textures/Walls_1/Walls_1_BaseColor.png"))
	{
		return false;
	}

	if (!DoorwayModel.Initialize(
		Device,
		"Assets/source/tile_doorway_1.fbx",
		L"Assets/textures/Doorway_1/Doorway_1_BaseColor.png"))
	{
		return false;
	}

	if (!CeilingModel.Initialize(
		Device,
		"Assets/source/ceiling_tile.fbx",
		L"Assets/textures/ceiling_1/Ceiling_1_BaseColor.png"))
	{
		return false;
	}

	if (!ExitSignModel.Initialize(
		Device,
		"Assets/source/Exit_sign.fbx",
		L"Assets/textures/exit_sign/Exit_sign_Base_color.png"))
	{
		return false;
	}

	BuildFloor(GameWorld, 0.0f, false);
	BuildFloor(GameWorld, FloorHeight, true);
	SpawnStairs(GameWorld);
	SpawnExitSign(GameWorld);

	return true;
}

void HospitalLevel::BuildFloor(World& GameWorld, float BaseHeight, bool bSecondFloor)
{
	for (int Row = 0; Row < TileCountZ; ++Row)
	{
		for (int Column = 0; Column < TileCountX; ++Column)
		{
			const float X = FirstTileX + Column * TileSize;
			const float Z = FirstTileZ + Row * TileSize;
			const bool bStairOpening = Column == 0 && (Row == 2 || Row == 3);

			if (!bSecondFloor || !bStairOpening)
			{
				SpawnFloor(GameWorld, X, BaseHeight, Z);
			}

			// 1층 천장은 2층 바닥과 같은 높이에 있다. 계단 위 두 칸은 뚫어 둔다.
			if (bSecondFloor || !bStairOpening)
			{
				SpawnCeiling(GameWorld, X, BaseHeight + FloorHeight, Z);
			}
		}
	}

	// 계단 끝과 2층 복도를 이어 주는 작은 착지 공간이다.
	if (bSecondFloor)
	{
		SpawnFloor(GameWorld, StairX, BaseHeight, 6.0f);
	}

	for (int Column = 0; Column < TileCountX; ++Column)
	{
		const float X = FirstTileX + Column * TileSize;
		SpawnWall(GameWorld, X, BaseHeight, SouthWallZ, 0.0f);

		SpawnWall(GameWorld, X, BaseHeight, NorthWallZ, 0.0f);
	}

	for (int Row = 0; Row < TileCountZ; ++Row)
	{
		const float Z = FirstTileZ + Row * TileSize;
		SpawnWall(GameWorld, WestWallX, BaseHeight, Z, RightAngle);
		SpawnWall(GameWorld, EastWallX, BaseHeight, Z, RightAngle);
	}

	for (int Row = 0; Row < TileCountZ; ++Row)
	{
		const float Z = FirstTileZ + Row * TileSize;

		if (Row == 1 || Row == 5)
		{
			SpawnDoorway(GameWorld, -4.0f, BaseHeight, Z, RightAngle);
		}
		else
		{
			SpawnWall(GameWorld, -4.0f, BaseHeight, Z, RightAngle);
		}

		if (Row == 2 || Row == 4)
		{
			SpawnDoorway(GameWorld, 4.0f, BaseHeight, Z, RightAngle);
		}
		else
		{
			SpawnWall(GameWorld, 4.0f, BaseHeight, Z, RightAngle);
		}
	}

	// 2층 북서쪽 모서리에 4 x 4 크기의 작은 탈출실을 만든다.
	// 북쪽과 서쪽은 외벽을 사용하고, 남쪽에는 출입구를 둔다.
	if (bSecondFloor)
	{
		SpawnDoorway(GameWorld, -12.0f, BaseHeight, 14.0f, 0.0f);
		SpawnWall(GameWorld, -10.0f, BaseHeight, 16.0f, RightAngle);
	}
}

void HospitalLevel::SpawnStairs(World& GameWorld)
{
	for (int StepIndex = 0; StepIndex < StairCount; ++StepIndex)
	{
		const float StepBlockHeight = (StepIndex + 1) * StairHeight;
		const float Z = StairStartZ + StairDepth * (StepIndex + 0.5f);
		SpawnStairStep(GameWorld, StairX, 0.2f, Z, StepBlockHeight);
	}

	SpawnStairRampCollider(GameWorld);
}

void HospitalLevel::SpawnFloor(World& GameWorld, float X, float Y, float Z)
{
	Actor* FloorActor = GameWorld.SpawnActor<Actor>();
	FloorActor->GetActorTransform().Position = { X, Y, Z };
	FloorActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, ModuleScale };
	FloorActor->AddComponent<MeshComponent>(&FloorModel);
	FloorActor->AddComponent<MeshColliderComponent>(&FloorModel);
}

void HospitalLevel::SpawnCeiling(World& GameWorld, float X, float Y, float Z)
{
	Actor* CeilingActor = GameWorld.SpawnActor<Actor>();
	CeilingActor->GetActorTransform().Position = { X, Y, Z };
	CeilingActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, ModuleScale };
	CeilingActor->AddComponent<MeshComponent>(&CeilingModel);
}

void HospitalLevel::SpawnWall(World& GameWorld, float X, float Y, float Z, float Yaw)
{
	Actor* WallActor = GameWorld.SpawnActor<Actor>();
	WallActor->GetActorTransform().Position = { X, Y, Z };
	WallActor->GetActorTransform().Rotation = { 0.0f, Yaw, 0.0f };
	WallActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, ModuleScale };
	WallActor->AddComponent<MeshComponent>(&WallModel);

	BoxColliderComponent* WallCollider = WallActor->AddComponent<BoxColliderComponent>();
	AABB WallCollision = {};
	// 실제 벽보다 약간 크게 잡아 모듈 사이의 미세한 틈과 빠른 이동 시 관통을 막는다.
	// Scale 0.02 적용 후: 길이 4.2, 두께 1.2, 높이 5.0
	WallCollision.Min = { -105.0f, 0.0f, -30.0f };
	WallCollision.Max = { 105.0f, 250.0f, 30.0f };
	WallCollider->SetCollisionBox(WallCollision);
}

void HospitalLevel::SpawnDoorway(World& GameWorld, float X, float Y, float Z, float Yaw)
{
	Actor* DoorwayActor = GameWorld.SpawnActor<Actor>();
	DoorwayActor->GetActorTransform().Position = { X, Y, Z };
	DoorwayActor->GetActorTransform().Rotation = { 0.0f, Yaw, 0.0f };
	DoorwayActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, ModuleScale };
	DoorwayActor->AddComponent<MeshComponent>(&DoorwayModel);

	// 문 전체를 하나의 Collider로 막으면 출입할 수 없으므로
	// 왼쪽 기둥, 오른쪽 기둥, 위쪽 문틀을 서로 다른 Actor로 만든다.
	// 실제 통로 폭은 3.4이며, 플레이어와 몬스터가 가장자리에 걸리지 않게 여유를 둔다.
	AABB LeftPost = {};
	LeftPost.Min = { -105.0f, 0.0f, -30.0f };
	LeftPost.Max = { -85.0f, 250.0f, 30.0f };
	SpawnCollisionBox(GameWorld, X, Y, Z, Yaw, LeftPost);

	AABB RightPost = {};
	RightPost.Min = { 85.0f, 0.0f, -30.0f };
	RightPost.Max = { 105.0f, 250.0f, 30.0f };
	SpawnCollisionBox(GameWorld, X, Y, Z, Yaw, RightPost);

	AABB TopPost = {};
	TopPost.Min = { -85.0f, 200.0f, -30.0f };
	TopPost.Max = { 85.0f, 250.0f, 30.0f };
	SpawnCollisionBox(GameWorld, X, Y, Z, Yaw, TopPost);
}

void HospitalLevel::SpawnCollisionBox(
	World& GameWorld,
	float X,
	float Y,
	float Z,
	float Yaw,
	const AABB& CollisionBox)
{
	Actor* CollisionActor = GameWorld.SpawnActor<Actor>();
	CollisionActor->GetActorTransform().Position = { X, Y, Z };
	CollisionActor->GetActorTransform().Rotation = { 0.0f, Yaw, 0.0f };
	CollisionActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, ModuleScale };
	CollisionActor->AddComponent<BoxColliderComponent>()->SetCollisionBox(CollisionBox);
}

void HospitalLevel::SpawnStairStep(World& GameWorld, float X, float Y, float Z, float Height)
{
	Actor* StepActor = GameWorld.SpawnActor<Actor>();
	StepActor->GetActorTransform().Position = { X, Y, Z };
	StepActor->GetActorTransform().Scale = { ModuleScale, Height / 10.0f, StairDepth / 200.0f };
	StepActor->AddComponent<MeshComponent>(&FloorModel);
}

void HospitalLevel::SpawnStairRampCollider(World& GameWorld)
{
	Actor* RampActor = GameWorld.SpawnActor<Actor>();

	// 계단 모양은 20개의 StepActor가 담당한다.
	// 바닥 판정은 그 아래의 하나로 이어진 경사면이 담당해서 높이가 흔들리지 않는다.
	// 위쪽 끝을 착지 바닥 안쪽까지 겹쳐, 레이캐스트가 메시 경계의 빈틈으로 빠지지 않게 한다.
	RampActor->GetActorTransform().Position = { StairX, 2.6404f, StairStartZ + StairRun * 0.5f + 1.0f };
	RampActor->GetActorTransform().Rotation = { -StairAngle, 0.0f, 0.0f };
	RampActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, StairSlopeLength / 200.0f };
	RampActor->AddComponent<MeshColliderComponent>(&FloorModel);
}

void HospitalLevel::SpawnExitSign(World& GameWorld)
{
	Actor* ExitSignActor = GameWorld.SpawnActor<Actor>();
	// 탈출실 남쪽 입구를 바라볼 때 보이도록 방 바깥쪽에 배치한다.
	ExitSignActor->GetActorTransform().Position = { -12.0f, 9.0f, 13.25f };
	ExitSignActor->GetActorTransform().Scale = { ModuleScale, ModuleScale, ModuleScale };
	ExitSignActor->AddComponent<MeshComponent>(&ExitSignModel);
}
