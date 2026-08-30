#include "PlayerController.h"
#include "Camera.h"
#include "InputManager.h"
#include <cmath>
#include "World.h"



void PlayerController::Update(float DeltaTime, InputManager& Input, World& GameWorld)
{
	UpdateMovement(DeltaTime, Input, GameWorld);
	UpdateVerticalMovement(DeltaTime, Input, GameWorld);
	UpdateRotation(Input);
	UpdateInteraction(Input, GameWorld);
}

void PlayerController::Possess(PlayerActor* InPlayer)
{
	ControlledPlayer = InPlayer;
	if (ControlledPlayer != nullptr)
	{
		MainCamera = &ControlledPlayer->GetCamera();
	}
	else
	{
		MainCamera = nullptr;
	}
}

void PlayerController::UpdateRotation(InputManager& Input)
{
	if (MainCamera == nullptr)
	{
		return;
	}
	MainCamera->AddRotation(Input.GetDeltaX() * MouseSpeed, -Input.GetDeltaY() * MouseSpeed);
}

void PlayerController::UpdateMovement(float DeltaTime, InputManager& Input, World& GameWorld)
{
	if (ControlledPlayer == nullptr || MainCamera == nullptr)
	{
		return;
	}

	float ForwardInput = 0.f;
	float RightInput = 0.f;
	if (Input.IsKeyDown('W'))
	{
		ForwardInput += 1.f;
	}
	if (Input.IsKeyDown('A'))
	{
		RightInput -= 1.f;
	}
	if (Input.IsKeyDown('S'))
	{
		ForwardInput -= 1.f;
	}
	if (Input.IsKeyDown('D'))
	{
		RightInput += 1.f;
	}


	float InputDistance = std::sqrt(ForwardInput * ForwardInput + RightInput * RightInput);
	if (InputDistance > 1.0f)
	{
		ForwardInput /= InputDistance;
		RightInput /= InputDistance;
	}
	float MoveDistance = CameraSpeed * DeltaTime;
	DirectX::XMVECTOR Forward = MainCamera->GetForwardVector();
	Forward = DirectX::XMVectorSetY(Forward, 0.0f);
	Forward = DirectX::XMVector3Normalize(Forward);

	DirectX::XMVECTOR Right = MainCamera->GetRightVector();
	Right = DirectX::XMVectorSetY(Right, 0.0f);
	Right = DirectX::XMVector3Normalize(Right);

	DirectX::XMVECTOR Movement = DirectX::XMVectorScale(Forward, ForwardInput);
	Movement = DirectX::XMVectorAdd(Movement, DirectX::XMVectorScale(Right, RightInput));
	Movement = DirectX::XMVectorScale(Movement, MoveDistance);

	DirectX::XMFLOAT3 MoveAmount;
	DirectX::XMStoreFloat3(&MoveAmount, Movement);
	DirectX::XMFLOAT3 PlayerPosition = ControlledPlayer->GetPlayerPosition();

	DirectX::XMFLOAT3 TestPosition = PlayerPosition;
	TestPosition.x += MoveAmount.x;
	if (!GameWorld.OverlapAABB(CreateAABBFromCenter(TestPosition, PlayerHalfSize)))
	{
		PlayerPosition.x = TestPosition.x;
	}

	// Z 이동 검사
	// 여기서는 성공한 X 위치가 포함됨
	TestPosition = PlayerPosition;
	TestPosition.z += MoveAmount.z;

	if (!GameWorld.OverlapAABB(CreateAABBFromCenter(TestPosition, PlayerHalfSize)))
	{
		PlayerPosition.z = TestPosition.z;
	}

	ControlledPlayer->SetPlayerPosition(PlayerPosition);
}

void PlayerController::UpdateVerticalMovement(float DeltaTime, InputManager& Input, World& GameWorld)
{
	if (ControlledPlayer == nullptr || MainCamera == nullptr)
	{
		return;
	}

	DirectX::XMFLOAT3 PlayerPosition = ControlledPlayer->GetPlayerPosition();

	// Y 축 검사
	bool bGround = false;
	Ray GroundRay = {};
	GroundRay.Direction = { 0,-1,0 };
	GroundRay.Origin = PlayerPosition;
	RaycastHitResult OutHit;
	const float GroundTolerance = 0.05f;
	const float GroundCheckDistance = PlayerHalfSize.y + GroundTolerance;
	if (VerticalVelocity <= 0.0f)
	{
		if (GameWorld.Raycast(GroundRay, GroundCheckDistance, OutHit))
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
		PlayerPosition.y = OutHit.Position.y + PlayerHalfSize.y;
	}
	if (bGround && Input.IsKeyPressed(VK_SPACE))
	{
		VerticalVelocity = JumpSpeed;
		bGround = false;
	}
	if (!bGround)
	{
		VerticalVelocity += Gravity * DeltaTime;
		PlayerPosition.y += VerticalVelocity * DeltaTime;
	}

	ControlledPlayer->SetPlayerPosition(PlayerPosition);
}

void PlayerController::UpdateInteraction(InputManager& Input, World& GameWorld)
{
	if (MainCamera == nullptr)
	{
		return;
	}

	if (!Input.IsKeyPressed('E'))
	{
		return;
	}

	Ray CameraRay = {};
	DirectX::XMStoreFloat3(&CameraRay.Direction, MainCamera->GetForwardVector());
	DirectX::XMStoreFloat3(&CameraRay.Origin, MainCamera->GetCameraPosition());

	RaycastHitResult OutHit;
	GameWorld.Raycast(CameraRay, 20.0f, OutHit);
}
