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
#include "SkeletalMeshComponent.h"

bool LightSaverGame::OnInitialize()
{
	constexpr float LevelScale = HospitalLevel::WorldScale;

	// Tree Ent의 몸과 Skeleton 정보를 읽는다.
	if (!TreeEntModel.Initialize(GetGraphics().Device, "Assets/Models/TreeEnt/TreeEntAsh.fbx",
		{
			{ "M_TreeEntAsh_Branch", "Textures/T_TreeEntAsh_Branches_BaseColor.png" },
			{ "M_TreeEntAsh_Head", "Textures/T_TreeEntAsh_Head_BaseColor.png" },
			{ "M_TreeEntAsh_Down", "Textures/T_TreeEntAsh_Down_BaseColor.png" },
			{ "M_TreeEntAsh_Up", "Textures/T_TreeEntAsh_Up_BaseColor.png" }
		})) return false;

	// 같은 Skeleton을 사용하는 애니메이션 동작들을 읽는다.
	if (!TreeEntIdleAnimation.Initialize("Assets/Models/TreeEnt/Animations/Idle1.fbx")) return false;
	if (!TreeEntWalkAnimation.Initialize("Assets/Models/TreeEnt/Animations/Walk.fbx")) return false;
	if (!TreeEntAttackAnimation.Initialize("Assets/Models/TreeEnt/Animations/Attack1.fbx")) return false;

	if (!GeneratorModel.Initialize(GetGraphics().Device, "Assets/Models/GeneratorBox.obj")) return false;
	if (!Hospital.Initialize(GetGraphics().Device, GameWorld)) return false;

	MainPlayer = GameWorld.SpawnActor<PlayerActor>();
	MainPlayerController.Possess(MainPlayer);

	SpiderActor = GameWorld.SpawnActor<MonsterActor>();
	SpiderActor->GetActorTransform().Scale = { 0.007f, 0.007f, 0.007f };
	SkeletalMeshComponent* TreeEntMesh = SpiderActor->AddComponent<SkeletalMeshComponent>(&TreeEntModel);
	SpiderActor->RegisterTarget(MainPlayer);
	SpiderActor->SetAnimations(&TreeEntIdleAnimation,&TreeEntWalkAnimation, &TreeEntAttackAnimation);

	LightGenerator = GameWorld.SpawnActor<GeneratorActor>();
	LightGenerator->GetActorTransform().Scale = { 5.0f, 5.0f, 5.0f };
	LightGenerator->AddComponent<MeshComponent>(&GeneratorModel);
	LightGenerator->AddComponent<MeshColliderComponent>(&GeneratorModel);
	BoxColliderComponent* LightGeneratorCollider = LightGenerator->AddComponent<BoxColliderComponent>();
	AABB LightGeneratorCollision;
	LightGeneratorCollision.Min = { -0.5f, -0.5f, -0.1f };
	LightGeneratorCollision.Max = { 0.5f,  0.5f,  0.1f };
	LightGeneratorCollider->SetCollisionBox(LightGeneratorCollision);

	const DirectX::XMFLOAT3 FirstFloorMin = { -14.0f * LevelScale,-1.0f * LevelScale,-10.0f * LevelScale };
	const DirectX::XMFLOAT3 FirstFloorMax = { 14.0f * LevelScale,4.5f * LevelScale,18.0f * LevelScale };
	const DirectX::XMFLOAT3 SecondFloorGridMin = { -14.0f * LevelScale,4.5f * LevelScale,-10.0f * LevelScale };
	const DirectX::XMFLOAT3 SecondFloorGridMax = { 14.0f * LevelScale,10.5f * LevelScale,18.0f * LevelScale };
	const DirectX::XMFLOAT3 MonsterHalfSize = { 1.12f,0.7f,1.12f };

	RaycastHitResult GroundHit = {};
	PlayerSpawnPosition = { 0.0f, FirstFloorMax.y, -6.0f * LevelScale };
	if (!GameWorld.FindFloor(PlayerSpawnPosition, 0.0f, FirstFloorMax.y - FirstFloorMin.y, GroundHit)) return false;
	PlayerSpawnPosition.y = GroundHit.Position.y + 0.8f;
	MainPlayer->SetPlayerPosition(PlayerSpawnPosition);

	SpiderSpawnPosition = { 0.0f, SecondFloorGridMax.y, 14.0f * LevelScale };
	if (!GameWorld.FindFloor(SpiderSpawnPosition, 0.0f, SecondFloorGridMax.y - SecondFloorGridMin.y, GroundHit)) return false;
	SpiderSpawnPosition.y = GroundHit.Position.y + SpiderActor->GetGroundOffset();
	SpiderActor->GetActorTransform().Position = SpiderSpawnPosition;

	DirectX::XMFLOAT3 GeneratorPosition = { 8.0f * LevelScale, SecondFloorGridMax.y, 8.0f * LevelScale };
	if (!GameWorld.FindFloor(GeneratorPosition, 0.0f, SecondFloorGridMax.y - SecondFloorGridMin.y, GroundHit)) return false;
	GeneratorPosition.y = GroundHit.Position.y + 2.5f;
	LightGenerator->GetActorTransform().Position = GeneratorPosition;

	ExitZone = GameWorld.SpawnActor<ExitZoneActor>();
	DirectX::XMFLOAT3 ExitZonePosition = { -12.0f * LevelScale, SecondFloorGridMax.y, 16.0f * LevelScale };
	if (!GameWorld.FindFloor(ExitZonePosition, 0.0f, SecondFloorGridMax.y - SecondFloorGridMin.y, GroundHit)) return false;
	ExitZonePosition.y = GroundHit.Position.y + 0.05f * LevelScale;
	ExitZone->GetActorTransform().Position = ExitZonePosition;
	ExitZone->GetActorTransform().Scale = { 4.0f * LevelScale, 0.1f * LevelScale, 4.0f * LevelScale };
	ExitZone->SetTriggerHalfSize({ 2.0f * LevelScale, 1.5f * LevelScale, 2.0f * LevelScale });
	ExitZone->AddComponent<MeshComponent>(&GeneratorModel);

	if (!FirstFloorMonsterNavGrid.Build(GameWorld, FirstFloorMin, FirstFloorMax, 0.5f * LevelScale, MonsterHalfSize)) return false;
	if (!SecondFloorMonsterNavGrid.Build(GameWorld, SecondFloorGridMin, SecondFloorGridMax, 0.5f * LevelScale, MonsterHalfSize)) return false;
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
