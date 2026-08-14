#include "Camera.h"
#include <cmath>
#include <algorithm>

DirectX::XMVECTOR Camera::GetForwardVector() const
{
    DirectX::XMVECTOR forward = DirectX::XMVectorSet(
        std::cos(Pitch) * std::sin(Yaw),
        std::sin(Pitch),
        std::cos(Pitch) * std::cos(Yaw),
        0.0f);

    return DirectX::XMVector3Normalize(forward);
}

DirectX::XMVECTOR Camera::GetRightVector() const
{
    DirectX::XMVECTOR WorldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(WorldUp, GetForwardVector()));
}

DirectX::XMVECTOR Camera::GetUpVector() const
{
    return DirectX::XMVector3Normalize(DirectX::XMVector3Cross(GetForwardVector(), GetRightVector()));
}

DirectX::XMMATRIX Camera::GetViewMatrix() const
{
    DirectX::XMVECTOR CameraPos = DirectX::XMLoadFloat3(&Position);
   
    return  DirectX::XMMatrixLookToLH(CameraPos, GetForwardVector(), GetUpVector());

}

DirectX::XMMATRIX Camera::GetProjectionMatrix() const
{
    return DirectX::XMMatrixPerspectiveFovLH(FovY, AspectRatio, NearZ, FarZ);
}

void Camera::AddForward(float distance)
{
    DirectX::XMVECTOR Forward = GetForwardVector();
    Forward = DirectX::XMVectorSetY(Forward, 0.f);

    Forward = DirectX::XMVector3Normalize(Forward);


    DirectX::XMVECTOR NewPos = DirectX::XMLoadFloat3(&Position);
    DirectX::XMVECTOR MoveDistance = DirectX::XMVectorScale(Forward, distance);

    NewPos = DirectX::XMVectorAdd(NewPos, MoveDistance);
    DirectX::XMStoreFloat3(&Position, NewPos);
}

void Camera::AddRight(float distance)
{
    DirectX::XMVECTOR Right = GetRightVector();

    DirectX::XMVECTOR NewPos = DirectX::XMLoadFloat3(&Position);
    DirectX::XMVECTOR MoveDistance = DirectX::XMVectorScale(Right, distance);

    NewPos = DirectX::XMVectorAdd(NewPos, MoveDistance);
    DirectX::XMStoreFloat3(&Position, NewPos);
}

void Camera::AddRotation(float yawDelta, float pitchDelta)
{
    Yaw += yawDelta;
    Pitch += pitchDelta;

    Pitch = std::clamp(Pitch, DirectX::XMConvertToRadians(-89.f), DirectX::XMConvertToRadians(89.f));
}
