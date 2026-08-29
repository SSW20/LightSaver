#include "LightSaverGame.h"
#include "Shader.h"
#include "Actor.h"
#include "MeshComponent.h"
#include "Component.h"
#include "BoxColliderComponent.h"
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
	BoxColliderComponent* WallCollider = WallActor->AddComponent<BoxColliderComponent>();

	AABB WallCollision;
	WallCollision.Min = { -0.5f, -0.5f, -0.1f };
	WallCollision.Max = { 0.5f,  0.5f,  0.1f };
	WallCollider->SetCollisionBox(WallCollision);

	FloorActor = GameWorld.SpawnActor<Actor>();
	FloorActor->GetActorTransform().Position = { 0.0f, -0.45f, 4.0f };
	FloorActor->GetActorTransform().Scale = { 12.0f, 1.0f, 12.0f };
	FloorActor->AddComponent<MeshComponent>(&FloorModel);
	BoxColliderComponent* FloorCollider = FloorActor->AddComponent<BoxColliderComponent>();
	FloorCollider->SetBlockMovement(false);

	AABB FloorCollision;
	FloorCollision.Min = { -0.5f, -0.5f, -0.5f };
	FloorCollision.Max = { 0.5f,  0.5f,  0.5f };
	FloorCollider->SetCollisionBox(FloorCollision);


	RenderManager.Initialize(GetGraphics());
	ShowCursor(FALSE);

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
	DirectX::XMVECTOR Forward = MainCamera.GetForwardVector();
	Forward = DirectX::XMVectorSetY(Forward, 0.0f);
	Forward = DirectX::XMVector3Normalize(Forward);

	DirectX::XMVECTOR Right = MainCamera.GetRightVector();
	Right = DirectX::XMVectorSetY(Right, 0.0f);
	Right = DirectX::XMVector3Normalize(Right);

	DirectX::XMVECTOR Movement = DirectX::XMVectorScale(Forward, ForwardInput);
	Movement = DirectX::XMVectorAdd(Movement, DirectX::XMVectorScale(Right, RightInput));
	Movement = DirectX::XMVectorScale(Movement, MoveDistance);

	DirectX::XMFLOAT3 MoveAmount;
	DirectX::XMStoreFloat3(&MoveAmount, Movement);
	DirectX::XMFLOAT3 CameraPosition;
	DirectX::XMStoreFloat3(&CameraPosition, MainCamera.GetCameraPosition());

	const DirectX::XMFLOAT3 PlayerHalfSize = { 0.3f, 0.8f, 0.3f };

	DirectX::XMFLOAT3 TestPosition = CameraPosition;
	TestPosition.x += MoveAmount.x;
	if (!GameWorld.OverlapAABB(CreateAABBFromCenter(TestPosition, PlayerHalfSize)))
	{
		CameraPosition.x = TestPosition.x;
	}

	// Z 이동 검사
	// 여기서는 성공한 X 위치가 포함됨
	TestPosition = CameraPosition;
	TestPosition.z += MoveAmount.z;

	if (!GameWorld.OverlapAABB(CreateAABBFromCenter(TestPosition, PlayerHalfSize)))
	{
		CameraPosition.z = TestPosition.z;
	}

	// Y 축 검사

	bool bGround = false;
	Ray GroundRay = {};
	GroundRay.Direction = { 0,-1,0 };
	GroundRay.Origin = CameraPosition;
	RaycastHitResult OutHit;
	if (VerticalVelocity <= 0.0f)
	{
		if (GameWorld.Raycast(GroundRay, 0.85f, OutHit))
		{
			if (OutHit.Normal.y > 0.8f)
			{
				bGround = true;
			}
		}
	}
	if (bGround)
	{
		VerticalVelocity = 0.0f;
		CameraPosition.y = OutHit.Position.y + PlayerHalfSize.y;
	}
	if (bGround && GetInput().IsKeyPressed(VK_SPACE))
	{
		VerticalVelocity = JumpSpeed;
		bGround = false;
	}
	if(!bGround)
	{
		VerticalVelocity += Gravity * deltaTime;
		CameraPosition.y += VerticalVelocity * deltaTime;
	}

	MainCamera.SetCameraPosition(CameraPosition);
	MainCamera.AddRotation(GetInput().GetDeltaX() * MouseSpeed, -GetInput().GetDeltaY() * MouseSpeed);


	if (GetInput().IsKeyDown('E'))
	{
		Ray CameraRay = {};
		DirectX::XMStoreFloat3(&CameraRay.Direction, MainCamera.GetForwardVector());
		DirectX::XMStoreFloat3(&CameraRay.Origin, MainCamera.GetCameraPosition());
		RaycastHitResult OutHit;
		GameWorld.Raycast(CameraRay, 20.0f, OutHit);
	}

	GameWorld.Update(deltaTime);
}

bool LightSaverGame::Render()
{
	return RenderManager.Render(GameWorld, MainCamera);
}



LightSaverGame::~LightSaverGame() = default;
