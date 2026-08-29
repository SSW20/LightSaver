#pragma once
#include <DirectXMath.h>
class Camera
{
public:
	DirectX::XMVECTOR GetForwardVector() const;
	DirectX::XMVECTOR GetRightVector() const;
	DirectX::XMVECTOR GetUpVector() const;

	DirectX::XMMATRIX GetViewMatrix() const;
	DirectX::XMMATRIX GetProjectionMatrix() const;

	DirectX::XMVECTOR GetCameraPosition() const;
	void SetCameraPosition(const DirectX::XMFLOAT3& Pos);
	void SetCameraPositionX(float PosX);
	void SetCameraPositionY(float PosY);
	void SetCameraPositionZ(float PosZ);

	void AddRight(float distance);
	void AddForward(float distance);
	void AddRotation(float yawDelta, float pitchDelta);
private:
	DirectX::XMFLOAT3 Position = { 0.0f, 0.85f, 0.0f };
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	float FovY = DirectX::XM_PIDIV4;
	float AspectRatio = 1280.0f / 720.0f;
	float NearZ = 0.1f;
	float FarZ = 100.f;
};

