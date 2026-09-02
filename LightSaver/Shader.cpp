#include "Shader.h"
#include <d3dcompiler.h>

bool Shader::Initialize(ID3D11Device* Device, const wchar_t* FilePath, const D3D11_INPUT_ELEMENT_DESC* layout, int NumElements)
{
	ID3DBlob* VSBlob = nullptr;
	ID3DBlob* PSBlob = nullptr;
	ID3DBlob* ErrBlob = nullptr;

	HRESULT result;
	result = D3DCompileFromFile(FilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS_Main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &VSBlob, &ErrBlob);
	if (FAILED(result)) return false;
	result = D3DCompileFromFile(FilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS_Main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &PSBlob, &ErrBlob);
	if (FAILED(result)) return false;

	result = Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &VS);
	if (FAILED(result)) return false;

	result = Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &PS);
	if (FAILED(result)) return false;
	result = Device->CreateInputLayout(layout, NumElements, VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), &InputLayout);
	if (FAILED(result)) return false;


	VSBlob->Release();
	PSBlob->Release();
	return true;
}



void Shader::Bind(ID3D11DeviceContext* DeviceContext)
{
	DeviceContext->IASetInputLayout(InputLayout);
	DeviceContext->VSSetShader(VS, nullptr, 0);
	DeviceContext->PSSetShader(PS, nullptr, 0);
}

Shader::~Shader()
{
	if (VS != nullptr)
	{
		VS->Release();
		VS = nullptr;
	}
	if (PS != nullptr)
	{
		PS->Release();
		PS = nullptr;
	}
	if (InputLayout != nullptr)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
}
