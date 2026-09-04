#include "PlayerActor.h"

Camera& PlayerActor::GetCamera()
{
	return PlayerCamera;
}

const Camera& PlayerActor::GetCamera() const
{
	return PlayerCamera;
}

const DirectX::XMFLOAT3& PlayerActor::GetPlayerPosition() const
{
	return GetActorTransform().Position;
}

void PlayerActor::SetPlayerPosition(const DirectX::XMFLOAT3& NewPosition)
{
	GetActorTransform().Position = NewPosition;
	DirectX::XMFLOAT3 CameraPosition = { 0.0f, 0.0f, 0.0f };
	CameraPosition.x = NewPosition.x + CameraOffset.x;
	CameraPosition.y = NewPosition.y + CameraOffset.y;
	CameraPosition.z = NewPosition.z + CameraOffset.z;
	PlayerCamera.SetCameraPosition(CameraPosition);

}

void PlayerActor::Reset(const DirectX::XMFLOAT3& SpawnPosition)
{
	bIsAlive = true;
	CurrentHealth = MaxHealth;
	InvincibleTimer = 0.0f;
	bFlashlightOn = false;
	FlashlightRemainingTime = 0.0f;
	FlashlightCooldownTimer = 0.0f;
	SetPlayerPosition(SpawnPosition);
}

void PlayerActor::Kill()
{
	if (!bIsAlive) return;
	bIsAlive = false;
}

void PlayerActor::TakeDamage(int Damage)
{
	if (!bIsAlive) return;
	if (InvincibleTimer > 0.0f) return;
	if (CurrentHealth <= Damage)
	{
		CurrentHealth = 0;
		Kill();
		return;
	}
	InvincibleTimer = InvincibleDuration;
	CurrentHealth -= Damage;
}

void PlayerActor::OnUpdate(float DeltaTime)
{
	if (InvincibleTimer > 0.0f)
	{
		InvincibleTimer -= DeltaTime;
	}
	InvincibleTimer = std::max(InvincibleTimer, 0.0f);

	if (bFlashlightOn)
	{
		FlashlightRemainingTime -= DeltaTime;
		if (FlashlightRemainingTime <= 0.0f)
		{
			bFlashlightOn = false;
			FlashlightRemainingTime = 0.0f;
			FlashlightCooldownTimer = FlashlightCooldown;
		}
	}
	else if (FlashlightCooldownTimer > 0.0f)
	{
		FlashlightCooldownTimer -= DeltaTime;
		FlashlightCooldownTimer = std::max(FlashlightCooldownTimer, 0.0f);
	}
}

void PlayerActor::ToggleFlashlight()
{
	if (bFlashlightOn)
	{
		bFlashlightOn = false;
		FlashlightRemainingTime = 0.0f;
		FlashlightCooldownTimer = FlashlightCooldown;
		return;
	}

	if (FlashlightCooldownTimer > 0.0f) return;

	bFlashlightOn = true;
	FlashlightRemainingTime = FlashlightDuration;
}

float PlayerActor::GetFlashlightCooldownRatio() const
{
	if (FlashlightCooldown <= 0.0f) return 0.0f;
	return FlashlightCooldownTimer / FlashlightCooldown;
}
