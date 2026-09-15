#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include<unordered_map>
#include <memory>
#include "SkeletalMesh.h"
using namespace DirectX;

struct aiMesh;
struct aiNode;
struct SkeletalModelData
{
    std::unique_ptr<SkeletalMesh> MeshData;
    UINT MaterialIndex = 0;
};

struct SkeletalNode
{
    std::string Name;
    DirectX::XMFLOAT4X4 LocalTransform = {};
    std::vector<SkeletalNode> Children;
};

struct BoneInfo
{
    std::string BoneName;
    XMFLOAT4X4 OffsetMatrix = {};
};

class SkeletalModel
{
public:
    bool Initialize(ID3D11Device* Device, const std::string& FilePath);
    const SkeletalNode& GetRootNode() const { return RootNode; }
    size_t GetBoneCount() const{return BoneInfos.size(); }
    bool FindBoneIndex(const std::string& BoneName, UINT& OutBoneIndex);
    const BoneInfo* GetBoneInfo(UINT BoneIndex) const { return &BoneInfos[BoneIndex]; }
    void Draw(ID3D11DeviceContext* DeviceContext);
private:
    void CopyNodeTree(const aiNode* SourceNode, SkeletalNode& DestinationNode);
    std::unique_ptr<SkeletalMesh> ProcessMesh(ID3D11Device* Device, aiMesh* SourceMesh);
    UINT FindOrCreateBoneIndex(const std::string& BoneName);
    std::vector<BoneInfo> BoneInfos;
    std::unordered_map<std::string, UINT> BoneInfoMap;

    SkeletalNode RootNode;
    std::vector<SkeletalModelData> SkeletalModelDatas;
};