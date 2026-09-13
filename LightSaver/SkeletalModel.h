#pragma once
#include <d3d11.h>
#include <string>

class SkeletalModel
{
public:
    bool Initialize(ID3D11Device* Device, const std::string& FilePath);

private:
};