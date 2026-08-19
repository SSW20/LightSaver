cbuffer CameraBuffer : register(b0)
{
    matrix View;
    matrix Projection;
}

cbuffer ObjectBuffer : register(b1)
{
    matrix World;
}

cbuffer LightBuffer : register(b2)
{
    float3 ToLightDirection;
    float AmbientStrength;

    float3 LightColor;
    float DiffuseStrength;
}

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);


struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
};

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;

    float4 localPosition = float4(input.position, 1.0f);
    float4 worldPosition = mul(localPosition,World);
    float4 viewPosition = mul(worldPosition, View);
    output.position = mul(viewPosition, Projection);
    output.texcoord = input.texcoord;
    output.normal = mul(input.normal, (float3x3)World);


    return output;
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    //return DiffuseTexture.Sample(DiffuseSampler, input.texcoord);
    //float3 NormalColor = (normalize(input.normal) * 0.5f + 0.5f);
    //return float4(NormalColor, 1.0f);

    float4 TextureColor = DiffuseTexture.Sample(DiffuseSampler, input.texcoord);
    float3 Normal = normalize(input.normal);
    float3 ToLight = normalize(ToLightDirection);
    float Diffuse = saturate(dot(Normal, ToLight));

    float3 AmbientLight = TextureColor.rgb * AmbientStrength;
    float3 DiffuseLight = TextureColor.rgb * LightColor * Diffuse * DiffuseStrength;


    return float4(AmbientLight + DiffuseLight, TextureColor.a);

}
