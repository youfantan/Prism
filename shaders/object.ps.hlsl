struct Pixel
{
    float4 position : SV_Position;
    float4 light_position : LPOS;
    float3 world_position : WPOS;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    uint tex_idx : TEXIDX;
    uint normal_tex_idx : NTEXIDX;
    uint rough_tex_idx : RTEXIDX;
};

cbuffer Presets : register(b1) {
    row_major float4x4 world;
}

cbuffer Scene : register(b0) {
    row_major float4x4 vp;
    row_major float4x4 light_vp;
    float4 ambient_light;
    float4 camera_pos;
    uint dotlight_count;
    float4 dotlight_pos[4];
    float4 dotlight_color[4];
    uint4 shadow_index;
}

SamplerState LinearSampler : register(s0);
SamplerComparisonState CompSampler : register(s1);

float4 main(Pixel p) : SV_Target {
    Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(p.tex_idx)];
    Texture2D shadow_map = ResourceDescriptorHeap[NonUniformResourceIndex(shadow_index[0])];
    float3 N = normalize(p.normal);
    float3 V = normalize(camera_pos.xyz - p.world_position);
    float3 tex_color = tex.Sample(LinearSampler, p.uv);
    float3 ndc = p.light_position.xyz / p.light_position.w;
    float2 uv = ndc.xy * 0.5 + 0.5;
    float d = ndc.z;
    float shadow = shadow_map.SampleCmpLevelZero(CompSampler, uv, d);
    float3 result = (float3)0;
    for (uint i = 0; i < dotlight_count; i++) {
        float3 L = normalize(dotlight_pos[i].xyz - p.world_position);
        float diffuse = saturate(dot(N, L));
        float3 R = reflect(-L, N);
        float intensity = pow(saturate(dot(R, V)), 32);
        result += (dotlight_color[i].rgb * diffuse * tex_color + intensity * dotlight_color[i].rgb) * shadow;
    }
    float3 ambient = float3(0.1, 0.1, 0.1) * tex_color;
    return float4(result + ambient, 1.0);
}
