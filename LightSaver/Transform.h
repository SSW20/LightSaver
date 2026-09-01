#pragma once
#include <DirectXMath.h>
using namespace DirectX;

struct Transform
{
    DirectX::XMFLOAT3 Position ={ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 Rotation ={ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

    DirectX::XMMATRIX GetWorldMatrix() const;
    DirectX::XMVECTOR GetRotationQuaternion() const;
    void SetRotationQuaternion(DirectX::FXMVECTOR InRotation);
    void UseEulerRotation() { bUseQuaternionRotation = false; }

private:
    DirectX::XMFLOAT4 QuaternionRotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool bUseQuaternionRotation = false;
};

