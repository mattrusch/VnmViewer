// shaders.hlsl

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 mWorldViewProj;
};

cbuffer OffsetBuffer : register(b1)
{
    float4x4 instanceWorldViewProj;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PsInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoords : TEXCOORD;
};

PsInput VsMain(float3 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float2 texcoord : TEXCOORD)
{
    PsInput result;

    result.position = mul(instanceWorldViewProj, float4(position, 1.0));
    result.color = float4(normal * 0.5 + 0.5, 1.0);//color;
    result.texcoords = float2(1.0, 1.0);//texcoords;

    return result;
}

float4 PsMain(PsInput input) : SV_TARGET
{
    float4 texCol = gTexture.Sample(gSampler, input.texcoords);
    return input.color * texCol;
}
