#include "LightSaverGame.h"
#include "Shader.h"
#include "Actor.h"
#include "MeshComponent.h"
#include "Component.h"
#include "BoxColliderComponent.h"
#include <cmath>
#include <vector>
#include "MeshColliderComponent.h"
#include "GeneratorActor.h"

bool LightSaverGame::OnInitialize()
{
	if (!SpiderModel.Initialize(GetGraphics().Device, "Assets/Models/Spider/spider.obj")) return false;
	if (!WallModel.Initialize(GetGraphics().Device, "Assets/Models/Room/Wall.obj")) return false;
	if (!FloorModel.Initialize(GetGraphics().Device, "Assets/Models/TestTerrain.obj")) return false;
	if (!GeneratorModel.Initialize(GetGraphics().Device, "Assets/Models/GeneratorBox.obj")) return false;

	MainPlayer = GameWorld.SpawnActor<PlayerActor>();
	MainPlayerController.Possess(MainPlayer);

	SpiderActor = GameWorld.SpawnActor<MonsterActor>();
	SpiderActor->GetActorTransform().Scale = { 0.01f, 0.01f, 0.01f };
	SpiderActor->AddComponent<MeshComponent>(&SpiderModel);
	SpiderActor->RegisterTarget(MainPlayer);

	LightGenerator = GameWorld.SpawnActor<GeneratorActor>();
	LightGenerator->GetActorTransform().Scale = { 5.0f, 5.0f, 5.0f };
	LightGenerator->GetActorTransform().Position = { 5.0f, 5.0f, 5.0f };
	LightGenerator->AddComponent<MeshComponent>(&GeneratorModel);
	LightGenerator->AddComponent<MeshColliderComponent>(&GeneratorModel);
	BoxColliderComponent* LightGeneratorCollider = LightGenerator->AddComponent<BoxColliderComponent>();
	AABB LightGeneratorCollision;
	LightGeneratorCollision.Min = { -0.5f, -0.5f, -0.1f };
	LightGeneratorCollision.Max = { 0.5f,  0.5f,  0.1f };
	LightGeneratorCollider->SetCollisionBox(LightGeneratorCollision);

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
	HUD.Initialize(GetGraphics());
	
	ShowCursor(FALSE);

	return true;
}

void LightSaverGame::Update(float deltaTime)
{
	if (CurrentGameState != GameState::Playing)
	{
		return;
	}

	MainPlayerController.Update(deltaTime, GetInput(), GameWorld);
	GameWorld.Update(deltaTime);

	if (MainPlayer != nullptr && !MainPlayer->IsAlive())
	{
		CurrentGameState = GameState::PlayerDead;
		return;
	}

	if (LightGenerator != nullptr && LightGenerator->IsRepaired())
	{
		CurrentGameState = GameState::GameClear;
	}
}

bool LightSaverGame::Render()
{
	if (MainPlayer == nullptr)
	{
		return false;
	}

	if (!RenderManager.Render(GameWorld, MainPlayer->GetCamera()))
	{
		return false;
	}

	return HUD.Render(CurrentGameState, MainPlayer->GetDamageAlpha(), MainPlayerController.IsFindGenerator(), MainPlayerController.IsInteracting(), LightGenerator->GetRepairProgress());
}



LightSaverGame::~LightSaverGame() = default;
