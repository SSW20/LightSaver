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
    float3 SpotDirection;
    float AmbientStrength;

    float3 LightColor;
    float DiffuseStrength;

    float3 LightPosition;
    float LightRange;

    float SpotOuterCos;
    float SpotInnerCos;
    float2 Padding;

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
    float3 worldPosition : POSITION1;
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
    output.worldPosition = worldPosition.xyz;


    return output;
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    //return DiffuseTexture.Sample(DiffuseSampler, input.texcoord);
    //float3 NormalColor = (normalize(input.normal) * 0.5f + 0.5f);
    //return float4(NormalColor, 1.0f);

    float4 TextureColor = DiffuseTexture.Sample(DiffuseSampler, input.texcoord);
    float3 Normal = normalize(input.normal);

    float3 ToLightDir = normalize(LightPosition - input.worldPosition);
    float ToLightDistance = length(LightPosition - input.worldPosition);

    float DistanceAttenuation = saturate(1 - ToLightDistance / LightRange);
    float3 ToObjDir = -ToLightDir;
    float SpotCos = dot(normalize(SpotDirection), ToObjDir);
    float SpotAttenuation = smoothstep(SpotOuterCos, SpotInnerCos, SpotCos);

    float Diffuse = saturate(dot(Normal, ToLightDir));

    float3 AmbientLight = TextureColor.rgb * AmbientStrength;
    float3 DiffuseLight = TextureColor.rgb * LightColor * Diffuse * DiffuseStrength * DistanceAttenuation * SpotAttenuation;


    return float4(AmbientLight + DiffuseLight, TextureColor.a);

}
