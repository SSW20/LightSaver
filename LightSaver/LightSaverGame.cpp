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
	MainPlayer->SetPlayerPosition({ 0.0f, 1.85f, 0.0f });
	MainPlayerController.Possess(MainPlayer);

	SpiderActor = GameWorld.SpawnActor<MonsterActor>();
	SpiderActor->GetActorTransform().Scale = { 0.01f, 0.01f, 0.01f };
	SpiderActor->AddComponent<MeshComponent>(&SpiderModel);
	SpiderActor->Initialize(&GameWorld);
	SpiderActor->RegisterTarget(MainPlayer);
	SpiderActor->GetActorTransform().Position.y = 2.05f;

	Actor* WallActor;
	Actor* FloorActor;
	WallActor = GameWorld.SpawnActor<Actor>();
	WallActor->GetActorTransform().Position = { 0.0f, 1.8f, 10.0f };
	WallActor->GetActorTransform().Scale = { 12.0f, 4.5f, 1.0f };
	WallActor->AddComponent<MeshComponent>(&WallModel);
	BoxColliderComponent* WallCollider = WallActor->AddComponent<BoxColliderComponent>();

	AABB WallCollision;
	WallCollision.Min = { -0.5f, -0.5f, -0.1f };
	WallCollision.Max = { 0.5f,  0.5f,  0.1f };
	WallCollider->SetCollisionBox(WallCollision);

	FloorActor = GameWorld.SpawnActor<Actor>();
	FloorActor->GetActorTransform().Position = { 0.0f, -0.45f, 4.0f };
	FloorActor->GetActorTransform().Scale = { 12.0f, 1.0f, 12.0f };
	FloorActor->AddComponent<MeshComponent>(&FloorModel);
	FloorActor->AddComponent<MeshColliderComponent>(&FloorModel);

	const DirectX::XMFLOAT3 GridMin = { -5.5f,-1.0f,-1.5f };
	const DirectX::XMFLOAT3 GridMax = { 5.5f,5.0f,9.5f };
	const DirectX::XMFLOAT3 MonsterHalfSize = { 0.4f,0.5f,0.4f };
	if (!MonsterNavGrid.Build(GameWorld, GridMin, GridMax, 0.5f, MonsterHalfSize)) return false;


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
