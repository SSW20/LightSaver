#include "Transform.h"

DirectX::XMMATRIX Transform::GetWorldMatrix() const
{
	DirectX::XMMATRIX RotationMatrix;
	if (bUseQuaternionRotation)
	{
		RotationMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&QuaternionRotation));
	}
	else
	{
		RotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(Rotation.x,Rotation.y,Rotation.z);
	}

	return DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z) * RotationMatrix * DirectX::XMMatrixTranslation(Position.x, Position.y, Position.z);
}

DirectX::XMVECTOR Transform::GetRotationQuaternion() const
{
	if (bUseQuaternionRotation)
	{
		return DirectX::XMLoadFloat4(&QuaternionRotation);
	}

	return DirectX::XMQuaternionRotationRollPitchYaw(Rotation.x,Rotation.y,Rotation.z);
}

void Transform::SetRotationQuaternion(DirectX::FXMVECTOR InRotation)
{
	DirectX::XMVECTOR NormalizedRotation = DirectX::XMQuaternionNormalize(InRotation);

	DirectX::XMStoreFloat4(&QuaternionRotation, NormalizedRotation);
	bUseQuaternionRotation = true;
}
