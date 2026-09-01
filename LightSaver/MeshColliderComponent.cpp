#include "MeshColliderComponent.h"
#include "Model.h"
#include "Mesh.h"
#include "Actor.h"

MeshColliderComponent::MeshColliderComponent(Actor* Owner, const Model* InModel)
	: Component(Owner), CollisionModel(InModel)
{
}

void MeshColliderComponent::SetCollisionModel(const Model* InModel)
{
	CollisionModel = InModel;
}

bool MeshColliderComponent::Raycast(const Ray& TestRay, float MaxDistance, RaycastHitResult& OutHit) const
{
	if (CollisionModel == nullptr)
	{
		return false;
	}
	
	DirectX::XMMATRIX WorldMatrix = GetOwner()->GetActorTransform().GetWorldMatrix();
	bool bHit = false;
	float ClosestDistance = MaxDistance;

	for (size_t MeshIndex = 0; MeshIndex < CollisionModel->GetMeshCount(); ++MeshIndex)
	{
		const Mesh* CurrentMesh = CollisionModel->GetMesh(MeshIndex);
		if (CurrentMesh == nullptr) continue;

		const std::vector<Vertex>& Vertices = CurrentMesh->GetVertices();
		const std::vector<UINT>& Indices = CurrentMesh->GetIndices();

		for (size_t i = 0; i + 2 < Indices.size(); i += 3)
		{
			UINT IndexA = Indices[i];
			UINT IndexB = Indices[i + 1];
			UINT IndexC = Indices[i + 2];

			if (IndexA >= Vertices.size() || IndexB >= Vertices.size() || IndexC >= Vertices.size()) continue;

			const Vertex& VertexA = Vertices[IndexA];
			const Vertex& VertexB = Vertices[IndexB];
			const Vertex& VertexC = Vertices[IndexC];

			DirectX::XMVECTOR LocalA = { VertexA.x, VertexA.y, VertexA.z, 1 };
			DirectX::XMVECTOR LocalB = { VertexB.x, VertexB.y, VertexB.z, 1 };
			DirectX::XMVECTOR LocalC = { VertexC.x, VertexC.y, VertexC.z, 1 };

			DirectX::XMVECTOR WorldA = DirectX::XMVector3TransformCoord(LocalA, WorldMatrix);
			DirectX::XMVECTOR WorldB = DirectX::XMVector3TransformCoord(LocalB, WorldMatrix);
			DirectX::XMVECTOR WorldC = DirectX::XMVector3TransformCoord(LocalC, WorldMatrix);

			Triangle NewTriangle = {};
			DirectX::XMStoreFloat3(&NewTriangle.A, WorldA);
			DirectX::XMStoreFloat3(&NewTriangle.B, WorldB);
			DirectX::XMStoreFloat3(&NewTriangle.C, WorldC);

			RaycastHitResult TriangleHitResult = {};
			if (RaycastTriangle(TestRay, NewTriangle, ClosestDistance, TriangleHitResult))
			{
				bHit = true;
				ClosestDistance = TriangleHitResult.Distance;
				OutHit = TriangleHitResult;
			}
		}
	}

	if (bHit)
	{
		OutHit.HitActor = GetOwner();
	}

	return bHit;
}
