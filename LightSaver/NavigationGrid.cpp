#include "NavigationGrid.h"
#include <cmath>

bool NavigationGrid::Build(World& GameWorld, const DirectX::XMFLOAT3& InMinBounds, const DirectX::XMFLOAT3& InMaxBounds, float InCellSize, const DirectX::XMFLOAT3& ActorHalfSize)
{
    MinBounds = InMinBounds;
    MaxBounds = InMaxBounds;
    CellSize = InCellSize;

    ColumnCount = std::ceil((InMaxBounds.x - InMinBounds.x) / InCellSize);
    RowCount = std::ceil((InMaxBounds.z - InMinBounds.z) / InCellSize);

    Cells.clear();
    Cells.resize(RowCount * ColumnCount);


    for (int Row = 0; Row < RowCount; ++Row)
    {
        for (int Column = 0; Column < ColumnCount; ++Column)
        {
            float CellX = InMinBounds.x + (Column + 0.5f) * CellSize;
            float CellY = InMaxBounds.y;
            float CellZ = InMinBounds.z + (Row + 0.5f) * CellSize;

            DirectX::XMFLOAT3 SamplePosition = { CellX, CellY, CellZ };
            NavigationCell& Cell = Cells[GetIndex(Column, Row)];
         
            float RayDistance = InMaxBounds.y - InMinBounds.y;
            RaycastHitResult GroundHit = {};
            bool bHit = GameWorld.FindFloor(SamplePosition, 0.0f, RayDistance, GroundHit);

            if (bHit)
            {
                DirectX::XMFLOAT3 ActorPos = { GroundHit.Position.x, GroundHit.Position.y + ActorHalfSize.y, GroundHit.Position.z };
                AABB ActorCollisionBox = CreateAABBFromCenter(ActorPos, ActorHalfSize);
                bool bBlocked = GameWorld.OverlapAABB(ActorCollisionBox);

                Cell.Position = GroundHit.Position;
                Cell.Normal = GroundHit.Normal;
                Cell.bWalkable = !bBlocked;
            }
            else
            {
                Cell.bWalkable = false;
                Cell.Position = SamplePosition;
            }
            
        }
    }
    return true;
}

bool NavigationGrid::ConvertToCell(const DirectX::XMFLOAT3& WorldPosition, int& OutColumn, int& OutRow)
{
    float LocalX = WorldPosition.x - MinBounds.x;
    float LocalZ = WorldPosition.z - MinBounds.z;

    OutColumn = std::floor(LocalX / CellSize);
    OutRow = std::floor(LocalZ / CellSize);

    if (OutColumn < 0 || OutColumn >= ColumnCount || OutRow < 0 || OutRow >= RowCount) return false;
    return true;
}

const NavigationCell* NavigationGrid::GetCell(int Column, int Row) const
{
    return &Cells[GetIndex(Column, Row)];
}

int NavigationGrid::GetIndex(int Column, int Row) const
{
    return Row * ColumnCount + Column;
}
