cbuffer MatrixBuffer : register(b0)
{
	matrix Model;
};

cbuffer CameraBuffer : register(b1)
{
	matrix vp;
	float3 CameraPos;
};

struct VS_INPUT
{
	float3 Pos : POSITION;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float3 Dir : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT o;
	float4 worldPos = mul(float4(input.Pos, 1.0f), Model);
	o.Pos = mul(worldPos, vp);
	o.Dir = normalize(input.Pos);
	return o;
}
