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
#include "ExitZoneActor.h"

bool LightSaverGame::OnInitialize()
{
	if (!SpiderModel.Initialize(GetGraphics().Device, "Assets/Models/Spider/spider.obj")) return false;
	if (!GeneratorModel.Initialize(GetGraphics().Device, "Assets/Models/GeneratorBox.obj")) return false;
	if (!Hospital.Initialize(GetGraphics().Device, GameWorld)) return false;

	MainPlayer = GameWorld.SpawnActor<PlayerActor>();
	MainPlayerController.Possess(MainPlayer);

	SpiderActor = GameWorld.SpawnActor<MonsterActor>();
	SpiderActor->GetActorTransform().Scale = { 0.01f, 0.01f, 0.01f };
	SpiderActor->AddComponent<MeshComponent>(&SpiderModel);
	SpiderActor->RegisterTarget(MainPlayer);

	LightGenerator = GameWorld.SpawnActor<GeneratorActor>();
	LightGenerator->GetActorTransform().Scale = { 5.0f, 5.0f, 5.0f };
	LightGenerator->AddComponent<MeshComponent>(&GeneratorModel);
	LightGenerator->AddComponent<MeshColliderComponent>(&GeneratorModel);
	BoxColliderComponent* LightGeneratorCollider = LightGenerator->AddComponent<BoxColliderComponent>();
	AABB LightGeneratorCollision;
	LightGeneratorCollision.Min = { -0.5f, -0.5f, -0.1f };
	LightGeneratorCollision.Max = { 0.5f,  0.5f,  0.1f };
	LightGeneratorCollider->SetCollisionBox(LightGeneratorCollision);

	const DirectX::XMFLOAT3 FirstFloorMin = { -14.0f,-1.0f,-10.0f };
	const DirectX::XMFLOAT3 FirstFloorMax = { 14.0f,4.5f,18.0f };
	const DirectX::XMFLOAT3 SecondFloorGridMin = { -14.0f,4.5f,-10.0f };
	const DirectX::XMFLOAT3 SecondFloorGridMax = { 14.0f,10.5f,18.0f };
	const DirectX::XMFLOAT3 MonsterHalfSize = { 0.8f,0.5f,0.8f };

	RaycastHitResult GroundHit = {};
	PlayerSpawnPosition = { 0.0f, FirstFloorMax.y, -6.0f };
	if (!GameWorld.FindFloor(PlayerSpawnPosition, 0.0f, FirstFloorMax.y - FirstFloorMin.y, GroundHit)) return false;
	PlayerSpawnPosition.y = GroundHit.Position.y + 0.8f;
	MainPlayer->SetPlayerPosition(PlayerSpawnPosition);

	SpiderSpawnPosition = { 0.0f, SecondFloorGridMax.y, 14.0f };
	if (!GameWorld.FindFloor(SpiderSpawnPosition, 0.0f, SecondFloorGridMax.y - SecondFloorGridMin.y, GroundHit)) return false;
	SpiderSpawnPosition.y = GroundHit.Position.y + 0.4223f;
	SpiderActor->GetActorTransform().Position = SpiderSpawnPosition;

	DirectX::XMFLOAT3 GeneratorPosition = { 8.0f, SecondFloorGridMax.y, 8.0f };
	if (!GameWorld.FindFloor(GeneratorPosition, 0.0f, SecondFloorGridMax.y - SecondFloorGridMin.y, GroundHit)) return false;
	GeneratorPosition.y = GroundHit.Position.y + 2.5f;
	LightGenerator->GetActorTransform().Position = GeneratorPosition;

	ExitZone = GameWorld.SpawnActor<ExitZoneActor>();
	DirectX::XMFLOAT3 ExitZonePosition = { -12.0f, SecondFloorGridMax.y, 16.0f };
	if (!GameWorld.FindFloor(ExitZonePosition, 0.0f, SecondFloorGridMax.y - SecondFloorGridMin.y, GroundHit)) return false;
	ExitZonePosition.y = GroundHit.Position.y + 0.05f;
	ExitZone->GetActorTransform().Position = ExitZonePosition;
	ExitZone->GetActorTransform().Scale = { 4.0f, 0.1f, 4.0f };
	ExitZone->SetTriggerHalfSize({ 2.0f, 1.5f, 2.0f });
	ExitZone->AddComponent<MeshComponent>(&GeneratorModel);

	if (!FirstFloorMonsterNavGrid.Build(GameWorld, FirstFloorMin, FirstFloorMax, 0.5f, MonsterHalfSize)) return false;
	if (!SecondFloorMonsterNavGrid.Build(GameWorld, SecondFloorGridMin, SecondFloorGridMax, 0.5f, MonsterHalfSize)) return false;
	SpiderActor->Initialize(&GameWorld, &FirstFloorMonsterNavGrid, &SecondFloorMonsterNavGrid);

	RenderManager.Initialize(GetGraphics());
	HUD.Initialize(GetGraphics());
	
	return true;
}

void LightSaverGame::Update(float deltaTime)
{
	if (CurrentGameState == GameState::MainMenu)
	{
		if (GetInput().IsKeyPressed('E'))
		{
			StartGame();
		}

		return;
	}

	if (CurrentGameState == GameState::PlayerDead || CurrentGameState == GameState::GameClear)
	{
		const GameHUDAction Action = HUD.UpdateResultInput(CurrentGameState, GetInput());
		if (Action == GameHUDAction::StartGame)
		{
			StartGame();
		}
		else if (Action == GameHUDAction::ExitGame)
		{
			RequestExit();
		}

		return;
	}

	MainPlayerController.Update(deltaTime, GetInput(), GameWorld);
	GameWorld.Update(deltaTime);

	if (MainPlayer != nullptr && !MainPlayer->IsAlive())
	{
		CurrentGameState = GameState::PlayerDead;
		GetInput().SetMouseLocked(false);
		return;
	}

	if (LightGenerator != nullptr && LightGenerator->IsRepaired() &&
		ExitZone != nullptr && ExitZone->Contains(MainPlayer->GetPlayerPosition()))
	{
		CurrentGameState = GameState::GameClear;
		GetInput().SetMouseLocked(false);
	}
}

void LightSaverGame::StartGame()
{
	if (MainPlayer != nullptr)
	{
		MainPlayer->Reset(PlayerSpawnPosition);
	}

	if (SpiderActor != nullptr)
	{
		SpiderActor->Reset(SpiderSpawnPosition);
	}

	if (LightGenerator != nullptr)
	{
		LightGenerator->Reset();
	}

	MainPlayerController.Reset();
	CurrentGameState = GameState::Playing;
	GetInput().SetMouseLocked(true);
}

bool LightSaverGame::Render()
{
	if (MainPlayer == nullptr)
	{
		return false;
	}

	if (!RenderManager.Render(GameWorld, MainPlayer->GetCamera(), MainPlayer->IsFlashlightOn()))
	{
		return false;
	}

	return HUD.Render(CurrentGameState, MainPlayer->GetDamageAlpha(), MainPlayerController.IsFindGenerator(), MainPlayerController.IsInteracting(), LightGenerator->GetRepairProgress());
}



LightSaverGame::~LightSaverGame() = default;
