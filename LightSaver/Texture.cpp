#include "Texture.h"
#include <vector>
bool Texture::Initialize(ID3D11Device* Device, const wchar_t* FilePath)
{
	/*IWICImagingFactory
		→ Decoder 생성,  WIC 객체를 만들어주는 공장

		IWICBitmapDecoder
		→ 파일 열기, 실제 PNG/JPG 파일을 열고 파일 구조를 해석하는 객체

		IWICBitmapFrameDecode
		→ 이미지 프레임 가져오기, 그 파일 안의 실제 이미지 한 장

		IWICFormatConverter
		→ RGBA 형식으로 변환
	*/

	// 공장 건설
	IWICImagingFactory* ImageFactory = nullptr;
	HRESULT result =  CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ImageFactory));
	if (FAILED(result)) return false;

	// 파일 열고 구조 해석
	IWICBitmapDecoder* Decoder = nullptr;
	result = ImageFactory->CreateDecoderFromFilename(FilePath, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &Decoder);
	if (FAILED(result)) return false;

	// 이미지 가져오기
	IWICBitmapFrameDecode* Frame = nullptr;
	result = Decoder->GetFrame(0,&Frame);
	if (FAILED(result)) return false;

	UINT Width = 0;
	UINT Height = 0;
	result = Frame->GetSize(&Width,&Height);
	if (FAILED(result)) return false;

	// 이미지 변환기
	IWICFormatConverter* Converter = nullptr;
	ImageFactory->CreateFormatConverter(&Converter);
	if (FAILED(result)) return false;

	result = Converter->Initialize(Frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
	if (FAILED(result)) return false;

	const UINT BytesPerPixel = 4;
	const UINT Stride = Width * BytesPerPixel;
	const UINT Size = Stride * Height;

	// 이미지 CPU 바이트 배열로 저장
	std::vector<unsigned char> PixelData(Size);
	result = Converter->CopyPixels(nullptr, Stride, Size, PixelData.data());
	if (FAILED(result)) return false;

	D3D11_TEXTURE2D_DESC TextureDesc = {};

	TextureDesc.Width = Width;
	TextureDesc.Height = Height;

	TextureDesc.MipLevels = 1;		// 원본 크기
	TextureDesc.ArraySize = 1;		// 이미지 1장
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	TextureDesc.SampleDesc.Count = 1;		// MSAA 안씀
	TextureDesc.SampleDesc.Quality = 0;

	TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = PixelData.data();

	InitialData.SysMemPitch = Stride;
	InitialData.SysMemSlicePitch = 0;

	ID3D11Texture2D* Image = nullptr;
	Device->CreateTexture2D(&TextureDesc, &InitialData, &Image);
	Device->CreateShaderResourceView(Image, nullptr, &SRV);

	D3D11_SAMPLER_DESC SamplerDesc = {};
	// 선형 보간
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;	
		
	// U, V 가 범위를 벗어났을 떄
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;		
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	// 밉맵 단계 지정
	SamplerDesc.MinLOD = 0.0f;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&SamplerDesc, &Sampler);

	Image->Release();
	Image = nullptr;

	Frame->Release();
	Frame = nullptr;

	Decoder->Release();
	Decoder = nullptr;

	ImageFactory->Release();
	ImageFactory = nullptr;



	return true;

}

void Texture::Bind(ID3D11DeviceContext* DeviceContext)
{
	DeviceContext->PSSetShaderResources(0, 1, &SRV);
	DeviceContext->PSSetSamplers(0, 1, &Sampler);
}

void Texture::InitializeByColor(ID3D11Device* Device, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	// 이미지 CPU 바이트 배열로 저장
	unsigned char PixelData[4] = { r,g,b,a };

	D3D11_TEXTURE2D_DESC TextureDesc = {};

	TextureDesc.Width = 1;
	TextureDesc.Height = 1;

	TextureDesc.MipLevels = 1;		// 원본 크기
	TextureDesc.ArraySize = 1;		// 이미지 1장
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	TextureDesc.SampleDesc.Count = 1;		// MSAA 안씀
	TextureDesc.SampleDesc.Quality = 0;

	TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = PixelData;

	InitialData.SysMemPitch = 4;
	InitialData.SysMemSlicePitch = 0;

	ID3D11Texture2D* Image = nullptr;
	Device->CreateTexture2D(&TextureDesc, &InitialData, &Image);
	Device->CreateShaderResourceView(Image, nullptr, &SRV);

	D3D11_SAMPLER_DESC SamplerDesc = {};
	// 선형 보간
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

	// U, V 가 범위를 벗어났을 떄
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	// 밉맵 단계 지정
	SamplerDesc.MinLOD = 0.0f;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&SamplerDesc, &Sampler);

	Image->Release();
	Image = nullptr;

}

Texture::~Texture()
{
	if (SRV != nullptr)
	{
		SRV->Release();
		SRV = nullptr;
	}

	if (Sampler != nullptr)
	{
		Sampler->Release();
		Sampler = nullptr;
	}
}
