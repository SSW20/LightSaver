#include "LightSaverGame.h"
#include "Shader.h"
#include "Actor.h"
#include "MeshComponent.h"
#include "Component.h"
#include "BoxColliderComponent.h"
#include <cmath>
#include <vector>
#include "MeshColliderComponent.h"

bool LightSaverGame::OnInitialize()
{
	if (!SpiderModel.Initialize(GetGraphics().Device, "Assets/Models/Spider/spider.obj")) return false;
	if (!WallModel.Initialize(GetGraphics().Device, "Assets/Models/Room/Wall.obj")) return false;
	if (!FloorModel.Initialize(GetGraphics().Device, "Assets/Models/TestTerrain.obj")) return false;

	MainPlayer = GameWorld.SpawnActor<PlayerActor>();
	MainPlayerController.Possess(MainPlayer);

	SpiderActor = GameWorld.SpawnActor<MonsterActor>();
	SpiderActor->GetActorTransform().Scale = { 0.01f, 0.01f, 0.01f };
	SpiderActor->AddComponent<MeshComponent>(&SpiderModel);
	SpiderActor->RegisterTarget(MainPlayer);

	Actor* WallActor;
	Actor* FloorActor;
	WallActor = GameWorld.SpawnActor<Actor>();
	WallActor->GetActorTransform().Position = { 0.0f, 1.8f, 6.0f };
	WallActor->GetActorTransform().Scale = { 8.0f, 4.5f, 1.0f };
	WallActor->AddComponent<MeshComponent>(&WallModel);
	BoxColliderComponent* WallCollider = WallActor->AddComponent<BoxColliderComponent>();

	AABB WallCollision;
	WallCollision.Min = { -0.5f, -0.5f, -0.1f };
	WallCollision.Max = { 0.5f,  0.5f,  0.1f };
	WallCollider->SetCollisionBox(WallCollision);

	FloorActor = GameWorld.SpawnActor<Actor>();
	FloorActor->GetActorTransform().Position = { 0.0f, -0.45f, 4.0f };
	FloorActor->GetActorTransform().Scale = { 30.0f, 1.0f, 30.0f };
	FloorActor->AddComponent<MeshComponent>(&FloorModel);
	FloorActor->AddComponent<MeshColliderComponent>(&FloorModel);

	const DirectX::XMFLOAT3 GridMin = { -14.5f,-1.0f,-10.5f };
	const DirectX::XMFLOAT3 GridMax = { 14.5f,5.0f,18.5f };
	const DirectX::XMFLOAT3 MonsterHalfSize = { 0.4f,0.5f,0.4f };

	RaycastHitResult GroundHit = {};
	DirectX::XMFLOAT3 PlayerSpawnPosition = { 0.0f, GridMax.y, 0.0f };
	if (!GameWorld.FindFloor(PlayerSpawnPosition, 0.0f, GridMax.y - GridMin.y, GroundHit)) return false;
	PlayerSpawnPosition.y = GroundHit.Position.y + 0.8f;
	MainPlayer->SetPlayerPosition(PlayerSpawnPosition);

	DirectX::XMFLOAT3 SpiderSpawnPosition = { 0.0f, GridMax.y, 12.0f };
	if (!GameWorld.FindFloor(SpiderSpawnPosition, 0.0f, GridMax.y - GridMin.y, GroundHit)) return false;
	SpiderSpawnPosition.y = GroundHit.Position.y + 0.4223f;
	SpiderActor->GetActorTransform().Position = SpiderSpawnPosition;

	if (!MonsterNavGrid.Build(GameWorld, GridMin, GridMax, 0.5f, MonsterHalfSize)) return false;
	SpiderActor->Initialize(&GameWorld, &MonsterNavGrid);


	RenderManager.Initialize(GetGraphics());
	ShowCursor(FALSE);

	return true;
}

void LightSaverGame::Update(float deltaTime)
{
	MainPlayerController.Update(deltaTime, GetInput(), GameWorld);
	GameWorld.Update(deltaTime);
}

bool LightSaverGame::Render()
{
	if (MainPlayer == nullptr)
	{
		return false;
	}

	return RenderManager.Render(GameWorld, MainPlayer->GetCamera());
}



LightSaverGame::~LightSaverGame() = default;
