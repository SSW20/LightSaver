#pragma once
#include <d3d11.h>
#include <wincodec.h>

#pragma comment(lib, "windowscodecs.lib")
class Texture
{
public:
    bool Initialize(ID3D11Device* Device,const wchar_t* FilePath);

    void Bind(ID3D11DeviceContext* DeviceContext);

    void InitializeByColor(ID3D11Device* Device, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    ~Texture();

private:
    ID3D11ShaderResourceView* SRV = nullptr;
    ID3D11SamplerState* Sampler = nullptr;
};