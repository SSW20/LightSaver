#pragma once

#include <DirectXMath.h>
#include "PlayerActor.h"

class Camera;
class InputManager;
class World;
class PlayerController
{
public:
	PlayerController() = default;
	void Update(float DeltaTime,InputManager& Input,World& GameWorld);
	void Possess(PlayerActor* InPlayer);

	bool IsInteracting() { return bInteracting; }
	bool IsFindGenerator() { return bFocusGenerator; }
	//bool IsLookingAt(Actor* Target) { return CurrentFocusActor == Target; }

private:
	void UpdateRotation(InputManager& Input);
	void UpdateMovement(float DeltaTime, InputManager& Input, World& GameWorld);
	void UpdateVerticalMovement(float DeltaTime, InputManager& Input, World& GameWorld);
	void UpdateInteraction(InputManager& Input, World& GameWorld, float DeltaTime);

	PlayerActor* ControlledPlayer = nullptr;
	Camera* MainCamera = nullptr;
	float CameraSpeed = 3.0f;
	float MouseSpeed = DirectX::XMConvertToRadians(0.1f);
	float VerticalVelocity = 0.0f;
	float Gravity = -9.8f;
	float JumpSpeed = 5.0f;
	DirectX::XMFLOAT3 PlayerHalfSize = { 0.3f, 0.8f, 0.3f };

	Actor* CurrentFocusActor = nullptr;
	bool bFocusGenerator = false;
	bool bInteracting = false;
};
