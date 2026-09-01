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

void PlayerActor::Kill()
{
	if (!bIsAlive) return;
	bIsAlive = false;
}
