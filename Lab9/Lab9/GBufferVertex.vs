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
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float3 Tangent : TEXCOORD3;
    float3 Bitangent : TEXCOORD4;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.Pos, 1.0f), Model);
    output.WorldPos = worldPos.xyz;
    output.Pos = mul(worldPos, vp);
    output.Normal = normalize(mul(input.Normal, (float3x3)Model));
    output.TexCoord = input.TexCoord;

    float3 tangent;
    if (abs(input.Normal.z) > 0.999f)
        tangent = float3(1.0f, 0.0f, 0.0f);
    else
        tangent = normalize(cross(input.Normal, float3(0.0f, 0.0f, 1.0f)));

    float3 bitangent = normalize(cross(input.Normal, tangent));
    output.Tangent = normalize(mul(tangent, (float3x3)Model));
    output.Bitangent = normalize(mul(bitangent, (float3x3)Model));

    return output;
}
