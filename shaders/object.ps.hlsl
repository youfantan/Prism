struct Pixel
{
    float4 position : SV_Position;
    float3 w_position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

cbuffer Presets : register(b0) {
    row_major float4x4 world;
    uint tex_index;
}

cbuffer Scene : register(b1) {
    row_major float4x4 vp;
    float4 camera_pos;
    uint dotlight_count;
    float3 dotlight_pos[16];
    float3 dotlight_color[16];
}

SamplerState Sampler : register(s0);

float4 main(Pixel p) : SV_Target {
    Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(tex_index)];
    float3 N = normalize(p.normal);
    float3 V = normalize(camera_pos.xyz - p.w_position);
    float3 tex_color = tex.Sample(Sampler, p.uv);
    float3 result = (float3)0;
    for (uint i = 0; i < dotlight_count; i++) {
        float3 L = normalize(dotlight_pos[i].xyz - p.w_position);
        float diffuse = saturate(dot(N, L));
        float3 R = reflect(-L, N);
        float intensity = pow(saturate(dot(R, V)), 32);
        result += dotlight_color[i].rgb * diffuse * tex_color + intensity * dotlight_color[i].rgb;
    }
    return float4(result, 1.0);
}
