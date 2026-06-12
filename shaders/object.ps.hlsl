struct Pixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

cbuffer Mapping : register(b0) {
    uint tex_index;
}
Texture2D Textures[8] : register(t0);
SamplerState Sampler : register(s0);

float4 main(Pixel p) : SV_Target {
    float3 tex_color = Textures[NonUniformResourceIndex(tex_index)].Sample(Sampler, p.uv);
    return float4(tex_color, 1.0);
}