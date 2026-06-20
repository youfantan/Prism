cbuffer UIPresets : register(b0) {
    uint tex_index;
    uint sb_index;
    uint width;
    uint height;
    float4 text_color;
    float scale_factor;
}

struct Pixel {
    float4 position : SV_Position;
    float4 debug : DEBUG;
    float2 uv : TEXCOORD;
};

Texture2D Textures[16] : register(t0);
SamplerState Sampler : register(s0);

float4 main(Pixel p) : SV_Target {
    float color = Textures[NonUniformResourceIndex(tex_index)].Sample(Sampler, p.uv);
    return float4(text_color.rgb, text_color.a * color);
}