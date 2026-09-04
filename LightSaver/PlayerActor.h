#pragma once

#include "Actor.h"
#include "Camera.h"

class PlayerActor : public Actor
{
public:
	Camera& GetCamera();
	const Camera& GetCamera() const;
	const DirectX::XMFLOAT3& GetPlayerPosition() const;
	void SetPlayerPosition(const DirectX::XMFLOAT3& NewPosition);
	void Reset(const DirectX::XMFLOAT3& SpawnPosition);
	void Kill();
	bool IsAlive() const { return bIsAlive; }

	void TakeDamage(int Damage);
	float GetDamageAlpha() const { return 1.0f - float(CurrentHealth) / float(MaxHealth); }
	int GetCurrentHealth() const { return CurrentHealth; }
	void ToggleFlashlight();
	bool IsFlashlightOn() const { return bFlashlightOn; }
	float GetFlashlightCooldownRatio() const;

protected:
	void OnUpdate(float DeltaTime) override;
private:
	Camera PlayerCamera;
	DirectX::XMFLOAT3 CameraOffset = { 0.0f, 0.65f, 0.0f };
	bool bIsAlive = true;
	int MaxHealth = 3;
	int CurrentHealth = 3;

	float InvincibleDuration = 1.0f;
	float InvincibleTimer = 0.0f;

	bool bFlashlightOn = false;
	float FlashlightDuration = 4.0f;
	float FlashlightRemainingTime = 0.0f;
	float FlashlightCooldown = 3.0f;
	float FlashlightCooldownTimer = 0.0f;
};
