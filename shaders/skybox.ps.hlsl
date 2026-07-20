struct Pixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    uint dir : SKYBOXDIR;
};

cbuffer Presets : register(b0) {
    row_major float4x4 world;
    uint back_tex_idx : BACKIDX;
    uint bottom_tex_idx : BOTTOMIDX;
    uint front_tex_idx : FRONTIDX;
    uint left_tex_idx : LEFTIDX;
    uint right_tex_idx : RIGHTIDX;
    uint top_tex_idx : TOPIDX;
}

SamplerState LinearSampler : register(s0);

float4 main(Pixel p) : SV_Target {
    Texture2D back_tex = ResourceDescriptorHeap[NonUniformResourceIndex(back_tex_idx)];
    Texture2D bottom_tex = ResourceDescriptorHeap[NonUniformResourceIndex(bottom_tex_idx)];
    Texture2D front_tex = ResourceDescriptorHeap[NonUniformResourceIndex(front_tex_idx)];
    Texture2D left_tex = ResourceDescriptorHeap[NonUniformResourceIndex(left_tex_idx)];
    Texture2D right_tex = ResourceDescriptorHeap[NonUniformResourceIndex(right_tex_idx)];
    Texture2D top_tex = ResourceDescriptorHeap[NonUniformResourceIndex(top_tex_idx)];
    float4 sample_color;
    switch (p.dir) {
        case 0: sample_color = back_tex.Sample(LinearSampler, p.uv); break;
        case 1: sample_color = bottom_tex.Sample(LinearSampler, p.uv); break;
        case 2: sample_color = front_tex.Sample(LinearSampler, p.uv); break;
        case 3: sample_color = left_tex.Sample(LinearSampler, p.uv); break;
        case 4: sample_color = right_tex.Sample(LinearSampler, p.uv); break;
        case 5: sample_color = top_tex.Sample(LinearSampler, p.uv); break;
        default: sample_color = float4(0, 0, 0, 1); break;
    }
    return sample_color;
    return float4(sample_color);
}