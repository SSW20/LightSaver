#pragma once
#include <d3d11.h>

class Shader
{
public:
	bool Initialize(ID3D11Device* Device, const wchar_t* FilePath);
	void Bind(ID3D11DeviceContext* DeviceContext);
	~Shader();

	ID3D11PixelShader* GetPS() const { return PS; };
	ID3D11VertexShader* GetVS() const { return VS; };
	ID3D11InputLayout* GetInputLayout() const { return InputLayout; };

private:
	ID3D11PixelShader* PS = nullptr;
	ID3D11VertexShader* VS = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
};
