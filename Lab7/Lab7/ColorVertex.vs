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

cbuffer ShadowLightBuffer : register(b4)
{
    matrix LightViewProj[4];
    float4 CascadeSplits;
    float4 ShadowLightDirStrength;
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
    float3 CameraPos : TEXCOORD5;
    float4 ShadowPos0 : TEXCOORD6;
    float4 ShadowPos1 : TEXCOORD7;
    float4 ShadowPos2 : TEXCOORD8;
    float4 ShadowPos3 : TEXCOORD9;
    float ViewDepth : TEXCOORD10;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(float4(input.Pos, 1.0f), Model);
    output.WorldPos = worldPos.xyz;
    output.ShadowPos0 = mul(worldPos, LightViewProj[0]);
    output.ShadowPos1 = mul(worldPos, LightViewProj[1]);
    output.ShadowPos2 = mul(worldPos, LightViewProj[2]);
    output.ShadowPos3 = mul(worldPos, LightViewProj[3]);

    float4 viewPos = mul(worldPos, view);
    output.ViewDepth = max(viewPos.z, 0.0f);


    output.Pos = mul(worldPos, vp);
    output.Normal = normalize(mul(input.Normal, (float3x3)Model));
    output.TexCoord = input.TexCoord;
    output.CameraPos = CameraPos;

    float3 tangent;
    if (abs(input.Normal.z) > 0.999f)
    {
        tangent = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        tangent = normalize(cross(input.Normal, float3(0, 0, 1)));
    }

    float3 bitangent = normalize(cross(input.Normal, tangent));
    output.Tangent = mul(tangent, (float3x3)Model);
    output.Bitangent = mul(bitangent, (float3x3)Model);

    return output;
}