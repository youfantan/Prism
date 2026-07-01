struct Pixel
{
    float4 position : SV_Position;
    float4 light_position : LPOS;
    float3 world_position : WPOS;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

cbuffer Presets : register(b1) {
    row_major float4x4 world;
    uint tex_index;
    uint shadow_index;
}

cbuffer Scene : register(b0) {
    row_major float4x4 vp;
    row_major float4x4 light_vp;
    float4 camera_pos;
    uint dotlight_count;
    float3 dotlight_pos[16];
    float3 dotlight_color[16];
}

SamplerState LinearSampler : register(s0);
SamplerComparisonState CompSampler : register(s1);


float4 main(Pixel p) : SV_Target {
    Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(tex_index)];
    Texture2D shadow_map = ResourceDescriptorHeap[NonUniformResourceIndex(shadow_index)];
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
        result += dotlight_color[i].rgb * diffuse * tex_color + intensity * dotlight_color[i].rgb;
    }
    return float4(result * shadow, 1.0);
}
