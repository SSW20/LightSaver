struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

Texture2D UITexture : register(t0);
SamplerState UISampler : register(s0);

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.color = input.color;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    
    return output;
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    float4 TextureColor = UITexture.Sample(UISampler, input.uv);
    return input.color * TextureColor;
}