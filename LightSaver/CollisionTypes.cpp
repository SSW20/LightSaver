#include "CollisionTypes.h"
#include <algorithm>
#include <cmath>


bool Intersects(const AABB& A, const AABB& B)
{
	if (A.Max.x <= B.Min.x || B.Max.x <= A.Min.x)
	{
		return false;
	}

	if (A.Max.y <= B.Min.y || B.Max.y <= A.Min.y)
	{
		return false;
	}

	if (A.Max.z <= B.Min.z || B.Max.z <= A.Min.z)
	{
		return false;
	}

	return true;
}

AABB CreateAABBFromCenter(const DirectX::XMFLOAT3& Center, const DirectX::XMFLOAT3& HalfSize)
{
	AABB NewAABB = {};
	DirectX::XMVECTOR CenterVec = DirectX::XMLoadFloat3(&Center);
	DirectX::XMVECTOR HalfSizeVec = DirectX::XMLoadFloat3(&HalfSize);

	DirectX::XMVECTOR NewMaxVec = DirectX::XMVectorAdd(CenterVec, HalfSizeVec);
	DirectX::XMVECTOR NewMinVec = DirectX::XMVectorSubtract(CenterVec, HalfSizeVec);

	DirectX::XMStoreFloat3(&NewAABB.Max, NewMaxVec);
	DirectX::XMStoreFloat3(&NewAABB.Min, NewMinVec);

	return NewAABB;
}

bool RaycastAABB(const Ray& TestRay, const AABB& Box, float MaxDistance, RaycastHitResult& OutHit)
{
	float TEnter = 0.0f;
	float TExit = MaxDistance;
	DirectX::XMFLOAT3 EnterNormal = { 0,0,0 };

	if (!UpdateRayInterval(TestRay.Origin.x, TestRay.Direction.x, Box.Min.x, Box.Max.x, { -1,0,0 }, {1,0,0},TEnter, TExit, EnterNormal))
	{
		return false;
	}
	if (!UpdateRayInterval(TestRay.Origin.y, TestRay.Direction.y, Box.Min.y, Box.Max.y, { 0,-1,0 }, { 0,1,0 }, TEnter, TExit, EnterNormal))
	{
		return false;
	}
	if (!UpdateRayInterval(TestRay.Origin.z, TestRay.Direction.z, Box.Min.z, Box.Max.z, { 0,0,-1 }, { 0,0,1 }, TEnter, TExit, EnterNormal))
	{
		return false;
	}
	
	OutHit.Distance = TEnter;
	
	DirectX::XMVECTOR OriginVec = DirectX::XMLoadFloat3(&TestRay.Origin);
	DirectX::XMVECTOR DirectionVec = DirectX::XMLoadFloat3(&TestRay.Direction);
	DirectX::XMVECTOR HitPosVec = DirectX::XMVectorAdd(OriginVec, DirectX::XMVectorScale(DirectionVec, TEnter));
	DirectX::XMStoreFloat3(&OutHit.Position, HitPosVec);
	OutHit.Normal = EnterNormal;

	return true;
}

bool UpdateRayInterval(float Origin, float Direction, float SlabMin, float SlabMax, DirectX::XMFLOAT3 MinFaceNormal, DirectX::XMFLOAT3 MaxFaceNormal, float& TEnter, float& TExit, DirectX::XMFLOAT3& EnterNormal)
{
	constexpr float Epsilon = 0.000001f;

	if (std::abs(Direction) < Epsilon)
	{
		return Origin >= SlabMin && Origin <= SlabMax;
	}

	float T1 = (SlabMin - Origin) / Direction;
	float T2 = (SlabMax - Origin) / Direction;

	if (T1 > T2)
	{
		std::swap(T1, T2);
		std::swap(MinFaceNormal, MaxFaceNormal);
	}
	if (TEnter < T1)
	{
		TEnter = T1;
		EnterNormal = MinFaceNormal;
	}
	TExit = std::min(TExit, T2);
	return TEnter <= TExit;
}

