// shaders.hlsl

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 mWorldViewProj;
};

cbuffer OffsetBuffer : register(b1)
{
    float4x4 instanceWorldViewProj;
    float4x4 instanceWorld;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PsInput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR;
    float2 texcoords : TEXCOORD;
};

static const float4 SkyColor = float4(0.8f, 0.85f, 1.0f, 1.0f);

PsInput VsMain(float3 position : POSITION, float3 normal : NORMAL, float3 tangent : TANGENT, float2 texcoords : TEXCOORD)
{
    PsInput result;

    result.position = mul(instanceWorldViewProj, float4(position, 1.0));

    const float4 SunColor = float4(1.0, 1.0, 1.0, 1.0);
    const float4 AmbientColor = float4(0.25, 0.25, 0.25, 0.25);
 
    float3 worldNormal = mul(instanceWorld, float4(normal, 0.0)).xyz;
    float4 light = lerp(AmbientColor,
                        SunColor,
                        saturate(dot(worldNormal, normalize(float3(1.0, 1.0, 1.0))).xxxx));
    float4 color = light; // lerp(light, SkyColor, pow(saturate(result.position.z / result.position.w), 256.0));
    result.color = color;

    result.texcoords = texcoords;

    return result;
}

float4 PsMain(PsInput input) : SV_TARGET
{
    float4 texCol = gTexture.Sample(gSampler, input.texcoords);
    if (texCol.a  < 0.1)
    {
        discard;
    }
    
    float4 output = input.color * texCol;
    output = lerp(output, SkyColor, pow(input.position.z, 256.0));
    return output;
}
