#include "PlayerActor.h"
#include "SoundManager.h"

#include <algorithm>

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
	BreathTimer = 0.0f;
	MonsterDistance = 1000000.0f;
	SoundManager::Get().StopLoop(SoundID::PlayerHeartbeat);
	bFlashlightOn = false;
	FlashlightRemainingTime = 0.0f;
	FlashlightCooldownTimer = 0.0f;
	SetPlayerPosition(SpawnPosition);
}

void PlayerActor::Kill()
{
	if (!bIsAlive) return;
	bIsAlive = false;
	BreathTimer = 0.0f;
	SoundManager::Get().StopLoop(SoundID::PlayerHeartbeat);
}

void PlayerActor::TakeDamage(int Damage)
{
	if (!bIsAlive) return;
	if (InvincibleTimer > 0.0f) return;
	SoundManager::Get().Play2D(SoundID::PlayerDamage, 0.9f);
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
	// 몬스터가 월드 거리 18 밖이면 0, 3 안이면 1이 되는 위험도다.
	const float DistanceRange = ThreatAudioStartDistance - ThreatAudioFullDistance;
	const float ThreatRatio = DistanceRange > 0.0f
		? std::clamp(
			(ThreatAudioStartDistance - MonsterDistance) / DistanceRange,
			0.0f,
			1.0f)
		: 0.0f;

	if (bIsAlive && ThreatRatio > 0.0f)
	{
		// 가까워질수록 심장 박동이 커지고, 범위 밖에서는 Loop 자체를 멈춘다.
		SoundManager::Get().StartLoop2D(
			SoundID::PlayerHeartbeat,
			0.8f * ThreatRatio);
	}
	else
	{
		SoundManager::Get().StopLoop(SoundID::PlayerHeartbeat);
	}

	if (bIsAlive && ThreatRatio > 0.0f)
	{
		BreathTimer -= DeltaTime;
		if (BreathTimer <= 0.0f)
		{
			// 심장 소리처럼 피해 여부와 관계없이 몬스터와 가까울수록 커진다.
			SoundManager::Get().PlayRandom2D(
				SoundID::PlayerBreath,
				0.55f * ThreatRatio);
			BreathTimer = BreathInterval;
		}
	}
	else
	{
		BreathTimer = 0.0f;
	}

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
			SoundManager::Get().Play2D(SoundID::FlashlightOff, 0.7f);
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
		SoundManager::Get().Play2D(SoundID::FlashlightOff, 0.7f);
		return;
	}

	if (FlashlightCooldownTimer > 0.0f) return;

	bFlashlightOn = true;
	FlashlightRemainingTime = FlashlightDuration;
	SoundManager::Get().Play2D(SoundID::FlashlightOn, 0.7f);
}

float PlayerActor::GetFlashlightCooldownRatio() const
{
	if (FlashlightCooldown <= 0.0f) return 0.0f;
	return FlashlightCooldownTimer / FlashlightCooldown;
}
