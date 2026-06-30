cbuffer UIPresets : register(b0) {
    uint width;
    uint height;
}

struct Pixel {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    uint tex_idx : TEXIDX;
};

SamplerState Sampler : register(s0);

float4 main(Pixel p) : SV_Target {
    Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(p.tex_idx)];
    float color = tex.Sample(Sampler, p.uv);
    return float4(p.color.rgb, p.color.a * color);
}