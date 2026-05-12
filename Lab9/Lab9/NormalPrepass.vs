cbuffer MatrixBuffer : register(b0)
{
    matrix Model;
};

cbuffer CameraBuffer : register(b1)
{
    matrix vp;
    matrix view;
    float3 CameraPos;
    float CameraPadding;
};

struct VS_INPUT
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 Normal : TEXCOORD0;
    float2 TexCoord : TEXCOORD1;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.Pos, 1.0f), Model);
    output.Pos = mul(worldPos, vp);
    output.Normal = normalize(mul(input.Normal, (float3x3)Model));
    output.TexCoord = input.TexCoord;

    return output;
}
