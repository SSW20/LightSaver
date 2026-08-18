cbuffer CameraBuffer : register(b0)
{
    matrix View;
    matrix Projection;
}

cbuffer ObjectBuffer : register(b1)
{
    matrix World;
}

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);


struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;

};

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;

    float4 localPosition = float4(input.position, 1.0f);
    float4 worldPosition = mul(localPosition,World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);
    output.texcoord = input.texcoord;

    return output;
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    return DiffuseTexture.Sample(DiffuseSampler, input.texcoord);
}
