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

void NavigationGrid::GetWalkableNeighborIndices(int Column, int Row, std::vector<int>& OutNeighborIndices)
{
    float MaxStepHeight = 0.4f;
    int Neighbor[4][2] = {{  0, -1 },{  1,  0 }, {  0,  1 }, { -1,  0 } };
    OutNeighborIndices.clear();

    const NavigationCell* CurrentCell = GetCell(Column, Row);
    if (CurrentCell == nullptr || !CurrentCell->bWalkable) return;

    for (int i = 0; i < 4; ++i)
    {
        int NeighborColumn = Column + Neighbor[i][0];
        int NeighborRow = Row + Neighbor[i][1];

        const NavigationCell* NeighborCell = GetCell(NeighborColumn, NeighborRow);
        if (NeighborCell == nullptr || !NeighborCell->bWalkable) continue;
        float HeightDiff = std::abs(CurrentCell->Position.y - NeighborCell->Position.y);
        if (HeightDiff > MaxStepHeight) continue;

        OutNeighborIndices.push_back(GetIndex(NeighborColumn, NeighborRow));
    }
}

const NavigationCell* NavigationGrid::GetCell(int Column, int Row) const
{
    if (Column < 0 || Column >= ColumnCount || Row < 0 || Row >= RowCount) return nullptr;
    return &Cells[GetIndex(Column, Row)];
}

bool NavigationGrid::FindPath(const DirectX::XMFLOAT3& StartWorldPosition, const DirectX::XMFLOAT3& GoalWorldPosition, std::vector<DirectX::XMFLOAT3>& OutPath)
{

    //    GCost       시작 셀부터 현재 셀까지 실제 이동한 비용
    //    HCost       현재 셀부터 목적지까지 남았다고 예상하는 비용
    //    F           G + H
    //    ParentIndex 현재 셀로 오기 직전의 셀
    //    bOpen       앞으로 조사할 후보인가
    //    bClosed     이미 조사가 끝났는가

    OutPath.clear();
    int StartColumn = 0;
    int StartRow = 0;

    int GoalColumn = 0;
    int GoalRow = 0;

    if (!ConvertToCell(StartWorldPosition, StartColumn, StartRow)) return false;
    if (!ConvertToCell(GoalWorldPosition, GoalColumn, GoalRow)) return false;

    const NavigationCell* StartCell = GetCell(StartColumn, StartRow);
    const NavigationCell* GoalCell = GetCell(GoalColumn, GoalRow);
    if (StartCell == nullptr || GoalCell == nullptr || !StartCell->bWalkable || !GoalCell->bWalkable) return false;


    int StartIndex = GetIndex(StartColumn, StartRow);
    int GoalIndex = GetIndex(GoalColumn, GoalRow);

    std::vector<NavigationNode> SearchNodes(Cells.size());

    NavigationNode& StartNode = SearchNodes[StartIndex];
    StartNode.GCost = 0.0f;
    StartNode.HCost = float(std::abs(StartColumn - GoalColumn) + std::abs(StartRow - GoalRow));
    StartNode.bOpen = true;

    std::vector<int> OpenList;
    OpenList.push_back(StartIndex);

    bool bFoundPath = false;

    while (!OpenList.empty())
    {
        int BestListPosition = 0;

        for (int i = 1; i < OpenList.size(); ++i)
        {
            int CandidateIndex = OpenList[i];
            int BestIndex = OpenList[BestListPosition];

            NavigationNode& CandidateNode = SearchNodes[CandidateIndex];
            NavigationNode& BestNode = SearchNodes[BestIndex];

            if (CandidateNode.GetFCost() < BestNode.GetFCost())
            {
                BestListPosition = i;
            }
        }
        int CurrentIndex = OpenList[BestListPosition];
        NavigationNode& CurrentNode = SearchNodes[CurrentIndex];
        OpenList.erase(OpenList.begin() + BestListPosition);
        CurrentNode.bClosed = true;
        CurrentNode.bOpen = false;
        if (CurrentIndex == GoalIndex)
        {
            bFoundPath = true;
            break;
        }

        int CurrentColumn = CurrentIndex % ColumnCount;
        int CurrentRow = CurrentIndex / ColumnCount;

        std::vector<int> NeighborIndices;
        GetWalkableNeighborIndices(CurrentColumn, CurrentRow, NeighborIndices);

        for (auto NeighborIndex : NeighborIndices)
        {
            NavigationNode& NeighborNode = SearchNodes[NeighborIndex];
            if (NeighborNode.bClosed) continue;
            if (CurrentNode.GCost + 1 >= NeighborNode.GCost) continue;

            NeighborNode.GCost = CurrentNode.GCost + 1;
            int NeighborRow = NeighborIndex / ColumnCount;
            int NeighborColumn = NeighborIndex % ColumnCount;
            NeighborNode.HCost = std::abs(GoalRow - NeighborRow) + std::abs(GoalColumn - NeighborColumn);
            NeighborNode.ParentIndex = CurrentIndex;

            if (!NeighborNode.bOpen)
            {
                NeighborNode.bOpen = true;
                OpenList.push_back(NeighborIndex);
            }
        }
    }
    if (!bFoundPath) return false;

    int PathIndex = GoalIndex;
    while (PathIndex != -1)
    {
        OutPath.push_back(Cells[PathIndex].Position);

        if (PathIndex == StartIndex) break;
        PathIndex = SearchNodes[PathIndex].ParentIndex;
    }

    if (OutPath.empty() || PathIndex != StartIndex)
    {
        OutPath.clear();
        return false;
    }
    std::reverse(OutPath.begin(),OutPath.end());
    return true;
}


int NavigationGrid::GetIndex(int Column, int Row) const
{
    return Row * ColumnCount + Column;
}
