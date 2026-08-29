#pragma once
#include <DirectXMath.h>
class Actor;
struct AABB
{
	DirectX::XMFLOAT3 Min = {};
	DirectX::XMFLOAT3 Max = {};
};

bool Intersects(const AABB& A, const AABB& B);
AABB CreateAABBFromCenter(const DirectX::XMFLOAT3& Center,const DirectX::XMFLOAT3& HalfSize);

struct Ray
{
    DirectX::XMFLOAT3 Origin = {};
    DirectX::XMFLOAT3 Direction = {};
};

struct RaycastHitResult
{
    float Distance = 0.0f;
    DirectX::XMFLOAT3 Position = {};
    DirectX::XMFLOAT3 Normal = {};
    Actor* HitActor = nullptr;
};

bool RaycastAABB(const Ray& TestRay, const AABB& Box,float MaxDistance, RaycastHitResult& OutHit);
bool UpdateRayInterval(float Origin, float Direction,float SlabMin, float SlabMax, DirectX::XMFLOAT3 MinFaceNormal, DirectX::XMFLOAT3 MaxFaceNormal, float& TEnter, float& TExit, DirectX::XMFLOAT3& EnterNormal);