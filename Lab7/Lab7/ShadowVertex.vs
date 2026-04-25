cbuffer MatrixBuffer : register(b0)
{
	matrix Model;
};

cbuffer ShadowCameraBuffer : register(b4)
{
	matrix LightViewProj;
};
struct VS_INPUT
{
	float3 Pos : POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	float4 worldPos = mul(float4(input.Pos, 1.0f), Model);
	output.Pos = mul(worldPos, LightViewProj);
	output.TexCoord = input.TexCoord;
	return output;
}