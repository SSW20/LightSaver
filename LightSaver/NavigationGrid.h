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
    bool Build(World& GameWorld,const DirectX::XMFLOAT3& MinBounds, const DirectX::XMFLOAT3& MaxBounds,float InCellSize, const DirectX::XMFLOAT3& ActorHalfSize);
    bool ConvertToCell(const DirectX::XMFLOAT3& WorldPosition, int& OutColumn, int& OutRow);

    const NavigationCell* GetCell(int Column, int Row) const;

private:
    int GetIndex(int Column, int Row) const;

    std::vector<NavigationCell> Cells;

    int ColumnCount = 0;
    int RowCount = 0;
    float CellSize = 1.0f;

    DirectX::XMFLOAT3 MinBounds = {};
    DirectX::XMFLOAT3 MaxBounds = {};
};
