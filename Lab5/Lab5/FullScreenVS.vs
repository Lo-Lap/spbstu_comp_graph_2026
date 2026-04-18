struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = float4(input.Pos, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}