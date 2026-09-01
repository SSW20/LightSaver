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
	void Kill();
	bool IsAlive() const { return bIsAlive; }

private:
	Camera PlayerCamera;
	DirectX::XMFLOAT3 CameraOffset = { 0.0f, 0.65f, 0.0f };
	bool bIsAlive = true;
};
