#include "Transform.h"

DirectX::XMMATRIX Transform::GetWorldMatrix() const
{
    return XMMatrixScaling(Scale.x, Scale.y, Scale.z)* XMMatrixRotationRollPitchYaw(Rotation.x, Rotation.y, Rotation.z)* XMMatrixTranslation(Position.x, Position.y, Position.z);
}
