#include "LightSaverGame.h"
#include "Shader.h"
#include "Actor.h"
#include "MeshComponent.h"
#include "Component.h"
#include <cmath>
#include <vector>
bool LightSaverGame::OnInitialize()
{
	HRESULT result;

	if (!SpiderModel.Initialize(GetGraphics().Device, "Assets/Models/Spider/spider.obj")) return false;
	if (!WallModel.Initialize(GetGraphics().Device, "Assets/Models/Room/Wall.obj")) return false;
	if (!FloorModel.Initialize(GetGraphics().Device, "Assets/Models/Room/Floor.obj")) return false;

	SpiderActor = GameWorld.SpawnActor<Actor>();
	SpiderActor->GetActorTransform().Scale = { 0.01f, 0.01f, 0.01f };
	SpiderActor->AddComponent<MeshComponent>(&SpiderModel);

	Actor* WallActor;
	Actor* FloorActor;
	WallActor = GameWorld.SpawnActor<Actor>();
	WallActor->GetActorTransform().Position = { 0.0f, 1.8f, 10.0f };
	WallActor->GetActorTransform().Scale = { 12.0f, 4.5f, 1.0f };
	WallActor->AddComponent<MeshComponent>(&WallModel);

	FloorActor = GameWorld.SpawnActor<Actor>();
	FloorActor->GetActorTransform().Position = { 0.0f, -0.45f, 4.0f };
	FloorActor->GetActorTransform().Scale = { 12.0f, 1.0f, 12.0f };
	FloorActor->AddComponent<MeshComponent>(&FloorModel);

	RenderManager.Initialize(GetGraphics());
	return true;
}

void LightSaverGame::Update(float deltaTime)
{
	Rotation += deltaTime;
	if (SpiderActor != nullptr)
	{
		SpiderActor->GetActorTransform().Rotation.y = Rotation;
	}
	float ForwardInput = 0.f;
	float RightInput = 0.f;
	if (GetInput().IsKeyDown('W'))
	{
		ForwardInput += 1.f;
	}
	if (GetInput().IsKeyDown('A'))
	{
		RightInput -= 1.f;
	}
	if (GetInput().IsKeyDown('S'))
	{
		ForwardInput -= 1.f;
	}
	if (GetInput().IsKeyDown('D'))
	{
		RightInput += 1.f;
	}

	float InputDistance = std::sqrt(ForwardInput * ForwardInput + RightInput * RightInput);
	if (InputDistance > 1.0f)
	{
		ForwardInput /= InputDistance;
		RightInput /= InputDistance;
	}
	float MoveDistance = CameraSpeed * deltaTime;
	MainCamera.AddForward(ForwardInput * MoveDistance);
	MainCamera.AddRight(RightInput * MoveDistance);
	MainCamera.AddRotation(GetInput().GetDeltaX() * MouseSpeed, -GetInput().GetDeltaY() * MouseSpeed);
	GameWorld.Update(deltaTime);
}

bool LightSaverGame::Render()
{
	return RenderManager.Render(GameWorld, MainCamera);
}



LightSaverGame::~LightSaverGame() = default;
