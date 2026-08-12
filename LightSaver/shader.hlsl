struct VS_INPUT
{
    float3 position : POSITION;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
};

PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    output.position = float4(input.position, 1.0f);

    return output;
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
