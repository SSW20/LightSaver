#pragma once
#include <DirectXMath.h>
#include "World.h"

struct NavigationCell
{
	DirectX::XMFLOAT3 Position = {};
	DirectX::XMFLOAT3 Normal = {};
	bool bWalkable = false;
};

class NavigationGrid
{
public:
	bool Build(World& GameWorld, const DirectX::XMFLOAT3& MinBounds, const DirectX::XMFLOAT3& MaxBounds, float InCellSize, const DirectX::XMFLOAT3& ActorHalfSize);
	bool ConvertToCell(const DirectX::XMFLOAT3& WorldPosition, int& OutColumn, int& OutRow);
	void GetWalkableNeighborIndices(int Column, int Row, std::vector<int>& OutNeighborIndices);
	const NavigationCell* GetCell(int Column, int Row) const;
	bool FindPath(const DirectX::XMFLOAT3& StartWorldPosition, const DirectX::XMFLOAT3& GoalWorldPosition, std::vector<DirectX::XMFLOAT3>& OutPath);
private:
	int GetIndex(int Column, int Row) const;

	std::vector<NavigationCell> Cells;

	int ColumnCount = 0;
	int RowCount = 0;
	float CellSize = 1.0f;

	DirectX::XMFLOAT3 MinBounds = {};
	DirectX::XMFLOAT3 MaxBounds = {};

	struct NavigationNode
	{
		float GCost = std::numeric_limits<float>::infinity();
		float HCost = 0.0f;
		int ParentIndex = -1;

		bool bOpen = false;
		bool bClosed = false;

		float GetFCost() const { return GCost + HCost; }
	};
};
